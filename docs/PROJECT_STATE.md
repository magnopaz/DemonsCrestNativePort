# Demon's Crest Fan Game — Project State and Handoff

Last updated: 2026-10-23
Project root: `projetos/opencode/DemonsCrestNativePort`
Reference recompilation: `projetos/opencode/DemonsCrestSNESRecomp`
ROM: `projetos/opencode/roms/Demon's Crest (USA).sfc`

This file is the authoritative handoff. It records the final objective, every
implemented piece, every recovered technical detail, the traps already hit, and
the ordered next steps. If work resumes later, read this file first, then
`docs/ROADMAP.md`, then the full-campaign gap plan in
`docs/FULL_CAMPAIGN_PLAN.md`, then the recompilation project's
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
- Extracted full Area 1 graveyard row (2048×256 px, screens 8/9/10/11 and
  16/17/18/19 from the populated grid row):
  - BG1 tilemap from the ROM block tables,
  - looping BG2 forest backdrop from the capture,
  - per-palette tile atlases,
  - Firebrand and the visible item as composed OAM sprites,
  - 128×16 collision grid from the ROM block attributes,
  - room JSON with spawn/camera/world bounds.
- Firebrand runs the authentic Bank `$80` grounded / jump / hover state machine
  in 8.8 fixed point, walks up/down the decoded slope surface table, and is
  constrained to the full-room bounds.
- Camera follows Firebrand across the full room with deadzones and clamps to
  the level bounds; BG2 wraps horizontally as the camera scrolls.
- Firebrand animation extracted from `save0`:
  - static idle,
  - 6-pose, 46-frame walk cycle,
  - jump rise,
  - hover / fly pose,
  - fall and landing,
  - Y Fireball spit (17 frames) and A Talisman/Spell (31 frames).
- Shot (`Y`) and special (`A`) timers plus landing timers in the player state
  machine.
- 7-slot fireball projectile pool (`src/projectile.c`): original spawn
  offset/velocity, palette-6 OAM animation, pillar and screen-edge despawn.
- 26-slot object pool (`src/object.c`) driven by the Area 1 object list:
  - destructible urns (species `$57`) that shatter on fireball hits,
  - bouncing heart pickups with the `$85:C180` arc and HP heal,
  - first-pass species `$67` ghouls with emerge, chase, contact damage,
    knockback and heart drop.
- BG3 HUD from `save0`: six-point health bar, three crest slots and an active
  spell-slot highlight at the top of the frame.
- Original procedural audio: looping chiptune theme plus jump/land/shoot/break/
  pickup/hurt/enemy-hit SFX; no ROM audio data is used.
- Vertical-slice flow: title screen, pause overlay, Area Clear trigger at the
  object-list exit (`species $09`, x=1920), and restart from title/clear.
- Full-campaign foundation (this session):
  - ROM CHR decompressor (`$80:C2AF`/`$80:C300`) and `$98:C000`/`$98:D000`
    table decoders in `tools/dc_extract/extract.py`.
  - All 113 valid areas from `$81:A291`/`$81:A6D8` extracted into
    `assets/generated/room_area<N>.json` with 2D multi-row/vertical grids,
    full collision grids, spawn records (`$81:A1A9`), transition boxes
    (`$81:981A`) and object lists (`$81:C874`).
  - Per-area BG CHR atlases baked from ROM CHR + ROM palettes
    (`bg_area<N>_pal<P>.png`), loaded on demand per room.
  - OAM metasprite table (`$8B:8000`, decoder from `$80:D9A7`) and real item
    pickup (Crest emblem) / ghoul sprites baked from ROM CHR; the procedural
    heart/enemy placeholders are gone.
  - BG2 per-scanline HDMA parallax bands recovered from `$84:AF65`
    (`BG2HOFS`, channel 1) and rendered as scanline clip bands.
  - Multi-room runtime: `Level_LoadArea`/`Game_TransitionToArea`, bounding-box
    transitions, boss-gated `$81:97DA` exits, and a 113-area room loader.
  - Campaign Area 0 Somulo boss (with HP bar), Area 2 skeleton patrols,
    Area 3/5 bats and wall gargoyles, moving platforms, crest/life-stone
    pickups, gold drops, hazard damage (`attr & 0xE0 == 0x20`), breakable
    terrain (`0x40`), water (`0x60`), and the `$1070` slope acceleration ramp.
  - Six Gargoyle forms (`CREST_BASE`, `EARTH`, `AIR`, `WATER`, `TIME`,
    `ULTIMATE`) with form tints, Earth/Ultimate shoulder tackle, Air free
    flight, Water swimming, Time/Ultimate damage reduction, and a
    `form_next` cycle key.
  - Crest/Item subscreen (`X`/Tab) with form, Fire-Crest element, Talisman,
    Urns, Spells and gold; shop; generated password screen; options volume;
    game over; ending; and a Mode 7-style overworld flight map with stage
    nodes, landing and shop access.
- Per-area BG2 backdrops from oracle frames (this session):
  - `tools/dc_extract/bg2_captures.json` maps an area to a `capture_anim.py`
    frame (`{"areas": {"3": {"dir": "captures/anim", "frame": 20614}}}`);
  - `extract.py` copies that frame's BG2 tilemap into the room document, keeps
    the original BG2-vs-BG1 scroll offset, and bakes the area atlases from the
    capture's VRAM/CGRAM (covers streamed tiles);
  - Areas 1 (`stage_600`) and 3 (`save0`) now render their real backdrops; the
    remaining areas keep a transparent BG2 until a frame is added;
  - the BG2 tilemap is generated by the guest at runtime (`$84:A057` DMAs it
    from WRAM `$7F`, tables `$84:C311/$C312` indexed by `$00F9`; static
    fallback from `$81:8068`), so no raw ROM tilemap table exists to extract.
- Campaign audit tooling (this session):
  - `tools/audit_campaign.py` checks every `room_area<N>.json` for dimension
    mismatches, BG1/collision coverage, BG2 presence, spawn support, object
    bounds and transition/gate targets, and classifies rooms as
    playable/partial/empty;
  - current result: 69 playable, 44 partial, 0 empty, 2 with BG2, 45 rooms with
    at least one structural issue (mostly zero-collision boss/cutscene arenas
    whose floor is not in the block tables);
  - `--strict` fails CI on a structural regression in a playable room.
- Windows build path (this session):
  - CMake gains `-DDC_RAYLIB_ROOT=<dir>` to link a local `libraylib.a` instead
    of fetching raylib 5.5, plus platform link libraries for Win32;
  - `tools/build_windows.ps1` configures/builds against the raylib Windows
    installer (default `C:\raylib\raylib`) and can run the game;
  - verified with raylib 6.2 + MinGW-w64 GCC 16.2; `-j` is avoided by default
    because some Windows shells block the jobserver named pipe used by
    make/ninja;
  - `DC_WINDOW_INFO=1` prints window/render-target size and integer scale.
- Save/options persistence (this session):
  - `Game_SaveProgress`/`Game_LoadProgress` write/read
    `saves/dc_save.json` (override with `DC_SAVE_PATH`) via cJSON: version,
    current area, cleared stages, HP/max HP, Crests and form, Talismans, Fire
    element, Urns, Spells, gold and music volume;
  - autosave on area clear, on entering the overworld and on exit; the title
    menu gains a `CONTINUE` entry only when a save exists (falls back to the
    old four-item menu otherwise, so `DC_DEMO=13` is unchanged);
  - deterministic `DC_RUN_FRAMES` runs never write unless `DC_SAVE_FORCE=1`;
    `DC_NO_SAVE=1` disables the feature and `DC_RESUME=1` boots straight into
    the saved run for headless tests;
  - the regression suite now autosaves in one run and asserts area/max HP/gold
    after resuming in a second run.
- HUD health bar fix (this session): `hud_bar.png` is a brightness mask
  (grey body, pink highlight); the renderer now tints the empty part dark and
  the filled part red instead of drawing the whole bar white.
- Password system (this session): a real 40-bit codec — 10 letters from a
  20-symbol alphabet carrying stages/max HP/crests/form/urns/spells/talismans
  plus an 8-bit FNV-1a checksum — with a letter-entry screen (up/down changes
  the letter, left/right moves, Enter applies, invalid codes are rejected),
  `DC_PASSWORD=<code>` to apply a code at boot and
  `DC_PASSWORD_SELFTEST=1` for a 2000-round-trip CI check. The retail
  encode/decode algorithm is still unknown; this is a stable native format.
- Per-species enemy sprites from injected-object captures (this session):
  `tools/oracle/capture_species.py` writes a minimal object record with `$00`
  bit 0 set into the first object slot, so the guest runs that species' init
  handler and draws it in the current room — no area travel or live-enemy save
  needed. `tools/dc_extract/extract_species.py` finds the object's OAM cluster
  (the world->screen offset is derived from the player's feet anchor because the
  guest restores the BG scroll registers after rendering), composes it with the
  captured CGRAM, deduplicates poses and emits `enemy_<species>_<n>.png` +
  `species_anim.json`. `src/assets.c`/`src/render.c` load and draw those sheets
  per species (with the shared sheet as fallback), replacing the one-size
  enemy sprite for the species whose init is self-contained (urn `$57`,
  pickup `$49`, `$2D`, ghoul `$67`, ...).
- Collision debug overlay (`F1`), FPS (`F3`).
- Per-frame CSV trace output plus `DC_TRACE_PROJECTILES`/`DC_TRACE_OBJECTS`.
- Deterministic demo input modes for screenshots and comparison.
- Oracle tooling:
  - WRAM trace parser,
  - OAM player-sprite parser,
  - native-vs-oracle comparison tool,
  - capture-directory CSV exporter
    (`tools/oracle/export_capture_csv.py`),
  - repeatable probe scripts for the area-2 route and `save0`,
  - debug-server animation capture (`tools/oracle/capture_anim.py`),
  - animation frame baker (`tools/dc_extract/extract_anim.py`),
  - deterministic native regression harness (`tools/run_regression.py`,
    `tools/run_regression.sh`, CTest `regression`).
- Movement constants and state machine recovered from the original Bank `$80`
  player code (`$80:DB00..F310`) and ported to `src/player.c`.
- Movement validated against `save0`:
  - walk speed 1.40 px/frame (`$1020` stays at the `0x0166` cap on flat ground;
    the `0x00B3 → 0x0166` slope ramp is wired behind the `$1070` flag),
  - default SNES controls (`$80:DB00` via `$1E3A..$1E4C`):
    **B** = Jump / Hover toggle, **Y** = Fireball shot,
    **A** = Talisman / Headbutt, **X** = Item/Crest menu,
  - tap jump is an ~11 px / 14-frame hop (initial `$1022 = 0x059C` = 5.61
    px/frame, gravity `$1026 = 0x0038` = 0.21875 px/frame², with `$1022` zeroed
    immediately on B release at `$80:E227`),
  - holding B gives the full jump arc; pressing B again in air toggles
    horizontal Hover (`State 0x06`, `$1022 = 0`, gravity bypassed),
  - 603-frame `save0` replay (`DC_DEMO=6`): max delta `x=0.00 px`,
    `y=0.53 px` (the residual Y is the known idle task oscillation).
- Root causes of two major blockers identified from the ROM:
  - `stage_600` is ROM **Area 1** (`screens 8, 9` = `slots 9, 10`), not Area 2
    (`screens 33, 34`); passing `--area 1 --screens 9,10` yields `1957/2048`
    exact u16 tilemap matches between the ROM block table and VRAM (no tile
    remap exists) and aligns the collision floor (`by=10`, `y=160..175`) and
    stone pillar (`cx=24`, `x=384..399`) with the visuals.
  - Pressing `A` in `save0` captured the 31-frame back-turned wing-spread
    Talisman/Spell invocation pose; pressing `Y` (`$105C`) selects the
    grounded fireball-spit animation (`$1053 = 0x0E`, 17 frames) and spawns a
    projectile in the first free `$1080..$1260` slot via the allocator at
    `$80:F222` (earlier notes referenced `$80:F1FE`). No `save2` is needed.

