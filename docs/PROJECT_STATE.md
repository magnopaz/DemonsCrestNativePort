# Demon's Crest Fan Game — Project State and Handoff

Last updated: 2026-10-22
Project root: `projetos/opencode/DemonsCrestNativePort`
Reference recompilation: `projetos/opencode/DemonsCrestSNESRecomp`
ROM: `projetos/opencode/roms/Demon's Crest (USA).sfc`

This file is the authoritative handoff. It records the final objective, every
implemented piece, every recovered technical detail, the traps already hit, and
the ordered next steps. If work resumes later, read this file first, then
`docs/ROADMAP.md`, then the recompilation project's
`docs/NATIVE_PORT_PLAN.md`.

---

## 1. Final objective

Build a **standalone native PC fan game** based on *Demon's Crest*:

- C11 + raylib, no emulator in the shipped game.
- Original game logic is **reimplemented**, not executed as 65816.
- Original assets (maps, tiles, palettes, sprites, animations, audio) are
  extracted from the player's own ROM into a modern data format.
- The original game running in the recompilation project is used as a
  **frame-accurate behavioural oracle** for movement, timing and feel.
- Once the authentic core is stable, add new levels, enemies, story and art on
  top of the same data-driven engine.

The long-term target is a full fan-game campaign. The first concrete target is a
playable **vertical slice** of the graveyard stage.

### What this project is not

- It is not a recompilation of the generated C. `src/gen/*.c` in the recomp
  project is 65816 guest code translated to C, tied to `CpuState`, `cpu_read*`,
  the SNES PPU/APU/DMA and the recomp scheduler. It cannot be linked to raylib
  as game code.
- It is not an emulator front-end. Embedding snesrecomp would preserve the
  original game but force all new content back into 65816.
- It is not a ROM distribution. No ROM or ROM-derived asset is committed.

---

## 2. Current status at a glance

The project is a playable technical foundation, not a finished game.

Working today:

- Native C11 + raylib application builds and runs.
- 256×224 internal render target, integer scaling, 60 Hz presentation.
- Extracted area-2 graveyard room:
  - BG1 + BG2 tilemaps,
  - per-palette tile atlases,
  - Firebrand and the visible item as composed OAM sprites,
  - collision grid from the ROM block attributes,
  - room JSON with spawn/camera/world bounds.
- Firebrand can walk, jump, hover, fly, and is constrained to the captured
  viewport.
- Firebrand animation extracted from `save0`:
  - static idle,
  - 6-pose, 46-frame walk cycle,
  - jump rise,
  - hover / fly pose,
  - fall and landing,
  - attack pose (visual only; no projectile yet).
- Attack and landing timers in the player state machine; `A` plays the attack
  animation.
- Collision debug overlay (`F1`), FPS (`F3`).
- Per-frame CSV trace output.
- Deterministic demo input modes for screenshots and comparison.
- Oracle tooling:
  - WRAM trace parser,
  - OAM player-sprite parser,
  - native-vs-oracle comparison tool,
  - repeatable probe scripts for the area-2 route and `save0`,
  - debug-server animation capture (`tools/oracle/capture_anim.py`),
  - animation frame baker (`tools/dc_extract/extract_anim.py`).
- Movement constants recovered from the original code.
- Movement validated against `save0`:
  - walk speed 1.40 px/frame,
  - B is jump, A is attack,
  - tap jump is an ~11 px / 14-frame hop,
  - hold B gives a higher, longer jump and hover.

Not done yet:

- Attack projectile/fireball (the attack pose is in; the shot is not).
- Enemies/object behaviour.
- HUD.
- Audio.
- Full-room camera scrolling.
- Full campaign, bosses, menus, password/save, Mode 7 overworld.
- Public-release asset replacement / legal packaging.

---

## 3. Repository and environment

```
projetos/opencode/DemonsCrestNativePort
├── CMakeLists.txt
├── README.md
├── LICENSE
├── docs/
│   ├── PROJECT_STATE.md   ← this file
│   └── ROADMAP.md
├── src/
│   ├── main.c
│   ├── game.c / game.h
│   ├── assets.c / assets.h
│   ├── anim.c / anim.h
│   ├── level.c / level.h
│   ├── input.c / input.h
│   ├── player.c / player.h
│   ├── camera.c / camera.h
│   └── render.c / render.h
├── tools/
│   ├── capture_graveyard.sh
│   ├── dc_extract/extract.py
│   ├── dc_extract/extract_anim.py
│   └── oracle/
│       ├── capture_anim.py
│       ├── parse_wram_trace.py
│       ├── parse_player_trace.py
│       ├── compare.py
│       ├── oracle_walk.script
│       ├── oracle_move.script
│       ├── oracle_physics.script
│       ├── oracle_short.script
│       ├── oracle_control.script
│       ├── oracle_dump.script
│       ├── oracle_late.script
│       ├── oracle_save.script
│       ├── oracle_save_start.script
│       └── oracle_save0_*.script
├── external/cJSON/        vendored, MIT
├── assets/generated/      ROM-derived, git-ignored
├── captures/              recomp dumps, git-ignored
└── build/                 CMake build, git-ignored
```

Environment used:

- Linux, GCC 15.2, CMake 3.20+.
- raylib 5.5 fetched by CMake `FetchContent`.
- Python 3 with Pillow for extraction.
- Git repository initialized; see section 15 for commits.
- The recomp trace build is at
  `DemonsCrestSNESRecomp/build-trace/DemonsCrestSNESRecomp`.

---

## 4. Build, run, capture, extract

