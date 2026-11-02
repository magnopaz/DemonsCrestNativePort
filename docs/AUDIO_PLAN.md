# Audio: playing the ROM's music and sound effects

Status: **not started**. `src/audio.c` synthesises an original chiptune theme and
seven SFX; no ROM audio data is used. This document is the plan and the recon, so
the work can start cold.

## What "correct" means here

- the colosseum theme (and every other stage/town track) played from the ROM's own
  music data, not an imitation;
- the original SFX: jump, land, wing flap, fireball, urn break, pickup, hurt,
  enemy hit, boss roar, gate, menu;
- the same trigger points the original uses (the port already fires events for
  most of them — `OBJECT_EVENT_BREAK`, `OBJECT_EVENT_ENEMY_HIT`, etc.).

## The hard part, stated plainly

The SNES soundtrack lives in SPC700 code plus BRR samples driven by the S-DSP.
There is **no** audio data you can just read out and play: the music *is* a
program for the APU. Three routes:

1. **Emulate the APU** (SPC700 + DSP) and feed it the ROM's audio program. Most
   faithful, and it is what the original hardware does. Big: an SPC700 core, the
   DSP, and the IPL/boot protocol; or vendoring an existing core such as
   `snes_spc` / `game-music-emu`.
2. **Render offline, ship audio files.** Produce `.spc` dumps of each track, render
   them to OGG/WAV with an existing player, and stream the files in-game. Cheap in
   engine code, still the ROM's real audio, and the rendering step is scripted
   like every other asset. Requires a way to obtain the `.spc` dumps.
3. **Re-author the music natively** from the sequence data (BRR samples + a small
   sequencer). Most work, most control, highest risk of sounding "almost right".

**Recommendation: route 2 first, route 1 only if the dumps prove unobtainable.**
Route 2 gets "correct music" into the game with a fraction of the risk, and the
asset pipeline already regenerates everything from the ROM.

## Recon (this machine, checked)

- No emulator or SPC tool is installed: `mednafen`, `snes9x`, `bsnes`, `higan`,
  `zsnes`, `spc2wav`, `spcplay`, `gme`, `vgmplay`, `sox` are all absent. `ffmpeg`
  is present but does not read SPC.
- **The oracle does emulate the APU** — this is the important find. It vendors a
  full SPC700 + DSP:
  - `snesrecomp/runner/src/snes/apu.c` with `apu_saveload`, `apu_cycle`,
    `apu_cpuRead/Write` — so a save state carries APU RAM and DSP state;
  - `snesrecomp/runner/src/snes/spc.c` and `src/spc_player.h` — an SPC player
    interface the framework drives;
  - `snesrecomp/cosim/apu_region_diff.py` — tooling that already diffs APU regions.

  So the music can be captured from the guest itself, which makes route 2 cheap:
  dump the APU RAM + SPC/DSP registers out of a save state at the moment a track
  starts, assemble a `.spc` (v0.30: 64 KB RAM + registers), and render it to
  OGG/WAV with the oracle's own DSP or with `game-music-emu` built from source.
- The port's audio is a single `src/audio.c` with an event-driven SFX table and a
  procedural music loop — the replacement should keep that event API and only
  change what produces the samples.

## Plan for the next session

1. **Decide the dump route** (30 min): check whether the recomp has any APU code;
   if not, pick an emulator that can dump `.spc` non-interactively (or build
   `game-music-emu` and use a public `.spc` set for the game — the ROM is owned,
   but a rip is a rip; prefer dumping it ourselves from the user's own ROM).
2. **Prove one track** (the colosseum): get a `.spc`, render it to OGG, load it in
   `raylib` (`LoadMusicStream`), and play it in area 0 in place of the procedural
   loop. This is the spike: if it plays and loops seamlessly, the rest is
   repetition.
3. **SFX**: extract the BRR samples the game uses for the seven existing event
   triggers, or render each SFX from the same dump; play them through the existing
   event API.
4. **Wire the rest of the soundtrack** to the campaign's stage list
   (`assets/generated/room_names.json` has all 34 maps), with the same
   regenerate-from-ROM rule as every other asset: audio files stay git-ignored and
   `tools/build_assets.sh` produces them.
5. **Verify**: an audio regression that asserts each stage loads its track and the
   events fire (no ear test), plus one manual listen.

## What the port already gives us

- `src/audio.c` with an event API and volume options.
- `DC_NO_AUDIO=1` for deterministic runs and `DC_AUDIO_DEBUG=1` to print device
  and music status — the hooks a regression needs.
- The stage → track mapping can be derived from `room_names.json` (34 maps) once
  the track list is known.
