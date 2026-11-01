#!/usr/bin/env python3
"""Fill a room's empty BG1 cells from an oracle capture of that same room.

Some rooms only get the screens the reference capture covered, so most of their
grid is zero and they render as a black band under the HUD (areas 43, 42, 30,
29).  Their atlases are complete — the *tilemap* is the gap.

This tool takes a captured frame, extracts the live BG1 tilemap, finds where it
sits inside the room's grid, and copies only the cells the room does not already
have.  It never overwrites extracted data: the ROM's own cells stay authoritative
and the capture fills the holes.

Usage:
  python3 tools/oracle/merge_capture_bg1.py --area 43 --capture captures/area43
  python3 tools/oracle/merge_capture_bg1.py --area 43 --capture captures/area43 --dry-run
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "dc_extract"))
from extract import tilemap_linear  # noqa: E402

MASK = 0x1FFF  # priority/flip bits are rewritten by the guest on load


def iter_capture_frames(capture_dir: Path, frame: int | None):
    """Yield (w, h, tiles, frame) for every frame dumped in the directory."""
    if frame is not None:
        frames = [frame]
    else:
        frames = sorted(int(p.name[1:7]) for p in capture_dir.glob("f*.vram.bin"))
    for n in frames:
        vram_path = capture_dir / f"f{n:06d}.vram.bin"
        state_path = capture_dir / f"f{n:06d}.json"
        if not vram_path.is_file() or not state_path.is_file():
            continue
        state = json.loads(state_path.read_text())
        ppu = state["ppu"]
        raw = vram_path.read_bytes()
        vram = [int.from_bytes(raw[i * 2:i * 2 + 2], "little") for i in range(0x8000)]
        bgsc = ppu["bgXsc"]
        if isinstance(bgsc, (list, tuple)):
            bgsc = bgsc[0]
        bgsc = int(bgsc, 16) if isinstance(bgsc, str) else int(bgsc)
        w, h, tiles = tilemap_linear(vram, bgsc)
        yield w, h, tiles, n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--area", type=int, required=True)
    ap.add_argument("--capture", type=Path, action="append", required=True,
                    help="capture dir; repeat for a tour of the room (each frame "
                         "fills the cells it covers)")
    ap.add_argument("--rooms", type=Path, default=ROOT / "assets" / "generated")
    ap.add_argument("--frame", type=int, default=None)
    ap.add_argument("--min-overlap", type=int, default=24,
                    help="non-zero cells that must agree before the alignment is trusted")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    room_path = args.rooms / f"room_area{args.area}.json"
    doc = json.loads(room_path.read_text())
    bg1 = [b for b in doc["bg"] if b["name"] == "bg1"][0]
    W, H, tiles = bg1["map_width"], bg1["map_height"], bg1["tilemap"]

    total_filled = 0
    for cap_path in args.capture:
        for cap_w, cap_h, cap, frame in iter_capture_frames(cap_path, args.frame):
            best = (0, 0, 0)  # matches, ox, oy
            for oy in range(0, max(1, H - cap_h + 1), 4):
                for ox in range(0, max(1, W - cap_w + 1), 4):
                    agree = 0
                    for y in range(cap_h):
                        row = (oy + y) * W + ox
                        for x in range(cap_w):
                            a = tiles[row + x]
                            b = cap[y * cap_w + x]
                            if a and b and (a & MASK) == (b & MASK):
                                agree += 1
                    if agree > best[0]:
                        best = (agree, ox, oy)
            agree, ox, oy = best
            if agree < args.min_overlap:
                print(f"area {args.area}: {cap_path.name} frame {frame} skipped "
                      f"({agree} < {args.min_overlap} agreeing cells)")
                continue

            filled = 0
            for y in range(cap_h):
                for x in range(cap_w):
                    idx = (oy + y) * W + ox + x
                    if not (tiles[idx] & 0x3FF) and (cap[y * cap_w + x] & 0x3FF):
                        tiles[idx] = cap[y * cap_w + x]
                        filled += 1
            total_filled += filled
            print(f"area {args.area}: frame {frame} at ({ox},{oy}) "
                  f"agree={agree} filled={filled}")

    total = sum(1 for t in tiles if t & 0x3FF)
    print(f"area {args.area}: filled {total_filled} cells total "
          f"({total}/{W * H} non-zero, {total / (W * H):.0%})")
    if not args.dry_run and total_filled:
        room_path.write_text(json.dumps(doc, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
