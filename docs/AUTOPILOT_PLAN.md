# Autopilot plan — finishing the original campaign

Status file for the autonomous continuation work. Ordered by dependency; every
item ends with a verification command, not a claim.

Environment (this machine):
- ROM: `~/Projetos/opencode/roms/Demon's Crest (USA).sfc`
- Oracle: `~/Projetos/opencode/DemonsCrestSNESRecomp/build-trace/DemonsCrestSNESRecomp`
  (trace build, debug server; 100+ named save states in `build-trace/saves/`)
- Headless display: `Xvfb :99` (export `DISPLAY=:99`)
- Assets are regenerated with `tools/dc_extract/extract.py` (ROM + one capture)

## M1 — Verified baseline (done 2026-11-01)

- [x] Configure + build Release (`build/demonscrest`).
- [x] Graveyard capture `captures/graveyard/stage_600.*` from the oracle.
- [x] `extract.py` → 113 `room_areaN.json`, 573 asset files, 55 MB.
- [x] `capture_anim.py` + `extract_anim.py` → player/fireball/HUD clips.
- [x] `tools/run_regression.py` **PASS** under `DISPLAY=:99` (exit 0):
      demos 6/7/9/10/11/12/13, boss data + runtime, boss defeat, multi-area loads.
      The suite started this session with 14 failures; the fixes that closed it
      are recorded in `docs/PROJECT_STATE.md` and the commits of 2026-11-01.
- [x] Post-extraction pipeline made reproducible: `tools/build_assets.sh`
      (extract → anim → font/endings → `apply_boss_table` → `apply_npc_table`
      → `apply_door_table`). Verified end to end: wiped and rebuilt
      `assets/generated` from ROM + `captures/graveyard` + `captures/anim`,
      then `run_regression.py` → **PASS (exit 0)** on the rebuilt tree.
      113 rooms, 28 with a non-empty BG2 after the rebuild.

## M2 — BG2 backdrop for every mapped area

- [x] Cross-platform capture helpers:
  `tools/oracle/dump_save_wram.py`, `tools/oracle/capture_area_frame.py`.
  (The old `capture_save_frames.py` hard-codes `M:\Opencode` and integer saves;
  a relative `--resume-state` is resolved against the recomp cwd and silently
  fails, so every path handed to the host must be absolute.)
- [x] Fresh `fNNNNNN.*` for 30 of the 31 manifest saves, manifest frames rewritten.
- [x] Re-extract with 30 captured backdrops (area 48 shows the night sky behind
      the dungeon; area 20 captured separately after a transient debug-server
      failure).
- [ ] **Area identification is unsolved.** `WRAM $008D` is a stage/room id, not
      the extractor's area index (`lvl_I_murkland-cemetery` reads 2 but its BG1
      matches area 1 at 1.000). `identify_area.py` matches BG1 exactly but ties
      on areas that share screen data (1 vs 73). Until a tie-break exists, the
      curated `bg2_captures.json` mapping cannot be extended safely.
- [ ] Screenshot spot-checks: area 48 renders with a backdrop, but BG1 shows
      black bands and misaligned bricks (separate extraction defect).
- [ ] Screenshot audit (30 areas, `/tmp/shots/shot_<area>.png`): black-pixel
      fraction ranges from 0.7% (area 14) to 91% (area 20). Areas above ~75%
      (20, 34, 38, 84, 98, 102, 104) render as a lone platform in a void.
      **Not an atlas problem** (only 4–10% of referenced tiles are blank) and
      **not a BG2 problem alone**: `tools/oracle/compare_rooms.py` puts the
      oracle frame beside `DC_AREA=1` and they are completely different rooms,
      while the default `room.json` (same graveyard, hand-extracted from
      `stage_600`) renders the full scene with trees, torches, water and the
      green statue. So the campaign `room_areaN.json` BG1 data is only correct
      for the screens the reference capture actually covered; the rest of the
      grid is filler/zero. Both `room.json` and `room_area1.json` can be
      "area 1"; only one of them is dressed.
- [ ] `identify_area.py`: empty blocks made every area tie at 1.000. It now
      ignores blocks that are <60% non-zero, but areas 1 and 73 still tie at
      1.000 on real data — they appear to share identical screen blocks, so a
      tie-break by world size or object list is still required.
- [ ] Screenshot spot-checks (`DC_AREA`, `DC_RUN_FRAMES`, `DC_SCREENSHOT`).

## M3 — Oracle screenshot comparison harness

- [x] `tools/oracle/compare_rooms.py`: oracle `screenshot` + native
      `DC_AREA/DC_SCREENSHOT`, brute-force shift alignment, side-by-side PNG and
      `compare.json` (mean abs diff). First result: area 1 native vs the
      cemetery save = MAD 26.45 at a 94 px shift, and the images are different
      rooms — see the M2 note.

## M4 — Enemies and bosses

- [ ] Bake the 17 object-list species with `capture_species.py` /
      `extract_species.py` (`$57 $81 $10 $67 $07 $2C $96 $26 $2D $08 $01 $09
      $A5 $2E $24 $33 $49`).
- [ ] Port the real `$83:AF74` species `$67` ghoul states 1–4 and the `$DEA2`
      behaviour dispatch.
- [ ] Port the remaining handlers in the `$82:8000` table, most-used first.
- [ ] Authentic player-object hitboxes (`$80:9C93`), damage, knockback, drops.

## M5 — Player forms

- [ ] Capture per-form OAM sets and animation tables (Earth, Air, Water, Time,
      Ultimate) from the oracle; replace palette-tint placeholders.
- [ ] Hurt/death clips.

## M6 — Audio

- [ ] Decode the SPC700 driver's sample directory; extract BRR instruments.
- [ ] Either re-sequence the tracks from the ROM sequence data or finish an
      original soundtrack with per-area/boss/menu tracks; wire SFX ids.

## M7 — Story and presentation

- [ ] Intro/cutscene sequencing (scene camera, timing, per-event script index).
- [ ] Ending artwork and credits.
- [ ] Town NPCs / shop dialogue / Headbutt minigame.

## M8 — Retail save and password

- [ ] Decode the retail 10-letter password encode/decode and the SRAM layout.

## M9 — Release

- [ ] Full campaign playthrough check without dev hooks.
- [ ] Windows build + CI; packaging notes; asset generation one-liner.

## Guardrails

- Never commit `assets/generated/` or `captures/` (ROM-derived).
- Do not tune physics by feel: use an oracle trace.
- Keep the simulation frame-based and deterministic.
- Every milestone commit must keep `tools/run_regression.py` green.