Not done yet:

- Real species `$67` enemy handler/animations and hurt/death player clips;
  current ghoul and boss art are reused ROM metasprite frames.
- Full talisman inventory/selection sprites and the exact retail HUD wiring
  beyond the HP bar and active slot.
- Per-form player OAM/animations (current forms are palette tints), text and
  cutscenes, the exact retail password algorithm, and retail-format SRAM
  compatibility (the native `saves/dc_save.json` format is in place).
- Remaining bosses and secret areas, full Mode 7 overworld map data, and ROM
  music/SFX extraction or replacement. BG2 backdrops exist for Areas 1 and 3;
  the other areas need one oracle frame each in `bg2_captures.json`.
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
│   ├── FULL_CAMPAIGN_PLAN.md ← full-campaign remaining work
│   └── ROADMAP.md
├── src/
│   ├── main.c
│   ├── game.c / game.h
│   ├── assets.c / assets.h
│   ├── anim.c / anim.h
│   ├── level.c / level.h
│   ├── input.c / input.h
│   ├── player.c / player.h
│   ├── projectile.c / projectile.h
│   ├── object.c / object.h
│   ├── audio.c / audio.h
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
│       ├── export_capture_csv.py
│       ├── segments_save0_physics.json
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
- Windows 11, MinGW-w64 GCC 16.2, CMake 4.5, raylib 6.2 from the raylib Windows
  installer (`C:\raylib\raylib`), using `-DDC_RAYLIB_ROOT` and
  `tools/build_windows.ps1` (verified in this session).
- raylib 5.5 fetched by CMake `FetchContent` on the Linux path.
- Python 3 with Pillow for extraction.
- Git repository initialized; see section 15 for commits.
- The recomp trace build is at
  `DemonsCrestSNESRecomp/build-trace/DemonsCrestSNESRecomp`
  (Windows: `build-trace\DemonsCrestSNESRecomp.exe` with `SDL2.dll` beside it).

---

## 4. Build, run, capture, extract

### Build and run

```sh
cd projetos/opencode/DemonsCrestNativePort
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/demonscrest
```

Windows (offline, local raylib):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_windows.ps1
# or directly:
cmake -S . -B build -G "MinGW Makefiles" -DDC_RAYLIB_ROOT=C:\raylib\raylib
cmake --build build
build\demonscrest.exe --assets assets\generated
```

`--assets <dir>` selects a different generated-asset directory.
`DC_WINDOW_INFO=1` prints the window/render-target size and integer scale.

> On a DSH-sandboxed Windows shell the *game* process can only create files
> inside the session workspace, so `DC_TRACE_CSV`/`DC_SCREENSHOT` paths must
> point there. Redirect `TEMP`/`TMP` into the workspace before running
> `tools/run_regression.py`, which writes traces to a temp directory.

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
  --full-room \
  --out assets/generated
```

`--full-room` emits every screen in the captured grid row (2048×256 px) using
the ROM block tables; without it the extractor keeps the older two-screen
512×256 viewport slice.

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
assets/generated/room_area0.json ... room_area112.json
assets/generated/bg_area0_pal*.png ... bg_area112_pal*.png
assets/generated/bg_pal0.png ... bg_pal7.png
assets/generated/obj_pal0.png ... obj_pal7.png
assets/generated/player_idle.png
assets/generated/item_00.png
assets/generated/campaign.json
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
air, hold jump, hover release, idle, A special, and Y shot. The extractor emits
`player_<clip>_<n>.png` plus `player_anim.json` with per-step anchor and
duration values. The clips are `idle`, `walk`, `jump`, `hover`, `fall`,
`land`, `attack` (Y shot) and `spell` (A). `fly` reuses the hover clip until a
separate fly capture exists. The same pass bakes `fireball_0..n.png` and
`fireball_anim.json` from the palette-6 projectile OAM group.

### Validate Step 3 movement against `save0`

```sh
python3 tools/oracle/capture_anim.py \
  --recomp ../DemonsCrestSNESRecomp \
  --rom "../roms/Demon's Crest (USA).sfc" \
  --save ../saves/save0.sav \
  --segments tools/oracle/segments_save0_physics.json \
  --out captures/phys

python3 tools/oracle/export_capture_csv.py captures/phys -o /tmp/oracle_phys.csv

DC_DEMO=6 DC_RUN_FRAMES=603 DC_TRACE_CSV=/tmp/native_demo6.csv \
  ./build/demonscrest   # add xvfb-run -a when headless

python3 tools/oracle/compare.py /tmp/native_demo6.csv /tmp/oracle_phys.csv \
  --oracle-start 0 --threshold 1.0
```

Expected result: `x=0.00 px`, `y=0.53 px` max delta over 603 frames. The
remaining Y offset is the `$1000` task's idle oscillation (trap #8), not a
physics difference.

---

## 5. Native engine

### Render path

- `RenderTexture2D` at 256×224.
- `TEXTURE_FILTER_POINT`.
- Integer scale in `main.c`, centered with letterboxing.
- `SetTargetFPS(0)` plus VSync; a `GetFrameTime()` accumulator drives
  `Game_Update` at an exact 60 Hz, capped to 0.25 s and 8 catch-up steps to
  avoid a spiral. `DC_RUN_FRAMES` deliberately stays one update per loop so
  traces/screenshots are deterministic.
- Screen is drawn with a vertically flipped source rectangle, as required by
  raylib render textures.

### Modules

- `main.c` — window, asset directory argument, fixed 60 Hz accumulator,
  integer scaling, screenshot/run-frame hooks.
- `game.c` — asset/level/player/camera ownership, title/playing/paused/clear
  state machine, target creation, update, render, trace CSV.
- `assets.c` — loads the eight BG and OBJ palette atlases, player and item
  textures, `player_anim.json`, `fireball_anim.json`; sets point filtering.
- `anim.c` — parses `player_anim.json` and `fireball_anim.json`, loads
  per-clip PNGs plus pre-baked mirrored textures, and exposes clip/step data
  with frame-based durations.
- `level.c` — parses `room.json` with cJSON, stores BG layers, collision,
  camera/play bounds, spawn, player feet offsets and static sprites.
- `input.c` — keyboard/gamepad mapping, edge detection, deterministic `DC_DEMO`
  modes.
- `player.c` — Bank `$80` 8.8 fixed-point grounded/jump/hover state machine,
  collision, bounds, shot/special and landing timers, animation clip/step
  timing.
- `projectile.c` — 7-slot player fireball pool, spawn offset/velocity,
  terrain and viewport despawn.
- `object.c` — 26-slot level object pool: urns, pickups, first-pass enemies,
  `$85:C180` bounce/pickup and fireball hit handling.
- `audio.c` — procedural chiptune music and SFX; silently disabled when the
  audio device is unavailable or in deterministic `DC_RUN_FRAMES` runs.
- `camera.c` — deadzone follow, clamped to level-provided camera bounds.
- `render.c` — BG tile drawing, static sprites, animated player, fireball
  stream, object sprites, BG3 HUD, debug collision overlay.

### Room format (`assets/generated/room.json`)

```json
{
  "name": "area1_full_room",
  "world": { "width": 2048, "height": 256 },
  "camera": {
    "x": 207, "y": 0,
    "min_x": 0, "max_x": 1792,
    "min_y": 0, "max_y": 32
  },
  "play": {
    "min_x": 8, "max_x": 2040,
    "min_y": 22, "max_y": 256
  },
  "bg": [
    {
      "name": "bg2", "map_width": 64, "map_height": 32,
      "scroll_x": 106, "scroll_y": 0,
      "tilemap": [ ... 2048 u16 ... ]
    },
    {
      "name": "bg1", "map_width": 256, "map_height": 32,
      "scroll_x": 207, "scroll_y": 0,
      "tilemap": [ ... 8192 u16 ... ]
    }
  ],
  "collision": { "cell": 16, "width": 128, "height": 16,
                 "attrs": [ ... 2048 u8 ... ] },
  "spawn": { "x": 332, "y": 164 },
  "player": { "texture": "player_idle.png",
              "feet_x": 13, "feet_y": 43 },
  "sprites": [],
  "objects": [
    { "type": "urn", "species": 87, "x": 392, "y": 120 },
    { "type": "enemy", "species": 103, "x": 958, "y": 76 }
  ]
}
```

BG layer order is back-to-front: BG2 first, BG1 second.

The full-room BG1 is 256×32 tiles (2048×256 px) from the populated ROM screen
row. The captured Area 1 grid is 8×2, but the second row is all-zero padding
for this room, so the extractor emits only the populated row. Camera bounds
span the full room; BG2 is the 64×32 captured backdrop and wraps horizontally.
`play` bounds keep Firebrand inside the room.

### Player physics

`src/player.c` now implements the recovered Bank `$80` state machine directly
in 8.8 fixed point:

- Positions/velocities: `x_fx`, `y_fx`, `vy_fx`, `walk_speed_fx` (Q8.8).
  `x`/`y`/`vx`/`vy` are derived floats for the camera, renderer and trace.
- Grounded (`State 0x02`): walk speed stays at the `0x0166` cap on flat ground,
  matching `save0`; direction input moves the 8.8 position by `0x0166` per
  frame. The `0x00B3` first step plus `$80:8B85` 3/256 ramp lives in
  `UpdateWalkSpeed`; the `$1070` terrain probe that selects it is still to be
  wired.
- Floor collision uses `Level_FloorAt`: the cell under the feet is decoded with
  `Level_SurfaceOffset` from the `$80:962B` table, so the player stands at
  `block_top + slope_offset` (spawn is `y=164` on `attr=0x09`).
- Jump (`State 0x04`): pressing B on the ground sets `vy_fx = -0x059C` and
  enters substate 0 without moving that frame. Substates 0→2→4 integrate
  gravity (`0x0038`) before the position update, matching the observed
  `save0` arc. On the first substate-4 frame with B released while rising,
  `vy_fx` is zeroed (`$80:E227`).
- Hover (`State 0x06`): B pressed in air zeroes `vy_fx`, freezes vertical
  motion and bypasses gravity. The next B press returns to `State 0x04`
  substate 4 and falls on the following frame.
- Terminal fall: `0x0600` (6.0 px/frame, y-down).
- Horizontal movement is the original single 8.8 add per frame. Walls are the
  full-solid shapes (`0x01/0x15/0x1A`, half blocks `0x02/0x17`); slope shapes
  are surfaces, not walls, so the player follows them via the floor snap.
- Collision AABB: 12×22 px, feet-centered; ceiling/horizontal probes sample
  the upper body so standing on a block does not count as a wall.
- Player is clamped to `play` bounds.
- Shot (`Y`): plays the captured 17-frame spit animation (idle press frame,
  6-frame pose A, 10-frame pose B) and spawns one fireball by default. Special
  (`A`) plays the captured 31-frame Talisman/Spell clip.
- Landing: the first grounded frame after airtime starts an 8-frame landing
  animation.
- `DC_FLY_DEBUG=1` enables the non-canonical B+Up vertical flight for testing;
  it is off by default so base Gargoyle behaviour matches `save0`.

### Projectiles

`src/projectile.c` ports the first seven player-object slots
(`$1080..$1260`, stride `$50`):

