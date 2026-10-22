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
- Movement constants and state machine recovered from the original Bank `$80`
  player code (`$80:DB00..F310`).
- Movement validated against `save0`:
  - walk speed 1.40 px/frame (`$1020`: `0x00B3` initial step → `0x0166` cap),
  - default SNES controls (`$80:DB00` via `$1E3A..$1E4C`):
    **B** = Jump / Hover toggle, **Y** = Fireball shot,
    **A** = Talisman / Headbutt, **X** = Item/Crest menu,
  - tap jump is an ~11 px / 14-frame hop (initial `$1022 = 0x059C` = 5.61
    px/frame, gravity `$1026 = 0x0038` = 0.21875 px/frame², with `$1022` zeroed
    immediately on B release at `$80:E227`),
  - holding B gives the full jump arc; pressing B again in air toggles
    horizontal Hover (`State 0x06`, `$1022 = 0`, gravity bypassed).
- Root causes of two major blockers identified from the ROM:
  - `stage_600` is ROM **Area 1** (`screens 8, 9` = `slots 9, 10`), not Area 2
    (`screens 33, 34`); passing `--area 1 --screens 9,10` yields `1957/2048`
    exact u16 tilemap matches between the ROM block table and VRAM (no tile
    remap exists) and aligns the collision floor (`by=10`, `y=160..175`) and
    stone pillar (`cx=24`, `x=384..399`) with the visuals.
  - Pressing `A` in `save0` captured `State 0x16` (the 31-frame back-turned
    wing-spread Talisman/Spell invocation pose), not the Fireball shot; pressing
    `Y` (`$105C`) triggers `State 0x12` (Fireball spit) and spawns the
    projectile via `$80:F1FE` directly in `save0` (no `save2` needed).

Not done yet:

- Porting the exact Bank `$80` player state machine (`$80:DB00..F310`) into
  `src/player.c` to replace the current float approximations.
