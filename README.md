# Demon's Crest — Native Fan Game

A native C11 + raylib foundation for a Demon's Crest fan game. The original
game is used as the reference: level/object data and graphics are extracted
from the player's own ROM, and the recompilation project is used as a
frame-accurate oracle for movement and timing.

> **No ROM data is distributed with this project.** The `assets/generated/`
> and `captures/` directories are git-ignored. You must own Demon's Crest
> (USA) and generate them locally.

**Full project state, recovered technical details, traps and next steps:
[`docs/PROJECT_STATE.md`](docs/PROJECT_STATE.md).**

## Current state

- 256×224 render target, integer-scaled, 60 Hz update.
- Extracted graveyard starting room from ROM **Area 1** (`screens 8, 9` =
  bottom-row `slots 9, 10`), with aligned collision floor (`by=10`,
  `y=160..175`) and stone pillar (`cx=24`, `x=384..399`, `y=128..159`) directly
  beneath the urn at `(385, 111)`.
- BG1 + BG2 tilemaps rendered from the captured VRAM (`rom.screen_tilemap` for
  Area 1 `screens 8, 9` matches the loaded BG1 VRAM viewport on `1957/2048`
  words identically — no tile remap exists).
- Player and item sprites extracted from OAM.
- Walk, jump, hover and fly with collision shape masking (`attr & 0x1F`) and
  viewport bounds.
- Firebrand animation set extracted from `save0` (ROM Area 3, `screens 52, 53`):
  idle, 6-pose walk cycle, jump rise, hover/fly, fall, landing and attack.
- Bank `$80` (`$80:DB00..F310`) player state machine and constants recovered
  from the ROM:
  - walk speed `$1020`: `0x00B3` (0.699 px/frame initial step) → `0x0166`
    (1.398 px/frame cap),
  - jump initial velocity `$1022 = 0x059C` (5.609 px/frame; `0x02CE`
    underwater), with immediate `vy = 0` cut on B release while rising
    (`$80:E227`),
  - gravity `$1026 = 0x0038` (0.21875 px/frame²; `0x002A` underwater), terminal
    fall `0xFA00` (-6.0 px/frame),
  - mid-air hover (`State 0x06` at `$80:E1A3` / `$80:E29F`): pressing B in air
    zeroes `$1022` (`vy = 0`) and bypasses gravity; pressing B again returns to
    falling (`State 0x04`). (`$85:C1D4` was identified as the dropped heart
    bounce routine, not player jump.)
- Saved-state validation from `save0` (normal graveyard gameplay):
  - walk speed measured 1.40 px/frame,
  - default SNES bindings (`$80:DB00` via `$1E3A..$1E4C`): **B = Jump/Hover**,
    **Y = Fireball shot** (`State 0x12`, spawns projectile in `$1080..$1260` via
    `$80:F1FE`), **A = Talisman/Headbutt** (`State 0x16` / `0x1A`), **X =
    Subscreen**,
  - B tap is an ~11 px / 14-frame hop; holding B gives the full jump arc.
- Camera fixed to the captured viewport (see limitations).
- Debug collision overlay (`F1` or `DC_DEBUG_COLLISION=1`), FPS (`F3`).

This is a playable technical foundation, not a finished game. The next
milestones are porting the exact Bank `$80` state machine into `src/player.c`
and capturing the `Y`-button fireball shot and projectile from `save0`.

### Current limitations

- The camera is locked to the captured 256×224 viewport while `room.json` holds
  a single 64×32 VRAM snapshot. Because Area 1's ROM block tables match VRAM
  directly (`1957/2048` words), unlocking full-room scrolling only requires
  emitting the full 8×2 screen map from `rom.area_info(1)` and wrapping BG2
  parallax horizontally.
- `src/player.c` still uses float jump-boost/hover approximations; porting the
  recovered Bank `$80` state machine (`0x059C` jump with B-release `vy=0` cut,
  `0x0038` gravity, `State 0x06` zero-G hover toggle) is the next step.
- The current `attack` clip was captured from `A` (`State 0x16` back-turned
  Talisman pose); the `Y`-button Fireball spit (`State 0x12`), projectile
  sprite (`$80:F1FE`), hurt, death and wall-cling clips are still to be
  captured from `save0`.
- No enemies, HUD or audio yet.
- Collision masks `attr & 0x1F` and treats every non-zero shape as a flat
  16×16 solid; decoding the per-pixel slope height formulas at `$80:962B`
  (`0x02..0x1b`) will let Firebrand walk smoothly up the graveyard hill and
  stand at `y=164` on `attr=0x09` instead of snapping to `y=160`.

## Requirements

- CMake 3.20+
- A C11 compiler
- Python 3 with Pillow
- Demon's Crest (USA), SHA-256
  `18d40a807d5f88c5b6a1ad849eec7e0f189225d9a1586037c850f6680b5844de`

raylib is fetched automatically at configure time. On Fedora the desktop build
also needs the X11/GL development headers:

```sh
sudo dnf install libXrandr-devel libXinerama-devel libXcursor-devel \
  libXi-devel libX11-devel mesa-libGL-devel alsa-lib-devel
```

## 1. Capture a reference frame

Use the recompilation project's trace build. The `gameplay.script` route reaches
the first playable area at frame 43659 and dumps the machine 600 frames later.