- Spawn on the shot press frame, at `floor(player.x) + 3 * facing` and
  `floor(player.y) - 8`, matching the measured `save0` object slot.
- Horizontal velocity `0x0300` (3.0 px/frame, 8.8 fixed point); vertical
  velocity is zero for the grounded shot.
- One slot is allocated per shot (first free slot, like `$80:F222`); up to
  seven can be active.
- Despawn when the centre leaves the current camera viewport or enters a
  solid collision block (the stone pillar is the reference case).
- The visual is the palette-6 OAM group baked as `fireball_0..8.png` and
  played at one frame per guest frame; it is mirrored for left-facing shots.
- `DC_TRACE_PROJECTILES=path` writes `frame,slot,x,y,vx,age` per active slot.

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
  - `0x12` (`$E4F4`): Fireball attack state entry (in `save0` the grounded
    animation path selects `$1053 = 0x0E` instead)
  - `0x16` (`$EA4A`): Talisman / Spell invocation (animation `0x38`, back-turned wing spread)
  - `0x1A` (`$EC20`): Headbutt
  - `0x1C` (`$EDF1`): Hurt / knockback
  - `0x1E` (`$EEB9`): Death
- **Vertical integrator (`$80:8D03`, called from `$80:F0F2`)**:
  `position += velocity; velocity -= gravity;` then clamp fall speed at `0xFA00`.
- **Horizontal integrator (`$80:8D35`)**:
  `position += velocity`.
- **Player projectile slot allocator (`$80:F222`)**:
  scans the first 7 object slots (`$1080..$1260`, stride `$50`, limit `$12B0`)
  for a free slot (`$0000 == 0`), calls `$82:8D84` to clear it, and initializes
  the spawned object. `$80:F1FE` is a separate object helper, not this
  allocator.

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
| Jump / hover / fly | Z or Space | B (right face) | **B** (`$1E3A` → `$1059`/`$105A`) |
| Fly upward (debug only) | hold jump + Up | hold B + Up | Crest-dependent (not base Gargoyle) |
| Fireball shot | X or J | X / Y (west/top) | **Y** (`$1E3C` → `$105B`/`$105C`) |
| Talisman / Headbutt / Earth dash | C or K | A (south) | **A** (`$1E3E` → `$105D`/`$105E`) |
| Crest/Item subscreen | Tab or I | — | **X** (`$1E4C` → `$80:DC12`) |
| Cycle Gargoyle form | Q / E | L / R | — |
| Start / confirm | Enter | Start | Start |
| Pause / resume | Escape | Back | Start |
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
| `DC_DEMO=5` | animation regression: walk, special, jump, fly |
| `DC_DEMO=6` | exact `save0` Step 3 physics replay (603 frames) |
| `DC_DEMO=7` | projectile demo: right shots into the pillar, long left shot |
| `DC_DEMO=8` | full-room traversal: walk left, then hover right across pillars |
| `DC_DEMO=9` | urn → bouncing heart → heal regression |
| `DC_DEMO=10` | enemy chase/damage/drop regression (pair with `DC_START_X`) |
| `DC_DEMO=11` | Crest-form cycle, Earth dash, Air flight, Aerial shots |
| `DC_DEMO=12` | campaign traversal / boss-gate flow |
| `DC_DEMO=13` | title → NEW GAME → Area 0 flow |
| `DC_START_X` / `DC_START_Y` | spawn override for focused tests |
| `DC_TITLE=1` | force the title screen even with `DC_RUN_FRAMES` |
| `DC_NO_AUDIO=1` | disable audio device/music/SFX |
| `DC_AUDIO_DEBUG=1` | print audio device/music status |
| `DC_AUDIO_FORCE=1` | allow audio during deterministic `DC_RUN_FRAMES` runs |
| `DC_TRACE_OBJECTS=path` | per-frame active object slots CSV |
| `DC_TRACE_PROJECTILES=path` | per-frame active projectile slots CSV |
| `DC_FLY_DEBUG=1` | enable non-canonical B+Up vertical flight |
| `DC_DEBUG_COLLISION=1` | enable the `F1` collision overlay in headless captures |
| `DC_LAYER_MASK=1|2|3` | BG2 only / BG1 only / both |
| `DC_AREA=N` | load `room_areaN.json` instead of `room.json` |
| `DC_CAMPAIGN=1` | enable campaign transitions/overworld progression |
| `DC_STATE=name` | render a menu state (`title`, `overworld`, `subscreen`, `shop`, `password`, `options`, `clear`, `gameover`, `ending`) |
| `DC_BOSS_DEFEATED=1` | skip boss spawns (campaign flow testing) |
| `DC_BOSS_HP=N` | override boss HP |
| `DC_GOLD=N` | starting gold for shop/subscreen testing |
| `DC_POTIONS=N` / `DC_SCROLLS=N` | starting Urn/Spell counts |
| `DC_UNLOCK_ALL=1` | unlock all six Gargoyle forms |
| `DC_NO_HDMA=1` | disable the BG2 scanline parallax bands |
| `DC_UNDERWATER=1` | force underwater physics for testing |
| `DC_SAVE_PATH=path` | native save file (default `saves/dc_save.json`) |
| `DC_NO_SAVE=1` | disable save loading/writing |
| `DC_SAVE_FORCE=1` | allow saving inside deterministic `DC_RUN_FRAMES` runs |
| `DC_RESUME=1` | boot into the saved run instead of the title screen |
| `DC_PASSWORD=<code>` | apply a native 10-letter password at boot |
| `DC_PASSWORD_SELFTEST=1` | run 2000 encode/decode round-trips and exit (`0`/`1`) |
| `DC_WINDOW_INFO=1` | print window/render-target size and integer scale |

`DC_SCREENSHOT` is handled by raylib, which writes the basename into the
working directory even when an absolute path is supplied.

Native trace CSV columns:

```
frame,player_x,player_y,vx,vy,state,camera_x,camera_y,hp,max_hp
```

Projectile trace CSV columns (one row per active slot per frame):

```
frame,slot,x,y,vx,age
```

Object trace CSV columns:

```
frame,slot,type,x,y,vx,vy,state
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

- Built from the populated Area 1 screen row (`screens 8, 9, 10, 11, 16, 17,
  18, 19`), which places the solid/sloped floor at `by=10` (`y=160..175`, with
  attributes `7, 8, 9, 0x0a, 0x16, 1, 0x16, 0x0b..0x0e`) directly beneath
  Firebrand's feet and places the stone pillar at `cx=24`
  (`x=384..399`, `by=8..9`, `y=128..159`) beneath the urn at `(385, 111)`.
- Full room: 128×16 block grid (2048×256 px). The ROM grid is 8×2, but its
  first row is all-zero padding for this graveyard room; `--full-room` emits
  the populated row and reports this in the manifest.
- Attribute byte from `$98:8000 + (set<<12) + block`.
- High 3 attribute bits (`0xE0`) are environment flags; collision uses
  `attr & 0x1F`.
- Shapes `0x01/0x15/0x1A` are full solids, `0x02/0x17` are half-height blocks,
  and `0x03..0x14` are slope surfaces. The native table follows the
  `$80:962B` jump table:
  - `0x03: ((15-x)>>1)+8`, `0x04: (15-x)>>1`
  - `0x07..0x0a: ((15-x)>>2)+{12,8,4,0}`
  - `0x0b..0x0e: (x>>2)+{0,4,8,12}`
  - `0x0f: 15-x`, `0x10: x`, `0x11/0x12`, `0x13/0x14` linear/half variants
  - `0x16/0x18/0x19/0x1b` are walkable transition surfaces at offset 0.
- `Level_FloorAt` returns `block_top + offset` for the cell under the feet;
  walls/ceilings ignore slope cells so the player can walk up the hill.
- The `$1070` walk-speed probe is still separate; slope surfaces do not yet
  select the `0x00B3 → 0x0166` speed ramp.

### Block-table vs. VRAM tilemap (resolved)

BG1 now comes from the **ROM block tables** for the full populated screen row.
BG2 remains the **VRAM ring-buffer** backdrop from the captured frame
(`stage_600`). Earlier attempts to render the ROM block-table tilemap produced
"fences/garbage" **only because `--area 2` (`screens 33, 34`) was passed instead
of `--area 1` (`screens 8, 9`)**.

Verification against `--area 1`:

- `rom.screen_tilemap(0, 8)` and `rom.screen_tilemap(0, 9)` match
  `stage_600`'s 64×32 VRAM BG1 tilemap on **1957 / 2048 words identically**
  (100% of the loaded viewport; the remaining 91 words are the 3 rightmost
  columns of screen 9 that the VRAM ring buffer had not streamed in yet at
  `hscroll = 207`).
- **There is no tile remap.** The ROM block table (`$95 + set_id`) stores direct
  VRAM tile indices, palette bits and flip flags for BG1.
- The populated Area 1 row uses 492 unique BG1 tile indices (max index 622);
  every non-blank one is already present in `stage_600.vram.bin`. The only
  missing non-empty check is tile 4, which is not referenced by the room.
- The ROM grid is 8×2, but the first row is all-zero BG1 and collision for this
  room. `--full-room` therefore emits the populated 8-screen row (2048×256 px)
  and records the row's screen indices in `manifest.json`.
- BG2 is a looping 64×32 forest backdrop. The renderer wraps it horizontally
  (`tx & 63`) as the camera scrolls; the original per-scanline HDMA parallax
  on channel 1 (`$210F` `BG2HOFS`) is still a future improvement.

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
| `attack` (`Y` shot) | 3 | 1,6,10 | 17 frames: idle press frame, 6-frame pose A, 10-frame pose B |
| `spell` (`A`) | 3 | 11,4,16 | 31 frames; back-turned wing-spread Talisman/Spell pose |
| `fireball` | 9 | 1 each | palette-6 projectile OAM stream; one step per captured frame |

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
- `fireball_anim.json` uses the same step shape, capped at 16 steps by
  `src/anim.c`; projectile anchors are measured from the object slot's world
  X/Y rather than the player's feet.

Fireball extraction (resolved):

- Pressing **`Y`** (`$1E3C` → `$105C`) in `save0` keeps the grounded state but
  selects animation `$1053 = 0x0E` and spawns an object in the first free
  `$1080..$1260` slot (allocator `$80:F222`; the earlier `$80:F1FE` label was a
  different helper).
- The shot starts on the press frame; the player OAM shows the idle pose for
  that frame, then the 6-frame and 10-frame shot poses.
- Measured object spawn: `X = floor(player.x) + 3`,
  `Y = floor(player.y) - 8`, `vx = 0x0300` (3.0 px/frame), `vy = 0`.
- The moving fireball slot is the active slot with non-zero `+$20`
  horizontal velocity. Its OAM group uses **palette 6**, while Firebrand uses
  palette 7, so the extractor can remove it from the player bbox cleanly. It
  additionally filters by the object's screen position to avoid unrelated
  sprites.
- The game despawns the shot after it leaves the camera viewport; in `save0`
  that happens at world X 704 when the camera right edge is `448 + 256`.
- `extract_anim.py` emits `fireball_0..8.png` and `fireball_anim.json`; the
  native pool draws one step per guest frame and mirrors it for left shots.

Object list and HUD extraction:

- `extract.py --full-room` reads the 3-byte pointer table at `$81:C874`
  (index `area` by default; Area 1 = index 1) and parses 6-byte records
  `[species, flags, x16, y16]`. The high Y byte is the screen-row offset
  (subtract `row_index * 256`).
- Species `$57` becomes a destructible urn object; species `$67` becomes an
  enemy object. The static OAM urn is omitted when the object list contains an
  urn so it is not drawn twice.
- Area 1 yields four urns at `(392,120)`, `(888,168)`, `(904,168)`,
  `(1640,152)` and four `$67` enemies.
- `extract_anim.py` bakes the save0 BG3 HUD into `hud_bar.png` (six 8×8 health
  tiles at tile column 3, row 2) and `hud_slots.png` (three 2×2 crest slots at
  columns 18..29, rows 1..2), with the brown BG3 backdrop made transparent.
- Heart/enemy sprites (`heart_00..01.png`, `enemy_00..01.png`) are now baked
  from ROM OAM/CHR via the `$8B:8000` metasprite decoder (`extract.py`); other
  species still share the ghoul sheet until their init handlers are baked.

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
  - First 7 slots (`$1080..$1260`, up to `$12B0`) are allocated by `$80:F222`
    for player projectiles/effects.
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
   BG atlases pre-bake four flip quadrants. Player and fireball animations now
   load a second, pre-mirrored texture in `Anim_Load`; the renderer picks it
   for left-facing sprites, so no negative source width is used.

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

12. **The camera is no longer locked.**
    `room.json` from `--full-room` holds the 2048×256 BG1 row and camera bounds;
    the renderer wraps the 64×32 BG2 backdrop horizontally. Vertical camera
    range is only 32 px for this row.

13. **Collision attributes use the `$80:962B` surface table.**
    Walls are full/half blocks; `0x03..0x14` are slope surfaces. `0x16/0x18/
    0x19/0x1B` are treated as flat walkable transitions, and `0xA0` remains
    pass-through. The `$1070` walk-speed probe is still separate from the
    surface table.

14. **The HUD is extracted from `save0`, not `stage_600`.**
    `stage_600` is a cinematic frame with no BG3 status bar; `save0` has the
    health bar and crest slots. `extract_anim.py` bakes them as
    `hud_bar.png`/`hud_slots.png`. The active Crest/Talisman slot is
    highlighted from the current form; exact retail slot icons are still
    approximate.

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

20. **The fireball OAM group uses palette 6, not the player's palette 7.**
    In `save0`, the projectile stream is the palette-6 group near the moving
    object slot, so palette filtering keeps it out of Firebrand's body PNG.
    `extract_anim.py` still applies an object-position window as a fallback in
    case a capture uses the same palette for both.

21. **BG2 backdrops are generated by the guest; there is no ROM tilemap table.**
    `$84:A057` uploads the BG2 tilemap to VRAM `$4000` by DMA from WRAM bank
    `$7F`, using chunk tables `$84:C311`/`$84:C312` indexed by `$00F9`; when the
    table entry is empty it DMAs a static 0x1C00-byte background from
    `$81:8068`. Area 1 and Area 3 backdrops differ (only 1.5% of BG2 words and
    27% of the first 32 KiB of CHR are equal), so per-area data is required.
    The extractor therefore takes BG2 from an oracle frame per area via
    `tools/dc_extract/bg2_captures.json`.

22. **WRAM `$008D` is not a "load this room" switch.**
    `$84:F0DA..F0EF` advances it by 2 (`$008D` is the room-sequence/stage id and
    the old value is copied to `$0E56`), but writing `$008D` through the debug
    server's `write_ram` does not load a room: 120 frames after
    `write_ram 8d <next>`, VRAM was byte-identical and the camera did not move.
    Per-area oracle captures must go through the game's transition/script path,
    not a raw area-id poke.

23. **The `$82:8300` species table holds `JSL` stubs, not full handlers.**
    All four object tables are 0xC0 entries (0x180 bytes) apart: `$82:8000`
    main update, `$82:8180` secondary, `$82:8300` init, `$82:8480` aux, each
    indexed by `species*2` (the loop does `lda $02; and #$00FF; asl; tax`).
    Late init entries are 5-byte `JSL $83:xxxx; RTS` thunks: species `$67`
    resolves to `$82:F382` → `JSL $83:AF19` (init), with its update handler at
    `$83:AF74`. That is the route to per-species OAM/animation data.