### Build and run

```sh
cd projetos/opencode/DemonsCrestNativePort
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/demonscrest
```

`--assets <dir>` selects a different generated-asset directory.

### Capture a reference dump from the recomp

```sh
RECOMP_DIR=projetos/opencode/DemonsCrestSNESRecomp \
ROM="projetos/opencode/roms/Demon's Crest (USA).sfc" \
./tools/capture_graveyard.sh
```

Or manually:

```sh
cd projetos/opencode/DemonsCrestSNESRecomp
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
SNESRECOMP_DUMP_DIR=projetos/opencode/DemonsCrestNativePort/captures/graveyard \
./build-trace/DemonsCrestSNESRecomp "projetos/opencode/roms/Demon's Crest (USA).sfc" \
  --script scripts/routes/gameplay.script
```

The extractor needs:

```
captures/graveyard/stage_600.vram.bin
captures/graveyard/stage_600.cgram.bin
captures/graveyard/stage_600.oam.bin
captures/graveyard/stage_600.regs.json
captures/graveyard/stage_600.fb.bmp   (optional, used for palette matching)
```

### Extract assets

```sh
python3 tools/dc_extract/extract.py \
  --rom "projetos/opencode/roms/Demon's Crest (USA).sfc" \
  --dump captures/graveyard \
  --prefix stage_600 \
  --area 2 \
  --out assets/generated
```

Defaults that matter:

- `--player-oam 1-11`
- `--player-palette 2`
- `--item-oam 0`
- `--item-palette 0`
- `--screens 1,2`

`--player-palette 2` is not the OAM palette value. OAM says palette 7, but the
game restores CGRAM before the dump; palette 2 is what the PPU actually used
while rendering Firebrand in this capture. This was verified by comparing
candidate palettes against the framebuffer and by visual inspection.

Generated outputs:

```
assets/generated/room.json
assets/generated/bg_pal0.png ... bg_pal7.png
assets/generated/obj_pal0.png ... obj_pal7.png
assets/generated/player_idle.png
assets/generated/item_00.png
assets/generated/manifest.json
```

`assets/generated/` and `captures/` are git-ignored. They are ROM-derived.

### Capture and extract Firebrand's animations

The animation pipeline uses the trace build's TCP debug server and its
6000-frame per-frame state ring. It drives the guest deterministically, then
queries OAM/CGRAM/VRAM snapshots after the fact; it does not rely on timed
screenshots.

```sh
python3 tools/oracle/capture_anim.py \
  --recomp ../DemonsCrestSNESRecomp \
  --rom "../roms/Demon's Crest (USA).sfc" \
  --save ../saves/save0.sav \
  --out captures/anim

python3 tools/dc_extract/extract_anim.py \
  --dump captures/anim \
  --out assets/generated \
  --clean
```

`capture_anim.py` starts the recomp with `--paused`, resumes a copy of
`save0.sav`, and records:

```
captures/anim/segments.json
captures/anim/f000000.json      OAM, CGRAM, PPU registers, WRAM $1000-$1FFF
captures/anim/f000000.vram.bin  64 KiB VRAM
```

The default route is idle, walk left/right (120 frames each), stop, tap jump,
air, hold jump, hover release, idle and attack. The extractor emits
`player_<clip>_<n>.png` plus `player_anim.json` with per-step anchor and
duration values. The clips are `idle`, `walk`, `jump`, `hover`, `fall`,
`land` and `attack`. `fly` reuses the hover clip until a separate fly capture
exists.

---

## 5. Native engine

### Render path

- `RenderTexture2D` at 256×224.
- `TEXTURE_FILTER_POINT`.
- Integer scale in `main.c`, centered with letterboxing.
- `SetTargetFPS(60)`; currently one `Game_Update` per presented frame
  (`main.c`). This is frame-based and deterministic, but not yet an
  accumulator-based catch-up loop.
- Screen is drawn with a vertically flipped source rectangle, as required by
  raylib render textures.

### Modules

- `main.c` — window, asset directory argument, fixed 60 Hz loop, integer
  scaling, screenshot/run-frame hooks.
- `game.c` — asset/level/player/camera ownership, target creation, update,
  render, trace CSV.
- `assets.c` — loads the eight BG and OBJ palette atlases, player and item
  textures, and the optional `player_anim.json` animation set; sets point
  filtering.
- `anim.c` — parses `player_anim.json`, loads per-clip PNGs, and exposes
  clip/step data with frame-based durations.
- `level.c` — parses `room.json` with cJSON, stores BG layers, collision,
  camera/play bounds, spawn, player feet offsets and static sprites.
- `input.c` — keyboard/gamepad mapping, edge detection, deterministic `DC_DEMO`
  modes.
- `player.c` — movement, variable jump, hover/fly, collision, bounds, attack
  and landing timers, animation clip/step timing.
- `camera.c` — deadzone follow, clamped to level-provided camera bounds.
- `render.c` — BG tile drawing, static sprites, animated player, debug
  collision overlay.

### Room format (`assets/generated/room.json`)

