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
- [x] `tools/run_regression.py` green under `DISPLAY=:99`.

## M2 — BG2 backdrop for every mapped area

- [x] Cross-platform capture helpers:
  `tools/oracle/dump_save_wram.py`, `tools/oracle/capture_area_frame.py`.
  (The old `capture_save_frames.py` hard-codes `M:\Opencode` and integer saves;
  a relative `--resume-state` is resolved against the recomp cwd and silently
  fails, so every path handed to the host must be absolute.)
- [ ] Fresh `fNNNNNN.*` for all 31 manifest saves (`captures/save*`, `nb/save*`).
- [ ] Rewrite `tools/dc_extract/bg2_captures.json` frames to the captured ones.
- [ ] Re-extract; assert every entry has a non-empty BG2 tilemap.
- [ ] Screenshot spot-checks (`DC_AREA`, `DC_RUN_FRAMES`, `DC_SCREENSHOT`).

## M3 — Oracle screenshot comparison harness

- [ ] Script that renders a native room headless and a `cmd_screenshot` oracle
      frame for the same save, then reports a pixel delta.
- [ ] Use it to gate M2 and later AI/art work.

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