- Capturing the `Y` Fireball shot pose and projectile from `save0` (separating
  projectile OAM sprites from Firebrand's palette-7 body group).
- Enemies/object behaviour (`$1080..$1D50` loop + `$85:C180` heart pickup).
- HUD (`BG3` status bar wired to `$1062` HP / `$1E50` max HP).
- Audio.
- Full-room camera scrolling and slope/ledge attribute decoding.
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
  --area 1 \
  --screens 9,10 \
  --out assets/generated
```

> **Important area index note:** The `stage_600` dump is ROM **Area 1**
> (`screens 8, 9` = bottom-row `slots 9, 10`), not `--area 2` (`screens 33, 34`).
> Earlier runs passed `--area 2` because WRAM `$008D == 2` during active
> gameplay, whereas the ROM table at `$81:A291` is 0-indexed (`0` = Somulo
> colosseum intro, `1` = Graveyard forest `stage_600`, `2` = Graveyard section
> 2, `3` = Graveyard ruins `save0`). Using `--area 1 --screens 9,10` aligns the
> collision grid and ROM block tables with `stage_600` (`1957/2048` exact u16
> tilemap matches).

Defaults that matter:

- `--player-oam 1-11`
- `--player-palette 2`
- `--item-oam 0`
- `--item-palette 0`
- `--area 1 --screens 9,10` (note: `extract.py` default `--area` should be
  updated from `2` to `1` so omitting `--screens` selects `grid[1][:2] = [9, 10]`)

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

Recovered original constants and routines (from Bank `$80` player code
`$80:DB00..F310`):

| Value | Original code (`$80`) | Meaning |
|---|---|---|
| `$1020 = 0x00B3` | 0.6992 px/frame (`$80:F0BE`) | initial walk speed on first step frame |
| `$1020 = 0x0166` | 1.3984 px/frame (`$80:F0D0`) | max walk speed cap (accelerated via `$80:8B85`) |
| `$1022 = 0x059C` | 5.6094 px/frame (`$80:E170`) | normal jump initial upward velocity (`0x02CE` underwater) |
| `$1026 = 0x0038` | 0.21875 px/frame² (`$80:E070`, `$80:ED11`, `$80:F306`) | normal Firebrand gravity (`0x002A` = 0.1641 underwater) |
| `0xFA00` | -6.0 px/frame (`$80:8D03`) | terminal fall-speed clamp (`0xFD00` = -3.0 underwater) |
| `$1022 = 0x0000` on B release | `$80:E227` (`STA $22; STA $23`) | variable jump cut: releasing B (`$1059 == 0`) while rising (`$1023 >= 0`) zeroes Y velocity immediately |
| `$1022 = 0x0000` in Hover | `$80:E1A3`, `$80:E29F` (`State 0x06`) | pressing B (`$105A`) in air zeroes Y velocity and bypasses gravity (`$80:F0F2` not called); pressing B again returns to fall (`State 0x04`) |

Key Bank `$80` routines:

- **Pad mapper (`$80:DB00..DB40`)**: reads held pad `$0090` and newly pressed
  pad `$0094` against configurable masks in WRAM:
  - `$1E3A` (default `0x8000` = **B**) → `$1059` (held), `$105A` (pressed): Jump / Hover toggle.
  - `$1E3C` (default `0x4000` = **Y**) → `$105B` (held), `$105C` (pressed): Fireball shot.
  - `$1E3E` (default `0x0080` = **A**) → `$105D` (held), `$105E` (pressed): Talisman / Headbutt.
  - `$1E4C` (default `0x0040` = **X**) → Item / Crest subscreen (`$80:DC12`).
- **State dispatcher (`$80:DB80`)**: `LDX $05; JSR ($E002,X)` where `$1005` is:
  - `0x00` (`$E02A`): Init
  - `0x02` (`$E0EE`): Grounded (idle / walk / land)
  - `0x04` (`$E1DA`): Jump / Fall (substates `0x00`/`0x02` takeoff while `$100F > 0`, `0x04` rising/falling flight)
  - `0x06` (`$E29F`): Hover (horizontal flight, `$1022 = 0`, no gravity)
  - `0x08` (`$E389`): Wall cling
  - `0x12` (`$E4F4`): Ground fireball attack
  - `0x16` (`$EA4A`): Talisman / Spell invocation (animation `0x38`, back-turned wing spread)
  - `0x1A` (`$EC20`): Headbutt
  - `0x1C` (`$EDF1`): Hurt / knockback
  - `0x1E` (`$EEB9`): Death
- **Vertical integrator (`$80:8D03`, called from `$80:F0F2`)**:
  `position += velocity; velocity -= gravity;` then clamp fall speed at `0xFA00`.
- **Horizontal integrator (`$80:8D35`)**:
  `position += velocity`.
- **Player projectile spawner (`$80:F1FE`)**:
  scans the first 7 object slots (`$1080..$1260`, stride `$50`, limit `$12B0`)
  for a free slot (`$0000 == 0`), calls `$82:8D84` to clear it, and initializes
  the fireball object.

> **Correction on `$85:C1D4`:** Earlier notes cited `$85:C1D4` (`$22 = 0x0400`,
> `$26 = 0x0040`, `$39 = 0x0400`) as the player jump initializer. Full
> disassembly of `$85:C180..C250` proved that routine is actually the **dropped
> heart item bounce and pickup handler** (bounces with decreasing `$39` by
> `0x0100` each floor hit, checks player touch via `$80:9C93`, plays SFX `$58`,
> and increments player HP `$1062` up to max HP `$1E50`). Do not use `$85:C1D4`
> constants for Firebrand.

### Controls

| Action | Keyboard | Gamepad | Original SNES (`$80:DB00`) |
|---|---|---|---|
| Move | Arrows / WASD | D-pad | D-pad (`$0091` bits 0–3) |
| Jump / hover / fly | Z or Space | B | **B** (`$1E3A` → `$1059`/`$105A`) |
| Fly upward (temporary) | hold jump + Up | hold B + Up | Crest-dependent (not base Gargoyle) |
| Attack (Fireball) | X or J | A / Y | **Y** (`$1E3C` → `$105B`/`$105C`) |
| Talisman / Headbutt | (planned) | (planned) | **A** (`$1E3E` → `$105D`/`$105E`) |
| Collision overlay | F1 | — | — |
| FPS | F3 | — | — |

The original SNES default mapping was verified from `$80:DB00..DB40` and `save0`:
**B = Jump/Hover, Y = Fireball shot, A = Talisman/Headbutt, X = Subscreen**.

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
| `DC_DEBUG_COLLISION=1` | enable the `F1` collision overlay in headless captures |
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

Stage 1 area table (`$81:A291` / `$81:A6D8`, 0-indexed):

- **Area 0**: Set `0`, grid 3×1 (`screens [1, 2, 3]`) — Intro Somulo colosseum.
- **Area 1**: Set `0`, grid 8×2, 16 screen slots (top row `screens [4, 5, 6, 7,
  12, 13, 14, 15]`, bottom row `screens [8, 9, 10, 11, ...]`) — **Graveyard
  forest starting room (`stage_600` is bottom-row slots `9, 10` = `screens 8,
  9`, starting at scroll `x=207, y=0`)**.
- **Area 2**: Set `0`, grid 8×2, 21 screen slots (`screens [33..43, 55..64]`) —
  Graveyard catacombs / town approach.
- **Area 3**: Set `0`, grid 4×5, 12 screen slots (`screens [51..54, 48..50,
  47, 46, 44, 45, 67]`) — **Graveyard vertical ruins (`save0` is at `screens
  52, 53`, scroll `x=448, y=48`)**.

> **Note:** WRAM `$008D` holds `2` during active Stage 1 gameplay across
> sub-areas, which originally caused `stage_600` to be extracted with
> `--area 2` instead of `--area 1`. Always use `--area 1 --screens 9,10` for
> `stage_600`.

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

- Built from `--area 1 --screens 9,10` (`screens 8, 9`), which places the
  solid/sloped floor at `by=10` (`y=160..175`, with attributes `7, 8, 9, 0x0a,
  0x16, 1, 0x16, 0x0b..0x0e`) directly beneath Firebrand's feet (`y=160` flat
  top / `y=164` on slope `0x09`) and places the stone pillar at `cx=24`
  (`x=384..399`, `by=8..9`, `y=128..159`) directly beneath the urn at
  `(385, 111)`.
- 32×16 block grid (512×256 px) for two screens, or 128×32 block grid
  (2048×512 px) for the full 8×2 Area 1 map.
- Attribute byte from `$98:8000 + (set<<12) + block`.
- Current native rule (`Level_IsSolidAt`): any block whose low 5 shape bits are
  non-zero (`(attr & 0x1F) != 0`) is solid; high 3 bits (`0xE0`) are
  environment flags (so `0xA0` is correctly treated as air).
- The exact per-pixel slope height formulas (`0x02..0x1b` at `$80:962B`) are
  scheduled for Step 5 so Firebrand walks smoothly up the graveyard hill and
  stands at `y=164` on `attr=0x09` instead of snapping to `y=160`.

### Block-table vs. VRAM tilemap (resolved)

The native BG1/BG2 tilemaps currently come from the **VRAM ring buffer** of the
captured frame (`stage_600`). Earlier attempts to render the ROM block-table
tilemap produced "fences/garbage" **only because `--area 2` (`screens 33, 34`)
was passed instead of `--area 1` (`screens 8, 9`)**.

Verification against `--area 1`:

- `rom.screen_tilemap(0, 8)` and `rom.screen_tilemap(0, 9)` match
  `stage_600`'s 64×32 VRAM BG1 tilemap on **1957 / 2048 words identically**
  (100% of the loaded viewport; the remaining 91 words are the 3 rightmost
  columns of screen 9 that the VRAM ring buffer had not streamed in yet at
  `hscroll = 207`).
- **There is no tile remap.** The ROM block table (`$95 + set_id`) stores direct
  VRAM tile indices, palette bits and flip flags for BG1.
- Therefore, unlocking the full 8×2 Area 1 room (2048×512 px) for BG1 does
  **not** require reverse-engineering a tile remap or stitching multiple
  captures: `extract.py` can emit the full multi-screen BG1 tilemap and
  collision grid directly from the ROM block tables (`$81:A291`, `$8F`, `$92`,
  `$95`, `$98`) as long as all BG1 tiles referenced by those screens are
  present in the VRAM atlas (or decompressed from `$98:D000` via `$80:C2AF`).
- Note on BG2: in `stage_600`, BG2 is a looping 64×32 forest backdrop scrolled
  with parallax (and HDMA channel 1 on `$210F` `BG2HOFS`), so BG2 should wrap
  horizontally (`tx & 63`) as the camera scrolls across the wider BG1 room.

### Player animation extraction

The animation capture uses `save0` (normal graveyard gameplay in ROM Area 3,
`screens 52, 53`), not the Area-1 landing route. The debug server is driven
while the guest is paused, frame targets are absolute, and every frame is read
back from the 6000-frame state ring. This avoids the timing problems of a
scripted `dump` during a button hold.

Observed clip data in `save0`:

| Clip | Poses | Duration (frames) | Notes |
|---|---|---|---|
| `idle` | 1 | loop | OAM group is visually static |
| `walk` | 6 | 7,7,9,7,7,9 | 46-frame cycle; first pose after start is an acceleration pose (`$1020 = 0x00B3`) and is excluded |
| `jump` | 3 | 1,2,hold | takeoff, launch, rise; last pose holds until hover/fall |
| `hover` | 1 | loop | 18-piece wing-spread pose; also used for `fly` |
| `fall` | 2 | 3,2 (hold) | descent poses |
| `land` | 2 | 3,5 | 8 frames, matching `land_timer` |
| `attack` (currently `A` = `State 0x16`) | 3 | 11,4,16 | 31 frames; `A` in `save0` triggered `State 0x16` (back-turned wing-spread Talisman/Spell pose), not `Y` (`State 0x12` fireball shot) |

Frame facts:

- The player is the palette-7 OAM group; sprites with screen Y ≥ 224 are
  offscreen and excluded.
- `save0`'s player palette is CGRAM OBJ palette 7. The `stage_600` dump needed
  palette 2 because CGRAM had been restored before dumping; do not reuse the
  `stage_600` palette override for animation captures.
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

Why no fireball appeared in the `attack` capture (and how to capture it in `save0`):

- `DEFAULT_SEGMENTS` in `capture_anim.py` pressed **`a`** (`$1E3E` → `$105E`),
  which triggers `State 0x16` (the 31-frame back-turned wing-spread Talisman /
  Spell invocation pose seen in `player_attack_1..2.png`), **not** the fireball
  shot.
- The Fireball shot button in *Demon's Crest* is **`y`** (`$1E3C` → `$105C`,
  `State 0x12` on ground at `$80:E4F4`, spawning a projectile in `$1080..$1260`
  via `$80:F1FE`).
- `save0` can fire projectiles directly by driving **`y`** in `capture_anim.py`
  (no `save2` is needed; `save2` was already deleted).
- **Extraction trap to avoid:** when `y` spawns a fireball in `save0`, if the
  projectile OAM sprites also use palette 7, `extract_anim.py`'s `s.palette == 7`
  filter will include the moving fireball inside Firebrand's bounding box!
  `extract_anim.py` must separate projectile OAM sprites from Firebrand's body
  sprites (by X-distance clustering from `$1030` or by correlating against the
  spawned object's screen coordinates `$109C`/`$109E` in `$1080..$1260`).

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

- Object table base `$1080`, stride `$50`, end `$1D50` (26 slots total).
  - First 7 slots (`$1080..$1260`, up to `$12B0`) are used by `$80:F1FE` for
    player projectiles/effects.
- Loop at `$82:8611` (mirror `$02:8611`):
  - skip if `$0000` is zero,
  - `TCD` to the object base,
  - species `$02` selects a handler via `jsr ($8000,X)` table,
  - bits in `$0000` choose main/secondary update paths (`$8000`, `$8180`,
    `$8300`, `$8480`),
  - copies `$31`/`$34` to screen positions `$1C`/`$1E`.
- Dropped heart pickup handler at `$85:C180..C250` (previously misidentified as
  player jump):
  - `$85:C1C1`: despawns (`STZ $00`) if `$34 >= 0x00DB`; sets initial pop-up
    `$22 = 0x0400`, bounce counter `$39 = 0x0400`, gravity `$26 = 0x0040`.
  - `$85:C1E3`: integrates vertical motion via `$80:8D03`; on floor hit (`$04`),
    decrements `$39` by `0x0100` each bounce until stationary.
  - `$85:C227`: checks player pickup collision via `$80:9C93`, plays SFX `$58`
    via `$80:BD57`, despawns (`STZ $00`), and increments player HP `$1062` up to
    max HP `$1E50`.
- Player task is at `$1000`, separate from the object table, updated from
  `$80:DB00..F310`.
- Player object fields (`$1000` direct page) identified:
  - `$04` collision/ground flags (`bit 2` = on ground, `bits 0..1` = wall),
  - `$05` main state index (even offset into `$80:E002` jump table),
  - `$06` substate index,
  - `$09` facing direction (`1` = right, `2` = left),
  - `$0E`/`$0F` animation step / frame timer,
  - `$1C`/`$1E` screen X / screen Y,
  - `$20` walk speed (`0x00B3` initial step, `0x0166` max),
  - `$22` Y velocity (`0x059C` normal jump init, `0x02CE` underwater jump init,
    `0x0000` on B-release cut or hover),
  - `$26` gravity (`0x0038` normal, `0x002A` underwater),
  - `$30..$32` 24-bit X position (8.8 fixed-point; `$31..$32` integer X),
  - `$33..$35` 24-bit Y position (8.8 fixed-point; `$34..$35` integer Y),
  - `$45` underwater flag,
  - `$59`/`$5A` Jump button (B) held / newly pressed (`$1E3A`),
  - `$5B`/`$5C` Shot button (Y) held / newly pressed (`$1E3C`),
  - `$5D`/`$5E` Special/Headbutt button (A) held / newly pressed (`$1E3E`),
  - `$62` current player HP (max HP at `$1E50`).
- Vertical integrator `80:8D03`:
  `$33 += $22; $22 -= $26;` clamp `$22` at `0xFA00` (-6.0).
- Horizontal integrator `80:8D35`:
  `$30 += $20`.
- Player jump initializer `80:E160`:
  `$04 = 0; $22 = 0x059C` (`0x02CE` if `$45 != 0`); `$05 = 0x04; $06 = 0`.
- Player variable-jump cut `80:E227`:
  if `$23 >= 0` (rising) and `$59 == 0` (B not held), `$22 = 0`.

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
  This matches the native `1.398` (`0x0166` cap after `0x00B3` initial step).
- Default button bindings (`$80:DB00` via `$1E3A..$1E4C`):
  - **B** = Jump / Hover toggle (`$1059`/`$105A`),
  - **Y** = Fireball shot (`$105B`/`$105C`),
  - **A** = Talisman / Headbutt (`$105D`/`$105E`),
  - **X** = Item/Crest subscreen.
- B tap: feet `191 → 180 → 187 → 191` over ~14 frames, an ~11 px hop (matches
  initial velocity `$1022 = 0x059C` with `$1026 = 0x0038` gravity and immediate
  `vy = 0` cut on B release at `$80:E227`).
- Holding B gives the full jump arc; pressing B again in air toggles horizontal
  hover (`State 0x06`, `$1022 = 0`).
- The native spawn `(332.5, 164.0)` matches the reference `stage_600` idle
  player position.

### Area-2 route caveat

The boot → graveyard landing route (`stage_600`) reaches a landing/cinematic in
ROM Area 1 (`screens 8, 9`). Scripted movement there is either ignored,
triggers camera pans, or triggers room transitions. Do **not** use that route
for clean physics measurements. Use `save0` (ROM Area 3, `screens 52, 53`) or
another normal gameplay save.

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
   stale/duplicate content. Scrolling past it reveals wrong tiles unless the
   full room is built from the ROM block tables (`--area 1`).

4. **ROM block-table tilemaps DO match VRAM when the right Area ID is used.**
   Earlier attempts rendered "fences/garbage" only because `--area 2` (`screens
   33, 34`) was passed for `stage_600`, which is actually ROM **Area 1**
   (`screens 8, 9` = `slots 9, 10`). With `--area 1 --screens 9,10`, the ROM
   block table matches `stage_600`'s VRAM tilemap on `1957/2048` words (100% of
   the loaded viewport; there is **no** tile remap).

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

9. **The `stage_600` landing is a cinematic.**
   Inputs may be ignored or cause transitions. `save0` is the clean state.

10. **Script button masks and SNES action bindings.**
    The raw `$0090/$0091` layout is the hardware automatic-read word: Right is
    `$0091:01`, Y is `$0091:40`, B is `$0091:80`, X is `$0090:40`, A is
    `$0090:80`. In gameplay (`$80:DB00`), **B = Jump/Hover**, **Y = Fireball
    shot**, **A = Talisman/Headbutt**, **X = Subscreen**.

11. **`--resume-state` consumes the file.**
    Copy `save0.sav` before each run.

12. **The camera is currently locked.**
    This is deliberate while `room.json` holds only one 64×32 VRAM viewport.
    Unlock it after generating the full multi-screen BG1 tilemap from the Area 1
    ROM block tables.

13. **Collision attributes are approximated.**
    Every non-zero block attribute is currently treated as a flat 16×16 solid.
    Slope/ledge heights (`0x03, 0x04, 0x07..0x0e, 0x16, 0x21, 0xa0`, handled at
    `$80:91B7` / `$80:94D3`) still need decoding.

14. **The HUD is absent.**
    BG3/status bar extraction is a later step.

15. **No audio yet.**
    The game is silent. Do not mistake this for a bug.

16. **Animation palette is not the `stage_600` palette.**
    `save0` renders Firebrand with CGRAM OBJ palette 7; the `stage_600`
    dump used palette 2 because CGRAM had been restored before the dump. The
    animation extractor defaults to palette 7 and verifies against the
    reference framebuffer.

17. **The debug server's `set_controller` does not accept bare decimal masks.**
    Use button names (`right`, `b+up`, `y`, `a`) or an explicit `0x` hex mask.
    `set_controller 128` is parsed as an unknown button and the command fails;
    capture tools must check the JSON error field instead of ignoring it.

18. **WRAM `$008D` is not the ROM `$81:A291` area index.**
    `$008D == 2` indicates active gameplay mode, whereas `$81:A291` is 0-indexed
    by sub-area (`0` = Somulo colosseum, `1` = Graveyard forest `stage_600`,
    `2` = Graveyard catacombs, `3` = Graveyard ruins `save0`).

19. **`$85:C1D4` is a bouncing heart pickup, not Firebrand's jump.**
    Firebrand's jump is at `$80:E160..E239` (`$1022 = 0x059C`, gravity
    `$1026 = 0x0038`, B-release zeroes `$1022` at `$80:E227`). `$85:C180..C250`
    bounces dropped hearts (`$22 = 0x0400`, `$26 = 0x0040`) and heals `$1062`.

20. **Projectile OAM sprites must be separated from Firebrand's palette-7 bbox.**
    When capturing `Y` (Fireball shot) in `save0`, if the fireball shares OAM
    palette 7, a naive `s.palette == 7` filter will merge the moving projectile
    into Firebrand's body PNG. Filter or cluster by distance from `$1030` or
    object slot `$1080..$1260` screen coordinates (`$1C`/`$1E`).

---

## 9. Current limitations

- Single captured graveyard viewport; no room scrolling yet.
- `assets/generated/room.json` was generated with `--area 2` instead of
  `--area 1 --screens 9,10`, causing the native player to fall 11 px through
  row `by=10` (`y=160..175`) onto `by=11` (`y=176`). Re-extracting with
  `--area 1 --screens 9,10` puts the solid/sloped floor at `by=10` (`y=160`,
  4 px above the `y=164` slope surface) and aligns the pillar at `cx=24`.
- Animation covers idle/walk/jump/hover/fall/land plus the `A`-button `State
  0x16` (Talisman/Spell) clip currently bound to `attack`. The real `Y`-button
  Fireball spit (`State 0x12`), hurt, death and wall-cling clips are still
  needed; one directional set is mirrored by the renderer.
- No fireball projectile spawned yet (`$80:F1FE` 7-slot pool).
- `src/player.c` still uses float approximations (`vy = -2.0` + `0.10` boost,
  `gravity = 0.25`, `B+Up` fly) instead of the recovered Bank `$80` state
  machine (`$1022 = 0x059C` with B-release cut, `$1026 = 0x0038`, `State 0x06`
  zero-G hover toggle).
- No enemies.
- No HUD.
- No audio.
- Slope/ledge block attributes (`0x03..0x21, 0xa0`) are treated as flat solids.
- No title screen, menus, save/password, stage select, map.
- No full campaign or bosses.
- No Windows build tested in this session (CMake/raylib are portable, but the
  build was verified on Linux).
- No public-release asset path yet.

---

## 10. Ordered next steps

The milestones below are ordered by technical dependency so each step unblocks
and validates the next without rework.

### Step 1 — Extract Firebrand base animation frames (done)

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

### Step 2 — Fix Graveyard room extraction (`--area 1 --screens 9,10`) (done)

Done:

1. Changed the default `--area` in `tools/dc_extract/extract.py` from `2` to `1`
   (where `grid[1][:2]` automatically selects `slots 9, 10` = `screens 8, 9`).
2. Re-ran `extract.py` on `captures/graveyard` (`stage_600`) to regenerate
   `assets/generated/room.json` and `manifest.json`.
3. Updated `Level_IsSolidAt` (`src/level.c`) and the debug collision overlay
   (`src/render.c`) to mask the low 5 shape bits (`attr & 0x1F`) so `0xA0`
   environment blocks are not treated as mid-air solids.
4. Fixed `Player_Init` and `Player_Update` (`src/player.c`) so Firebrand aligns
   to the block top (`y=160`) on init and only triggers `land_timer = 8` when
   transitioning from airborne to grounded (`!was_on_ground`).
5. Verified with `DC_DEBUG_COLLISION=1` (`physics_check.png`) that row `by=10`
   (`y=160..175`) sits under Firebrand's spawn `(332, 160)` and `cx=24`
   (`x=384..399`, `y=128..159`) boxes the stone pillar under the urn `(385, 111)`.

### Step 3 — Port the authentic Bank `$80` player state machine & physics (next)

Why before projectiles/scrolling: the exact constants and transitions are now
known from `$80:DB00..F310`; replacing the temporary float heuristics in
`src/player.c` now locks down movement feel and input handling (`B` = Jump /
Hover toggle, `Y` = Fireball shot, `A` = Talisman/Headbutt).

Tasks:

1. Use 8.8 fixed-point (or exact `/ 256.0f` equivalents) from Bank `$80`:
   - Walk (`$80:F0A0..F0E5`): first step frame `$1020 = 0x00B3`
     (`0.69921875`), accelerate via `$80:8B85` up to `0x0166` (`1.3984375`).
   - Gravity (`$80:E070`, `$80:ED11`): `$1026 = 0x0038` (`0.21875`), terminal
     fall clamp `0xFA00` (`-6.0`).
   - Jump (`$80:E160..E239`, `State 0x04`): takeoff frames (`$100F`) before
     vertical integration, initial `$1022 = 0x059C` (`5.609375`), and at
     `$80:E227` **if `B` (`jump_held`) is released while rising (`vy < 0`),
     immediately set `vy = 0`**.
   - Hover (`$80:E1A3`, `$80:E29F..E340`, `State 0x06`): pressing `B`
     (`jump_pressed`) while airborne toggles `PLAYER_HOVER` with `vy = 0` and
     gravity bypassed; pressing `B` again exits hover into falling (`State
     0x04`). Keep `B + Up` fly-up behind a debug/crest flag so base Gargoyle
     matches `save0`.
2. Update `input.c` so SNES `Y` (or keyboard `X`/`J`, gamepad `X`/`Y`/`West`)
   triggers Fireball shot (`$105C`), while `A` (`$105E`) is reserved for
   Talisman/Headbutt.
3. Validate against `save0` OAM traces with `tools/oracle/compare.py`.

Acceptance:

- Tap-jump, full-hold jump, walk acceleration, and mid-air hover toggle match
  `save0` within ≤1 px / 1 frame across a 600-frame trace.

### Step 4 — Capture `Y` Fireball shot in `save0` & implement projectiles (`$80:F1FE`)

Tasks:

1. Add a `("shoot", "y", 35)` segment to `tools/oracle/capture_anim.py` and
   capture from `save0.sav` (no `save2` needed).
2. Inspect the spawned projectile slot in WRAM (`$1080..$1260`, stride `$50`,
   allocated by `$80:F1FE`) and its OAM sprites across the segment:
   - Record projectile spawn frame delay, `$1030`/`$1033` offset from player,
     `$1020` horizontal velocity, species ID (`$1002`), and OAM tile/palette.
3. Update `tools/dc_extract/extract_anim.py`:
   - Exclude projectile OAM sprites from Firebrand's palette-7 body composition
     so the moving fireball never stretches Firebrand's clip bbox.
   - Emit the true `Y`-button Fireball spit clip as `attack` (and optionally
     keep the `A`-button `State 0x16` clip as `spell` / `talisman`), plus bake
     `fireball_0..n.png`.
4. Implement the 7-slot player projectile pool in C (`src/projectile.c` or
   `src/object.c`):
   - Spawn on the exact animation frame, move at the recovered 8.8 velocity,
     despawn on wall/pillar collision or screen exit.

Acceptance:

- Pressing Shot plays the Fireball spit pose (`State 0x12`) and spawns a
  fireball at the original frame and offset.
- Up to 7 projectiles can be active (matching `$1080..$1260`), and shots
  despawn cleanly on the `cx=24` stone pillar or screen bounds.

### Step 5 — Full-room Area 1 tilemap, slope/ledge attributes & camera scrolling

Why now: since Trap #4 is resolved (ROM block tables match VRAM directly when
`--area 1` is used), full-room scrolling no longer requires reverse-engineering
a tile remap.

Tasks:

1. Extend `tools/dc_extract/extract.py` to optionally build the full 8×2
   Area 1 BG1 tilemap (2048×512 px) and 128×32 collision grid directly from
   `rom.area_info(1)` (`$81:A291`, `$8F`, `$92`, `$95`, `$98`):
   - Verify whether all BG1 tile indices across Area 1's 16 screens are
     already resident in `stage_600.vram.bin`; for any tiles loaded by
     mid-stage segments (`$80:BFB5` / `$80:C11B`), either decode CHR from ROM
     (`$98:D000` via `$80:C2AF`) or merge VRAM from a walk-through capture.
   - Wrap BG2 horizontally (`tx & 63`) with its parallax scroll ratio (and
     optional HDMA row split from `$210F`).
2. Decode the graveyard block attribute bytes (`0x01` solid, `0x03/0x04` and
   `0x07..0x0e` slope heights, `0x16` slope/ground transition, `0x21` hazard /
   ledge, `0xa0` pass-through / foreground) from `$80:91B7` / `$80:94D3`.
3. Unlock `camera.min_x..max_x` and `camera.min_y..max_y` in `room.json` and
   match the original camera deadzone (`$00C0`/`$00C4` vs `$1030`/`$1033` in
   `save0` traces).

Acceptance:

- Firebrand walks smoothly up and down the graveyard slopes (`by=10..11`),
  stands at `y=164` on `attr=0x09` at spawn, and the camera scrolls across the
  full starting room without stale or missing tiles.

### Step 6 — Object system (`$1080..$1D50`), one graveyard enemy & heart pickup (`$85:C180`)

Tasks:

1. Implement the 26-slot object table (`$1080..$1D50`, stride `$50`) and the
   `$82:8611` update/cull loop + `$82:8BDB` area object list spawner (`$81:C874`
   for Area 1).
2. Port the already-disassembled dropped heart / urn item (`$85:C180..C250`:
   pop-up `$22 = 0x0400`, gravity `$26 = 0x0040`, bounce decay `-0x0100`, pickup
   check `$80:9C93`, heals `$1062` up to `$1E50`).
3. Port one graveyard enemy species from Area 1's object list (rising zombie /
   skeleton), including fireball hit detection, damage, knockback (`State 0x1C`
   at `$80:EDF1`), and item drop.

Acceptance:

- Destroying the urn at `(385, 111)` or killing the graveyard enemy drops a
  bouncing pickup with the exact `$85:C180` arc, and touching it increments
  player HP.

### Step 7 — BG3 HUD status bar

- Extract the BG3 status bar (`bgsc[2] = $5A` → `$5800`, 2bpp character base
  `$5000`) and crest/health icons from `save0` or `stage_600`.
- Render current HP (`$1062`) and max HP (`$1E50`) roses/skulls plus current
  talisman/spell icon at the top of the 256×224 frame.

### Step 8 — Engine hardening & automated regression harness

- **Fixed-timestep accumulator in `main.c`**: decouple the 60 Hz `Game_Update`
  step from display refresh rate (`GetFrameTime()` accumulator capped to avoid
  spiral of death) so 120 Hz / 144 Hz / VSync-off monitors advance gameplay at
  an exact 60 Hz while `DC_RUN_FRAMES` remains deterministic 1-update-per-loop.
- **Pre-baked left/right player frames (or verified flip)**: `render.c` still
  uses negative `src.width` (`DrawTexturePro`) for left-facing player sprites
  despite Trap #6; verify across drivers or pre-bake mirrored textures in
  `Anim_Load`.
- **Single-command regression test (`ctest` / `tools/run_regression.sh`)**:
  automate running `DC_DEMO` traces and `tools/oracle/compare.py` against
  checked-in reference CSV assertions so physics/animation regressions fail CI
  immediately.

### Step 9 — Audio, menus, game flow & full content

- Audio: development loop first, then BRR/DSP sample + sequence extractor or
  replacement soundtrack for legal public release.
- Flow: Boot → Title → New Game / Password → Graveyard vertical slice (3–5
  rooms + boss gate at `$81:97DA`).
- Full content: remaining areas, bosses, elemental Gargoyle crests, Mode 7
  overworld, and original fan-game levels/story/art.

---

## 11. Long-term roadmap

| Phase | Goal | Status |
|---|---|---|
| 0 | Extraction proof: graveyard map + VRAM/CGRAM dump + static render | done (needs `--area 1` re-extract) |
| 1 | Engine skeleton: raylib, fixed 60 Hz, BG layers, input, camera | done |
| 2 | Firebrand core: Bank `$80` state machine, physics, slopes, animations | base clips done; Bank `$80` state port & `Y` shot clip next |
| 3 | Combat and objects: `$80:F1FE` fireball, `$1080` objects, enemy, `$85:C180` pickup, HUD | `A` spell pose done; `Y` fireball/enemies/HUD next |
| 4 | Vertical slice: full Area 1–3 scrolling rooms, transitions, title/menu, audio | not started |
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
| Mid-stage CHR streaming (`$80:BFB5`/`$80:C11B`) | unvisited screens in an area may reference CHR tiles not yet in a single VRAM dump | decode CHR from `$98:D000` via `$80:C2AF` or merge VRAM across a room walkthrough |
| Player state machine not fully ported | feel drifts from `save0` | port Bank `$80` routines (`$80:DB00..F310`) directly; validate with `compare.py` |
| Slope/ledge block attributes (`0x03..0x21, 0xa0`) | player steps on flat 16×16 tops instead of smooth hill heights (`y=160` vs `y=164`) | decode height tables at `$80:91B7` / `$80:94D3` |
| Projectile OAM overlap with palette-7 player group | `extract_anim.py` merges fireball into player sprite bbox if both use palette 7 | filter by distance from `$1030` or correlate with projectile object slots `$1080..$1260` |
| BG2 HDMA parallax scroll splits | flat `scroll_x` on BG2 loses per-scanline sky/tree parallax | read HDMA table on channel 1 (`$210F` `BG2HOFS`) or approximate band scroll |
| Audio extraction | no soundtrack | BRR/DSP decoder or recorded development placeholder |
| Object AI breadth | huge scope | port `$82:8611` loop + `$85:C180` heart pickup + one enemy at a time |
| Legal | distribution risk | no assets in repo; ROM-generated locally at install/setup time |
| GPL randomizer code | license contamination | reimplement formats from ROM disassembly; keep GPL tools separate |
| raylib flip quirks | visual corruption | pre-baked flips for BG; verify or pre-bake mirrored player frames |
| CGRAM restore timing | wrong sprite palettes | sample framebuffer; per-capture palette config |

Resolved questions (formerly open):

- **How the loader remaps block tile indices to VRAM**: Resolved — there is no
  tile remap. `stage_600` is ROM Area 1 (`screens 8, 9` = `slots 9, 10`), not
  Area 2 (`screens 33, 34`); with `--area 1 --screens 9,10`, the ROM block table
  matches `stage_600`'s VRAM BG1 tilemap `1957/2048` words identically.
- **Vertical origin of the collision grid (`y=164` vs `y=176`)**: Resolved —
  `y=176` (`by=11`) came from loading Area 2's collision grid into Area 1. In
  Area 1 (`screens 8, 9`), the ground/slope row is `by=10` (`y=160..175`), where
  `attr=0x09` places Firebrand's feet at `y=164`, and the stone pillar is at
  `cx=24` (`x=384..399`, `y=128..159`).
- **Exact player jump/hover/walk constants and button mapping**: Resolved from
  Bank `$80` (`$80:DB00..F310`): `B` = Jump/Hover (`$1059`/`$105A`), `Y` =
  Fireball shot (`$105B`/`$105C`), `A` = Talisman/Headbutt (`$105D`/`$105E`),
  `X` = Subscreen; walk `0x00B3 → 0x0166`, jump `0x059C` with B-release `vy=0`
  cut at `$80:E227`, gravity `0x0038`, hover `State 0x06` (`vy=0`). (`$85:C1D4`
  was identified as the dropped heart bounce routine.)

Remaining open technical questions:

- Exact slope height-table lookup at `$80:91B7` / `$80:94D3` for attributes
  `0x03, 0x04, 0x07..0x0e, 0x16, 0x21, 0xa0`.
- Full object species ID (`$02`) → handler table (`$82:8000,X`) for Area 1
  graveyard enemies.
- Exact frame delay, spawn offset and velocity of the `Y`-button fireball in
  `$80:E4F4` / `$80:F1FE`.
- Best audio path for distribution (native BRR+sequence synth vs replacement
  tracks).
- Widescreen support policy and Windows CI/test build path.

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
original graveyard room from extracted ROM data and drives Firebrand's extracted
idle, walk, jump, hover, fall, landing and spell/attack poses through a
frame-based animation state machine. Disassembly of Bank `$80` (`$80:DB00..F310`)
and ROM level tables (`$81:A291`) resolved the major open mysteries:
`stage_600` is ROM Area 1 (`screens 8, 9` = `slots 9, 10`), not Area 2, which
eliminates the 11 px spawn offset and proves the ROM block tables match VRAM
directly (`1957/2048` words) without any tile remap; `$85:C1D4` is a bouncing
heart pickup while Firebrand's true physics live in Bank `$80` (walk
`0x00B3 → 0x0166`, jump `0x059C` with immediate `vy=0` cut on B release at
`$80:E227`, gravity `0x0038`, zero-G hover toggle at `State 0x06`); and `Y`
(`$105C`) is the Fireball shot button (`State 0x12`, spawning projectiles in
`$1080..$1260` via `$80:F1FE`) whereas `A` (`$105E`) triggered the Talisman
invocation pose (`State 0x16`). The ordered next milestones are: (1) re-extract
`stage_600` with `--area 1 --screens 9,10`, (2) port the Bank `$80` player state
machine, (3) capture `Y` in `save0` and implement the 7-slot fireball pool,
(4) build the full-room Area 1 tilemap with decoded slope attributes and camera
scrolling, and (5) port the `$1080` object loop, one graveyard enemy, the
`$85:C180` heart pickup, and the BG3 HUD.