```json
{
  "name": "area2_room",
  "world": { "width": 512, "height": 256 },
  "camera": {
    "x": 207, "y": 0,
    "min_x": 207, "max_x": 207,
    "min_y": 0, "max_y": 0
  },
  "play": {
    "min_x": 222, "max_x": 448,
    "min_y": 22, "max_y": 256
  },
  "bg": [
    {
      "name": "bg2", "map_width": 64, "map_height": 32,
      "scroll_x": 106, "scroll_y": 0,
      "tilemap": [ ... 2048 u16 ... ]
    },
    {
      "name": "bg1", "map_width": 64, "map_height": 32,
      "scroll_x": 207, "scroll_y": 0,
      "tilemap": [ ... 2048 u16 ... ]
    }
  ],
  "collision": { "cell": 16, "width": 32, "height": 16,
                 "attrs": [ ... 512 u8 ... ] },
  "spawn": { "x": 332, "y": 164 },
  "player": { "texture": "player_idle.png",
              "feet_x": 13, "feet_y": 43 },
  "sprites": [ { "texture": "item_00.png", "x": 385, "y": 111 } ]
}
```

BG layer order is back-to-front: BG2 first, BG1 second.

The camera is currently locked (`min_x == max_x`, `min_y == max_y`) because the
captured VRAM tilemap is only the current ring-buffer viewport. Scrolling past
it reveals stale/empty tiles. `play` bounds keep the player sprite inside the
visible 256 px.

### Player physics

Current values in `src/player.c`:

- Max walk speed: `1.398 px/frame`.
- Acceleration: `0.28`; friction: `0.28`.
- Gravity: `0.25 px/frame²`.
- Terminal fall: `6.0 px/frame`.
- Jump tap: initial `-2.0 px/frame`, then while B is held an upward boost of
  `0.10 px/frame` up to `-4.0`.
- Hover fall cap: `1.05 px/frame`.
- Fly-up cap: `-2.0 px/frame`.
- Collision AABB: 12×22 px, feet-centered.
- All non-zero block attributes are currently treated as solid.
- Player is clamped to `play` bounds.
- Attack: `A` starts a 31-frame attack animation and cooldown. This is visual
  only; no projectile is spawned yet.
- Landing: the first grounded frame after airtime starts an 8-frame landing
  animation.
- Fly/jump interaction: once the fly branch is active, it no longer re-enters
  the jump-boost branch each frame. This removed the state flicker between
  `FLY` and `JUMP` while holding B + Up.

Recovered original constants (from the recompiled player code):

| Value | Original code | Meaning |
|---|---|---|
| `$1020 = 0x0166` | 1.398 px/frame | max walk speed |
| `$22 = 0x0400` | 4.0 px/frame | full/held jump velocity |
| `$26 = 0x0040` | 0.25 px/frame² | gravity at jump init |
| `$26 = 0x0038` (observed in save0 runtime) | 0.21875 | current object gravity |
| `0xFA00` | -6.0 px/frame | fall-speed clamp |

Vertical integrator: bank `80:8D03`:
`position += velocity; velocity -= gravity;` then clamp fall speed at
`0xFA00`.

Horizontal integrator: bank `80:8D35`:
`position += velocity`.

Jump initializer: bank `85:C1D4`:
sets `$22 = 0x0400`, `$26 = 0x0040`, and a state byte `$39`.

### Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | Arrows / WASD | D-pad |
| Jump / hover / fly | Z or Space | B |
| Fly upward | hold jump + Up | hold B + Up |
| Attack | X or J | A |
| Collision overlay | F1 | — |
| FPS | F3 | — |

The original mapping was verified from `save0`:
**B = jump, A = attack**.
This is the SNES hardware mapping, not the script-runner mask.

### Debug / test hooks

| Variable | Effect |
|---|---|
| `DC_RUN_FRAMES=N` | run N updates then exit |
| `DC_SCREENSHOT=name.png` | save last presented frame |
| `DC_TRACE_CSV=path` | per-frame player/camera CSV |
| `DC_DEMO=1` | long walk/jump/fly demo |
| `DC_DEMO=2` | short walk/jump demo |
| `DC_DEMO=3` | mirrors the area-2 physics probe timing |
| `DC_DEMO=4` | tap-jump vs hold-jump comparison |
| `DC_DEMO=5` | animation regression: walk, attack, jump, fly |
| `DC_LAYER_MASK=1|2|3` | BG2 only / BG1 only / both |

`DC_SCREENSHOT` is handled by raylib, which writes the basename into the
working directory even when an absolute path is supplied.

Native trace CSV columns:

```
frame,player_x,player_y,vx,vy,state,camera_x,camera_y
```

---

## 6. Asset extraction details

`tools/dc_extract/extract.py` verifies the ROM and bakes one room. It does not
read the ROM at game runtime.

### ROM identity

- File: `Demon's Crest (USA).sfc`
- Size: 2 MiB, unheadered
- Mapping: LoROM, FastROM, cartridge type `$00`, no SRAM, no coprocessor
- CRC32: `e8236ad2`
- SHA-256: `18d40a807d5f88c5b6a1ad849eec7e0f189225d9a1586037c850f6680b5844de`

### Level/object tables used

| Data | Address |
|---|---|
| Area info: set, screen list | `$81:A291` |
| Area grid / width / height | `$81:A6D8` |
| Screens (superblock lists) | `$8F + set`, `$8000 + screen*128` |
| Superblocks | `$92 + set`, `$8000 + super*8` |
| Blocks: 4 tilemap words + attribute | `$95 + set`, `$98:8000 + (set<<12) + block` |
| Object lists | `$81:C874` (normal), `$81:C9D0` (cleared areas) |
| Area exit / boss gate | `$81:97DA`, `$81:80A1` |
| Tile sets (sprite graphics) | `$BD:9953` → `$98:D000` |
| Sprite list per area | `$81:AE11` |
| Palettes per area | `$81:8479` |
| Mid-stage / segment loaders | `$80:BFB5`, `$80:C11B` |
| Tile animation | `$81:C125` |

