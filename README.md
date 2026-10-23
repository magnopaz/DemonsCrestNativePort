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
- Extracted full graveyard starting room from ROM **Area 1** (`screens 8, 9` =
  bottom-row `slots 9, 10`), 2048×256 px across the populated 8-screen row,
  with the stone pillar (`cx=24`, `x=384..399`, `y=128..159`) beneath the urn
  at `(385, 111)`.
- BG1 comes from the ROM block tables for all eight screens; BG2 is the
  captured 64×32 forest backdrop and loops horizontally as the camera scrolls.
- Slope surface heights decoded from the `$80:962B` jump table: Firebrand
  walks up/down the `0x07..0x0e` hill, stands at `y=164` on `attr=0x09`, and
  the camera follows across the full room with horizontal and vertical
  deadzones.
- Player and item sprites extracted from OAM.
- Walk, jump, hover and fall driven by the authentic Bank `$80` state machine
  in 8.8 fixed point, with slope-aware floor collision and viewport bounds.
  `B + Up` flight is a debug-only extra (`DC_FLY_DEBUG=1`).
- Firebrand animation set extracted from `save0` (ROM Area 3, `screens 52, 53`):
  idle, 6-pose walk cycle, jump rise, hover/fly, fall, landing, 17-frame Y
  fireball spit and 31-frame A spell.
- Bank `$80` (`$80:DB00..F310`) player state machine and constants ported:
  - walk speed `$1020`: flat ground keeps the `0x0166` cap (1.398 px/frame);
    the `0x00B3` first step and `$80:8B85` ramp belong to the `$1070` terrain
    path and are not wired to the new slope surface table yet,
  - jump initial velocity `$1022 = 0x059C` (5.609 px/frame; `0x02CE`
    underwater), with immediate `vy = 0` cut on B release while rising
    (`$80:E227`),
  - gravity `$1026 = 0x0038` (0.21875 px/frame²; `0x002A` underwater), terminal
    fall `0xFA00` (-6.0 px/frame),
  - mid-air hover (`State 0x06` at `$80:E1A3` / `$80:E29F`): pressing B in air
    zeroes `$1022` (`vy = 0`) and bypasses gravity; pressing B again returns to
    falling (`State 0x04`). (`$85:C1D4` was identified as the dropped heart
    bounce routine, not player jump.)
- Fireball shot implemented end to end:
  - 7-slot projectile pool (`src/projectile.c`) matching `$1080..$1260`,
  - spawn on the Y press frame at `floor(player.x) + 3`,
    `floor(player.y) - 8` with velocity `0x0300` (3.0 px/frame),
  - palette-6 OAM stream baked as `fireball_0..8.png` + `fireball_anim.json`,
  - despawn on solid walls or camera viewport exit, mirrored for left shots.
- Object system (`src/object.c`, 26 slots) driven by the Area 1 `$81:C874`
  object list:
  - four destructible urns (`species $57`) at the listed positions,
  - fireball hits shatter urns and drop a heart with the `$85:C180` arc
    (4.0 px/frame pop-up, 0.25 gravity, decreasing bounces),
  - touching a heart increments HP up to max,
  - first-pass `species $67` ghouls emerge, chase, damage on contact with
    knockback and drop hearts when killed by fireballs.
- BG3 HUD extracted from `save0`: six-point health bar plus three crest slots
  at the top of the frame; the active spell slot is highlighted.
- Saved-state validation from `save0` (normal graveyard gameplay):
  - walk speed measured 1.40 px/frame,
  - default SNES bindings (`$80:DB00` via `$1E3A..$1E4C`): **B = Jump/Hover**,
    **Y = Fireball shot** (spawns in `$1080..$1260` via `$80:F222`), **A =
    Talisman/Headbutt** (`State 0x16` / `0x1A`), **X = Subscreen**,
  - B tap is an ~11 px / 14-frame hop; holding B gives the full jump arc,
  - 603-frame `DC_DEMO=6` replay (validated before the full-room terrain
    change): max delta `x=0.00 px`, `y=0.53 px`.
- Debug collision overlay (`F1` or `DC_DEBUG_COLLISION=1`), FPS (`F3`).

This is a playable technical foundation, not a finished game. The next
milestone is engine hardening: fixed timestep, automated regression, real
enemy/HUD art and audio.