24. **The Windows trace build's debug server supports state writes.**
    `write_ram <addr_hex> <hex bytes>`, `read_ram <addr_hex> <len>`,
    `dump_vram`, `get_frame_extended <frame>`, `dump_frame_vram <frame>` and
    `run_to_frame <frame>` all work against
    `build-trace\DemonsCrestSNESRecomp.exe` with `SDL2.dll` beside it. Remember
    `--resume-state` consumes the save file and `run_to_frame` re-pauses the
    guest at the target.

25. **`save0` (Area 3) has no live objects, so teleporting will not show enemies.**
    All 26 object slots (`$1080..$1D50`) are inactive in that save; writing the
    player position (`$1030/$1033`, 8.8 fixed) to an enemy's listed position and
    running 45+ frames did not activate the Area 3 species `$26`/`$07` objects.
    Enemy sprite capture therefore needs either a different save/route or the
    ROM-side animation data, not the current save0.

26. **Injecting an object record runs the species init, but not every species
    draws its own art.**
    The object loop starts with `$00` bit 0 set as "init pending": it clears the
    bit, sets `$00 = 6` and calls `JSR ($8300,X)` with `X = species*2`. Writing a
    record with `$00=1` therefore initialises any species in place. However the
    real spawner (`$82:8BDB`) also fills fields the init expects, so species
    whose sprite setup depends on them render Firebrand's sheet. In one 11-way
    capture pass `$57/$49/$2D/$67` baked correctly while `$26/$2E/$81/$24/$33`
    came out identical to the player and were rejected. Reproduce the spawner's
    remaining writes before trusting a species' capture.

27. **`$81:AE11` is the per-area *ambient* sprite list, not the object species list.**
    The table holds 16-bit little-endian pointers per area (`$AEF9` area 0,
    `$AF15` area 1, `$AF22` area 2, `$AF2F` area 3, ...). Each list is a run of
    2-byte pairs terminated by `0x00`; the first byte is a `$8B:8000` metasprite
    group id and the second is an even frame count (area 1:
    `4E/2, 03/4, 61/6, 33/8, 0E/8, 16/8`). It is **not** a species→group map:
    intersecting the group sets of every area where a species occurs leaves
    contradictory candidates (55% of area pairs share no group at all), so these
    are the room's ambient animated sprites. Object sprites come from elsewhere:
    the species `$67` init writes `$8B`-bank frame pointers into object fields
    `$28/$2A/$2E` (`$DE87/$DE7B/$DE6C`), which land inside the metasprite data
    block at the end of bank `$8B` — that is the next thread to pull.

---

## 9. Current limitations

- All 113 campaign areas extract and load. BG2 backdrops are captured for
  Areas 1 and 3; other areas keep a transparent layer until an oracle frame is
  added to `tools/dc_extract/bg2_captures.json`. `tools/audit_campaign.py`
  classifies 69 rooms as playable and 44 as partial (mostly zero-collision
  boss/cutscene arenas whose floor is not in the block tables).
- BG2 parallax in Area 1 now uses the recovered `$84:AF65` HDMA bands; other
  areas have no per-scanline effect until their BG2 tables are decoded.
- Player animation covers idle/walk/jump/hover/fall/land, the 17-frame `Y`
  shot and the 31-frame `A` spell. Hurt, death and wall-cling clips are still
  needed; the six Gargoyle forms currently reuse the base sheet with palette
  tints instead of separate form OAM sets.
- The projectile kind covers normal/charged shots and the Fire-Crest element
  tint; Tornado platforms and Claw lava-freezing are not fully simulated.
- Enemy behavior covers the first-pass ghoul plus Area 2 skeletons, bats and
  wall gargoyles; other species still reuse the shared enemy sprite/AI.
- The HUD shows HP and three crest slots and highlights the active form; the
  exact per-talisman icons and retail slot layout are still approximations.
- The `$1070` terrain probe is wired to `attr & 0xE0 == 0x80` and selects the
  `0x00B3 → 0x0166` ramp; exact per-shape ramp behaviour still needs oracle
  comparison.
- Audio is an original procedural development loop/SFX, not extracted ROM
  music; there is no music/sfx menu beyond the master volume option.
- Text/dialogue, cutscenes and the exact retail password algorithm are stubs
  (the password screen now uses a working native 10-letter codec; the retail
  format is still unknown). Campaign progression persists through the native
  save file (`saves/dc_save.json`), but that is not the retail SRAM/password
  format.
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
- `DC_DEMO=5` exercises walk, special, jump and fly for screenshots.

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

### Step 3 — Port the authentic Bank `$80` player state machine & physics (done)

Why before projectiles/scrolling: the exact constants and transitions are now
known from `$80:DB00..F310`; replacing the temporary float heuristics in
`src/player.c` locked down movement feel and input handling (`B` = Jump /
Hover toggle, `Y` = Fireball shot, `A` = Talisman/Headbutt).

Done:

1. `src/player.c` now stores `x_fx`, `y_fx`, `vy_fx` and `walk_speed_fx` as
   8.8 fixed point:
   - Walk (`$80:F0A0..F0E5`): flat ground keeps `$1020 = 0x0166` (oracle
     behaviour); `UpdateWalkSpeed` implements the `$1020 = 0x00B3` first step
     and `$80:8B85` 3/256 ramp for the `$1070` slope path (surface walking is
     done in Step 5; the speed probe is still separate).
   - Gravity (`$80:E070`, `$80:ED11`): `$1026 = 0x0038`; terminal fall
     `0xFA00` (`6.0` px/frame in y-down coordinates).
   - Jump (`$80:E160..E239`, `State 0x04`): the press frame enters substate 0
     without moving; substates 0→2→4 integrate gravity and position; the first
     substate-4 frame applies the `$80:E227` B-release cut (`vy = 0`).
   - Hover (`$80:E1A3`, `$80:E29F..E340`, `State 0x06`): B pressed in air
     enters hover with `vy = 0` and no gravity; the next B press returns to
     `State 0x04` substate 4 and falls on the following frame. `B + Up` flight
     is behind `DC_FLY_DEBUG=1`, off by default.
2. `src/input.c` maps SNES `Y` to the shot input (keyboard `X`/`J`, gamepad
   west/top) and SNES `A` to the special input (keyboard `C`/`K`, gamepad
   south). `Input` carries held/pressed pairs like `$1059..$105E`.
3. Added `tools/oracle/export_capture_csv.py` and
   `tools/oracle/segments_save0_physics.json`; `DC_DEMO=6` replays the same
   603-frame walk / tap-jump / hold-jump / hover sequence.

Acceptance:

- Tap-jump, full-hold jump, walk speed, and mid-air hover toggle match `save0`
  within ≤1 px / 1 frame across a 600-frame trace.
- Result: 603 frames compared, max delta `x=0.00 px`, `y=0.53 px`. The
  residual Y delta is the known `$1000` task-idle oscillation documented in
  trap #8.

### Step 4 — Capture `Y` Fireball shot in `save0` & implement projectiles (done)

Done:

1. Added `("shoot", "y", 35)` to `capture_anim.py`'s default route and captured
   it from `save0` (no `save2` needed).
2. Inspected the spawned slot in WRAM (`$1080..$1260`, stride `$50`, allocated
   by `$80:F222`; earlier notes called this `$80:F1FE`):
   - spawn frame = the Y press frame; slot `$1080` becomes active before the
     first shot pose is visible,
   - X = `floor(player.x) + 3`, Y = `floor(player.y) - 8`,
   - horizontal velocity `$1020 = 0x0300` (3.0 px/frame), vertical velocity 0,
   - the moving slot is identified by non-zero `+$20`; the projectile OAM
     group is palette 6 and the player is palette 7.