Area 2 data observed:

- Set `0`.
- Grid 8×2.
- 21 screen slots.
- 20 objects in the normal list.
- First playable scene starts with camera scroll `x=207`, `y=0`.

### Graphics decoder

At `$98:D000 + id`:

- 24-bit pointer to compressed data,
- 4th byte = number of 16×16 units.

Decompressor at `$80:C2AF` / `$80:C300`:

- each call reads two mask bytes and one fill byte;
- each mask nibble controls four output bytes;
- each call emits 16 bytes;
- a 16×16 unit is 8 calls (128 bytes);
- tile allocation advances 0x40 bytes per 16×16 unit, skipping the bottom
  tile row (`+0x200` when bit 9 set).

The extractor does not use this decoder yet; it uses VRAM dumps for exact
visuals. The decoder is documented here for the future full-content pipeline.

### Captured frame facts (`stage_600`)

- PPU mode 1 (plus BG3 priority), `inidisp=15`.
- BG1 tilemap base `$4800`, 64×32; scroll `207,0`.
- BG2 tilemap base `$4000`, 64×32; scroll `106,0`.
- BG3 tilemap base `$5800`.
- BG1/BG2 character base 0.
- BG3 character base `$5000`.
- OBSEL `3`: OBJ base `0x6000` words, small 8×8, large 16×16.
- Player OAM group uses palette 7 in OAM, but the frame was rendered with
  CGRAM palette 2.
- Visible item uses OAM slot 0, palette 0.
- Player composite: 27×43 px; feet offset `(13, 43)`.
- Spawn used by the native game: `(332, 164)`.
- Player’s idle OAM center/feet in the reference frame: `(332.5, 164.0)`.
- The item sprite is at world `(385, 111)`.

### Atlases

BG atlases:

- 512×512 RGBA.
- 32×32 tiles of 8×8, per palette 0–7.
- Four 256×256 quadrants: normal, horizontal flip, vertical flip, both.
- Pre-flipping is baked because raylib `DrawTexturePro` with negative source
  rectangles produced garbage on flipped BG tiles.
- Pixel index 0 is transparent.

OBJ atlases:

- 128×128 RGBA, 16×16 tiles, per palette 0–7.
- Currently generated for future use; the player/item are baked PNGs.

### OAM parser

- Low table: 512 bytes, 128 sprites × 4 bytes.
- High table: 32 bytes at offset `0x200`.
- Each sprite: x, y, tile, attribute.
- High table bits: X bit 8 and size select.
- `parse_oam()` returns screen positions, 8/16 px size, tile, palette, flips.
- Player identification is **palette 7** in the OAM group; slot range changes
  per animation/frame, so never assume slots 1–11 outside the reference frame.

### Collision

- Built from screens `[1, 2]` for area 2.
- 32×16 block grid (512×256 px).
- Attribute byte from `$98:8000 + (set<<12) + block`.
- Current native rule: any non-zero attribute is solid.
- The meaning of the attribute bits (slopes, water, one-way ledges) is not yet
  decoded.

### Important extraction limitation

The native BG1/BG2 tilemaps come from the **VRAM ring buffer** of the captured
frame, not from the full block/screen data. The block-table approach was tried
and rendered fences/wrong tiles because the game’s loader remaps/deduplicates
tiles into VRAM. Therefore:

- the captured viewport is exact;
- scrolling outside it shows stale/empty tiles;
- the camera is currently locked.

To unlock the full room, either:

1. decode the loader’s tile remap and build the room from the block tables, or
2. capture multiple viewports and stitch them with overlap checking.

### Player animation extraction

The animation capture uses `save0` (normal graveyard gameplay), not the area-2
landing route. The debug server is driven while the guest is paused, frame
targets are absolute, and every frame is read back from the 6000-frame state
ring. This avoids the timing problems of a scripted `dump` during a button
hold.

Observed clip data in `save0`:

| Clip | Poses | Duration (frames) | Notes |
|---|---|---|---|
| `idle` | 1 | loop | OAM group is visually static |
| `walk` | 6 | 7,7,9,7,7,9 | 46-frame cycle; first pose after start is an acceleration pose and is excluded |
| `jump` | 3 | 1,2,hold | takeoff, launch, rise; last pose holds until hover/fall |
| `hover` | 1 | loop | 18-piece wing-spread pose; also used for `fly` |
| `fall` | 2 | 3,2 (hold) | descent poses |
| `land` | 2 | 3,5 | 8 frames, matching `land_timer` |
| `attack` | 3 | 11,4,16 | 31 frames, matching `attack_timer` |

Frame facts:

- The player is the palette-7 OAM group; sprites with screen Y ≥ 224 are
  offscreen and excluded.
- `save0`'s player palette is CGRAM OBJ palette 7. The area-2 stage-600 dump
  needed palette 2 because CGRAM had been restored before dumping; do not
  reuse the area-2 palette override for animation captures.
- Frames are canonicalized to face right by mirroring when the OAM group is
  not hflipped. The renderer mirrors the texture for left-facing movement.
- Each frame anchor is `object_X_screen - bbox_min_x` horizontally and
  `bbox_height` vertically. The body stays aligned while wings change the
  bbox width; the feet stay on `player.y`.
- PNGs are 8-bit RGBA at native resolution (no scaling). The engine sets
  `TEXTURE_FILTER_POINT`.
- `player_anim.json` stores, per clip, `loop` plus steps with `texture`,
  `anchor_x`, `anchor_y` and `duration` in game frames. The C loader in
  `src/anim.c` caps the set at 64 steps and falls back to `player_idle.png`
  when the file is absent.

