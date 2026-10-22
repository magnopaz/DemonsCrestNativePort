# Roadmap

Status: playable graveyard slice with Firebrand animations.

## Done

- Standalone CMake + C11 + raylib project.
- ROM verification and area-2 extraction from a recomp dump.
- 256×224 render target, integer scaling, 60 Hz update.
- BG1/BG2 tilemaps with per-palette flip atlases.
- OAM-composed player and item sprites.
- Walk, jump, hover, fly, first-pass collision and viewport bounds.
- Deterministic demo input for screenshots (`DC_DEMO=1|2|5`).
- Debug collision overlay (`F1`), FPS (`F3`).
- Animation capture through the recomp debug server's state ring.
- Firebrand animation frames: static idle, 6-pose walk cycle, jump rise,
  hover, fall, landing and attack. Frame-based animation state machine.
- Attack and landing timers; fly/jump state flicker fixed.

## Physics constants and Bank `$80` state machine recovered

Disassembly of Bank `$80` (`$80:DB00..F310`) recovered Firebrand's true state
machine and fixed-point constants (note: `$85:C1D4` was proved to be the
bouncing heart pickup routine, not Firebrand's jump):

| Value | Original (`$80:DB00..F310`) | Current `src/player.c` | Target |
|---|---|---|---|
| initial walk step | `$1020 = 0x00B3` (`$80:F0BE`) → 0.6992 px/frame | `accel = 0.28` | `0x00B3` + `$80:8B85` ramp |
| max walk speed | `$1020 = 0x0166` (`$80:F0D0`) → 1.3984 px/frame | `1.398` | `0x0166` (`1.3984375`) |
| jump velocity | `$1022 = 0x059C` (`$80:E170`) → 5.6094 px/frame | `2.0` tap + `0.10` boost to `4.0` | `0x059C` (`0x02CE` underwater) |
| variable jump cut | `$80:E227`: if rising (`$23 >= 0`) and B released (`$59 == 0`), `$22 = 0` | boost stops on B release | immediate `vy = 0` on B release |
| gravity | `$1026 = 0x0038` (`$80:E070`, `$80:ED11`) → 0.21875 px/frame² | `0.25` (from heart `$85:C1D4`) | `0x0038` (`0.21875`) |
| hover (`State 0x06`) | `$80:E1A3` / `$80:E29F`: B in air zeroes `$22 = 0`, bypasses gravity | `1.05` sink cap while holding B | toggle `State 0x06` (`vy = 0`) on B press |
| terminal fall | `0xFA00` (`$80:8D03`) → -6.0 px/frame | `6.0` | `0xFA00` (`-6.0`) |

Pad mapping at `$80:DB00..DB40` (via WRAM masks `$1E3A..$1E4C`) and `save0`:

- **B** (`$1E3A` → `$1059`/`$105A`) = Jump / Hover toggle;
- **Y** (`$1E3C` → `$105B`/`$105C`) = Fireball shot (`State 0x12`, spawns projectile via `$80:F1FE`);
- **A** (`$1E3E` → `$105D`/`$105E`) = Talisman/Spell (`State 0x16`) or Headbutt (`State 0x1A`);
- **X** (`$1E4C` → `$80:DC12`) = Item/Crest subscreen;
- In `save0`, world X moves 14 px in 10 frames (1.40 px/frame) and a B tap lifts
  the feet ~11 px over ~14 frames (`0x059C` initial velocity cut to `0` on B
  release under `0x0038` gravity).

## Measurement tooling (done)

1. **Native trace output**
   - `DC_TRACE_CSV=path` writes
     `frame, x, y, vx, vy, state, camera_x, camera_y`.

2. **Recomp oracle trace**
   - `save0` (ROM Area 3, `screens 52, 53`) is a clean normal-gameplay state;
     the `stage_600` landing route (ROM Area 1, `screens 8, 9`) is a cinematic
     and must not be used for physics probes.
   - OAM traces are the reliable visual position source; the `$1000` task Y
     oscillates during idle.
   - The debug server's 6000-frame state ring drives deterministic animation
     capture (`tools/oracle/capture_anim.py`).

3. **Diff harness**
   - `tools/oracle/compare.py` aligns the two CSVs and reports the first
     divergent frame.
   - Acceptance remains: ≤1 px / 1 frame divergence over 600 frames.

## Next (ordered by dependency)

1. **Port the real Bank `$80` player state machine (`$80:DB00..F310`) (next)**
   - Replace the float jump-boost/hover approximations in `src/player.c` with
     the recovered Bank `$80` constants (`0x00B3 → 0x0166` walk, `0x059C` jump
     with B-release `vy = 0` cut at `$80:E227`, `0x0038` gravity, `State 0x06`
     zero-G hover toggle on second B press).
   - Map `Y` (`$105C`) to Fireball shot and validate movement against `save0`
     with `tools/oracle/compare.py`.

2. **Capture `Y` Fireball shot in `save0` & implement projectiles (`$80:F1FE`)**
   - Drive `y` in `capture_anim.py` on `save0` (`A` captured `State 0x16`
     Talisman pose, which is why no projectile appeared; `save2` is not needed).
   - Separate projectile OAM sprites from Firebrand's palette-7 body group in
     `extract_anim.py` so the moving shot does not stretch the player bbox.
   - Implement the 7-slot player projectile pool (`$1080..$1260`, stride `$50`)
     with original spawn frame, offset, 8.8 velocity and wall/pillar despawn.

3. **Full-room Area 1 tilemap, slope attributes & camera scrolling**
   - With `--area 1`, `rom.screen_tilemap(0, 8..9)` matches `stage_600` VRAM
     `1957/2048` words directly (no tile remap exists).
   - Build the full 8×2 Area 1 BG1 map (2048×512 px) and 128×32 collision grid
     from the ROM block tables (`$81:A291`, `$8F`, `$92`, `$95`, `$98`), wrap
     BG2 parallax horizontally, decode slope heights (`0x02..0x1b` at
     `$80:962B`), and unlock camera deadzone scrolling.

4. **Combat, `$1080` object table, one enemy & `$85:C180` pickup**
   - Port the 26-slot `$1080..$1D50` object loop (`$82:8611`), `$82:8BDB` area
     spawner (`$81:C874`), one graveyard enemy species, and the already-decoded
     `$85:C180..C250` bouncing heart pickup (heals `$1062` up to `$1E50`).

5. **HUD, engine hardening & audio**
   - Extract BG3 status bar (`$5800` map, `$5000` 2bpp tiles) wired to `$1062` /
     `$1E50` HP.
   - Add a fixed-timestep 60 Hz accumulator in `main.c`, verify or pre-bake
     left-facing player frames, and add an automated `compare.py` regression
     test script.
   - Add development audio loop and finalize legal public-release asset path.

## Done recently

1. **Graveyard Area 1 extraction & collision alignment**
   - Updated `tools/dc_extract/extract.py` default `--area` to `1` (`screens 8,
     9` = `slots 9, 10`) and regenerated `assets/generated/room.json`.
   - Masked block collision shapes by `attr & 0x1F` (`$80:8FDB`) so `0xA0`
     environment blocks are ignored as air.
   - Fixed `Player_Init` ground snapping (`y=160` instead of `y=176`) and fixed
     `land_timer` so it only triggers on airborne-to-grounded transitions.
   - Verified stone pillar (`cx=24`, `x=384..399`, `y=128..159`) and floor
     (`by=10`, `y=160..175`) alignment in `physics_check.png`.

2. **Animation extraction**
   - Captured OAM/VRAM/CGRAM per frame through the debug-server state ring.
   - Extracted idle, 6-pose walk, jump rise, hover/fly, fall, landing and
     attack clips with per-frame anchors and frame durations.
   - Added `src/anim.c` and a frame-based animation state machine.

## Guardrails

- Never commit `assets/generated/` or `captures/`; they are ROM-derived.
- Do not tune physics by feel when a trace can answer the question.
- Keep the engine frame-based and deterministic; no float delta time in logic.
- One routine at a time: port, trace, compare, then move on.