3. Updated `tools/dc_extract/extract_anim.py`:
   - projectile OAM is grouped around the moving object slot and excluded from
     Firebrand's palette-7 body composition,
   - the real Y clip is exported as `attack` (3 steps, durations 1/6/10 =
     17 frames), the A clip as `spell` (11/4/16 = 31 frames),
   - baked `fireball_0..8.png` + `fireball_anim.json` (one step per frame,
     palette 6, per-step object anchor).
4. Added `src/projectile.c` / `src/projectile.h`:
   - 7-slot pool matching `$1080..$1260`,
   - spawn on the shot press frame with the measured offset and velocity,
   - despawn on solid collision (pillar) or camera viewport exit.
   - `src/render.c` draws the palette-6 fireball group and mirrors it for
     left-facing shots.
5. Added `DC_DEMO=7` and `DC_TRACE_PROJECTILES=path` for reproducible visual
   and CSV checks.

Acceptance:

- Pressing Shot plays the captured Y spit animation and spawns a fireball on
  the press frame at the measured offset.
- The pool allocates up to seven slots; `DC_DEMO=7` verifies pillar despawn,
  viewport despawn, concurrent slots and left-facing mirroring.
- Corrected finding: `$80:F222` is the slot allocator; `$80:F1FE` is a
  different object helper and was mislabeled in the earlier notes.

### Step 5 — Full-room Area 1 tilemap, slope/ledge attributes & camera scrolling (done)

Why now: since Trap #4 is resolved (ROM block tables match VRAM directly when
`--area 1` is used), full-room scrolling no longer requires reverse-engineering
a tile remap.

Done:

1. Extended `tools/dc_extract/extract.py` with `--full-room`:
   - finds the populated screen row of the area grid and emits all eight
     screens as a 2048×256 BG1 map (256×32 tiles) plus a 128×16 collision grid,
   - all 492 referenced BG1 tile indices are already resident in
     `stage_600.vram.bin`; no ROM CHR decode or VRAM merge was needed,
   - BG2 stays the captured 64×32 backdrop and the renderer wraps it
     horizontally as the camera moves.
   - The ROM Area 1 grid is 8×2, but its other row is all-zero BG1/collision
     padding, so the extractor emits the populated row and records the screen
     indices `[8, 9, 10, 11, 16, 17, 18, 19]` in `manifest.json`.
2. Decoded the graveyard surface table from `$80:962B`:
   - full solids `0x01/0x15/0x1A`, half blocks `0x02/0x17`,
   - slopes `0x03..0x14` with the per-pixel formulas,
   - walkable transitions `0x16/0x18/0x19/0x1B` at offset 0.
   `Level_FloorAt`/`Level_SurfaceOffset` now snap Firebrand to
   `block_top + offset`; walls ignore slope cells.
3. Unlocked `camera.min_x..max_x` and `min_y..max_y` in `room.json`:
   - horizontal deadzone 96..160 and vertical deadzone 96..176,
   - clamps to the 2048×256 room,
   - `DC_DEMO=8` walks left across the lower graveyard and then hovers right
     across pillars to exercise the full map.

Acceptance:

- Firebrand walks smoothly up and down the graveyard slopes (`by=10..11`),
  stands at `y=164` on `attr=0x09` at spawn, and the camera scrolls across the
  full starting room without stale or missing tiles.
- Verified by trace and screenshots at camera X 0, ~90, ~370 and ~425; the
  player height follows the slope as `164 → 163 → 162 → 161 → 160` (up) and
  back down.
- Remaining Step 5 sub-item: BG2 HDMA per-scanline parallax; the current
  backdrop uses a flat 1:1 offset plus horizontal wrapping.

### Step 6 — Object system (`$1080..$1D50`), one graveyard enemy & heart pickup (`$85:C180`) (done)

Done:

1. Added a 26-slot `ObjectPool` (`src/object.c`) and a data-driven spawn pass
   from the Area 1 object list (`$81:C874`, 6-byte records). The first seven
   projectile slots remain in `src/projectile.c`; the object pool holds urns,
   pickups and enemies.
2. Ported the `$85:C180` pickup behavior:
   - pop-up `0x0400` (4.0 px/frame), gravity `0x0040` (0.25),
   - `$39` bounce value starts at `0x0400`, loses `0x0100` per landing, then
     rests,
   - player overlap increments `$1062`/HP up to `$1E50`/max.
3. Added destructible urns for species `$57`: fireball hits shatter the urn and
   spawn a pickup at its bottom edge. Area 1 has four urns.
4. Added a first-pass species `$67` enemy:
   - emerges for 24 frames, then chases within 220 px,
   - contact costs 1 HP with 60-frame invulnerability and horizontal knockback,
   - two fireball hits kill it and drop a heart.
   This is the correct species/object-list plumbing, but its sprite and
   behavior are placeholders until `$83:AF74` is fully ported.
5. Added `DC_TRACE_OBJECTS`, `DC_DEMO=9` (urn → heart → heal) and
   `DC_DEMO=10` + `DC_START_X` (enemy chase/damage/drop) for validation.

Acceptance:

- Destroying the urn at `(392,120)` drops a heart that follows the measured
  `$85:C180` arc and heals HP on touch.
- `DC_DEMO=10` confirms the species `$67` enemy can be hit, killed and drops a
  heart.

### Step 7 — BG3 HUD status bar (done)

- `extract_anim.py` decodes the save0 BG3 2bpp layer and emits
  `hud_bar.png` (six health tiles) and `hud_slots.png` (three 2×2 crest slots).
- `render.c` draws the HUD in screen space: translucent top strip, current HP
  bar scaled over a dark max-HP bar, three crest slots, and a gold highlight on
  the active spell slot.
- Player HP is initialized to 4/6 so hearts visibly heal; the trace CSV now
  includes `hp,max_hp`.
- Remaining HUD work: real talisman inventory/selection and the exact
  per-talisman icons. The current first-slot highlight is a placeholder.

### Step 8 — Engine hardening & automated regression harness (done)

- **Fixed-timestep accumulator in `main.c`**: `SetTargetFPS(0)` + VSync, a
  `GetFrameTime()` accumulator capped at 0.25 s / 8 catch-up steps, and exact
  60 Hz `Game_Update`. `DC_RUN_FRAMES` remains deterministic
  one-update-per-loop for traces/screenshots.
- **Pre-baked mirrored frames**: `Anim_Load`/`ProjectileAnim_Load` create a
  mirrored texture per step; `render.c` selects it for left-facing player and
  fireball draws. The static fallback player also has a pre-flipped texture.
  No negative `DrawTexturePro` source widths remain.
- **Single-command regression**: `tools/run_regression.py` (also
  `tools/run_regression.sh` and CTest `regression`) runs demos 6/7/9/10/11/12 and
  asserts walk/jump/hover, fireball spawn/despawn, urn/heart arc, enemy damage
  and HP changes. `ctest --output-on-failure` passes. Missing binary/assets
  return 77 so CTest reports a skip.
- Acceptance: `ctest` passes in ~60–80 s, and the local `compare.py` oracle flow
  remains available for ROM-accurate checks.

### Step 9 — Audio, menus, game flow & full content (vertical-slice portion started)

Done in this session:

- **Audio**: `src/audio.c` synthesizes an original looping chiptune theme and
  jump/land/shoot/break/pickup/hurt/enemy-hit SFX at startup. No ROM audio is
  used. Audio initializes only when a device is available and is skipped for
  `DC_RUN_FRAMES`; `DC_NO_AUDIO`, `DC_AUDIO_DEBUG` and `DC_AUDIO_FORCE` are
  available for testing.
- **Flow**: `Game` now has Title / Playing / Paused / Clear states. The title
  screen renders the graveyard behind an overlay and starts on Enter/Start;
  Escape/Back pauses. The object-list exit (`species $09` at x=1920) triggers
  an Area Clear screen; Enter restarts.
- **Menus**: pause overlay and restart-on-clear are implemented. Password,
  stage select, map and title options are not.

Remaining Step 9 work:

- Room transitions and the rest of Areas 1–3, boss gate `$81:97DA`, bosses,
  crest forms, Mode 7 overworld, password/save/menu flow.
- ROM music/SFX extraction (BRR/DSP + sequences) or a replacement soundtrack
  for public release, plus volume/mute options.
- Original fan-game levels/story/art.

### Step 10 — Full-campaign foundation (started this session)

Done:

- **ROM extraction**
  - `tools/dc_extract/extract.py` now implements the native decompressor at
    `$80:C2AF`/`$80:C300` (16 bytes per call, 8 calls per 16×16 unit, `+0x200`
    bottom-row skip) and the `$98:C000` (BG CHR) / `$98:D000` (OBJ CHR)
    24-bit pointer tables.
  - All 113 valid areas from `$81:A291` + `$81:A6D8` are emitted as
    `room_area<N>.json`: 2D multi-row and vertical grids (e.g. Area 2 8×2,
    Area 3 4×5), collision attributes, spawns from `$81:A1A9`, bounding-box
    transitions from `$81:981A`, boss-gate target from `$81:97DA`, and object
    species/flags/positions from `$81:C874`.
  - Per-area BG atlases (`bg_area<N>_pal<P>.png`) are baked from ROM CHR and
    the ROM BG palette blocks (`$99:9080`, `$99:90A0`, `$99:A900 +
    ($81:8909[area] & 0xFFC0)`), limited to palettes the area actually uses.
    `Assets_LoadAreaBg` swaps the 8 atlases on room transitions.
  - OAM metabug: `$8B:8000` metasprite groups decoded from `$80:D9A7`, with
    the `$80:DA33` tile-base carry rule and per-part flip bits. Real ROM
    heart/Crest pickup and ghoul sprites are baked; the procedural
    `heart_*`/`enemy_*` placeholders are no longer used.
  - Item palette extracted from `$99:9240`; Gargoyle form palettes from
    `$99:9100..91BF`.
- **Level engine**
  - `Level` now carries `area_id`, `gate_next_area`, `backdrop`, up to 8
    spawn records and 16 transitions, plus `ObjectType` platform/crest/boss.
  - `Level_LoadArea`/`Game_TransitionToArea` load any extracted room; the
    runtime handles mid-room bounding-box transitions, boss-gated exits,
    respawn logic and per-area BG atlas swaps.
  - Collision environment flags: hazards (`0x20`, e.g. `0x21`), breakable
    `0x40` blocks (removed from the grid and BG1 tilemap), water (`0x60`),
    slope acceleration (`0x80` → `$1070`), plus the `$80:962B` surface table.
    Underwater physics uses `$1045` constants (`0x02CE` jump, `0x002A`
    gravity, `0x0300` terminal fall).
  - BG2 HDMA scanline parallax recovered from `$84:AF65` (32/44/28/46/74
    line bands with 0.25/0.125/0.25/0.50/1.25 factors) and rendered with
    per-band clipped tiles. `DC_NO_HDMA=1` disables it.
  - Room backdrop colour emitted from ROM palette 0; the renderer clears to
    it instead of a fixed colour.
- **Player/crest system**
  - `CrestForm` enum with Base, Earth, Aerial, Water, Time and Ultimate
    Gargoyle. `Q`/`E` or L/R cycles owned forms; clearing Areas 0–4 unlocks the
    next Crest, and `DC_UNLOCK_ALL=1` (or `DC_DEMO=11`) unlocks all six for
    testing.
  - Earth/Ultimate shoulder tackle (breaks `0x40` blocks ahead), Aerial/
    Ultimate vertical flight, Water free swimming, Time/Ultimate and Armor
    Talisman damage reduction, overhead headbutt block breaking, and wall
    cling (hold A against a tall wall while descending).
  - Fire-Crest element selector (Fireball, Buster, Tornado, Claw, Demon
    Fire); Buster/charged shots break `0x40` blocks. Projectiles carry
    damage/element and are tinted per element.
  - Crest/Item subscreen (Tab/X) with HP, Urns, Spells, gold, form and
    Fire-Crest/Talisman selection; Urn heals; Spell spends a scroll.