The attack capture is visual only. Pressing A in `save0` produced the attack
pose but no on-screen projectile in the captured window; projectile extraction
needs a later save/area with an actual shot (save2 is the likely candidate).

---

## 7. Oracle tooling and results

### Recomp trace facility

`SNESRECOMP_WRAM_TRACE_FILE=path` writes JSONL of every changed byte in low WRAM
(`$0000–$1FFF`), one line per byte:

```json
{"f":123,"adr":"0x01030","old":"0x00","val":"0x12"}
```

This is the cheapest way to record player/object state without building the Lua
bridge.

### `parse_wram_trace.py`

Reconstructs per-frame player/task fields from the trace. It currently emits:

```
frame,frame_rel,area,player_x,player_y,screen_x,screen_y,cam_x,cam_y
```

It reads the `$1000` task fields:

- `$1030–$1032` 24-bit X,
- `$1033–$1035` 24-bit Y,
- `$00C0/$00C1` camera X,
- `$00C4/$00C5` camera Y.

Caveat: the `$1000` task’s Y oscillates during idle animation and is **not** a
clean visual position source. X is more stable. The OAM-based parser below is
the reliable visual ground truth.

### `parse_player_trace.py`

Reconstructs the OAM buffer from the same WRAM trace:

- low OAM `$0700–$08FF`,
- high OAM `$0900–$091F`,
- filters the palette-7 player group,
- computes the sprite bbox,
- outputs center X and feet Y plus scroll.

Output:

```
frame,frame_rel,screen_x,screen_y,w,h,scroll_x,scroll_y,player_x,player_y,area
```

`player_x = bbox_center + scroll_x`
`player_y = bbox_bottom + scroll_y`

This is the current oracle position. It matched the native spawn exactly in
`save0`: `(332.5, 164.0)`.

### `compare.py`

Aligns native `DC_TRACE_CSV` and oracle CSV at a chosen oracle `frame_rel`,
compares world-space deltas, reports max/mean delta and first frame above a
threshold.

It is ready, but the area-2 route probes were contaminated by the landing
cinematic and room transitions. Use `save0` for clean probes.

### Probe routes

- `oracle_walk.script` — boot → area 2 → movement probes.
- `oracle_move.script` — long movement sequence.
- `oracle_physics.script` — movement/jump probes after the landing.
- `oracle_short.script` — short probe after frame 800.
- `oracle_control.script` — attack/walk/jump test.
- `oracle_dump.script` — movement with machine dumps.
- `oracle_late.script` — long-settle probe (area transitions).
- `oracle_save.script` — generic save-state probe.
- `oracle_save_start.script` — attempts to leave a locked intro.
- `oracle_save0_short.script` — clean short walk probe from save0.
- `oracle_save0_buttons.script` — B/X/Y/A identification.
- `oracle_save0_buttons2.script` — Up/R/L/Start/Select identification.
- `oracle_save0_press.script` — first frames after A/B.
- `oracle_save0_jump.script` — B jump arc samples.

### Save states

Only `save0.sav` remains after the owner cleaned the others:

```
projetos/opencode/DemonsCrestSNESRecomp/build-trace/saves/save0.sav
```

It is a clean normal graveyard gameplay state with Firebrand standing on a
ledge, HUD visible, no boss.

`--resume-state` **deletes the file after loading**, so always copy it first:

```sh
cp saves/save0.sav /tmp/save0_probe.sav
./build-trace/DemonsCrestSNESRecomp "$ROM" \
  --resume-state /tmp/save0_probe.sav --script ...
```

### Debug server

The trace build opens a TCP debug server (default port 4377; override with
`SNESRECOMP_DEBUG_PORT`). Useful commands:

- `wram_writes_at <hex_addr> [from] [to] [limit]`
- `save_state <filename>`
- `load_state <filename>`
- `set_controller`
- `dump_ram`, `dump_vram`, `dump_cgram`, `dump_oam`
- `oam_render_get`
- `block_watch_*`, `set_wram_watch`, `wram_watch_log_get`

`wram_writes_at` identified the original player code:

- `$1030` written by `bank_80_8D35_M1X1`
  (parent `bank_80_8C77_M1X1`) — horizontal integrator.
- `$1033` and `$1022` written by `bank_80_8D03_M1X1`
  (parent `bank_00_F0F2_M1X1`) — vertical integrator.
- `$1400` written by `bank_02_86C4_M0X1`
  (parent `interp@$8286A4`) — object cull/update loop.

### Original object/task system

- Object table base `$1080`, stride `$50`, end `$1D50`.
- Loop at `$82:8611` (mirror `$02:8611`):
  - skip if `$0000` is zero,
  - `TCD` to the object base,
  - species `$02` selects a handler via `jsr ($8000,X)` table,
  - bits in `$0000` choose main/secondary update paths (`$8000`, `$8180`,
    `$8300`, `$8480`),
  - copies `$31`/`$34` to screen positions `$1C`/`$1E`.
- Player task is at `$1000`, separate from the object table.
- Player object fields identified:
  - `$20` max walk speed (`0x0166`),
  - `$22` Y velocity (initial `0xFF00`, jump init `0x0400`),
  - `$26` gravity (`0x0038` runtime, `0x0040` jump init),
  - `$30` X position,
  - `$33` Y position,
  - `$35` timer/flag,
  - `$39` state byte.
- Vertical integrator `80:8D03`:
  `$22 -= $26; $33 += $22;` clamp `$22` at `0xFA00` (-6.0).
