#!/usr/bin/env python3
"""Identify which extracted room an oracle frame belongs to.

The guest builds BG1 from the ROM block tables, so a captured frame's BG1
tilemap contains the same 16-bit map words the extractor baked into
`room_area<N>.json`.  Matching the captured 32x32 window against every area's
tilemap is therefore an exact test: the true area matches cell-for-cell, while
other areas only share common filler tiles.

This replaces save-name guesses and WRAM heuristics.  `WRAM $008D` looked like
the area index at first (it coincided with eight "known" mappings), but a save
in the murkland cemetery reads `$008D == 2` while its BG1 matches area **1** at
100%, so `$008D` is a stage/room id, not the `$81:A291` area index.

Usage:
  python3 tools/oracle/identify_area.py --capture captures/save7
  python3 tools/oracle/identify_area.py --dir captures --json captures/areas.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "dc_extract"))
from extract import tilemap_linear  # noqa: E402

BLOCK = 32
MIN_MATCH = 0.90


def load_capture_frame(capture_dir: Path, frame: int | None = None):
    if frame is None:
        vrams = sorted(capture_dir.glob("f*.vram.bin"))
        if not vrams:
            raise SystemExit(f"no f*.vram.bin in {capture_dir}")
        vram_path = vrams[-1]
        frame = int(vram_path.name[1:7])
    else:
        vram_path = capture_dir / f"f{frame:06d}.vram.bin"
    state_path = capture_dir / f"f{frame:06d}.json"
    if not vram_path.is_file() or not state_path.is_file():
        raise SystemExit(f"missing frame {frame} in {capture_dir}")
    state = json.loads(state_path.read_text())
    ppu = state["ppu"]
    raw = vram_path.read_bytes()
    vram = [int.from_bytes(raw[i * 2:i * 2 + 2], "little") for i in range(0x8000)]
    bgsc = int(ppu["bgXsc"][0], 16) if isinstance(ppu["bgXsc"][0], str) else int(ppu["bgXsc"][0])
    w, h, tiles = tilemap_linear(vram, bgsc)
    return w, h, tiles, frame


def area_score(area_doc: dict, cap_w: int, cap_h: int, cap: list[int]) -> float:
    """Best cell-for-cell match over non-empty blocks.

    Empty (all-zero) blocks are excluded: a blank region of VRAM matches every
    empty corner of every room at 1.000, which produced meaningless ties (areas
    1 and 73 "tying" for the same frame).  A block only counts when at least
    60% of its cells are non-zero on both sides.
    """
    bg1 = [b for b in area_doc["bg"] if b["name"] == "bg1"][0]
    W, H, tiles = bg1["map_width"], bg1["map_height"], bg1["tilemap"]
    bw, bh = min(BLOCK, cap_w), min(BLOCK, cap_h)
    cap_blocks = []
    for by in range(0, cap_h, BLOCK):
        for bx in range(0, cap_w, BLOCK):
            cells = [cap[y * cap_w + x]
                     for y in range(by, min(by + bh, cap_h))
                     for x in range(bx, min(bx + bw, cap_w))]
            if sum(1 for c in cells if c) >= 0.6 * len(cells):
                cap_blocks.append(cells)
    if not cap_blocks:
        return 0.0
    best = 0.0
    for cy in range(0, max(1, H - bh + 1)):
        for cx in range(0, max(1, W - bw + 1)):
            block = [tiles[(cy + y) * W + (cx + x)]
                     for y in range(bh) for x in range(bw)]
            if sum(1 for c in block if c) < 0.6 * len(block):
                continue
            for capb in cap_blocks:
                matches = sum(1 for a, b in zip(block, capb) if a == b)
                score = matches / len(block)
                if score > best:
                    best = score
                    if best >= 0.999:
                        return best
    return best


def identify(capture_dir: Path, areas: dict[int, dict]) -> dict:
    w, h, tiles, frame = load_capture_frame(capture_dir)
    ranked = sorted(((area_score(doc, w, h, tiles), area)
                     for area, doc in areas.items()), reverse=True)
    best_score, best_area = ranked[0]
    return {"capture": str(capture_dir), "frame": frame,
            "area": best_area if best_score >= MIN_MATCH else None,
            "score": round(best_score, 3),
            "runners_up": [(a, round(s, 3)) for s, a in ranked[1:4]]}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--capture", type=Path)
    ap.add_argument("--dir", type=Path, help="parent directory of capture dirs")
    ap.add_argument("--rooms", type=Path, default=ROOT / "assets" / "generated")
    ap.add_argument("--json", type=Path)
    ap.add_argument("--min-match", type=float, default=MIN_MATCH)
    args = ap.parse_args()

    areas = {}
    for path in args.rooms.glob("room_area*.json"):
        doc = json.loads(path.read_text())
        areas[int(doc["area_id"])] = doc
    if not areas:
        raise SystemExit(f"no room_area*.json in {args.rooms}")

    dirs = [args.capture] if args.capture else sorted(
        p for p in args.dir.iterdir() if p.is_dir() and list(p.glob("f*.vram.bin")))
    results = []
    for d in dirs:
        try:
            r = identify(d, areas)
        except SystemExit as exc:
            print(f"{d.name}: {exc}")
            continue
        results.append(r)
        print(f"{d.name:32s} area={r['area']} score={r['score']} "
              f"runners_up={r['runners_up']}")
    if args.json:
        args.json.write_text(json.dumps(results, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