- **Enemies, bosses and objects**
  - 26-slot object pool extended with platforms, crest/life-stone pickups and
    bosses. Area 0 spawns **Somulo** (HP bar, lunge/leap cycles); Areas 2/3/5
    add skeleton patrols, bats and wall gargoyles; enemy contact uses the
    shared damage/knockback/invulnerability path; bosses can be tackled or
    shot, drop a pickup and set `boss_defeated`.
  - Urns drop hearts; hearts heal and award gold; crest pickups raise max HP,
    gold and refill potions.
- **Campaign flow**
  - Title menu with New Game / Overworld Map / Password / Options.
  - Mode 7-style overworld with six landing nodes, Firebrand marker,
    nearest-node selection, stage locking (Demon Keep requires the first four
    stages), and shop access.
  - Pause menu with Resume / Return to Overworld; shop (Urns, Spells, Life
    Stones); generated password screen; volume options; Game Over;
    clear screen with password; ending screen; `GAME_ENDING` after Demon
    Keep when all stages are cleared.
  - Deterministic `DC_STATE` hook renders each menu for screenshots.

Acceptance:

- `dc_extract` regenerates all 113 area rooms plus per-area atlases from the
  ROM in ~65 s.
- `DC_AREA=0/1/2/3` loads the corresponding room and renders its own BG set.
- `DC_AREA=0 DC_CAMPAIGN=1 DC_BOSS_DEFEATED=1 DC_DEMO=12` walks through the
  Area 0 boss gate into Area 17 (trace `area` column).
- `DC_DEMO=11` cycles at least three Crest forms and exercises the
  Earth/Ultimate dash, Air flight and Aerial shots.
- `tools/run_regression.py` covers demos 6/7/9/10/11/12 and Area 2/3 loads.

Still placeholder / remaining:

- Enemy and boss animations beyond the shared ghoul sprite; real per-species
  OAM groups need to be baked from their init handlers.
- Player form sprites/animations; current forms are palette tints of the base
  Gargoyle sheet.
- Text/dialogue/cutscene scripting, exact retail password decode, SRAM-style
  save persistence, and shop NCP dialogue.
- ROM audio (SPC700/BRR/DSP) extraction or replacement.
- Mid-stage `$98:E000` CHR streaming coverage and per-area BG2 backdrops.


### Step 11 — Save/options persistence and HUD polish (done)

Done:

1. `src/game.c` gained `Game_SaveProgress` / `Game_LoadProgress` /
   `Game_HasSave`:
   - JSON (cJSON) file at `saves/dc_save.json` (override `DC_SAVE_PATH`),
     versioned; stores area, `stages_cleared`, HP/max HP, Crest mask + form,
     Talisman mask + active, Fire element, Urns, Spells, gold and volume,
   - `Game_LoadProgress` transitions to the saved area first (so the room,
     collision and atlases are rebuilt), then applies the saved player fields
     and clamps them,
   - autosave at `Game_CompleteArea`, `Game_OpenOverworld` and `Game_Shutdown`
     (only while `campaign_mode` is set),
   - deterministic `DC_RUN_FRAMES` runs skip saves unless `DC_SAVE_FORCE=1`;
     `DC_NO_SAVE=1` disables load/save; `DC_RESUME=1` skips the title.
2. Title menu shows `CONTINUE` only when a save exists; otherwise the original
   four entries are kept so `DC_DEMO=13` still starts a new game.
3. HUD health bar: the baked texture is a brightness mask, so the renderer now
   draws the empty part dark and the filled part red (`(235, 88, 120)`), which
   restores the segmented red health bar look.
4. `tools/run_regression.py` covers the save round-trip: run A autosaves with
   `DC_SAVE_FORCE`, run B resumes with `DC_RESUME` and the trace must report the
   same area, max HP and gold.

Remaining polish: a proper CONTINUE/options screen layout, per-slot saves,
retail SRAM/password parity, and saving mid-room position rather than the room
spawn.

### Step 12 — Native password codec and entry screen (done)

Done:

1. Replaced the placeholder 8-letter generator with a real codec
   (`Game_PasswordEncode` / `Game_PasswordDecode` / `Game_ApplyPassword`):
   - 32-bit payload `stages[7:0] max_hp[11:8] crests[17:12] form[20:18]
     potions[23:21] scrolls[26:24] talismans[31:27]`,
   - 8-bit FNV-1a checksum, encoded as 10 base-20 digits from
     `BCDFGHJKLMNPQRSTVWXYZ` (40 bits total),
   - invalid or truncated codes are rejected, so a mistyped password cannot
     corrupt a run.
2. Password screen is now interactive: up/down cycles the letter under the
   cursor, left/right moves it, Enter applies a valid code (restoring stages,
   max HP, Crests/form, Urns, Spells and Talismans, then opening the overworld
   and autosaving), Esc returns to the title, and the screen reports
   `INVALID CODE` / `PROGRESS RESTORED`.
3. `DC_PASSWORD=<code>` applies a code at boot (tests, cheats, support);
   `DC_PASSWORD_SELFTEST=1` runs 2000 pseudo-random encode/decode round-trips
   and exits with `0`/`1`, which needs no window or assets.
4. `tools/run_regression.py` mirrors the encoder in Python and asserts that a
   generated code restores `max_hp` and `form` in a real run, alongside the
   binary self-test. The cross-implementation check also catches codec drift.

Still pending: the retail password/SRAM algorithm and the exact retail screen
layout.

### Step 13 — Per-species OAM via injected objects (started)

Done:

1. `tools/oracle/capture_species.py`: the guest object loop treats bit 0 of an
   object's `$00` word as "run the species init", so writing a minimal record
   (`$00=1`, `$02=species`, `$0A=0x30`, `$26=0x40`, world position) into the
   first slot makes the game initialise and draw that species in the current
   room. The tool starts the trace build paused on a fresh save copy, derives
   the world->screen offset from the player's palette-7 OAM cluster (feet
   anchor, since the guest restores the BG scroll registers after rendering),
   injects the record, and dumps OAM/CGRAM/VRAM/PPU at `--frames` offsets.
   Eleven species were captured in one pass; the technique needs no area travel.
2. `tools/dc_extract/extract_species.py`: selects the object's OAM cluster
   (window around its screen position, player cluster excluded), composes it
   with the captured CGRAM and per-sprite palettes, crops, records the anchor
   relative to the object position, deduplicates poses and writes
   `enemy_<species>_<n>.png` + `species_anim.json`. Species whose capture came
   out identical to the player sheet are rejected automatically.
3. `src/assets.c` loads `species_anim.json` (up to 16 species × 4 poses with
   anchors and a frame duration); `src/render.c` draws the per-species sheet for
   enemy and boss objects, falling back to the shared `enemy_*.png` sheet.
4. Verified in Area 1 (`DC_DEMO=10 DC_START_X=900`): the species `$67` enemy now
   draws its own captured creature instead of the shared sheet; the urn and
   pickup species bake correctly.

Known limitation: species whose init depends on spawner-supplied fields (the
real spawner `$82:8BDB` copies a 6-byte record and sets further fields) still
render the player sheet, because the injection only provides the record fields.
Captured-and-rejected species: `$26`, `$2E`, `$81`, `$24`, `$33`. Reproducing
the spawner's remaining field writes (or invoking the spawner itself) is the
next step to cover every species.

### Step 14 — Species `$67` ghoul: authentic drop + porting map (started)

Done:

1. Ported the opening of the real `$83:AF74` handler into `src/object.c`:
   - emergence (24 frames) then **state 1** = the perch drop described by
     `$B2B1`/`$AFCB`: Y is lifted 9 px (`$34 -= 9`, object Y is y-down), then
     `$20 = 0x0050` horizontal, `$22 = 0x0398` downward, gravity `$26 = 0x0040`,
     windup `$3B = 3`, landing sets `$3B = 6` (`$B2C7` snaps Y by 7),
   - after landing it falls back to the previous chase as **state 2** until the
     remaining behaviour states are ported.
2. Added `substate`/`phase`/`facing` to `Object`. `phase` is required because
   `timer` is incremented every frame for animation, so a countdown must not
   share it.
3. Regression (`demo 10`) passes: the ghoul drops from its perch, closes on the
   player, damages on contact and dies to fireballs.

Porting map recovered from `$83:AF74` (for the next pass):

| Address | Role |
|---|---|
| `$AF89` | state 0 dispatcher on `$06`; substate 0 calls the `$B2E8` gate, else picks a behaviour from `$03` or the random table `$DEA2` (`$BC904B & 0x0F`) |
| `$AFCB` | substate 2: windup `$3B`, then `$B2B1`, vx `$20 = 0x50/0x88/0x70`, vy `$22 = 0x398/0x4D0`, `$2A = $DE7F` collision shape, `$39 = 5` |
| `$B000` | substate 4: `JSL $808C14`/`$808CBF` (object integration), ground test `$04 & 4`, then `$B2C7` |
| `$B065` | state 1 handler (not ported) |
| `$B0E4` | state 2 handler (not ported) |
| `$B1AF` | state 3 handler (not ported) |
| `$B249` | state 4 handler (not ported) |
| `$B298` | face the player: `$0A` bit 6 = facing right, `$09` = 1/2 |
| `$B2B1` | lift Y 9, `$06 = 4`, anim 4 |
| `$B2C7` | land: `$2A = $DE7B`, Y += 7, clear fractional X/Y, `$06 = 6` |
| `$B2E8` | gate: carry set when `ownX+0x70-playerX >= 0xE0`, `$47 == 0`, or `playerY-ownY` outside `[0, 0x50)` |

Further decoding (round 1):

- Substate 6 (`$B034`) runs a **3-hop burst**: `$38` counts hops, each landing
  alternates facing (`$0A ^= $40`, `$09` swapped) and restarts the hop at
  `$AFBC`; after 3 hops it clears `$06` and re-decides. The launch velocity is
  `$22 = 0x0398` for even `$38` and `0x04D0` for odd (`$B019..$B02D`). Ported:
  `Object.hops` drives the burst with the alternating facing and velocity
  variant, then the chase fallback takes over.
- Substate 4 (`$B000`) integrates through `JSL $808C14`/`$808CBF`, lands on
  `$04 & 4`, then jumps to `$B2C7`; its animation timer `$39` is loaded from
  `$DE8F` (even `$38`) or `$DE93` (odd) with the animation id from `$DE9B[$3A]`.
- State 1 (`$B065`, substate table `$B06A`): windup 32 frames → `$B2B1` →
  `$2A = $DE7F`, `vx = 0x0120`, `vy = 0x0540` (a much bigger leap) → land
  (`$3B = 0x10`) → hold → `$05 = 4`.
