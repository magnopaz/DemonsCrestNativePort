#!/usr/bin/env python3
"""Extract a playable room from a Demon's Crest (USA) ROM + recomp dump.

The native game does not read the ROM at runtime. This tool bakes the data it
needs into `assets/generated/`:

  * the 64x32 BG tilemaps as loaded in VRAM at the captured frame
  * per-palette 4bpp tile atlases
  * per-palette OBJ tile atlases
  * the player and item sprites composed from OAM
  * a collision grid built from the ROM's block-attribute table

All output is ROM-derived and must stay out of version control.

Usage:
  python3 tools/dc_extract/extract.py \
    --rom "/path/to/Demon's Crest (USA).sfc" \
    --dump captures/graveyard --prefix stage_600 \
    --area 1 --screens 9,10 --out assets/generated
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

from PIL import Image

ROM_SHA256 = "18d40a807d5f88c5b6a1ad849eec7e0f189225d9a1586037c850f6680b5844de"
ROM_CRC32 = 0xE8236AD2

# OAM sprite-size table indexed by OBSEL bits 5..7.
OBJ_SIZES = ((8, 16), (8, 32), (8, 64), (16, 32),
             (16, 64), (32, 64), (16, 32), (16, 32))


def lo(bank: int, addr: int) -> int:
    """LoROM SNES address -> file offset."""
    return ((bank & 0x7F) << 15) | (addr & 0x7FFF)


class Rom:
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        if len(self.data) == 0x200000 + 512:
            self.data = self.data[512:]  # copier header
        if len(self.data) != 0x200000:
            raise SystemExit(f"ROM must be 2 MiB (got {len(self.data)} bytes)")
        sha = hashlib.sha256(self.data).hexdigest()
        if sha != ROM_SHA256:
            raise SystemExit(f"unexpected ROM SHA-256:\n  {sha}\n  {ROM_SHA256}")
        self.sha256 = sha

    def u8(self, bank: int, addr: int) -> int:
        return self.data[lo(bank, addr)]

    def u16(self, bank: int, addr: int) -> int:
        return struct.unpack_from("<H", self.data, lo(bank, addr))[0]

    # -- level tables -----------------------------------------------------
    def area_info(self, area: int):
        p = self.u16(0x81, 0xA291 + area * 2)
        set_id = self.u8(0x81, p)
        count = self.u8(0x81, p + 1)
        screens = [self.u8(0x81, p + 2 + k) for k in range(count)]
        q = self.u16(0x81, 0xA6D8 + area * 2)
        w, h = self.u8(0x81, q), self.u8(0x81, q + 1)
        grid = [[self.u8(0x81, q + 2 + y * w + x) for x in range(w)]
                for y in range(h)]
        return {"set": set_id, "screens": screens, "grid": grid,
                "width": w, "height": h}

    def screen_blocks(self, set_id: int, screen: int):
        """16x16 block ids for a screen, expanded from 2x2 superblocks."""
        out = [[0] * 16 for _ in range(16)]
        for k in range(64):
            sb = self.u16(0x8F + set_id, 0x8000 + screen * 128 + k * 2)
            sx, sy = (k % 8) * 2, (k // 8) * 2
            for j in range(4):
                b = self.u16(0x92 + set_id, 0x8000 + sb * 8 + j * 2)
                out[sy + j // 2][sx + j % 2] = b
        return out

    def block_attr(self, set_id: int, block: int) -> int:
        return self.u8(0x98, 0x8000 + (set_id << 12) + block)

    def block_tiles(self, set_id: int, block: int):
        return tuple(self.u16(0x95 + set_id, 0x8000 + block * 8 + j * 2)
                     for j in range(4))

    def screen_tilemap(self, set_id: int, screen: int):
        """32x32 tilemap words for one screen, built from its block table."""
        out = [[0] * 32 for _ in range(32)]
        blocks = self.screen_blocks(set_id, screen)
        for by in range(16):
            for bx in range(16):
                tiles = self.block_tiles(set_id, blocks[by][bx])
                for j, word in enumerate(tiles):
                    out[by * 2 + j // 2][bx * 2 + j % 2] = word
        return out


def read_vram(path: Path) -> list[int]:
    raw = path.read_bytes()
    if len(raw) != 0x10000:
        raise SystemExit(f"{path}: expected 64 KiB VRAM dump")
    return list(struct.unpack("<32768H", raw))


def read_cgram(path: Path) -> list[tuple[int, int, int]]:
    raw = path.read_bytes()
    if len(raw) != 0x200:
        raise SystemExit(f"{path}: expected 512-byte CGRAM dump")
    words = struct.unpack("<256H", raw)
    return [((c & 31) * 8, ((c >> 5) & 31) * 8, ((c >> 10) & 31) * 8)
            for c in words]


def tile_pixels(vram: list[int], tile_word: int, bpp: int = 4) -> list[list[int]]:
    """Decode one 8x8 tile at a VRAM word address."""
    rows = []
    for y in range(8):
        w0 = vram[(tile_word + y) & 0x7FFF]
        w1 = vram[(tile_word + 8 + y) & 0x7FFF]
        row = []
        for x in range(8):
            bit = 7 - x
            px = ((w0 >> bit) & 1) | (((w0 >> (8 + bit)) & 1) << 1)
            if bpp >= 4:
                px |= (((w1 >> bit) & 1) << 2) | (((w1 >> (8 + bit)) & 1) << 3)
            row.append(px)
        rows.append(row)
    return rows


def make_bg_atlas(vram: list[int], cgram, palette: int, base_word: int = 0) -> Image.Image:
    """32x32 tiles, 8x8 each, in four 256x256 flip quadrants.

    Quadrants: normal (0,0), H (256,0), V (0,256), HV (256,256).
    Pre-flipping avoids relying on negative source rectangles in the renderer.
    """
    img = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    px = img.load()
    for tile in range(1024):
        tx, ty = (tile % 32) * 8, (tile // 32) * 8
        rows = tile_pixels(vram, base_word + tile * 16)
        for y, row in enumerate(rows):
            for x, p in enumerate(row):
                if not p:
                    continue
                color = (*cgram[palette * 16 + p], 255)
                for qx, qy, fx, fy in ((0, 0, False, False),
                                       (256, 0, True, False),
                                       (0, 256, False, True),
                                       (256, 256, True, True)):
                    sx = 7 - x if fx else x
                    sy = 7 - y if fy else y
                    px[qx + tx + sx, qy + ty + sy] = color
    return img


def make_obj_atlas(vram: list[int], cgram, obj_base_word: int, palette: int) -> Image.Image:
    """16x16 tiles, 8x8 each -> 128x128 RGBA. Palette 0 pixel is transparent."""
    img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
    px = img.load()
    for tile in range(256):
        tx, ty = (tile % 16) * 8, (tile // 16) * 8
        for y, row in enumerate(tile_pixels(vram, obj_base_word + tile * 16)):
            for x, p in enumerate(row):
                if p:
                    px[tx + x, ty + y] = (*cgram[0x80 + palette * 16 + p], 255)
    return img


def tilemap_linear(vram: list[int], bgsc: int) -> tuple[int, int, list[int]]:
    """Linearize a SNES BG tilemap (32/64 x 32/64) into world order."""
    base = (bgsc & 0xFC) << 8
    wide = bool(bgsc & 1)
    high = bool(bgsc & 2)
    w = 64 if wide else 32
    h = 64 if high else 32
    out = []
    for y in range(h):
        for x in range(w):
            page_x = 0x400 if (wide and x >= 32) else 0
            if high:
                page_y = 0x800 if (wide and y >= 32) else (0x400 if y >= 32 else 0)
            else:
                page_y = 0
            out.append(vram[(base + page_x + page_y + (y & 31) * 32 + (x & 31)) & 0x7FFF])
    return w, h, out


class OamSprite:
    def __init__(self, index: int, x: int, y: int, size: int, tile: int,
                 palette: int, hflip: bool, vflip: bool):
        self.index = index
        self.x, self.y, self.size = x, y, size
        self.tile, self.palette = tile, palette
        self.hflip, self.vflip = hflip, vflip


def parse_oam(oam: bytes, obsel: int) -> list[OamSprite]:
    if len(oam) != 0x220:
        raise SystemExit("OAM dump must be 544 bytes")
    obj_base = (obsel & 7) << 13
    small, large = OBJ_SIZES[(obsel >> 5) & 7]
    out = []
    for i in range(128):
        oam0, oam1 = struct.unpack_from("<HH", oam, i * 4)
        y = oam0 >> 8
        x = oam0 & 0xFF
        hi = oam[0x200 + (i // 4)]
        x |= ((hi >> ((i % 4) * 2)) & 1) << 8
        if x >= 256:
            x -= 512
        size = large if ((hi >> ((i % 4) * 2 + 1)) & 1) else small
        out.append(OamSprite(i, x, y, size, oam1 & 0xFF,
                             (oam1 >> 9) & 7, bool(oam1 & 0x4000),
                             bool(oam1 & 0x8000)))
    return out


def compose_oam(vram: list[int], cgram,
                sprites: list[OamSprite],
                palette_override: int | None = None) -> tuple[Image.Image, int, int]:
    """Compose a group of OAM sprites into one RGBA image.

    The returned image is anchored at (min_x, min_y) in screen coordinates.
    `palette_override` is used when the game has already restored CGRAM by the
    time the dump was written (the player palette is one such case).
    """
    if not sprites:
        raise SystemExit("no OAM sprites to compose")
    min_x = min(s.x for s in sprites)
    min_y = min(s.y for s in sprites)
    max_x = max(s.x + s.size for s in sprites)
    max_y = max(s.y + s.size for s in sprites)
    img = Image.new("RGBA", (max_x - min_x, max_y - min_y), (0, 0, 0, 0))
    px = img.load()
    for s in sorted(sprites, key=lambda s: s.index, reverse=True):
        pal = s.palette if palette_override is None else palette_override
        for row in range(s.size):
            for col in range(s.size):
                used_col = s.size - 1 - col if s.hflip else col
                used_row = s.size - 1 - row if s.vflip else row
                used_tile = (((s.tile >> 4) + (used_row >> 3)) << 4) | \
                            (((s.tile & 0x0F) + (used_col >> 3)) & 0x0F)
                # The caller passes a VRAM slice already rebased to OBJ tile 0.
                p = tile_pixels(vram, used_tile * 16)[used_row & 7][used_col & 7]
                if p:
                    px[s.x - min_x + col, s.y - min_y + row] = \
                        (*cgram[0x80 + pal * 16 + p], 255)
    return img, min_x, min_y


def compose_oam_indices(vram: list[int],
                        sprites: list[OamSprite]) -> tuple[Image.Image, int, int]:
    """Compose OAM sprites into an index image (R = palette index)."""
    min_x = min(s.x for s in sprites)
    min_y = min(s.y for s in sprites)
    max_x = max(s.x + s.size for s in sprites)
    max_y = max(s.y + s.size for s in sprites)
    img = Image.new("RGBA", (max_x - min_x, max_y - min_y), (0, 0, 0, 0))
    px = img.load()
    for s in sorted(sprites, key=lambda s: s.index, reverse=True):
        for row in range(s.size):
            for col in range(s.size):
                used_col = s.size - 1 - col if s.hflip else col
                used_row = s.size - 1 - row if s.vflip else row
                used_tile = (((s.tile >> 4) + (used_row >> 3)) << 4) | \
                            (((s.tile & 0x0F) + (used_col >> 3)) & 0x0F)
                p = tile_pixels(vram, used_tile * 16)[used_row & 7][used_col & 7]
                if p:
                    px[s.x - min_x + col, s.y - min_y + row] = (p, 0, 0, 255)
    return img, min_x, min_y


def sample_palette(index_img: Image.Image, origin_x: int, origin_y: int,
                   framebuffer: Image.Image | None,
                   fallback: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    """Recover the palette actually used on screen for each sprite index.

    CGRAM at dump time can differ from the CGRAM used while the frame was
    rendered (the game restores/overwrites palette slots). Sampling the
    framebuffer at the sprite's opaque pixels gives the visible colours.
    """
    if framebuffer is None:
        return fallback
    buckets: dict[int, list[tuple[int, int, int]]] = {}
    px = index_img.load()
    w, h = index_img.size
    for y in range(h):
        for x in range(w):
            idx, _g, _b, a = px[x, y]
            if not a:
                continue
            fx, fy = origin_x + x, origin_y + y
            if 0 <= fx < framebuffer.width and 0 <= fy < framebuffer.height:
                buckets.setdefault(idx, []).append(framebuffer.getpixel((fx, fy)))
    out = list(fallback)
    for idx, colors in buckets.items():
        if 1 <= idx <= 15:
            out[idx] = Counter(colors).most_common(1)[0][0]
    return out


def colorize(index_img: Image.Image,
             palette: list[tuple[int, int, int]]) -> Image.Image:
    img = Image.new("RGBA", index_img.size, (0, 0, 0, 0))
    src = index_img.load()
    dst = img.load()
    for y in range(index_img.height):
        for x in range(index_img.width):
            idx, _g, _b, a = src[x, y]
            if a:
                dst[x, y] = (*palette[idx], 255)
    return img


def crop_sprite(img: Image.Image, origin_x: int, origin_y: int):
    """Crop transparent padding; return absolute screen bbox."""
    bbox = img.getbbox()
    if bbox is None:
        raise SystemExit("sprite composition is empty")
    cropped = img.crop(bbox)
    return (cropped,
            origin_x + bbox[0], origin_y + bbox[1],
            bbox[2] - bbox[0], bbox[3] - bbox[1])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", required=True, type=Path)
    ap.add_argument("--dump", required=True, type=Path)
    ap.add_argument("--prefix", default="stage_600")
    ap.add_argument("--area", type=int, default=1,
                    help="ROM area index at $81:A291 (default 1 for graveyard stage_600)")
    ap.add_argument("--screens", help="screen slots, e.g. 9,10 (default: bottom row first two)")
    ap.add_argument("--player-oam", default="1-11",
                    help="OAM slots composing the player (default 1-11)")
    ap.add_argument("--player-palette", type=int, default=2,
                    help="CGRAM OBJ palette used by the player at render time "
                         "(default 2 for this graveyard capture)")
    ap.add_argument("--item-oam", default="0",
                    help="OAM slot for the visible item (default 0; 'none' to skip)")
    ap.add_argument("--item-palette", type=int, default=0,
                    help="CGRAM OBJ palette for the item (default 0; -1 = use OAM bits)")
    ap.add_argument("--fb", type=Path,
                    help="framebuffer BMP for palette matching (default: <dump>/<prefix>.fb.bmp)")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    rom = Rom(args.rom)
    out = args.out
    out.mkdir(parents=True, exist_ok=True)

    vram = read_vram(args.dump / f"{args.prefix}.vram.bin")
    cgram = read_cgram(args.dump / f"{args.prefix}.cgram.bin")
    oam = (args.dump / f"{args.prefix}.oam.bin").read_bytes()
    regs = json.loads((args.dump / f"{args.prefix}.regs.json").read_text())
    fb_path = args.fb or (args.dump / f"{args.prefix}.fb.bmp")
    framebuffer = Image.open(fb_path).convert("RGB") if fb_path.exists() else None

    mode = regs["bgmode"] & 7
    if mode not in (0, 1, 2, 3):
        raise SystemExit(f"unsupported BG mode {mode} (this extractor handles 4bpp modes)")
    if regs.get("inidisp", 0) & 0x80:
        raise SystemExit("capture is in forced blank; choose a visible gameplay frame")

    info = rom.area_info(args.area)
    set_id = info["set"]
    if args.screens:
        slots = [int(s) for s in args.screens.split(",")]
    else:
        slots = info["grid"][info["height"] - 1][:2]
    if len(slots) != 2:
        raise SystemExit("this first extractor expects exactly two screens (32x16 blocks)")

    # --- tilemaps -------------------------------------------------------
    # The VRAM ring buffer holds the room as the game has currently loaded it.
    # For this first slice the playable area is the captured viewport, so the
    # buffer is exact; the block-table route needs the loader's tile remap and
    # is deferred until a wider room capture exists.
    bg_layers = []
    for layer, name in ((1, "bg2"), (0, "bg1")):
        bgsc = regs["bgsc"][layer]
        w, h, tiles = tilemap_linear(vram, bgsc)
        bg_layers.append({
            "name": name,
            "map_width": w,
            "map_height": h,
            "scroll_x": regs["hscroll"][layer],
            "scroll_y": regs["vscroll"][layer],
            "tilemap": tiles,
        })
    # BG layers are listed back-to-front by the renderer.

    # --- palettes -------------------------------------------------------
    for p in range(8):
        make_bg_atlas(vram, cgram, p).save(out / f"bg_pal{p}.png")
    # OBJ base for the current OBSEL.
    obj_base = (regs["obsel"] & 7) << 13
    for p in range(8):
        make_obj_atlas(vram, cgram, obj_base, p).save(out / f"obj_pal{p}.png")

    # --- OAM sprites ----------------------------------------------------
    oam_sprites = parse_oam(oam, regs["obsel"])
    player_slots = set()
    if args.player_oam.lower() != "none":
        for part in args.player_oam.split(","):
            if "-" in part:
                a, b = part.split("-", 1)
                player_slots |= set(range(int(a), int(b) + 1))
            else:
                player_slots.add(int(part))
    player_sprites = [s for s in oam_sprites if s.index in player_slots]
    # The OBJ tile base is already in the VRAM word addresses; compose_oam
    # expects a vram array where OBJ tile 0 starts at word 0.
    obj_vram = vram[obj_base:obj_base + 0x2000]
    player_index, player_ox, player_oy = compose_oam_indices(
        obj_vram, player_sprites)
    if args.player_palette >= 0:
        player_colors = [cgram[0x80 + args.player_palette * 16 + i]
                         for i in range(16)]
    else:
        player_fallback = [cgram[0x80 + 7 * 16 + i] for i in range(16)]
        player_colors = sample_palette(player_index, player_ox, player_oy,
                                       framebuffer, player_fallback)
    player_img = colorize(player_index, player_colors)
    player_img, bx, by, bw, bh = crop_sprite(player_img, player_ox, player_oy)
    player_img.save(out / "player_idle.png")
    player_feet_x = bw // 2
    player_feet_y = bh

    sprites = []
    if args.item_oam.lower() != "none":
        item_slots = set()
        for part in args.item_oam.split(","):
            if "-" in part:
                a, b = part.split("-", 1)
                item_slots |= set(range(int(a), int(b) + 1))
            else:
                item_slots.add(int(part))
        item_sprites = [s for s in oam_sprites if s.index in item_slots]
        if item_sprites:
            item_index, item_ox, item_oy = compose_oam_indices(
                obj_vram, item_sprites)
            if args.item_palette >= 0:
                item_colors = [cgram[0x80 + args.item_palette * 16 + i]
                               for i in range(16)]
            else:
                item_fallback = [cgram[0x80 + item_sprites[0].palette * 16 + i]
                                 for i in range(16)]
                item_colors = sample_palette(item_index, item_ox, item_oy,
                                             framebuffer, item_fallback)
            item_img = colorize(item_index, item_colors)
            item_img, ibx, iby, ibw, ibh = crop_sprite(
                item_img, item_ox, item_oy)
            item_img.save(out / "item_00.png")
            sprites.append({
                "texture": "item_00.png",
                "x": regs["hscroll"][0] + ibx,
                "y": regs["vscroll"][0] + iby,
            })

    # --- collision ------------------------------------------------------
    attrs = []
    for cby in range(16):
        for cbx in range(32):
            slot = slots[cbx // 16]
            block = info["screens"][slot - 1] if slot else None
            if block is None:
                attrs.append(0)
                continue
            g = rom.screen_blocks(set_id, block)
            b = g[cby][cbx % 16]
            attrs.append(rom.block_attr(set_id, b))

    # --- room document --------------------------------------------------
    spawn_x = regs["hscroll"][0] + bx + bw // 2
    spawn_y = regs["vscroll"][0] + by + bh
    room = {
        "name": f"area{args.area}_room",
        "world": {"width": bg_layers[0]["map_width"] * 8,
                  "height": bg_layers[0]["map_height"] * 8},
        "camera": {
            "x": regs["hscroll"][0], "y": regs["vscroll"][0],
            "min_x": regs["hscroll"][0], "max_x": regs["hscroll"][0],
            "min_y": regs["vscroll"][0], "max_y": regs["vscroll"][0],
        },
        "play": {
            "min_x": regs["hscroll"][0] + 15,
            "max_x": regs["hscroll"][0] + 241,
            "min_y": 22,
            "max_y": bg_layers[0]["map_height"] * 8,
        },
        "bg": bg_layers,
        "collision": {"cell": 16, "width": 32, "height": 16, "attrs": attrs},
        "spawn": {"x": spawn_x, "y": spawn_y},
        "player": {
            "texture": "player_idle.png",
            "feet_x": player_feet_x,
            "feet_y": player_feet_y,
        },
        "sprites": sprites,
    }
    (out / "room.json").write_text(json.dumps(room, indent=1))
    manifest = {
        "rom_sha256": rom.sha256,
        "area": args.area,
        "screens": slots,
        "source": f"{args.prefix} ({args.dump})",
        "generated_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "tool": "dc_extract 0.1",
        "tile_size": 8,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1))

    print(f"room: {out / 'room.json'}")
    print(f"player: {player_img.width}x{player_img.height}, "
          f"feet=({player_feet_x},{player_feet_y}), spawn=({spawn_x},{spawn_y}), "
          f"palette={args.player_palette}")
    print(f"collision: {len(attrs)} cells, screens {slots}, tileset set {set_id}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
