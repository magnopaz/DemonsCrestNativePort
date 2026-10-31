#!/usr/bin/env python3
"""Bake Arma's first-fight poses from the labelled sprite sheet.

The previous version cropped whole sheet rows (0..148, 158..252, 283..402),
which also captured the row captions and two or three Arma poses in one PNG.
Those boxes are replaced by individual, measured frame boxes. The second fight
palette and the hurricane are still out of scope.
"""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "docs/sprites/SNES - Demon's Crest - Enemies & Bosses - Arma.gif"
GEN = ROOT / "assets/generated"

# Individual poses of the first fight. (x0, x1, y0, y1), exclusive x1/y1.
BANDS: dict[str, list[tuple[int, int, int, int]]] = {
    "idle": [
        (18, 52, 57, 113),
        (65, 99, 57, 113),
        (103, 137, 57, 113),
    ],
    "breath": [
        (9, 67, 268, 318),
        (71, 133, 268, 318),
        (137, 195, 268, 318),
    ],
    "dive": [
        (254, 303, 265, 310),
    ],
    "hurt": [
        (365, 399, 264, 320),
    ],
}


def main() -> int:
    im = Image.open(SRC).convert("RGB")
    small = im.resize((im.width // 4, im.height // 4))
    bg = Counter(small.getdata()).most_common(1)[0][0]
    print("background", bg)

    for stale in GEN.glob("enemy_a5_*.png"):
        stale.unlink()

    frames: list[dict] = []
    clip_indices: dict[str, list[int]] = {}
    for clip, boxes in BANDS.items():
        clip_indices[clip] = []
        for x0, x1, y0, y1 in boxes:
            crop = im.crop((x0, y0, x1, y1)).convert("RGBA")
            px = crop.load()
            for y in range(crop.height):
                for x in range(crop.width):
                    r, g, b, a = px[x, y]
                    if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) <= 30:
                        px[x, y] = (0, 0, 0, 0)
            bbox = crop.getbbox()
            if bbox is None:
                print(f"  {clip} {x0},{y0}: empty after keying")
                continue
            crop = crop.crop(bbox)
            index = len(frames)
            tex = f"enemy_a5_{index}.png"
            crop.save(GEN / tex)
            frames.append({
                "texture": tex,
                "anchor_x": crop.width // 2,
                "anchor_y": crop.height,
                "duration": 8,
                "width": crop.width,
                "height": crop.height,
            })
            clip_indices[clip].append(index)
            print(f"  {clip}[{len(clip_indices[clip]) - 1}] {crop.size}")

    anim_path = GEN / "species_anim.json"
    anim = json.loads(anim_path.read_text()) if anim_path.is_file() else {
        "species": {}}
    states: dict[str, list[int]] = {}
    if clip_indices.get("idle"):
        states["0"] = clip_indices["idle"]
    if clip_indices.get("breath"):
        states["1"] = clip_indices["breath"]
    if clip_indices.get("dive"):
        states["2"] = clip_indices["dive"]
        states["3"] = clip_indices["dive"]
    hurt = clip_indices.get("hurt", [])
    if hurt:
        states["4"] = hurt
    anim.setdefault("species", {})["a5"] = {
        "species": 0xA5,
        "frames": frames,
        "source": "user sprite sheet (Spriters Resource, ripped by Lord Zymeth)",
        "states": states,
        "hurt": hurt[0] if hurt else 0,
        "default": clip_indices["idle"][0] if clip_indices.get("idle") else 0,
    }
    anim_path.write_text(json.dumps(anim, indent=1))
    print("species a5 frames:", [e["texture"] for e in frames])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