- State 2 (`$B0E4`, substate table `$B0E9`): face the player, anim 0, 32 frames →
  anim 6 with `$3C = 0x40` (64 frames), `$3D = 3` bursts, `$48 = 0xFF` →
  `JSL $83B8EF` blocks until the "fire" phase, plays SFX `$27` and spawns
  **species `$67`** as a projectile via `$82877B` at `x ± 0x13`, `y - 0x14`
  (the spawned object's own update runs at `$82:8BDB`), three times, then
  anim 0 with `$3B = 0x20`.
  This means the area object list's `$67` entries are the ghoul's *projectile*
  species, which explains why the injected capture of `$67` looked like a
  floating round object rather than a humanoid.

Implementation (round 2):

- New `OBJECT_ENEMY_SHOT` (type 7): hostile projectile carrying the captured
  species `$67` sprite, flying toward Firebrand at 1.5 px/frame, despawning on
  solid terrain/out of bounds/240 frames, damaging on overlap, and destroyable
  by the player's fireballs.
- Ghoul state 2 now runs the fire sequence: face the player, 3 volleys on the
  `$3C = 0x40` cadence, spawn offsets `x ±0x13`, `y - 0x14` (pixels), then a
  leap. Outside ~200 px it closes distance with the placeholder approach so the
  player can still engage.
- Ghoul state 3 is the leap attack from `$B065`: 32-frame windup, `vx = 0x120`,
  `vy = 0x540` (y-down dive), gravity `0x40`, 16-frame landing hold, then back
  to firing.
- Regression updated to the authentic behaviour: demo 10 asserts the ghoul fires
  projectiles and damages the player (kills now require a ground target), and a
  new area-2 run asserts skeletons die to fireballs (`DC_AREA=2`,
  `DC_START_X=360`).

Known gap: the ghoul lands on its perch at y≈96 and cannot descend to Firebrand
on the floor, because the `$DEA2` behaviour states that would move it between
ledges are still unported. Its volleys are the main threat in that room.

Sprite-data lead (round 3): diffing the object slots of a species that captures
correctly (urn `$57`) against one that draws Firebrand (`$2C`) shows the init
handlers write **species-specific 16-bit pointers** into `$28/$2A/$2E`
(`$D229/$D219/$D874` for the urn, `$D5A6/$D59A/0` for the skeleton). Those
offsets land inside the metasprite data block at the end of bank `$8B`
(`$8B:D219` etc. decode as 5-byte sprite parts), i.e. the ROM does hold
per-species frame data and it is addressable from the init handlers. The exact
frame-selection rule (which of the three pointers the renderer uses per state)
is still open, and decoding the frames needs the area's OBJ CHR
(`decompress_area_obj_vram`) plus the object palette.

Remaining: port states 1-4 fully and the `$DEA2` behaviour dispatch, which need
the object integration helpers (`$808C14`/`$808CBF`) and the animation driver
(`$80F81D`) mapped first.

### Step 15 — Text format and BG3 font cracked (started)

The script turned out to be **plain ASCII** with two control codes, not a
custom encoding:

- `0x01` = line break inside a page,
- `0x02` = page break (next text box),
- `0x5D` (`]`) terminates the last line of a page.

Evidence: the guest's pause subscreen draws labels through BG3 with tile index
== ASCII code (a captured frame contains tiles `0x46 0x49 0x52 0x45` = "FIRE").
The intro/ending lines sit at `$81:8DF2`:

```
I NEVER<01>EXPECTED THAT<01>PHALANX WOULD<01>BE DEFEATED]]]<02>
IT IS WRITTEN<01>THAT THE ONE<01>WHO DEFEATS<01>THE KING<01>SHALL RULE]]]<02>
THE NEXT TIME<01>IT WILL BE MY<01>TURN]]]<02>
```

Done:

1. `tools/dc_extract/extract_text.py --rom <rom>` walks printable runs that
   contain control codes, splits them into pages/lines and writes
   `assets/generated/text_strings.json` (518 strings, with ROM addresses).
   Strings that are really font/glyph tables (`$81:A49E..A60C`) are included but
   harmless.
2. `tools/oracle/capture_menu.py` opens the guest pause subscreen (press Start
   on a save), dumps VRAM/CGRAM/PPU to `captures/menu/`.
3. `tools/dc_extract/extract_font.py` decodes the BG3 2bpp tiles 0x20..0x7E at
   the character base into `font.png` (white mask, 128x48, 86/95 glyphs) plus
   `font.json` (first/last/cell). The renderer tints it per context.
4. `$81:A49E..A60C` also contains runs like `"0123456789:;<=LMNO"` — a glyph
   order/character table worth inspecting when the text renderer needs the
   exact symbol layout.

5. `src/text.c` / `src/text.h`: `Text_LoadScript` parses `text_strings.json`
   (up to 64 strings, 6 pages, 8 lines), `Text_DrawString` draws 8x8 glyphs from
   `font.png` with a tint and `Text_DrawPage` paints a dialogue box. `Game_Render`
   shows page 0 of the first string behind `DC_DIALOGUE=1` (page advances every
   120 frames) as a dev hook; the regression suite runs without it.

Font CHR resolution: the BG3 character base **is** `$5000` words
(`bgTileAdr & 0xF << 12`); the earlier "garbage" was a measurement mistake and a
stale fade-in capture. A 24-candidate montage of the BG3 layer
(`captures/menu/base_montage.png`) shows "FIRE" rendering correctly at `$5000`.
The pause-menu font is a bold italic 8x8 design (glyph cells are full height),
so the renderer needs 2 px of leading: `Text_DrawPage` now advances
`TEXT_LINE_H = 10`, and `DC_DIALOGUE=1` renders the real intro page legibly.

Shop/NPC text wiring (round 3):

- `Text_Find(script, needle)` locates a script entry by substring, and the
  script is now loaded unconditionally (it is small): `src/game.c` caches the
  shop entries for `"BLACK LOTUS"`, `"NEED AN URN"` and `"VELLUM IS"`.
- The shop screen draws the authentic ROM box `WELCOME TO / "THE BLACK LOTUS" /
  WE CARRY ONLY THE / FINEST POTIONS` through `Text_DrawPage`, with GOLD placed
  below the measured box height, and failed purchases display
  `YOUR URN IS ALREADY FULL OF` / `YOUR VELLUM IS ALREADY FULL OF` for 120
  frames. Regression stays green.
- The ROM text uses `[` for an apostrophe (`DON[T`), `"`/`\` for quotes and
  `]` as the page terminator — worth normalising in the text renderer later.

Next: wire the dialogue state into the flow (intro before Area 0, ending
variants), index the script table by event, and normalise the `[`/`]` control
characters.

---

### Step 16 — Campaign load coverage (done)

`tools/check_areas.py` boots the native build once per `room_area<N>.json`
(`DC_AREA=N`, fixed frame budget, trace CSV) and validates: clean exit,
expected frame count, finite player coordinates inside the room bounds. A full
sweep of all **113 areas passes** (`ok=113`), and `run_regression.py` now runs a
12-area subset (`0,1,2,3,4,5,17,21,25,46,92,109`) on every CI run.

Note for sandboxed shells: the game process must be able to write the trace CSV
where the tool puts it — pass `--tmp <dir>` (or `DC_CHECK_TMP_DIR`) when only a
specific tree is writable.

### Step 17 — Ending narration from the ROM (done)

The ending screen no longer uses placeholder lines: `Game_Init` caches the
`"PHALANX WOULD"` script entry and `GAME_ENDING` paginates its three pages with
Enter (the last page returns to the title), drawn through `Text_DrawPage` with
the extracted BG3 font. The stats line and title stay as native UI. This is the
first story beat wired from ROM data to the campaign flow; the same mechanism
can drive the intro, boss intros and the other endings once their script entries
are identified.

---

### Step 18 — Room classification and safe transitions (done)

- `extract.py` factors `room_stats(doc)` and writes it for every area into
  `campaign.json["rooms"]` plus a `room_kinds` summary. Classification:
  `playable` (collision + BG1 geometry + walkable ground under the spawn),
  `partial` (boss arenas without block-table floors, cutscene stages),
  `empty` (neither collision nor BG1 tiles). Current split: **69 playable,
  44 partial, 0 empty** — and `tools/audit_campaign.py` now cross-checks its own
  classification against `campaign.json` and agrees on every room.
- The native side loads those kinds (`Assets_RoomKind`, `campaign.json`) and
  `Game_TransitionToArea` refuses rooms classified `empty`, so a data regression
  can no longer drop the player into a collision-free world.
- `check_areas.py` still passes 113/113 and the regression suite stays green.

Next uses of the metadata: filter the overworld stage list to playable rooms,
and drive cutscene/arena handling for `partial` rooms explicitly.

---

### Step 19 — Story dialogue state and crest narrative (done)

- New `GAME_DIALOGUE` state: dims the frame, draws a `Text_DrawPage` page and
  advances with Enter; after the last page it returns to `dialogue_return`
  (`GAME_CLEAR` or `GAME_TITLE`).
- `Game_CompleteArea` now shows the ROM's **"YOU HAVE FOUND THE CREST OF ..."**
  pages (Earth/Air/Water/Time, cached from `$BE:A599..A6F7`) before the Area
  Clear banner. Deterministic runs (`DC_RUN_FRAMES`) and `DC_NO_STORY` skip the
  beat so tests stay byte-stable.
- Text loading normalises the ROM's in-band symbols: `[` → `'`, `\` → `.` and
  `j` → a line break inside the page (the `j` split is done at load time, so
  pages keep their real line count).
- The script cap rose from 64 to 512 strings (the crest texts sit at index
  ~483 in ROM order). Since `Game` now embeds ~1.2 MB of script storage it is
  allocated on the heap in `main.c` instead of on the stack.
- `DC_STORY_TEST=<crest>` / `DC_STORY_PAGE=n` open a story page directly and
  `DC_STORY_DEBUG=1` prints the cached crest indices, for deterministic
  screenshots.
- Verified: story page renders the real Earth-crest text; regression PASS;
  `check_areas` 113/113; audit agrees with the extractor on every room.

Next: use `GAME_DIALOGUE` for boss intros (bank `$BE:B947..C128` has the Phalanx
dialogue pairs) and for the item-found messages (`$BE:A427..A533`).

---

### Step 20 — Fire-crest piece pickups (done)

- Crest-item objects (`0x49`/`0x24`/`0x2D`/`0x2E`) now grant the next
  **Fire-Crest element** (Buster → Tornado → Claw → Demon Fire) before their
  stat bonus, matching the ROM's own `"YOU GOT THE PIECE OF THE FIRE CREST
  CALLED ..."` pages (`$BE:A427..A533`). The pool raises
  `OBJECT_EVENT_FIRE_PIECE` and `Game_UpdateSimulation` opens `GAME_DIALOGUE`
  with that piece's pages (narrative runs only).
- `DC_FIRE_TEST=<1..4>` opens a piece page directly for screenshots, and the
  player trace CSV gained a `fire` column so the flow is assertable.
- Regression now walks into the area-3 piece and asserts `fire` goes 0 → 1.

Verification: page 2 renders `"TORNADO" / WITH IT YOU CAN JUMP` through the
normalised text path; regression PASS.

Next: boss-intro dialogue using the `$BE:B947..C128` Phalanx sets once their
area mapping is pinned down, and the intro before Area 0.

---

## 11. Long-term roadmap

| Phase | Goal | Status |
|---|---|---|
| 0 | Extraction proof: graveyard map + VRAM/CGRAM dump + static render | done |
| 1 | Engine skeleton: raylib, fixed 60 Hz, BG layers, input, camera | done |
| 2 | Firebrand core: Bank `$80` state machine, physics, slopes, animations | done: state machine, slopes, shot/spell/fireball |
| 3 | Combat and objects: fireball, `$1080` objects, enemy, `$85:C180` pickup, HUD | done: fireballs, object pool, urns, hearts, enemies, HUD |
| 4 | Vertical slice: full Area 1–3 scrolling rooms, transitions, title/menu, audio | **campaign foundation done**: 113 areas, transitions, bosses, crests, overworld/menus; audio placeholder |
| 5 | Full campaign: all areas, bosses, forms, Mode 7, password/save | in progress: all area rooms, crests/overworld/menus present; per-species OAM/AI, text, exact password/save and ROM audio remain |
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
| BG2 backdrop generator (WRAM `$7F`) | areas without a capture show a transparent background | one oracle frame per area in `tools/dc_extract/bg2_captures.json`; decode the generator later |
| Player state machine drift | feel drifts from `save0` | ported Bank `$80` state machine with 8.8 fixed point; `DC_DEMO=6`/`compare.py` regression keeps ≤1 px |
| Slope/ledge block attributes (`0x03..0x21, 0xa0`) | player steps on flat 16×16 tops instead of smooth hill heights (`y=160` vs `y=164`) | resolved: `$80:962B` surface table ported; `DC_DEMO=8` walks the hill |
| Projectile OAM overlap with palette-7 player group | fireball could stretch the player bbox | resolved for `save0`: projectile OAM is palette 6 and the extractor also windows around the object slot |
| BG2 HDMA parallax scroll splits | flat `scroll_x` on BG2 loses per-scanline sky/tree parallax | read HDMA table on channel 1 (`$210F` `BG2HOFS`) or approximate band scroll |
| Audio extraction | no soundtrack | BRR/DSP decoder or recorded development placeholder |
| Object AI breadth | huge scope | 26-slot pool + Area 1 list wired; species `$67` is a first-pass chase/damage/drop enemy, remaining species pending |
| Legal | distribution risk | no assets in repo; ROM-generated locally at install/setup time |
| GPL randomizer code | license contamination | reimplement formats from ROM disassembly; keep GPL tools separate |
| raylib flip quirks | visual corruption | pre-baked flips for BG; verify or pre-bake mirrored player frames |
| CGRAM restore timing | wrong sprite palettes | sample framebuffer; per-capture palette config |

Resolved questions (formerly open):

- **Object species dispatch layout**: `$82:8000` (main update), `$82:8180`
  (secondary), `$82:8300` (init) and `$82:8480` (aux) are 192-entry tables
  indexed by `species*2`; init entries are mostly 5-byte `JSL $83:xxxx; RTS`
  thunks (species `$67` → `$82:F382` → `$83:AF19`, update at `$83:AF74`).
- **BG2 backdrop storage**: generated at runtime, not in the ROM block tables;
  `$84:A057` DMAs it from WRAM `$7F` (see trap #21). The extractor consumes one
  oracle frame per area instead.
- **Per-area sprite list**: `$81:AE11` is a 16-bit pointer table to
  (metasprite group, frame count) pairs, one list per area, `0x00`-terminated
  (see trap #27). A cross-area consistency test proved it is the room's ambient
  animation list, not a species→group map. Object fields `$28/$2A/$2E` are not
  sprite pointers either: `$2A` is a map/offset table base used by the
  collision probe at `$80:91C7`. Per-species art is captured from live OAM via
  injected objects (Step 13).

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
- **Player movement parity with `save0`**: Resolved for the base ground/jump/
  hover loop. `src/player.c` runs the recovered substate flow in 8.8 fixed
  point; a 603-frame replay matches on X exactly and stays within 0.53 px on Y
  (the task-idle oscillation). Walk speed on flat ground is the `0x0166` cap,
  not a per-step ramp; the `0x00B3` first step belongs to the `$1070` slope
  path and is awaiting slope wiring.
- **Fireball shot timing, spawn offset and velocity**: Resolved from the `Y`
  capture. The press frame uses the idle pose, then a 6-frame and a 10-frame
  shot pose (17 total). The projectile spawns on the press frame in the first
  free `$1080..$1260` slot via `$80:F222`, at
  `(floor(player.x) + 3, floor(player.y) - 8)`, with `vx = 0x0300`
  (3.0 px/frame). The projectile OAM group is palette 6 and despawns at the
  camera edge in `save0`.
- **Area 1 full-room layout and slope heights**: Resolved. The ROM area grid
  is 8×2, but the graveyard row is the populated one (`screens 8, 9, 10, 11,
  16, 17, 18, 19`); the other row is all-zero BG1/collision padding. All 492
  referenced BG1 tiles are in the `stage_600` VRAM atlas. The `$80:962B` table
  gives surface offsets for `0x03..0x14`; `Level_FloorAt` snaps Firebrand to
  `block_top + offset`, so spawn is `y=164` on `attr=0x09` and the hill is
  walkable in both directions.
- **Area 1 object list and heart arc**: Resolved. The `$81:C874` pointer table
  gives a 6-byte record list; species `$57` is the urn (four in Area 1) and
  `$67` the enemy group. Urns drop a pickup using the `$85:C180` values
  (`0x0400` pop-up, `0x0040` gravity, `-0x0100` bounce decay); touching it
  increments HP. The HUD BG3 source is `save0` (not the HUD-less `stage_600`
  cinematic).

Remaining open technical questions:

- BG2 backdrop generator and HDMA tables for areas other than Area 1 (Area 1's
  `$84:AF65` bands are implemented; other areas use capture frames until the
  WRAM `$7F` generator path is decoded).
- Full object species ID (`$02`) → handler table (`$82:8000,X`) coverage;
  current AI is first-pass for skeletons/bats/gargoyles.
- Exact projectile-enemy/player-object hitbox, damage, knockback and drop
  tables from `$80:9C93` (current values are approximations).
- Per-species OAM/animation extraction from `$82:8300` init handlers.
- Mid-stage `$98:E000` CHR streaming and tile animation (`$81:C125`).
- Text/dialogue/cutscene encoding, exact password algorithm and retail SRAM
  format (the native JSON save format is implemented).
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
        BG2 capture pipeline, Windows build and campaign audit (2026-10-23)
        Save/options persistence, title CONTINUE and HUD bar fix (2026-10-23)
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
   Windows:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_windows.ps1
   ```
4. Verify assets exist:
   ```sh
   ls assets/generated/room.json assets/generated/player_idle.png \
      assets/generated/player_anim.json assets/generated/hud_bar.png
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
10. Run `./tools/run_regression.sh` (or `ctest --output-on-failure` in
    `build/`) before committing; it must pass (it now includes the save/load
    round-trip). Then run
    `python3 tools/audit_campaign.py` and compare its summary with the numbers
    recorded in section 9 (69 playable / 44 partial / 2 with BG2 as of
    2026-10-23).
11. Do not commit `assets/generated/`, `captures/` or `saves/`.

---

## 17. One-paragraph summary

The project has a working native C11 + raylib foundation that renders the
original graveyard room from extracted ROM data and runs Firebrand's authentic
Bank `$80` movement state machine in 8.8 fixed point. Disassembly of Bank `$80`
(`$80:DB00..F310`) and ROM level tables (`$81:A291`) resolved the major open
mysteries: `stage_600` is ROM Area 1 (`screens 8, 9` = `slots 9, 10`), not Area
2, which eliminates the 11 px spawn offset and proves the ROM block tables
match VRAM directly (`1957/2048` words) without any tile remap; `$85:C1D4` is a
bouncing heart pickup while Firebrand's true physics live in Bank `$80` (jump
`0x059C` with immediate `vy=0` cut on B release at `$80:E227`, gravity
`0x0038`, zero-G hover toggle at `State 0x06`); and `Y` (`$105C`) is the
Fireball shot button, spawning projectiles in `$1080..$1260` via the allocator
`$80:F222` (the earlier `$80:F1FE` label was a different helper), whereas `A`
(`$105E`) triggered the Talisman invocation pose. The port now matches `save0`
over a 603-frame ground/jump/hover replay (max delta `x=0.00 px`,
`y=0.53 px`), and the Y shot is implemented end to end: 17-frame spit pose,
7-slot fireball pool, palette-6 OAM stream, `(floor(player.x)+3,
floor(player.y)-8)` spawn and `0x0300` velocity, with pillar/viewport despawn.
Step 5 is also done: `--full-room` emits the populated Area 1 screen row as a
2048×256 BG1 map (the ROM grid's other row is empty padding) with a 128×16
collision grid, the `$80:962B` surface table gives smooth `0x03..0x14` slope
walking (spawn `y=164` on `attr=0x09`), BG2 wraps horizontally, and the camera
follows across the room with deadzones. Steps 6–7 add the 26-slot object pool
and the `$81:C874` Area 1 object list: four destructible species `$57` urns,
bouncing `$85:C180` heart pickups that heal HP, and a first-pass species `$67`
ghoul that chases, damages with knockback and drops hearts. The save0 BG3
status bar is baked as `hud_bar.png`/`hud_slots.png` and rendered with the
current HP bar and crest slots. Step 8 hardens the engine: a fixed 60 Hz
accumulator decoupled from display refresh, pre-baked mirrored player/fireball
textures, and a CTest/`tools/run_regression.sh` harness that re-runs demos
6/7/9/10/11/12 and asserts physics, projectile, pickup, enemy, Crest-form and campaign-transition invariants.
Step 9 now has an original procedural chiptune/SFX layer, a title screen,
pause overlay, and an Area Clear trigger at the graveyard exit. Step 10 moves
from the vertical slice to a **full-campaign foundation**: the extractor now
implements the ROM CHR decompressor, emits all 113 valid areas as 2D room
documents (multi-row/vertical grids, spawns, transitions, boss gates and
object lists), and bakes per-area BG atlases from ROM CHR/palettes. The OAM
metasprite decoder (`$8B:8000`) provides the real item/Crest and ghoul sprites;
Area 1 BG2 uses the recovered `$84:AF65` HDMA scanline bands. The runtime can
load any area, transition on bounding boxes or `$81:97DA` boss gates, run Area 0
Somulo and basic skeleton/bat/gargoyle encounters, and exposes six Gargoyle
forms (Earth dash, Air flight, Water swim, Time/Ultimate defense, Fire-Crest
elements, wall cling, headbutt), a Crest/Item subscreen, shop, password,
options, Game Over/endings and a Mode 7-style overworld. Remaining work is
fidelity: per-species OAM/AI, BG2/mid-stage CHR for other areas, text/cutscenes,
the exact password/save format and ROM audio.

The 2026-10-23 continuation added the three pieces the fidelity phase needs
before it can scale: (1) a per-area BG2 backdrop pipeline driven by
`tools/dc_extract/bg2_captures.json`, which bakes an oracle frame's BG2 tilemap
plus a capture-based atlas for that room — Areas 1 and 3 now render their real
backgrounds, and the guest-side generator (WRAM `$7F`, `$84:A057`) is
documented as the remaining reverse-engineering target; (2) `tools/audit_campaign.py`,
a structural audit over all 113 generated rooms (69 playable, 44 partial,
2 with BG2, 45 with at least one flagged issue — mostly zero-collision
boss/cutscene arenas), giving the project a repeatable CI gate instead of
ad-hoc inspection; and (3) an offline Windows build (`-DDC_RAYLIB_ROOT`,
`tools/build_windows.ps1`) verified with raylib 6.2 + MinGW GCC 16.2, so the
native port and the regression suite run on the same machine as the oracle.
The species dispatch tables (`$82:8000/$8180/$8300/$8480`, `JSL` init thunks)
and the debug server's `write_ram`/`dump_vram` state access were also mapped,
which is the starting point for the next content task: per-species OAM/AI.

A follow-up pass added native save/options persistence: `Game_SaveProgress` /
`Game_LoadProgress` write a versioned `saves/dc_save.json` (area, cleared
stages, HP, Crests/forms, Talismans, Urns, Spells, gold, volume) on area clear,
overworld entry and exit; the title gains a `CONTINUE` entry only when a save
exists; `DC_SAVE_PATH`/`DC_NO_SAVE`/`DC_SAVE_FORCE`/`DC_RESUME` cover tests.
The regression suite now autosaves in one run and asserts the resumed area,
max HP and gold in a second, and the HUD health bar was fixed to read as a
segmented red bar (the baked texture is a brightness mask, so the empty part is
tinted dark and the filled part red).