- Horizontal integrator `80:8D35`:
  `$30 += $20`.
- Jump initializer `85:C1D4`:
  `$22 = 0x0400; $39 = ...; $26 = 0x0040`.

### Input mapping (verified)

The game stores the raw automatic-joypad word at `$0090/$0091` (active-high
after inversion). The bits are:

| Button | Store location |
|---|---|
| Right | `$0091` bit 0 (0x01) |
| Left | `$0091` bit 1 (0x02) |
| Down | `$0091` bit 2 (0x04) |
| Up | `$0091` bit 3 (0x08) |
| Start | `$0091` bit 4 (0x10) |
| Select | `$0091` bit 5 (0x20) |
| Y | `$0091` bit 6 (0x40) |
| B | `$0091` bit 7 (0x80) |
| A | `$0090` bit 7 (0x80) |
| X | `$0090` bit 6 (0x40) |
| L | `$0090` bit 5 (0x20) |
| R | `$0090` bit 4 (0x10) |

This was a source of confusion earlier: the script mask and the raw pad word
are in different orders, but the scripts are correct. The script `press right`
does produce Right, `press a` produces A, etc.

### `save0` validation results

- Walk right 10 frames: world X increases exactly 14 px → **1.40 px/frame**.
  This matches the native `1.398`.
- B is jump; A is attack.
- B tap: feet `191 → 180 → 187 → 191` over ~14 frames, an ~11 px hop.
- Holding B gives a higher/longer jump, then hover.
- The native spawn `(332.5, 164.0)` matches the reference idle player position.
- Native variable jump now reproduces the tap hop closely.

### Area-2 route caveat

The boot → area-2 route reaches a long landing/cinematic. Scripted movement
there is either ignored, triggers camera pans, or triggers room transitions.
Do **not** use that route for clean physics measurements. Use `save0` or another
normal gameplay save.

---

## 8. Frozen technical facts and traps

These were already discovered the hard way; do not rediscover them.

1. **Generated C is not portable game code.**
   It is 65816 translated to C with `CpuState` and `cpu_read*`/`cpu_write*`.
   It runs only inside the recomp runtime.

2. **The recomp is the oracle, not the game.**
   Keep it for traces, dumps, visual references and frame-accurate comparison.

3. **VRAM is a ring buffer, not a full room.**
   A single captured 64×32 tilemap covers the current viewport plus some
   stale/duplicate content. Scrolling past it reveals wrong tiles. Lock the
   camera until a wider capture or the loader remap is implemented.

4. **Block-table tilemaps do not directly match VRAM.**
   The loader remaps/deduplicates tiles. The block-table renders looked like
   fences/garbage with the captured atlas. Do not use the block tilemaps for
   BG rendering until the remap is reversed.

5. **CGRAM can change after rendering.**
   OAM said player palette 7; the visible sprite uses palette 2. The game
   restores/overwrites CGRAM before the dump. The extractor defaults to
   `--player-palette 2` for this capture and verifies visually.

6. **raylib negative source rectangles are unreliable for tile flipping.**
   Pre-bake four flip quadrants per BG palette atlas. This fixed garbage on
   flipped BG2 tiles. The player draw still uses a negative source width; if
   facing bugs appear, pre-bake the player mirrored frames too.

7. **Player OAM slots are dynamic.**
   The player is the palette-7 group, but its OAM slot range changes with
   animation/effects. Never hardcode slots 1–11 outside the reference frame.
   The item is slot 0 in the reference frame.

8. **The player task’s `$1033` Y oscillates at idle.**
   It is not a clean visual position. Use the OAM bbox center/feet for visual
   oracle comparisons. The `$1000` task X is useful; Y is not yet trustworthy.

9. **The area-2 landing is a cinematic.**
   Inputs may be ignored or cause transitions. `save0` is the clean state.

10. **Script button masks are correct.**
    The raw `$0090/$0091` layout is the hardware automatic-read word; Right is
    `$0091:01`, A is `$0090:80`, B is `$0091:80`. Do not “fix” the scripts.

11. **`--resume-state` consumes the file.**
    Copy `save0.sav` before each run.

12. **The camera is currently locked.**
    This is deliberate while the room data is only one VRAM viewport. Unlock it
    only after the tilemap capture/remap problem is solved.

13. **Collision attributes are approximated.**
    Every non-zero block attribute is solid. Slopes, water, one-way ledges and
    hazards still need decoding.

14. **The HUD is absent.**
    BG3/status bar extraction is a later step.

15. **No audio yet.**
    The game is silent. Do not mistake this for a bug.

16. **Animation palette is not the area-2 palette.**
    `save0` renders Firebrand with CGRAM OBJ palette 7; the area-2 stage-600
    dump used palette 2 because CGRAM had been restored before the dump. The
    animation extractor defaults to palette 7 and verifies against the
    reference framebuffer.

17. **The debug server's `set_controller` does not accept bare decimal masks.**
    Use button names (`right`, `b+up`, `a`) or an explicit `0x` hex mask.
    `set_controller 128` is parsed as an unknown button and the command fails;
    capture tools must check the JSON error field instead of ignoring it.

---

## 9. Current limitations

- Single captured graveyard viewport; no room scrolling.
- Animation covers idle/walk/jump/hover/fall/land/attack. Hurt, death and a
  dedicated fly-up clip are still missing; one directional set is mirrored.
