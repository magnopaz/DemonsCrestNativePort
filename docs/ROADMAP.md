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

## Physics constants recovered

The generated player code at bank `80:8D03` (vertical integrator) and the jump
initializer at `85:C1D4` gave the first exact constants:

| Value | Original | Native |
|---|---|---|
| max walk speed | `$1020 = 0x0166` → 1.398 px/frame | 1.398 |
| jump velocity | `$22 = 0x0400` → 4.0 px/frame (held) | 2.0 tap + hold boost to 4.0 |
| gravity | `$26 = 0x0040` → 0.25 px/frame² | 0.25 |
| terminal fall | `0xFA00` → 6.0 px/frame | 6.0 |

The vertical routine is `$33 += $22; $22 -= $26` with the fall clamp at
`0xFA00`. These are now in `src/player.c`.

`save0` is a clean normal-gameplay state. Measured there:

- world X moves 14 px in 10 frames → walk speed 1.40 px/frame;
- **B** is jump, **A** is attack;
- a tap-jump lifts the feet ~11 px over ~14 frames, so the native jump is
  variable-height (tap = short hop, hold = higher/hover).

## Measurement tooling (done)

1. **Native trace output**
   - `DC_TRACE_CSV=path` writes
     `frame, x, y, vx, vy, state, camera_x, camera_y`.

2. **Recomp oracle trace**
   - `save0` is a clean normal-gameplay state; the area-2 landing route is a
     cinematic and must not be used for physics probes.
   - OAM traces are the reliable visual position source; the `$1000` task Y
     oscillates during idle.
   - The debug server's 6000-frame state ring now also drives animation
     capture (`tools/oracle/capture_anim.py`).

3. **Diff harness**
   - `tools/oracle/compare.py` aligns the two CSVs and reports the first
     divergent frame.
   - Acceptance remains: ≤1 px / 1 frame divergence over 600 frames.

4. **Port the real player state machine** (remaining)
   - Disassemble the pad-reading routine and the state transitions.
   - Replace the guessed constants with the original fixed-point values.
   - Keep the native update frame-based; no delta time in gameplay.

## Next

1. **Attack projectile**
   - Capture from a save where the shot is visible (`save2`).
   - Fireball spawn frame/offset, velocity, lifetime, collision, hit reaction.

2. **Camera and room transitions**
   - Match the original deadzone/scroll behavior.
   - Capture adjacent viewports or implement the loader's tile remap so the
     camera can leave the initial 256×224 capture without stale edges.

3. **Combat and objects**
   - Object list spawn/update/cull; one graveyard enemy.
   - Damage, knockback, pickup and the HUD.

4. **Audio and presentation**
   - Placeholder/recorded track for development.
   - Decide the public-release path: user-generated assets or replacement art.

## Done recently

1. **Animation extraction**
   - Captured OAM/VRAM/CGRAM per frame through the debug-server state ring.
   - Extracted idle, 6-pose walk, jump rise, hover/fly, fall, landing and
     attack clips with per-frame anchors and frame durations.
   - Added `src/anim.c` and a frame-based animation state machine.

## Guardrails

- Never commit `assets/generated/` or `captures/`; they are ROM-derived.
- Do not tune physics by feel when a trace can answer the question.
- Keep the engine frame-based and deterministic; no float delta time in logic.
- One routine at a time: port, trace, compare, then move on.
