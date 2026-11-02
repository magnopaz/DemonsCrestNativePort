#!/usr/bin/env python3
"""Boot the driver, replay the music-bank upload into an embedded APU, listen.

The end-to-end S1 experiment:

  1. cold-boot the oracle, run to --aram-frame (driver loaded, nothing playing),
     dump ARAM and the entry PC;
  2. run to --end-frame (the bank upload just finished) and dump the port ring;
  3. replay that CPU->APU traffic into `apu_host` with --handshake (each byte
     waits for the SPC's acknowledge, as the console's CPU did);
  4. render and report level + halt, so "did the driver start?" is a number.

Usage:
  python3 tools/audio/replay_upload.py --out captures/audio/replay
"""
from __future__ import annotations

import argparse
import json
import math
import os
import socket
import struct
import subprocess
import sys
import time
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECOMP = Path(os.environ.get("RECOMP_DIR", ROOT.parent / "DemonsCrestSNESRecomp"))
ROM = Path(os.environ.get("ROM", ROOT.parent / "roms" / "Demon's Crest (USA).sfc"))


class Client:
    def __init__(self, sock):
        self.f = sock.makefile("rwb")

    def cmd(self, t):
        self.f.write((t + "\n").encode())
        self.f.flush()
        line = self.f.readline()
        if not line:
            raise RuntimeError("closed")
        m = json.loads(line)
        if "error" in m:
            raise RuntimeError(f"{t}: {m['error']}")
        return m

    def frame(self):
        return int(self.cmd("frame")["frame"])

    def run_to(self, t):
        self.cmd(f"run_to_frame {t}")
        dl = time.monotonic() + 300
        while self.frame() < t:
            if time.monotonic() > dl:
                raise RuntimeError("timeout")
            time.sleep(0.004)


def connect(port, timeout=90.0):
    dl = time.monotonic() + timeout
    while time.monotonic() < dl:
        try:
            return Client(socket.create_connection(("127.0.0.1", port), timeout=3))
        except OSError:
            time.sleep(0.1)
    raise SystemExit("replay_upload: no debug server")


def wav_rms(path: Path) -> float:
    with wave.open(str(path)) as w:
        raw = w.readframes(w.getnframes())
    if not raw:
        return 0.0
    s = struct.unpack("<%dh" % (len(raw) // 2), raw)
    return math.sqrt(sum(v * v for v in s) / len(s))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--aram-frame", type=int, default=60)
    ap.add_argument("--end-frame", type=int, default=145)
    ap.add_argument("--mid-frame", type=int, default=132,
                    help="second port-ring sample; the ring keeps the last"
                         " 65536 events, so a long transfer needs two dumps")
    ap.add_argument("--seconds", type=float, default=30.0)
    ap.add_argument("--port", type=int, default=4409)
    ap.add_argument("--no-handshake", action="store_true")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ, SNESRECOMP_DEBUG_PORT=str(args.port),
               SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    log = open(args.out / "oracle.log", "wb")
    proc = subprocess.Popen([str(RECOMP / "build-trace" / "DemonsCrestSNESRecomp"),
                             str(ROM), "--paused"],
                            cwd=str(RECOMP), env=env, stdout=log, stderr=log)
    client = None
    try:
        client = connect(args.port)
        client.run_to(args.aram_frame)
        time.sleep(0.15)
        apu = client.cmd("get_apu_state")
        produced0 = int(client.cmd("audio_stats")["produced"])
        aram = bytes.fromhex(
            client.cmd("dump_apu_ram 0 65536")["hex"].replace(" ", ""))
        aram_path = args.out / "aram.bin"
        aram_path.write_bytes(aram)
        # The .spc at the same frame carries the synthesized $F0-$FF IO region
        # (timer target/enable), which ARAM alone does not describe.
        spc_path = (args.out / "io.spc").resolve()
        client.cmd(f"spc_dump {spc_path}")
        # Enter at the driver's port-poll loop, not at the sampled PC: the
        # sampled PC can be a RET (the driver was inside a CALL when dumped),
        # and the poll loop is the state the CPU talks to.
        entry = 0x046A
        print("sampled PC at dump:", apu["spc"]["pc"], "-> entry 0x046A")

        def dump_ring(tag: str) -> list[tuple[int, int, int]]:
            tsv = (args.out / f"apu_{tag}.tsv").resolve()
            dumped = client.cmd(f"port_events {tsv}")
            print(f"port ring {tag}:", dumped.get("events"), "of",
                  dumped.get("total"))
            out = []
            for line in tsv.read_text().splitlines():
                if line.startswith("#") or not line.strip():
                    continue
                _frame, kind, port, value, sample = line.split("\t")
                if kind == "cpu_wr" and int(sample) > produced0:
                    out.append((int(sample), int(port), int(value, 16)))
            return out

        # The ring only holds the last 65536 events, so a long byte-by-byte
        # transfer is sampled twice: the first dump has its head, the second
        # its tail. Merge on the sample clock.
        head = dump_ring("head") if args.mid_frame else []
        client.run_to(args.end_frame)
        time.sleep(0.15)
        tail = dump_ring("tail")
        writes = head
        if head:
            last = head[-1][0]
            writes = head + [w for w in tail if w[0] > last]
        else:
            writes = tail
        # Start at a block boundary: the ring can begin mid-transfer, and the
        # pacing heuristic needs to see the $CC that opens a block.
        start = next((i for i, w in enumerate(writes)
                      if w[1] == 0 and w[2] == 0xCC), None)
        if start is not None:
            print(f"trimming {start} writes before the first $CC block")
            writes = writes[start:]
        ports = args.out / "ports.txt"
        ports.write_text("\n".join(f"{s} {p} {v}" for s, p, v in writes) + "\n")
        print(f"replaying {len(writes)} writes (first at sample"
              f" {writes[0][0] - produced0 if writes else '-'} after the ARAM)"
              f" into an APU booted at PC 0x{entry:04X}")
    finally:
        if client:
            try:
                client.f.close()
            except Exception:
                pass
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
        log.close()

    if not writes:
        print("replay_upload: nothing to replay")
        return 1

    out_wav = args.out / "replay.wav"
    cmd = [str(ROOT / "build" / "apu_host"), str(ROM),
           "--aram", str(aram_path), "--entry", f"0x{entry:04X}",
           "--ports", str(ports), "--port-offset", str(writes[0][0]),
           "--spc-io", str(args.out / "io.spc"),
           "--out", str(out_wav), "--seconds", str(args.seconds), "--json"]
    if not args.no_handshake:
        cmd.append("--handshake")
    res = subprocess.run(cmd, capture_output=True, text=True)
    print(res.stdout.strip() or res.stderr.strip())
    if out_wav.is_file():
        print(f"rendered rms {wav_rms(out_wav):.1f} over {args.seconds}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