- Attack is visual only; there is no fireball/projectile yet.
- No enemies.
- No HUD.
- No audio.
- Approximate collision.
- Area-2 spawn alignment is still off: the reference player sprite stands with
  its feet at y=164, while the first solid collision row under that point is
  y=176, so the native player settles about 11 px lower. This predates the
  animation work and needs the collision-cell/block-attribute alignment
  decoded.
- Approximate variable jump/hover/fly tuning.
- No title screen, menus, save/password, stage select, map.
- No full campaign or bosses.
- No Windows build tested in this session (CMake/raylib are portable, but the
  build was verified on Linux).
- No public-release asset path yet.

---

## 10. Ordered next steps

### Step 1 — Extract Firebrand animation frames (done)

Done in this session:

1. Added `tools/oracle/capture_anim.py`, which drives the trace build's debug
   server while paused and reads per-frame OAM/CGRAM/VRAM/register snapshots
   from the state ring.
2. Added `tools/dc_extract/extract_anim.py`, which composes the palette-7
   player group, canonicalizes facing, deduplicates poses, recovers the walk
   cycle and emits `player_<clip>_<n>.png` plus `player_anim.json`.
3. Added `src/anim.c` / `src/anim.h` and a frame-based animation state machine
   in `player.c` / `render.c`.
4. Added attack and landing timers, and fixed the fly/jump state flicker.

Acceptance:

- Walk animation plays at the original 46-frame cadence (6 poses).
- Jump rise, hover/fly, fall, landing and attack use the captured poses.
- Feet stay aligned with the ground/collision box via per-frame anchors.
- The collision AABB remains independent of the sprite frame.
- `DC_DEMO=5` exercises walk, attack, jump and fly for screenshots.

Remaining polish: the art is still one directional set mirrored by the
renderer; hurt/death frames and a real fly-up clip are future captures.

### Step 2 — Attack and fireball (next)

- A is attack; the 31-frame attack animation and cooldown are already in.
- Capture the projectile in a save/area where a shot is actually visible
  (`save2` is the likely candidate; the save0 capture showed the pose but no
  on-screen projectile).
- Extract the fireball sprite from OAM (it may use palette 7 or another
  player-effect palette).
- Implement:
  - projectile spawn on the attack frame (match the original timing),
  - spawn offset and facing direction,
  - projectile velocity, lifetime and collision with room geometry,
  - hit reaction on the future enemy object.
- Add a placeholder HUD or at least a debug counter.

Acceptance:

- Attack animation and fireball appear at the original timing.
- Fireball travels in the facing direction and despawns correctly.

### Step 3 — Full jump/hover tuning with a continuous B hold

- Use the trace build debug server (`set_controller`) to hold B continuously
  while dumping every frame. The script alone cannot dump during a long hold.
- Measure:
  - tap jump arc,
  - full hold jump height/airtime,
  - hover fall cap,
  - fly-up cap,
  - acceleration/deceleration for walk.
- Update `player.c` only from measured values.
- Re-run `compare.py` against the save0 OAM trace.

### Step 4 — Room scrolling and full area 2

Two options:

1. Decode the loader’s tile remap:
   - find the mapping from block-table tile indices to VRAM slots,
   - apply it when building the full 8×2 area map,
   - verify against multiple reference frames.
2. Multi-view capture:
   - capture overlapping viewports while walking,
   - stitch tilemaps with overlap validation,
   - store the full room tilemap.

Then:

- unlock the camera,
- implement the original deadzone/scroll,
- add room/screen transitions,
- add the object spawn/despawn/cull window.

Acceptance:

- Camera follows across the whole starting room without stale/garbage tiles.
- Player and background remain aligned at every scroll position.

### Step 5 — Object system and one enemy

- Use the object table at `$1080`, stride `$50`, types/species dispatch.
- Object list loader: `$82:8BDB`.
- Choose one graveyard enemy with a simple state machine.
- Port:
  - spawn from area object list,
  - update order,
  - cull/despawn,
  - collision with player/fireball,
  - damage/knockback/death.
- Validate against save0/WRAM traces.

### Step 6 — HUD

- Extract the BG3 status bar/crest icons from a gameplay frame.
- Render health, crest/form and item icons.
- Match the original layout, including the top bar anchoring used by the
  widescreen work.
- Add pause/menu later.

### Step 7 — Audio

Options:

1. Record a track from the recomp for development (not distribution-safe).
2. Extract BRR samples and sequence data and implement a small DSP/BRR player.
3. Embed an SPC snapshot player (still emulation).

Decide the public-release path before shipping. For the slice, a placeholder
or recorded loop is acceptable.

### Step 8 — Title/menu and game flow

- Boot → title → new game → graveyard.
- Stage select / map later.
- Save/password handling later.
- Options: scale, audio volume, controls.

### Step 9 — Full content

- All areas, object IDs, bosses, forms, spells, items.
- Mode 7 overworld.
- Full audio.
- New fan-game levels/story/art on top of the authentic core.

---

## 11. Long-term roadmap

| Phase | Goal | Status |
|---|---|---|
| 0 | Extraction proof: area 2 map + VRAM/CGRAM dump + static render | done |
| 1 | Engine skeleton: raylib, fixed 60 Hz, BG layers, input, camera | done |
| 2 | Firebrand core: walk/jump/hover/fly/collision/animation | done except hurt/death and fly-up frames |
| 3 | Combat and objects: attack, one enemy, damage, pickup, HUD | attack pose done; projectile/enemies/HUD next |
| 4 | Vertical slice: 3–5 rooms, transitions, title/menu, audio placeholder | not started |
| 5 | Full campaign: all areas, bosses, forms, Mode 7, password/save | not started |
| 6 | Fan-game content: new levels/story/art | not started |

