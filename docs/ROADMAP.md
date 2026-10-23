# Roadmap

Status: playable graveyard slice with physics, slopes, fireballs, objects and HUD.

## Done

- Standalone CMake + C11 + raylib project.
- ROM verification and area-2 extraction from a recomp dump.
- 256×224 render target, integer scaling, 60 Hz update.
- BG1/BG2 tilemaps with per-palette flip atlases.
- OAM-composed player and item sprites.
- Walk, jump, hover, fireball shot, first-pass collision and viewport bounds.
- Deterministic demo input for screenshots (`DC_DEMO=1|2|5|6|7`).
- Debug collision overlay (`F1`), FPS (`F3`).
- Animation capture through the recomp debug server's state ring.
- Firebrand animation frames: static idle, 6-pose walk cycle, jump rise,
  hover, fall, landing, Y fireball spit and A spell. Frame-based animation
  state machine.
- Attack and landing timers; fly/jump state flicker fixed.
- Authentic Bank `$80` player state machine ported to 8.8 fixed point
  (`src/player.c`): grounded, jump takeoff substates, B-release jump cut,
  gravity, terminal fall, hover toggle and SNES Y/A input mapping.
- Movement validated against `save0`: 603-frame trace, max delta
  `x=0.00 px`, `y=0.53 px` (`DC_DEMO=6`).
- Fireball projectile pool (`src/projectile.c`): 7 slots, original spawn
  offset/velocity, palette-6 OAM extraction, pillar and screen-edge despawn.
- Full 2048×256 Area 1 room from the populated ROM screen row (`--full-room`),
  with a 128×16 collision grid and horizontally looping BG2.
- `$80:962B` slope-surface table: Firebrand walks up/down `0x03..0x14` slopes
  and spawns at `y=164` on `attr=0x09`.
- Camera deadzone scrolling across the full room; `DC_DEMO=8` exercises it.
- 26-slot object pool (`src/object.c`) with data-driven Area 1 object list:
  destructible urns (`species $57`), bouncing hearts (`$85:C180` arc) and a
  first-pass species `$67` ghoul that chases, damages and drops hearts.
- BG3 status bar extracted from `save0`: health bar + three crest slots, wired
  to `$1062`/`$1E50` HP and drawn at the top of the frame.

## Physics constants and Bank `$80` state machine recovered

