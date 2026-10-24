#!/usr/bin/env python3
"""Bake Firebrand's form sheets into per-form player animations.

The sheets use one layout: caption, sprite run, next caption. The runs were
measured once on the normal sheet with a coordinate grid; the five other sheets
share that layout (the ground sheet is compressed vertically, so it uses its own
bands). Each band is split into frames by narrow column gaps, then written as
`fb_<form>_<clip>_<i>.png` with `player_forms.json`.
"""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path

from PIL import Image

ROOT = Path(r"M:\Opencode\DemonsCrestNativePort")
SPRITES = ROOT / "docs/sprites"
GEN = ROOT / "assets/generated"

# (x0, x1, y0, y1) in sheet pixels, measured on the normal sheet.
BANDS = {
    "idle": (2, 80, 18, 62),
    "walk": (82, 180, 18, 62),
    "attack": (181, 262, 18, 62),
    "jump": (2, 150, 100, 150),
    "fall": (192, 264, 100, 150),
    "land": (266, 316, 100, 150),
    "hover": (2, 220, 198, 252),
    "flying": (222, 442, 198, 252),
    "hurt": (0, 66, 276, 334),
}
GROUND_BANDS = {
    "idle": (2, 80, 12, 58),
    "walk": (82, 180, 12, 58),
    "attack": (181, 262, 12, 58),
    "jump": (2, 150, 64, 116),
    "fall": (192, 264, 64, 116),
    "land": (266, 316, 64, 116),
    "hover": (2, 220, 122, 182),
    "flying": (222, 442, 122, 182),
    "hurt": (0, 66, 190, 258),
}
SHEETS = [
    (0, "normal", "Playable Characters - Firebrand.gif", BANDS),
    (1, "ground", "Playable Characters - Firebrand (Ground).gif", GROUND_BANDS),
    (2, "aerial", "Playable Characters - Firebrand (Aerial).gif", BANDS),
    (3, "tidal", "Playable Characters - Firebrand (Tidal).gif", BANDS),
    (4, "legendary", "Playable Characters - Firebrand (Legendary).gif", BANDS),
    (5, "ultimate", "Playable Characters - Firebrand (Ultimate).gif", BANDS),
]


def background_colour(im):
    small = im.convert("RGB").resize((max(1, im.width // 4),
                                      max(1, im.height // 4)))
    return Counter(small.getdata()).most_common(1)[0][0]


def split_frames(im, box):
    x0, x1, y0, y1 = box
    x1 = min(x1, im.width - 1)
    y1 = min(y1, im.height - 1)
    px = im.load()
    # Background is the sheet's dominant colour; anything else is sprite.
    bg = background_colour(im)
    cols = []
    for x in range(x0, x1 + 1):
        hit = False
        for y in range(y0, y1 + 1):
            r, g, b = px[x, y][:3]
            if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) > 30:
                hit = True
                break
        cols.append(hit)
    runs = []
    start = None
    for i, c in enumerate(cols):
        if c and start is None:
            start = i
        elif not c and start is not None:
            runs.append((x0 + start, x0 + i - 1))
            start = None
    if start is not None:
        runs.append((x0 + start, x0 + len(cols) - 1))
    if not runs:
        return []
    merged = [runs[0]]
    for r in runs[1:]:
        if r[0] - merged[-1][1] - 1 <= 1:
            merged[-1] = (merged[-1][0], r[1])
        else:
            merged.append(r)
    frames = []
    for fx0, fx1 in merged:
        ys = [y for y in range(y0, y1 + 1)
              if any(abs(px[x, y][0] - bg[0]) + abs(px[x, y][1] - bg[1]) +
                     abs(px[x, y][2] - bg[2]) > 30
                     for x in range(fx0, fx1 + 1))]
        if not ys:
            continue
        width = fx1 + 1 - fx0
        height = max(ys) + 1 - min(ys)
        if width < 12 or height < 24:
            continue  # a slice of a sprite, not a frame
        crop = im.crop((fx0, min(ys), fx1 + 1, max(ys) + 1)).convert("RGBA")
        cp = crop.load()
        for cy in range(crop.height):
            for cx in range(crop.width):
                r, g, b, a = cp[cx, cy]
                if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) <= 30:
                    cp[cx, cy] = (0, 0, 0, 0)
        frames.append((crop, fx1 + 1 - fx0, max(ys) + 1 - min(ys)))
    return frames


def main() -> int:
    for stale in GEN.glob("fb_*.png"):
        stale.unlink()
    forms = []
    for form, name, sheet, bands in SHEETS:
        im = Image.open(SPRITES / f"SNES - Demon's Crest - {sheet}").convert(
            "RGBA")
        clips = {}
        for clip, box in bands.items():
            frames = split_frames(im, box)
            if not frames:
                continue
            steps = []
            for i, (crop, w, h) in enumerate(frames):
                tex = f"fb_{name}_{clip}_{i}.png"
                crop.save(GEN / tex)
                steps.append({"texture": tex, "anchor_x": w // 2,
                              "anchor_y": h, "duration": 8})
            loop = clip not in ("attack", "land", "hurt")
            clips[clip] = {"loop": loop, "steps": steps}
        forms.append({"form": form, "name": name, "clips": clips})
        print(f"{name}: "
              f"{ {k: len(v['steps']) for k, v in clips.items()} }")
    (GEN / "player_forms.json").write_text(json.dumps({"forms": forms}, indent=1))
    print("wrote player_forms.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