### Current limitations

- BG2 parallax is currently a flat 1:1 offset with horizontal wrapping. The
  original uses HDMA channel 1 on `$210F` for per-scanline BG2 scroll; that
  table is not extracted yet.
- The ROM Area 1 grid is 8×2, but the second grid row is all-zero padding for
  the graveyard row. The extractor emits the populated 8-screen row
  (2048×256 px), not a 2048×512 canvas.
- The `$1070` terrain probe that selects the `0x00B3` initial walk step and
  `$80:8B85` acceleration path is not wired to the slope surface table yet;
  flat/slope walking keeps the `0x0166` cap.
- Hurt/death player animations and the real species `$67` handler
  (`$83:AF74`) are still to be ported; the current ghoul is a first-pass
  chase/damage/drop implementation with placeholder art. The heart sprite and
  HUD active-slot marker are also placeholders.
- No audio yet.
- The HUD health bar is scaled from the extracted six-point bar; talisman
  inventory/selection is not implemented yet, so the first slot is highlighted.

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
  --full-room \
  --out assets/generated
```

`--full-room` builds every screen in the captured 8-screen row (2048×256 px
for Area 1) from the ROM block tables; omit it for the old two-screen
256×224 viewport slice.

Outputs:

- `room.json` — full-row BG1 tilemap, looping BG2 backdrop, slope collision
  grid, spawn, camera/play bounds.
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
`player_land_0..1.png`, `player_attack_0..2.png`, `player_spell_0..2.png`,
`fireball_0..8.png`, `heart_00..01.png`, `enemy_00..01.png`,
`hud_bar.png`, `hud_slots.png`, `player_anim.json` and
`fireball_anim.json`.
The engine falls back to `player_idle.png` if the player JSON is absent, and
skips the HUD if `hud_bar.png`/`hud_slots.png` are absent.

## 2c. Validate Step 3 movement against `save0`

```sh
python3 tools/oracle/capture_anim.py \
  --recomp /path/to/DemonsCrestSNESRecomp \
  --rom "/path/to/Demon's Crest (USA).sfc" \
  --save /path/to/DemonsCrestSNESRecomp/saves/save0.sav \
  --segments tools/oracle/segments_save0_physics.json \
  --out captures/phys

python3 tools/oracle/export_capture_csv.py captures/phys -o /tmp/oracle_phys.csv

DC_DEMO=6 DC_RUN_FRAMES=603 DC_TRACE_CSV=/tmp/native_demo6.csv \
  ./build/demonscrest   # xvfb-run -a when headless

python3 tools/oracle/compare.py /tmp/native_demo6.csv /tmp/oracle_phys.csv \
  --oracle-start 0 --threshold 1.0
```

Expected: 603 frames compared, max delta `x=0.00 px`, `y=0.53 px`.

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
| Jump / hover | Z or Space | B (right face) | **B** (`$1E3A`) |
| Fly upward (debug only) | Z + Up | B + Up | Crest-dependent |
| Fireball shot | X or J | X / Y (west/top) | **Y** (`$1E3C`) |
| Talisman / Headbutt | C or K | A (south) | **A** (`$1E3E`) |
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
| `DC_DEMO=5` | Animation regression: walk, special, jump, fly |
| `DC_DEMO=6` | Exact `save0` Step 3 physics replay (603 frames) |
| `DC_DEMO=7` | Fireball demo: right shots into the pillar, long left shot |
| `DC_DEMO=8` | Full-room traversal: walk left, then hover right across pillars |
| `DC_DEMO=9` | Urn → bouncing heart → heal regression |
| `DC_DEMO=10` | Enemy chase/damage/drop regression (pair with `DC_START_X`) |
| `DC_START_X` / `DC_START_Y` | Override the player spawn for focused tests |
| `DC_TRACE_OBJECTS=path` | Write active object slots as CSV |
| `DC_TRACE_PROJECTILES=path` | Write per-frame active projectile slots as CSV |
| `DC_FLY_DEBUG=1` | Enable the non-canonical B+Up vertical flight |
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
src/anim.c            player_anim.json / fireball_anim.json loaders
src/projectile.c      7-slot fireball pool
src/object.c          26-slot urn/enemy/pickup pool
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
