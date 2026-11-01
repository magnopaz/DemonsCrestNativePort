# Tooling

## Auditing generated sprites

`tools/audit_sprites.py` is the structural gate for `species_anim.json`
(and reports the player/form sheets). It rejects missing textures, anchors
outside the image, duplicate images assigned to different species, and
fragmented captures that are clearly tilemap fragments:

```
python tools/audit_sprites.py --assets assets/generated --strict
python tools/audit_sprites.py --assets assets/generated --quarantine
```

`--quarantine` removes failing species entries from `species_anim.json` and
keeps a `.bak` next to it. `run_regression.py` runs the strict audit when the
asset file exists, so a bad bake cannot silently enter the build.

## Baking the player form sheets

`tools/dc_extract/bake_player_forms.py` uses measured per-sheet clip regions
and connected pose components, not empty-column splitting. It removes captions,
keeps the white/gold hurt pose and writes `fb_<form>_<clip>_<i>.png` plus
`player_forms.json`. The forms are the default; `DC_PLAYER_FORMS=0` restores the
captured base animation.

## Baking Arma's first fight

`tools/dc_extract/bake_arma.py` uses individually measured pose boxes. Do not
replace them with whole sheet rows: the rows contain captions and several
poses, which is what produced the earlier multi-sprite PNGs.

## Baking one labelled sheet row

For enemy sheets such as the labelled `Enemies 1.gif`, use
`tools/sheet_row.py` after measuring the band that contains only the sprites
(exclude the caption row):

```
python tools/sheet_row.py \
  --sheet "docs/sprites/SNES - Demon's Crest - Enemies & Bosses - Enemies 1.gif" \
  --species 0x2c --x0 0 --x1 420 --y0 300 --y1 380 \
  --source "Scavenger Demon row" --state 0
```

The tool splits on background columns, keys the flat background, removes small
detached caption letters/sparks, and writes/merges the species entry. It is
deliberately not a whole-sheet auto-detector: the caller must give the correct
sprite band.

## Boss sheet mappings

The named boss saves are the authoritative source for species ids. Current
mappings recovered from their live object pools:

| boss/room | species |
|---|---|
| Holothurion (52) | `$6F` (parts `$90`, `$92`) |
| Scula (26) | `$7B`, `$7C` (plus `$7F`) |
| Ovnunu (54) | `$16`, `$17`, `$18` |
| Crawler (ice palace) | `$82` (plus `$83/$84/$85` parts) |
| Belth (boneyard) | `$68` |
| Phalanx (84) | `$86` first form + `$88`; `$8D` second-form body |
| demon realm (86) | `$58`, `$5C`, `$5D`, `$5E`, `$5F` |
| final second form (90) | `$BC` (plus `$8A`) |

The demon-realm `$58` object uses `Flame Lord.gif`; the final second form `$BC`
uses `Dark Demon.gif`. Phalanx's second form uses `$8D` and the three full-body
poses from `Ultimate Phalanx.gif`; the second-form object keeps the `$86` head
AI. Boss sheets can be baked directly from measured full-body boxes with the
generic tool:

```text
python tools/dc_extract/bake_boss_rows.py \
  --sheet "docs/sprites/...Dark Demon.gif" --species 0xBC \
  --boxes "6,50,132,148;140,52,272,148;280,52,400,148"
```

`bake_holothurion.py` writes `$6F` from `Holothurion.gif`;
`bake_scula.py` writes `$7B/$7C` from `Scula.gif` (the boss save has two
enemies at the same arena position); `bake_ovnunu.py` writes the temple boss
`$16` from `Ovnunu.gif`; `bake_boss_rows.py` bakes measured full-body boxes
for Crawler (`$82`) and Belth (`$68`); `clean_somulo.py` removes the two
measured gate-bar columns from the native Somulo reference and re-anchors the
full body at the floor; `bake_npc96.py` keeps the area-8 town NPC on
`NPCs.png`.
`fix_room_boss_species.py --room room_area52.json --old 150 --new 111` patches
the boss object when the ROM room list names the wrong species.
`add_room_boss.py --room room_area26.json --bosses "123:1320:154,124:1380:154"`
adds the two Scula boss objects from the save positions.

Some species ids are reused by different object kinds; `species_anim.json`
also supports an optional `"variant": "boss"` entry, used only for
`OBJECT_BOSS`.

## Full-stage map fallback

When a room's ROM BG tilemap is demonstrably wrong, install a community
full-stage map as a world-locked backdrop and leave collision/object data
untouched:

```
python tools/dc_extract/install_full_map_backdrop.py \
  --area 2 \
  --map "docs/FullMaps/images/demons-crest-murkland-cemetery-snes-map.webp" \
  --parallax 1.0 --y -70
```

The tool writes `bg_backdrop_area<N>.png`, replaces the BG tile layers with a
transparent one-cell layer (tile 0 in the baked atlases is transparent), and
sets `backdrop_parallax`/`backdrop_y` in the room JSON. This is the deliberate
fallback for areas whose tilemap/CHR cannot be trusted; the image is expected
to contain baked scenery (and possibly baked props), so it is not a substitute
for a proper ROM/VRAM rebuild.

## Diagnosing background layers

`tools/dc_extract/bg_recompose.py <dump_dir> --frame <reference.png>` renders
BG1/BG2 from the same VRAM/CGRAM/registers and writes the flattened result plus
separate `<out>_bg1.png` / `<out>_bg2.png` layers. Compare the result with the
reference frame before touching the room data; the area-2 investigation showed
that the extracted BG1 tilemap itself was wrong (~47% tile agreement), so an
atlas rebake alone cannot fix that room.

## Replaying a recomp recording

`tools/oracle/replay_capture.py` converts a recorder `input_log.jsonl` into the
format `replay_input` expects, replays it from boot, and dumps exact OAM/CGRAM/
VRAM/WRAM plus a screenshot for requested guest frames:

```
python tools/oracle/replay_capture.py \
  --recording "M:/Opencode/DemonsCrestSNESRecomp/captures/rec_20261027_151150" \
  --out captures/ref --frames 1200,2200,7002 --png
```

This is the reference path for checking a sprite or animation against the
original game without re-playing it.

## Extracting frames from a video

`tools/video_frames.py` pulls frames out of any mp4/mkv using ffmpeg (installed as
a Python wheel, no system install needed):

```
python tools/video_frames.py "C:\path\take.mp4" --every 20 --sheet
```

- `--every N` keeps one frame in N (default 10), `--fps F` asks for F per second;
- `--start/--end SEC` limit the range, `--scale W` downscales;
- `--sheet` writes `contact_sheet.png` with frame numbers, so a whole take can be
  skimmed at once.

Frames land in `<video-name>/frame_00001.png`. Requires
`pip install imageio-ffmpeg` (the tool says so if it is missing).