```sh
RECOMP_DIR=/path/to/DemonsCrestSNESRecomp \
ROM="/path/to/Demon's Crest (USA).sfc" \
./tools/capture_graveyard.sh
```

Or run it by hand:

```sh
cd /path/to/DemonsCrestSNESRecomp
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
SNESRECOMP_DUMP_DIR=/path/to/DemonsCrestNativePort/captures/graveyard \
./build-trace/DemonsCrestSNESRecomp "/path/to/Demon's Crest (USA).sfc" \
  --script scripts/routes/gameplay.script
```

Copy/rename the four files the extractor needs:

```text
captures/graveyard/stage_600.vram.bin
captures/graveyard/stage_600.cgram.bin
captures/graveyard/stage_600.oam.bin
captures/graveyard/stage_600.regs.json
```

The `dump` command writes the framebuffer too; it is useful for visual
comparison but the extractor does not need it.

## 2. Extract assets

```sh
python3 tools/dc_extract/extract.py \
  --rom "/path/to/Demon's Crest (USA).sfc" \
  --dump captures/graveyard \
  --prefix stage_600 \
  --area 1 \
  --screens 9,10 \
  --out assets/generated
```

Outputs:

- `room.json` — 64×32 BG1/BG2 tilemaps, collision grid, spawn, camera.
- `bg_pal0..7.png` — 4bpp BG tile atlases.
- `obj_pal0..7.png` — 4bpp OBJ tile atlases.
- `player_idle.png` — Firebrand composed from OAM.
- `item_00.png` — the visible item composed from OAM.
- `manifest.json` — ROM identity and capture provenance.

## 2b. Capture and extract Firebrand animations

The trace build exposes a 6000-frame per-frame state ring plus a TCP debug
server. `capture_anim.py` starts it paused, resumes a copy of `save0.sav`, and
drives absolute frame targets with held buttons. It then saves OAM, CGRAM,
registers and VRAM for each frame.

```sh
python3 tools/oracle/capture_anim.py \
  --recomp /path/to/DemonsCrestSNESRecomp \
  --rom "/path/to/Demon's Crest (USA).sfc" \
  --save /path/to/DemonsCrestSNESRecomp/saves/save0.sav \
  --out captures/anim

python3 tools/dc_extract/extract_anim.py \
  --dump captures/anim \
  --out assets/generated \
  --clean
```

This writes `player_idle_0.png`, `player_walk_0..5.png`,
`player_jump_0..2.png`, `player_hover_0.png`, `player_fall_0..1.png`,
`player_land_0..1.png`, `player_attack_0..2.png` and `player_anim.json`.
The engine falls back to `player_idle.png` if the JSON is absent.

## 3. Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/demonscrest
```

Use `--assets <dir>` to load assets from another directory.

## Controls

| Action | Keyboard | Gamepad | Original SNES (`$80:DB00`) |
|---|---|---|---|
| Move | Arrows / WASD | D-pad | D-pad |
| Jump / hover / fly | Z or Space | B | **B** (`$1E3A`) |
| Fly upward (hold) | Z + Up | B + Up | Crest-dependent |
| Attack | X or J | A / Y | **Y** = Fireball (`$1E3C`), **A** = Talisman/Headbutt (`$1E3E`) |
| Collision overlay | F1 | — | — |
| FPS | F3 | — | — |

## Development hooks

These environment variables exist for deterministic testing:

| Variable | Effect |
|---|---|
| `DC_RUN_FRAMES=N` | Run N updates then exit (used for screenshots) |
| `DC_SCREENSHOT=name.png` | Save the last presented frame (raylib writes it in the working directory) |
| `DC_TRACE_CSV=path` | Write per-frame player/camera state as CSV |
| `DC_DEMO=1` | Walk right, jump, then fly (long demo) |
| `DC_DEMO=2` | Walk right into the pillar, jump and land (short demo) |
| `DC_DEMO=3` | Mirrors the physics probe timing |
| `DC_DEMO=4` | Tap-jump vs hold-jump comparison |
| `DC_DEMO=5` | Animation regression: walk, attack, jump, fly |
| `DC_DEBUG_COLLISION=1` | Enable the `F1` collision overlay in headless runs |
| `DC_LAYER_MASK=1\|2\|3` | Draw BG2 only / BG1 only / both |
| `SNESRECOMP_LAYER_MASK` | On the recomp side, isolate BG/OBJ layers for comparison |

Example:

```sh
DC_DEMO=2 DC_RUN_FRAMES=120 DC_TRACE_CSV=/tmp/native.csv ./build/demonscrest
```

## Layout

```
src/                  native engine
src/anim.c            player_anim.json loader and clip data
tools/dc_extract/     extraction from ROM + dump (room and animation frames)
tools/capture_graveyard.sh  recomp dump helper
tools/oracle/         WRAM/OAM trace parsers, probe routes, comparison and
                      debug-server animation capture
captures/             per-run recomp dumps (git-ignored)
assets/generated/     extracted assets (git-ignored)
docs/PROJECT_STATE.md full handoff: state, findings, traps, next steps
docs/ROADMAP.md       next milestones
```

The larger feasibility study is in the recompilation repository at
`docs/NATIVE_PORT_PLAN.md`.

## Legal

The engine and tools are original code. The ROM, extracted graphics, maps,
music and any other game data remain the property of Capcom. Do not commit or
distribute the generated assets. For a public release, either require the
player's own ROM at generation time or replace the original art/music with
licensed or original assets.