Disassembly of Bank `$80` (`$80:DB00..F310`) recovered Firebrand's true state
machine and fixed-point constants (note: `$85:C1D4` was proved to be the
bouncing heart pickup routine, not Firebrand's jump):

| Value | Original (`$80:DB00..F310`) | Ported `src/player.c` |
|---|---|---|
| initial walk step | `$1020 = 0x00B3` (`$80:F0BE`) → 0.6992 px/frame | `PLAYER_WALK_INITIAL_FX`; slope path, awaiting the `$1070` probe |
| max walk speed | `$1020 = 0x0166` (`$80:F0D0`) → 1.3984 px/frame | exact `0x0166` Q8.8; flat ground keeps the cap (matches `save0`) |
| jump velocity | `$1022 = 0x059C` (`$80:E170`) → 5.6094 px/frame | `-0x059C` y-down; substates 0→2→4 before integration |
| variable jump cut | `$80:E227`: if rising (`$23 >= 0`) and B released (`$59 == 0`), `$22 = 0` | `vy_fx = 0` on the first substate-4 frame with B released |
| gravity | `$1026 = 0x0038` (`$80:E070`, `$80:ED11`) → 0.21875 px/frame² | exact `0x0038` Q8.8 |
| hover (`State 0x06`) | `$80:E1A3` / `$80:E29F`: B in air zeroes `$22 = 0`, bypasses gravity | `PLAYER_HOVER`, `vy_fx = 0`; B press enters/exits (`State 0x04`/`0x04`) |
| terminal fall | `0xFA00` (`$80:8D03`) → -6.0 px/frame | exact `0x0600` y-down |

Pad mapping at `$80:DB00..DB40` (via WRAM masks `$1E3A..$1E4C`) and `save0`:

- **B** (`$1E3A` → `$1059`/`$105A`) = Jump / Hover toggle;
- **Y** (`$1E3C` → `$105B`/`$105C`) = Fireball shot (spawns projectile via `$80:F222`);
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
   - `tools/oracle/export_capture_csv.py` turns a `capture_anim.py` directory
     into the oracle CSV shape; `tools/oracle/segments_save0_physics.json` is
     the repeatable walk/tap-jump/hold-jump/hover route.
   - Latest Step 3 result: 603 frames, max delta `x=0.00 px`, `y=0.53 px`.

## Next (ordered by dependency)

1. **Engine hardening, regression harness & audio**
   - Add a fixed-timestep 60 Hz accumulator in `main.c`, verify or pre-bake
     left-facing player frames, and add an automated `compare.py` regression
     test script.
   - Tighten the first-pass enemy into the real species `$67` handler
     (`$83:AF74`) and replace placeholder heart/enemy art with ROM OAM.
   - Add development audio and finalize the legal public-release asset path.

2. **Flow, content & polish**
   - Room transitions, title/password/menu flow, remaining Area 1–3 rooms,
     bosses, crest forms and the Mode 7 overworld.

## Done recently

1. **Object system, urn, heart pickup & HUD (Steps 6–7)**
   - `src/object.c` implements a 26-slot pool and loads the `$81:C874` object
     list for Area 1: four destructible urns (`species $57`) and four species
     `$67` enemies.
   - Urns shatter on fireball contact and drop a pickup with the `$85:C180`
     arc: initial 4.0 px/frame up, gravity 0.25, bounce decay 1.0 per landing.
     Touching it increments HP up to max.
   - The first-pass enemy emerges, chases within 220 px, damages on contact
     with knockback/invulnerability, dies to fireballs and drops a heart.
   - `extract_anim.py` bakes the save0 BG3 HUD (`hud_bar.png`,
     `hud_slots.png`); `render.c` draws the 6-point health bar, three crest
     slots and highlights the active spell slot.
   - `DC_DEMO=9` exercises urn → heart → heal; `DC_DEMO=10` with
     `DC_START_X` exercises enemy chase, damage and drop.

2. **Full-room Area 1 map, slopes & camera scrolling (Step 5)**
   - `extract.py --full-room` emits the populated 8-screen Area 1 row as a
     2048×256 BG1 map and a 128×16 collision grid. The ROM grid's second row is
     all-zero padding, so it is omitted and recorded in `manifest.json`.
   - All 492 referenced BG1 tiles are already in the `stage_600` VRAM atlas; no
     CHR decode or extra capture was required.
   - Decoded the `$80:962B` surface table (`0x03..0x14`) and added
     `Level_FloorAt`/`Level_SurfaceOffset`; spawn is `y=164` on `attr=0x09`
     and the `0x07..0x0e` hill is walkable both ways.
   - Unlocked camera bounds and added horizontal/vertical deadzones; BG2 loops
     horizontally. `DC_DEMO=8` walks left then hovers right across pillars.
   - Remaining polish: BG2 HDMA per-scanline parallax and the `$1070` walk-speed
     ramp are not wired yet.

3. **Y Fireball shot & projectile pool (Step 4)**
   - Captured `Y` in `save0`: the grounded shot animation is 17 frames
     (idle press frame + 6-frame pose A + 10-frame pose B), and the projectile
     spawns on the press frame in object slot `$1080`.
   - Recovered the spawn/velocity values: X = `floor(player.x) + 3`,
     Y = `floor(player.y) - 8`, horizontal velocity `0x0300` (3.0 px/frame).
   - `extract_anim.py` now separates the palette-6 projectile OAM group from
     the palette-7 player body, emits `fireball_0..8.png` +
     `fireball_anim.json`, and exports the real `Y` clip as `attack` while
     keeping the `A` clip as `spell`.
   - `src/projectile.c` implements the 7-slot pool (`$1080..$1260`, stride
     `$50`); shots despawn on the stone pillar or when they leave the camera
     viewport. `DC_DEMO=7` exercises right shots into the pillar and a
     long left-facing shot; `DC_TRACE_PROJECTILES=path` logs every active slot.

4. **Bank `$80` player state machine ported (Step 3)**
   - `src/player.c` now stores positions, velocities and walk speed as 8.8
     fixed point and follows the recovered grounded / jump-substate / hover
     flow (`$80:E0EE`, `$80:E1DA`, `$80:E29F`).
   - Jump takeoff holds position on the press frame, integrates through
     substates 0→2→4, then applies the `$80:E227` B-release cut on the first
     substate-4 frame.
   - Hover is `State 0x06`: B in air zeroes `vy`, freezes vertical motion and
     bypasses gravity; the next B press returns to `State 0x04` and falls.
   - `Y`/`X`/`J` (gamepad west/top) trigger the Fireball shot input; `A`/`C`/
     `K` (gamepad south) remain Talisman/Headbutt. Step 4 later replaced the
     placeholder shot clip with the captured Y animation.
   - `save0` validation via `DC_DEMO=6` and
     `tools/oracle/segments_save0_physics.json`: 603 frames, max delta
     `x=0.00 px`, `y=0.53 px` (threshold 1 px). The residual Y delta is the
     known `$1000` task-idle oscillation, not a physics divergence.

5. **Graveyard Area 1 extraction & collision alignment**
   - Updated `tools/dc_extract/extract.py` default `--area` to `1` (`screens 8,
     9` = `slots 9, 10`) and regenerated `assets/generated/room.json`.
   - Masked block collision shapes by `attr & 0x1F` (`$80:8FDB`) so `0xA0`
     environment blocks are ignored as air.
   - Fixed `Player_Init` ground snapping (`y=160` instead of `y=176`) and fixed
     `land_timer` so it only triggers on airborne-to-grounded transitions.
   - Verified stone pillar (`cx=24`, `x=384..399`, `y=128..159`) and floor
     (`by=10`, `y=160..175`) alignment in `physics_check.png`.

6. **Animation extraction**
   - Captured OAM/VRAM/CGRAM per frame through the debug-server state ring.
   - Extracted idle, 6-pose walk, jump rise, hover/fly, fall, landing, Y shot,
     A spell and fireball clips with per-frame anchors and frame durations.
   - Added `src/anim.c` and a frame-based animation state machine.

## Guardrails

- Never commit `assets/generated/` or `captures/`; they are ROM-derived.
- Do not tune physics by feel when a trace can answer the question.
- Keep the engine frame-based and deterministic; no float delta time in logic.
- One routine at a time: port, trace, compare, then move on.
