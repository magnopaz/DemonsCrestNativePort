#!/usr/bin/env python3
"""Bake Firebrand animation frames from a capture_anim.py dump.

Input is the directory produced by `tools/oracle/capture_anim.py`: one JSON
state snapshot plus one VRAM image per guest frame, and `segments.json`
labelling the controller phases. Output is a set of PNGs and
`player_anim.json` consumed by the native engine.

The player is composed exactly like the static idle sprite in extract.py:
the palette-7 OAM group, on-screen sprites only, OBJ tiles from the OAM
palette. Frames are canonicalized to face right; the renderer mirrors them
for left-facing movement. Each frame records an anchor measured from the
player object's 24-bit X, so wings and projectiles do not shift the body
between poses.

Example:

  python3 tools/dc_extract/extract_anim.py \
    --dump captures/anim --out assets/generated
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract as dc  # noqa: E402

CLIP_NAMES = ["idle", "walk", "jump", "hover", "fall", "land", "attack"]


class Pose:
    __slots__ = ("image", "hash", "anchor_x", "anchor_y", "parts",
                 "start", "duration")

    def __init__(self, image, digest, anchor_x, anchor_y, parts,
                 start, duration=1):
        self.image = image
        self.hash = digest
        self.anchor_x = anchor_x
        self.anchor_y = anchor_y
        self.parts = parts
        self.start = start
        self.duration = duration


def cgram_from_hex(text: str):
    raw = bytes.fromhex(text)
    if len(raw) != 0x200:
        raise SystemExit(f"expected 512-byte CGRAM, got {len(raw)}")
    words = struct.unpack("<256H", raw)
    return [((c & 31) * 8, ((c >> 5) & 31) * 8, ((c >> 10) & 31) * 8)
            for c in words]


def frame_paths(dump: Path, frame: int):
    stem = dump / f"f{frame:06d}"
    return stem.with_suffix(".json"), stem.with_suffix(".vram.bin")


def compose_player(dump: Path, frame: int, palette: int) -> Pose:
    json_path, vram_path = frame_paths(dump, frame)
    if not json_path.exists() or not vram_path.exists():
        raise FileNotFoundError(frame)
    state = json.loads(json_path.read_text())
    vram = dc.read_vram(vram_path)
    cgram = cgram_from_hex(state["cgram"])
    oam = bytes.fromhex(state["oam"]) + bytes.fromhex(state["highOam"])
    obsel = int(state["ppu"]["obsel"], 16)

    sprites = [s for s in dc.parse_oam(oam, obsel)
               if s.palette == 7 and s.y < 224]
    if not sprites:
        raise SystemExit(f"frame {frame}: no palette-7 player sprites")

    obj_base = (obsel & 7) << 13
    obj_vram = vram[obj_base:obj_base + 0x2000]
    index_img, ox, oy = dc.compose_oam_indices(obj_vram, sprites)
    colors = [cgram[0x80 + palette * 16 + i] for i in range(16)]
    image = dc.colorize(index_img, colors)
    image, ox, oy, bw, bh = dc.crop_sprite(image, ox, oy)

    # Canonical facing: the reference save starts facing right (OAM hflip
    # set). Mirror left-facing poses so one PNG works in both directions.
    hflipped = sum(1 for s in sprites if s.hflip) > len(sprites) / 2
    wram = bytes.fromhex(state["wram_1000"])
    object_x = (wram[0x30] | (wram[0x31] << 8) | (wram[0x32] << 16)) / 256.0
    body_screen_x = object_x - state["ppu"]["hScroll"][0]
    if hflipped:
        anchor_x = round(body_screen_x - ox)
        digest = hashlib.sha1(image.tobytes()).hexdigest()
    else:
        image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        anchor_x = round(bw - (body_screen_x - ox))
        digest = hashlib.sha1(image.tobytes()).hexdigest()

    return Pose(image, digest, anchor_x, bh, len(sprites), frame)


def segment_frames(meta: dict, name: str):
    for seg in meta["segments"]:
        if seg["name"] == name:
            return range(seg["start"] + 1, seg["end"] + 1)
    raise SystemExit(f"capture has no segment named '{name}'")


def pose_runs(meta: dict, dump: Path, name: str, palette: int) -> list[Pose]:
    runs: list[Pose] = []
    for frame in segment_frames(meta, name):
        try:
            pose = compose_player(dump, frame, palette)
        except FileNotFoundError:
            continue
        if runs and runs[-1].hash == pose.hash:
            runs[-1].duration += 1
        else:
            runs.append(pose)
    if not runs:
        raise SystemExit(f"segment '{name}' produced no player frames")
    return runs


def find_cycle(runs: list[Pose]):
    """Split a walk sequence into (pose cycle, steady-state timings).

    The first pose after starting to walk is an acceleration pose, and the
    final pose can be cut short by the stop segment. Look for two identical
    hash sequences and use the second one's durations.
    """
    for start in range(len(runs)):
        for length in range(3, min(9, (len(runs) - start) // 2 + 1)):
            a = [r.hash for r in runs[start:start + length]]
            b = [r.hash for r in runs[start + length:start + 2 * length]]
            if a == b:
                cycle = runs[start:start + length]
                timing = runs[start + length:start + 2 * length]
                return cycle, timing
    # Short walk: drop the acceleration pose and use what is left.
    return runs[1:], runs[1:]


def pick_clip(runs: list[Pose], count: int | None = None,
              offset: int = 0) -> list[Pose]:
    picked = runs[offset:offset + count] if count else runs[offset:]
    return picked or runs[:1]


def load_layout(path: Path | None):
    if path is None:
        return None
    return json.loads(path.read_text())


def build_clips(meta: dict, dump: Path, palette: int, layout: dict | None):
    if layout:
        # Optional override: {clip: {"segment": ..., "offset": ..., "count": ...,
        # "loop": ..., "max_parts": ..., "cycle": ...}}
        clips = {}
        for name in layout:
            spec = layout[name]
            runs = pose_runs(meta, dump, spec["segment"], palette)
            if spec.get("cycle"):
                cycle, timing = find_cycle(runs)
                for pose, timed in zip(cycle, timing):
                    pose.duration = timed.duration
                poses = cycle
            elif spec.get("max_parts"):
                poses = [max(runs, key=lambda p: p.parts)]
            else:
                poses = pick_clip(runs, spec.get("count"),
                                  spec.get("offset", 0))
            clips[name] = {"loop": bool(spec.get("loop", False)),
                           "poses": poses}
        return clips

    walk_runs = pose_runs(meta, dump, "walk_right", palette)
    walk_cycle, walk_timing = find_cycle(walk_runs)
    for pose, timed in zip(walk_cycle, walk_timing):
        pose.duration = timed.duration

    jump_runs = pose_runs(meta, dump, "jump_hold", palette)
    jump = pick_clip(jump_runs, 3)
    # The rise pose holds until the apex; the state machine switches to
    # hover/fall, so a short last-step duration is enough.
    if len(jump) >= 3:
        jump[0].duration = max(1, jump[0].duration)
        jump[1].duration = max(1, jump[1].duration)
        jump[2].duration = 1

    air_runs = pose_runs(meta, dump, "tap_air", palette)
    hover_pose = max(air_runs, key=lambda p: p.parts)
    hover_pose.duration = 1

    release_runs = pose_runs(meta, dump, "hover_release", palette)
    fall = pick_clip(release_runs, 2)
    land = pick_clip(release_runs, 2, offset=2) if len(release_runs) >= 4 \
        else release_runs[-1:]

    attack_runs = pose_runs(meta, dump, "attack", palette)

    idle_runs = pose_runs(meta, dump, "idle", palette)
    idle = idle_runs[:1]
    idle[0].duration = 1

    return {
        "idle": {"loop": True, "poses": idle},
        "walk": {"loop": True, "poses": walk_cycle},
        "jump": {"loop": False, "poses": jump},
        "hover": {"loop": True, "poses": [hover_pose]},
        "fall": {"loop": False, "poses": fall},
        "land": {"loop": False, "poses": land},
        "attack": {"loop": False, "poses": attack_runs},
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dump", required=True, type=Path,
                    help="capture directory from capture_anim.py")
    ap.add_argument("--out", required=True, type=Path,
                    help="asset output directory (e.g. assets/generated)")
    ap.add_argument("--palette", type=int, default=7,
                    help="OBJ palette used by the player in this save (default 7)")
    ap.add_argument("--layout", type=Path,
                    help="optional JSON clip layout override")
    ap.add_argument("--clean", action="store_true",
                    help="remove existing player_* animation PNGs first")
    args = ap.parse_args()

    meta_path = args.dump / "segments.json"
    if not meta_path.exists():
        raise SystemExit(f"{meta_path} missing; run capture_anim.py first")
    meta = json.loads(meta_path.read_text())
    layout = load_layout(args.layout)
    clips = build_clips(meta, args.dump, args.palette, layout)

    args.out.mkdir(parents=True, exist_ok=True)
    if args.clean:
        for old in args.out.glob("player_*.png"):
            if old.name != "player_idle.png":
                old.unlink()

    document = {
        "source": "capture_anim.py",
        "capture_start": meta.get("start"),
        "capture_end": meta.get("end"),
        "palette": args.palette,
        "generated_utc": datetime.now(timezone.utc).isoformat(
            timespec="seconds"),
        "clips": {},
    }
    total = 0
    for name in CLIP_NAMES:
        clip = clips.get(name)
        if not clip:
            continue
        steps = []
        for index, pose in enumerate(clip["poses"]):
            filename = f"player_{name}_{index}.png"
            pose.image.save(args.out / filename)
            steps.append({
                "texture": filename,
                "anchor_x": pose.anchor_x,
                "anchor_y": pose.anchor_y,
                "duration": max(1, pose.duration),
            })
        document["clips"][name] = {
            "loop": clip["loop"],
            "steps": steps,
        }
        durations = ", ".join(f'{s["duration"]}' for s in steps)
        print(f"{name:7s} {len(steps)} step(s)  durations [{durations}]")
        total += len(steps)

    (args.out / "player_anim.json").write_text(json.dumps(document, indent=1))
    print(f"wrote {args.out / 'player_anim.json'} ({total} steps)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