A reasonable estimate for the vertical slice is **10–14 full-time weeks** for
one experienced developer, plus buffer for reverse engineering. A full port is
a **12–24 month** scale, not a sprint.

---

## 12. Definition of done for the first version

A short, playable build where:

- the game opens directly into the graveyard;
- Firebrand walks, jumps, hovers and flies with the original timing;
- a fireball kills one enemy, which drops or triggers a pickup;
- the camera scrolls through 3–5 rooms and an exit transitions;
- the HUD shows health and the current crest;
- a 600-frame input script produces the same positions/animations as the
  recomp within 1 px / 1 frame;
- a new room can be added by editing JSON and a PNG atlas without touching C;
- the build runs on Linux and Windows;
- no ROM-derived asset is committed or distributed.

Definition of done for the **final fan game**:

- A complete native game with its own campaign, using the original game’s
  movement/combat feel as the foundation.
- Data-driven levels, enemies, art and audio.
- Stable save/options/menus.
- Legally distributable: either require the player’s own ROM at generation
  time, or ship with original/replacement assets.

---

## 13. Risks and open questions

| Risk | Impact | Mitigation |
|---|---|---|
| Tilemap remap not decoded | camera/full room blocked | debug loader; multi-view capture |
| Player state machine not fully ported | feel drifts | use generated C + traces; port balanced |
| Collision attributes unknown | wrong slopes/water/ledges | probe original geometry; decode bits |
| Collision cell vertical alignment | player settles ~11 px below the reference sprite | decode block origin/offset against multiple frames |
| Animation extraction | visual mismatch | per-frame OAM/VRAM dumps |
| Audio extraction | no soundtrack | BRR/DSP or recorded placeholder |
| Object AI breadth | huge scope | one enemy at a time |
| Legal | distribution risk | no assets in repo; ROM-generated at runtime |
| GPL randomizer code | license contamination | reimplement formats; keep GPL tools separate |
| raylib flip quirks | visual corruption | pre-baked flips; avoid negative rects |
| CGRAM restore timing | wrong sprite palettes | sample framebuffer; per-capture palette config |

Open technical questions:

- Exact player state machine/transition constants.
- Meaning of block attribute bits.
- Vertical origin of the collision grid relative to the sprite feet.
- Full object ID → behaviour map.
- How the loader remaps block tile indices to VRAM.
- Whether the `$1000` task Y can be made a clean position source.
- Best audio path for distribution.
- Widescreen support policy.
- Windows test/build path.

---

## 14. Legal and licensing

- The engine and tools in this repository are original code under MIT
  (`LICENSE`).
- *Demon's Crest* and all ROM-derived art, music, maps and data remain the
  property of Capcom.
- `assets/generated/` and `captures/` must never be committed or distributed.
- A public release must either:
  1. require the player’s own ROM and generate assets locally (like the
     recomp’s setup host), or
  2. replace all original art/music with original or licensed assets.
- The external randomizer used as a format reference is GPL-3.0. Do not copy
  its code into this MIT engine. Reimplement the documented formats or keep a
  separate GPL tool that communicates through files.
- The recompilation project is MIT, but its generated C and extracted game
  data are ROM-derived and also must not be distributed.

---

## 15. Git history

The current repository is a fresh copy whose history starts at `Initial
commit`; the older hash list that used to live here came from the previous
workspace copy and is no longer the authoritative history.

```
283ed28 Initial commit
8070c50 ... (imported native-port state)
        Add Firebrand animation capture and state machine (this session)
```

The `docs/` directory is now tracked. The accidental `docs` entry in
`.gitignore` was removed so the handoff files travel with the code.

---

## 16. Resume checklist

When resuming:

1. Read this file and `docs/ROADMAP.md`.
2. `git status` — confirm clean.
3. Build:
   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```
4. Verify assets exist:
   ```sh
   ls assets/generated/room.json assets/generated/player_idle.png \
      assets/generated/player_anim.json
   ```
   If missing, run the capture + extract steps in section 4. For animation
   assets specifically:
   ```sh
   python3 tools/oracle/capture_anim.py --out captures/anim
   python3 tools/dc_extract/extract_anim.py --dump captures/anim \
     --out assets/generated --clean
   ```
5. Run the game and take a reference screenshot:
   ```sh
   DC_RUN_FRAMES=10 DC_SCREENSHOT=check.png ./build/demonscrest
   ```
6. Confirm the graveyard scene, player and item render.
7. Copy `save0.sav` before any `--resume-state` run.
8. For oracle work, use `tools/oracle/oracle_save0_short.script` and
   `parse_player_trace.py`, not the area-2 landing route.
9. For physics changes, compare with `compare.py` and keep a trace before/after.
10. Do not commit `assets/generated/` or `captures/`.

---

## 17. One-paragraph summary

The project has a working native C11 + raylib foundation that renders the
original graveyard room from extracted ROM data and lets Firebrand walk, jump,
hover and fly with the original movement constants. Firebrand's idle, walk,
jump, hover, fall, landing and attack frames were extracted from `save0`
through a deterministic debug-server capture and are now driven by a
frame-based animation state machine. The recompilation is used as a
frame-accurate oracle, not as game code. The camera is intentionally locked to
the captured viewport because the current tilemap is a VRAM snapshot, not a
full-room layout. The next milestone is the fireball and projectile collision,
followed by a full-room tilemap, one enemy, HUD and audio. The long-term
objective is a complete standalone fan game with the original game's feel and
new content built on the same data-driven engine.
