#!/usr/bin/env python3
"""Compose sprites exactly from a probe's OAM/VRAM/CGRAM dump.

Pipeline A of docs/PLAN_SPRITES_ANIMATION.md. A probe directory holds
`vram.bin`, `cgram.bin`, `oam.bin` and `ppu.json`; this decodes every OAM entry
(including 16x16 quadrant order, flips and palette bits) with the game's own
CGRAM, and writes:

  <out>/oam_layer.png     every sprite, in screen space
  <out>/sprites.json      each sprite: screen rect, palette, tile, size
  <out>/boss.png          the largest cluster, cropped, with a transparent key

Then the port can use `boss.png` with the anchor taken from the cluster bounds
instead of guessing from a screenshot.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image

W, H = 256, 224
# Probe dumps have been little-endian so far; the flag exists because the
# recomposition gate has not passed yet and this is the first suspect.
BIG_ENDIAN_DUMP = False


def load_probe(folder: Path):
    vram = (folder / "vram.bin").read_bytes()
    cgram = (folder / "cgram.bin").read_bytes()
    oam = (folder / "oam.bin").read_bytes()
    ppu = json.loads((folder / "ppu.json").read_text())
    info_path = folder / "info.json"
    info = json.loads(info_path.read_text()) if info_path.is_file() else {}
    return vram, cgram, oam, ppu, info


OBJ_BASE, OBJ_STRIDE, OBJ_SLOTS = 0x1080, 0x50, 26


def read_pool(wram: bytes):
    """[{slot, species, x, y}] from the object pool ($1080, stride $50)."""
    out = []
    for slot in range(OBJ_SLOTS):
        rec = wram[OBJ_BASE + slot * OBJ_STRIDE:
                   OBJ_BASE + (slot + 1) * OBJ_STRIDE]
        if len(rec) < 0x36 or rec[0] == 0 or rec[2] == 0:
            continue

        def fx24(off):
            return (rec[off] | (rec[off + 1] << 8) | (rec[off + 2] << 16)) / 256.0

        out.append({"slot": slot, "species": rec[2], "x": fx24(0x30),
                    "y": fx24(0x33)})
    return out


def cgram_colours(cgram: bytes):
    cols = []
    for i in range(256):
        lo, hi = cgram[i * 2], cgram[i * 2 + 1]
        v = lo | (hi << 8)
        r = (v & 0x1F) * 255 // 31
        g = ((v >> 5) & 0x1F) * 255 // 31
        b = ((v >> 10) & 0x1F) * 255 // 31
        cols.append((r, g, b))
    return cols


SIZE_PAIRS = {
    0: (8, 16), 1: (8, 32), 2: (8, 64),
    3: (16, 32), 4: (16, 64), 5: (32, 64),
    6: (16, 32), 7: (32, 64),
}


def sprite_base_words(obsel: int) -> int:
    """OBSEL bits 0..2: sprite pattern base, in 16-bit words (as the verified
    decoder in extract.py uses: (obsel & 7) << 13)."""
    return (obsel & 0x07) << 13


def tile_pixels(vram: bytes, tile: int, palette: int, colours, flip_h, flip_v,
                base_words: int = 0):
    """8x8 4bpp tile -> 64 RGBA tuples.

    SNES 4bpp: words base+0..7 hold planes 0 (low byte) and 1 (high byte) for
    rows 0..7; words base+8..15 hold planes 2 and 3 the same way.
    """
    base = base_words + (tile & 0x1FF) * 16   # word address
    out = [None] * 64
    for y in range(8):
        if BIG_ENDIAN_DUMP:
            w0 = (vram[(base + y) * 2] << 8) | vram[(base + y) * 2 + 1]
            w1 = (vram[(base + 8 + y) * 2] << 8) | vram[(base + 8 + y) * 2 + 1]
        else:
            w0 = vram[(base + y) * 2] | (vram[(base + y) * 2 + 1] << 8)
            w1 = (vram[(base + 8 + y) * 2] |
                  (vram[(base + 8 + y) * 2 + 1] << 8))
        for x in range(8):
            bit = 7 - x
            p0 = (w0 >> bit) & 1
            p1 = (w0 >> (8 + bit)) & 1
            p2 = (w1 >> bit) & 1
            p3 = (w1 >> (8 + bit)) & 1
            idx = p0 | (p1 << 1) | (p2 << 2) | (p3 << 3)
            srcy = 7 - y if flip_v else y
            srcx = 7 - x if flip_h else x
            if idx:
                r, g, b = colours[(0x80 + palette * 16 + idx) & 0xFF]
                out[srcy * 8 + srcx] = (r, g, b, 255)
            else:
                out[srcy * 8 + srcx] = (0, 0, 0, 0)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("probe", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--big-endian-words", action="store_true")
    ap.add_argument("--object", type=lambda v: int(v, 0), default=None,
                    help="species to cut out by its pool position (e.g. 0x33)")
    args = ap.parse_args()

    global BIG_ENDIAN_DUMP
    BIG_ENDIAN_DUMP = args.big_endian_words
    vram, cgram, oam, ppu, info = load_probe(args.probe)
    colours = cgram_colours(cgram)
    obsel = int(str(ppu.get("obsel", "0x00")), 16)
    base_words = sprite_base_words(obsel)
    small, large = SIZE_PAIRS.get((obsel >> 5) & 7, (8, 16))
    print(f"obsel {obsel:02x}: tile base word {base_words}, "
          f"sizes {small}/{large}")
    args.out.mkdir(parents=True, exist_ok=True)

    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sprites = []
    for i in range(128):
        x = oam[i * 4]
        y = oam[i * 4 + 1]
        tile = oam[i * 4 + 2]
        attr = oam[i * 4 + 3]
        hi = oam[0x200 + i // 4]
        x |= ((hi >> ((i % 4) * 2)) & 1) << 8
        size16 = (hi >> ((i % 4) * 2 + 1)) & 1
        if y >= 224 or x >= 256 or (x == 0 and y == 0 and tile == 0):
            continue
        # Attribute bit 0 is tile bit 8 ONLY for 16x16 sprites; 8x8 entries
        # ignore it (applying it read the wrong tiles and dropped parts).
        if size16:
            tile = (tile & 0xFE) | ((attr & 1) << 8)
        palette = (attr >> 1) & 7
        flip_h = bool(attr & 0x40)
        flip_v = bool(attr & 0x80)
        size = large if size16 else small
        # A 16x16 sprite is four tiles: the low bit selects the column and the
        # 0x10 bit the row, in row-major order.
        quads = []
        n = size // 8
        base_tile = (tile & 0x1FE) if size16 else tile
        for row in range(n):
            for col in range(n):
                quads.append((base_tile + col + row * 16, col * 8, row * 8))
        width = height = size
        for t, ox, oy in quads:
            pix = tile_pixels(vram, t, palette, colours, flip_h, flip_v,
                              base_words)
            for py in range(8):
                for px in range(8):
                    r, g, b, a = pix[py * 8 + px]
                    if not a:
                        continue
                    sx = (x + ox + px) & 0xFF
                    sy = y + oy + py
                    if 0 <= sy < H:
                        canvas.putpixel((sx, sy), (r, g, b, 255))
        sprites.append({"index": i, "x": x, "y": y, "tile": tile,
                        "palette": palette, "size16": bool(size16),
                        "flip_h": flip_h, "flip_v": flip_v,
                        "width": width, "height": height})

    canvas.save(args.out / "oam_layer.png")

    if args.object is not None:
        wram_path = args.probe / "wram.bin"
        if wram_path.is_file():
            pool = read_pool(wram_path.read_bytes())
            target = [o for o in pool if o["species"] == args.object]
            if target:
                obj = target[0]
                hs = ppu.get("hScroll", [0])[0]
                vs = ppu.get("vScroll", [0])[0]
                sx, sy = obj["x"] - hs, obj["y"] - vs
                near = [s for s in sprites
                        if abs((s["x"] + s["width"] / 2) - sx) < 200 and
                        abs((s["y"] + s["height"] / 2) - sy) < 170]
                if near:
                    x0 = max(0, min(s["x"] for s in near) - 2)
                    y0 = max(0, min(s["y"] for s in near) - 2)
                    x1 = min(W, max(s["x"] + s["width"] for s in near) + 2)
                    y1 = min(H, max(s["y"] + s["height"] for s in near) + 2)
                    crop = canvas.crop((x0, y0, x1, y1))
                    crop.save(args.out / f"object_{args.object:02x}.png")
                    meta = {"species": args.object, "obj_world": [obj["x"], obj["y"]],
                            "scroll": [hs, vs], "obj_screen": [sx, sy],
                            "rect": [x0, y0, x1, y1],
                            "anchor": [sx - x0, sy - y0],
                            "sprites": len(near)}
                    (args.out / f"object_{args.object:02x}.json").write_text(
                        json.dumps(meta, indent=1))
                    print(f"object {args.object:02x}: crop {crop.size} "
                          f"anchor {meta['anchor']} from {len(near)} sprites")
    (args.out / "sprites.json").write_text(json.dumps(sprites, indent=1))
    print(f"{len(sprites)} sprites -> {args.out / 'oam_layer.png'}")

    # Largest cluster of sprites = the boss.
    boxes = []
    for s in sprites:
        boxes.append([s["x"], s["y"], s["x"] + s["width"], s["y"] + s["height"]])
    merged = []
    for box in boxes:
        placed = False
        for m in merged:
            if not (box[2] < m[0] - 8 or box[0] > m[2] + 8 or
                    box[3] < m[1] - 8 or box[1] > m[3] + 8):
                m[0] = min(m[0], box[0])
                m[1] = min(m[1], box[1])
                m[2] = max(m[2], box[2])
                m[3] = max(m[3], box[3])
                placed = True
                break
        if not placed:
            merged.append(list(box))
    if merged:
        big = max(merged, key=lambda m: (m[2] - m[0]) * (m[3] - m[1]))
        crop = canvas.crop((max(0, big[0]), max(0, big[1]),
                            min(W, big[2]), min(H, big[3])))
        crop.save(args.out / "boss.png")
        print(f"boss cluster {crop.size} at {big[:2]}")
        (args.out / "boss.json").write_text(json.dumps(
            {"rect": big, "size": crop.size}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
