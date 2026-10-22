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
- Extracted graveyard starting room from area 2.
- BG1 + BG2 tilemaps rendered from the captured VRAM.
- Player sprite extracted from OAM.
- Walk, jump, hover and fly with a first-pass collision map.
- Movement constants recovered from the original player code:
  walk 1.398 px/frame, gravity 0.25 px/frame², terminal fall 6.0 px/frame.
- Saved-state validation from `save0` (normal graveyard gameplay):
  walk speed measured 1.4 px/frame; **B is jump**, A is attack; a tap-jump
  is an ~11 px / 14-frame hop, holding B extends it. The native jump is
  variable-height (tap vs hold).
- Camera fixed to the captured viewport (see limitations).
- Debug collision overlay (`F1`).

This is a foundation, not a finished game. The next milestone is matching the
player physics against recomp traces.

### Current limitations

- The camera is locked to the captured 256×224 viewport. The VRAM tilemap is a
  ring buffer for the current view, not a full-room layout; wider scrolling
  needs either a multi-view capture or the loader's tile remap.
- The player uses the idle sprite for every state. Walk/jump/hover frames come
  from the animation capture step.
- No enemies, combat, HUD or audio yet. Those are the next milestone.
- Collision treats every non-zero block attribute as solid. The attribute bits
  (slopes, water, one-way ledges) are refined against oracle traces.

## Requirements

- CMake 3.20+
- A C11 compiler
- Python 3 with Pillow
- Demon's Crest (USA), SHA-256
  `18d40a807d5f88c5b6a1ad849eec7e0f189225d9a1586037c850f6680b5844de`

raylib is fetched automatically at configure time.

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
  --area 2 \
  --out assets/generated
```

Outputs:

- `room.json` — 64×32 BG1/BG2 tilemaps, collision grid, spawn, camera.
- `bg_pal0..7.png` — 4bpp BG tile atlases.
- `obj_pal0..7.png` — 4bpp OBJ tile atlases.
- `player_idle.png` — Firebrand composed from OAM.
- `item_00.png` — the visible item composed from OAM.
- `manifest.json` — ROM identity and capture provenance.

## 3. Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/demonscrest
```

Use `--assets <dir>` to load assets from another directory.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move | Arrows / WASD | D-pad |
| Jump / hover / fly | Z or Space | B |
| Fly upward (hold) | Z + Up | B + Up |
| Attack (reserved) | X or J | A |
| Collision overlay | F1 | — |
| FPS | F3 | — |

## Development hooks

These environment variables exist for deterministic testing:

| Variable | Effect |
|---|---|
| `DC_RUN_FRAMES=N` | Run N updates then exit (used for screenshots) |
| `DC_SCREENSHOT=name.png` | Save the last presented frame (raylib writes it in the working directory) |
| `DC_TRACE_CSV=path` | Write per-frame player/camera state as CSV |
| `DC_DEMO=1` | Walk right, jump, then fly (long demo) |
| `DC_DEMO=2` | Walk right, jump and land (short demo) |
| `DC_LAYER_MASK=1\|2\|3` | Draw BG2 only / BG1 only / both |
| `SNESRECOMP_LAYER_MASK` | On the recomp side, isolate BG/OBJ layers for comparison |

Example:

```sh
DC_DEMO=2 DC_RUN_FRAMES=120 DC_TRACE_CSV=/tmp/native.csv ./build/demonscrest
```

## Layout

```
src/                  native engine
tools/dc_extract/     extraction from ROM + dump
tools/capture_graveyard.sh  recomp dump helper
tools/oracle/         WRAM/OAM trace parsers, probe routes and compare tool
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
