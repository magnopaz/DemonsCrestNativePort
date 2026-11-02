# Audio: playing the ROM's music and sound effects

Status: **music is real**. The colosseum theme (`music_boss.wav`, area 0) is the
ROM's own audio, played through `LoadMusicStream` and looping at its measured
period. Sound effects are still the procedural stand-ins — that is the next
step.

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

1. **Emulate the APU** (SPC700 + DSP) and feed it the ROM's audio program.
2. **Render offline, ship audio files.**
3. **Re-author the music natively** from the sequence data.

## What was done (2026-11-02)

### The dump: the oracle's APU snapshots cleanly

`spc_dump <path>` (debug server) already existed: it writes a standard `.spc`
v0.30 (64 KB ARAM + 128 DSP registers + SPC700 registers + a synthesized
`$F0-$FF` IO region). `tools/audio/dump_spc.py` drives it deterministically —
resume a save paused, run N frames, snapshot — and records provenance: the
save's and ROM's SHA-1, the frame, and **the guest's area byte (`$008D`)** so a
dump cannot be silently attributed to the wrong room. Verified on
`somulo_player_control.sav`: area 0, `music.spc` = 66,048 bytes,
`spc_pc=0x046A`.

### The trap: a bare `.spc` does not survive this game's driver

`tools/audio/spc_render.c` renders a `.spc` offline with the oracle's own
SPC700 + S-DSP cores and can trace the SPC's PC stream. Result: after a bare
resume the driver plays ~2 s and then executes data, ending in a `STOP` (0xFF)
at `$FFB5`.

Measured evidence:

- the first PCs are the driver's main loop (`$046A` port poll → `$02FC` timer
  accumulate → `BEQ $02F9`), with timer 0 running (target 64, counter
  advancing) — so the load itself is right;
- the derail is not a bad snapshot: six dumps at different frames all die the
  same way, and replaying the CPU→APU port writes captured by `audio_events`
  does not save it;
- the **live** game never executes that `STOP`: `SNESRECOMP_SPC_TRACE=1` over
  600 frames reports no HALT.

Conclusion: this driver needs the CPU behind it (it polls `$F4` and takes
branches on the game's command handshake), so a `.spc` resumed with no CPU is
not a viable render source here. Whether a full SPC player (game-music-emu,
which models timer IRQs) would fare better is **untested** and is the first
thing to try if the offline path is ever needed again — the tool and the dump
are kept for exactly that.

### The route that works: the oracle's own DSP output

`audio_wav <path> [start] [count]` dumps the DSP's native output ring (32040 Hz
stereo, the S-DSP rate). That is the emulated console's own samples, which is
what the plan allows and what "real audio" means here.

`tools/audio/capture_music.py` records it deterministically:

- resume the save, settle, then **neutralise the object pool** (`$1080`,
  stride `$50`, 26 slots — zero the active byte of every slot). Area 0's save
  is mid-boss-fight, so a naive capture is full of roars and hits; with the
  enemies gone the capture is music only (peak 25,121 → 11,043, RMS steady at
  ~1,778) and no SFX are triggered.
- run 120 s, then pull the last N seconds from the ring.

### The loop is measured, not guessed

`tools/audio/wav_loop.c` finds the period with a 10 ms RMS-envelope
correlation, refines it in the sample domain, then snaps the cut point to where
the end flows into the start most cleanly.

Measured on the colosseum capture:

| value | measurement |
|---|---|
| period | 1,311,739 samples = **40.941 s** (envelope correlation 0.880; the 20.47 s peak is its half) |
| seam jump | **210.5** vs a typical adjacent-sample step of **415.1** → 0.51× |
| asset | `assets/generated/audio/music_boss.wav`, 32,040 Hz stereo, RMS 1,778, peak 9,502 |

A seam smaller than an ordinary sample step is why the loop has no click — the
cut does not need a crossfade, and the check refuses a cut whose seam exceeds
4× the typical step.

### In the port

`src/audio.c` loads `<assets>/audio/<track>.wav` for each `MusicTrack` and
falls back to the procedural loop only when the file is absent. `Music.looping`
is on, and a headless run with a null ALSA PCM shows the stream clock crossing
the boundary (`t=40.875 → 0.553`), i.e. raylib restarts the track at the exact
sample boundary.

Debug hooks: `DC_AUDIO_DEBUG=1` prints which file each track loaded from
(`ROM` vs `procedural`), and `DC_AUDIO_PROBE=1` prints
`audio: probe track= rom= time= playing=` every 60 frames.

### Verification (no ear test)

`tools/check_audio_assets.py`, run by `tools/run_regression.py` (77 = no
assets, i.e. skip):

1. **asset** — manifest hash, 32,040 Hz stereo, non-silent, frame count, loop
   length in range, seam ratio ≤ 4×;
2. **playback** — a headless run at the track's area logs
   `music_boss.wav ... (ROM)` and a probe line with `rom=1`, `playing=1`, and a
   stream clock that advanced;
3. **determinism** — the same 600-frame demo with audio forced on and with
   `DC_NO_AUDIO=1` produces **byte-identical** trace CSVs, so audio cannot
   perturb the simulation.

### Reproducing it

```sh
tools/audio/build_audio_tools.sh         # spc_render + apu_host
python3 tools/audio/dump_spc.py --save <save> --out captures/audio/<name>   # .spc artifact
python3 tools/audio/make_music_asset.py  # capture -> loop -> asset + manifest
python3 tools/check_audio_assets.py      # the three checks above
```

`tools/build_assets.sh` runs the music step (reusing `captures/audio/` when
present, capturing from the oracle otherwise, and skipping cleanly when neither
exists); `--no-audio` disables it. Track specs live in
`tools/audio/music_tracks.json`. Every generated file stays git-ignored.

## Route 1 investigation: can the port run the driver itself? (2026-11-02, S1)

Yes in principle, and the pieces are now identified — but **not from a snapshot**.

### The upload is the CPU's, and the game does not declare an HLE hook

`recomp/bank00.cfg` has no `hle_spc_upload` directive, so Demon's Crest uploads
the sound driver through the plain IPL byte-by-byte protocol, driven by guest
code. There is no tidy "block stream in the ROM" to lift: the CPU half of the
transfer is real work the console did.

Measured on a cold boot (`tools/audio/boot_capture.py`):

| observation | measurement |
|---|---|
| driver in ARAM | by frame 20 (`$046A` region non-zero) |
| idle point at frame 60 | PC `$048C`/`$0470` (the `$046A` port-poll loop) |
| first upload | 2001 CPU writes between samples 561 and 1346 (~0.8 s) |
| next CPU→APU traffic | none until sample ~68,700 |
| second upload (music bank) | 1999 writes at sample ~68,746 (frame ~128) |
| after that | **no CPU→APU writes at all**, and music plays |

Two consequences: the driver is self-sufficient during playback (no per-frame
commands), and everything the port needs to reproduce is the two uploads.

### Booting an embedded APU from the clean image works

`tools/audio/apu_host` (new) boots the oracle's SPC700 + DSP from a captured
ARAM image and an entry PC. From the frame-60 image, entry `$046A`:

```
aram f0060 entry 0x046A: rendered 20.00 s, peak 0/0 rms 0.0/0.0, halted_at=-1
```

No halt and no sound — correct: the driver is idling and no music bank is
loaded. The core, the image and the loader are fine.

### But mid-song `.spc` snapshots derail, and the DSP is the trigger

Every mid-song snapshot (colosseum fight, title screen with music) derails in
the embedded APU after ~13k instructions, ending in a `STOP` executed from data.
Isolating the input:

| render | result |
|---|---|
| `.spc` with all 128 DSP registers | derails (executes data, `STOP` at `$FE83`/`$FFB5`) |
| `.spc`, DSP registers skipped (`DC_SPC_NO_DSP=1`) | no halt, silent |
| `.spc`, DSP registers loaded but echo disabled (`DC_SPC_NO_ECHO=1`) | no halt, music plays ~5 s then goes quiet (rms ~1.9) |

So the driver's behaviour depends on the **DSP's echo state**, not only on ARAM
and the register image. A `.spc` cannot carry the DSP's per-voice and echo
state, which is why resuming one hijacks the driver's control flow — the same
reason the earlier six-dump sweep failed.

### What the port's APU host must do (next step)

1. **Boot**: upload the driver image (a captured post-boot ARAM is enough — it
   runs forever) and jump to the entry.
2. **Load a song**: replay the bank upload *with the handshake*. A raw replay of
   the captured writes produces silence, because the IPL transfer is
   request/acknowledge: the CPU writes a byte and waits for the SPC's answer on
   `$2142`/`$2143` before the next one. The oracle's RTL already models this
   (`apu_waitForTransferReady`, `apu_finishHleTransfer`) and is the reference to
   port.
3. **Play**: nothing else — playback needs no commands, so SFX become the only
   runtime traffic (their ids are in the captured stream).

Success criterion for the next session: `apu_host` renders 60 s of the
colosseum theme from the embedded APU, A/B against `music_boss.wav`, without a
halt and without echo tricks.

Tools added for this: `apu_host.c` (boot + render + port replay + ARAM dump),
`boot_capture.py`, `press_probe.py`, `aram_compare.py` (live-vs-offline ARAM
diff), `capture_upload.py`, and the `--aram`/`--entry`/`--ports`/`--port-offset`
flags they use.

### Where the capture of that handshake stands

Two blockers, both measured:

- The driver's code region is in the ROM **linearly**, not as a length/target
  block stream: ARAM `$02C0..$0DA0` is byte-identical to file `0x3010C..0x30BEC`
  (constant delta `0x2FE4C`), and the bytes right after that region are not a
  terminator. So there is no header to parse — a "boot from ROM" path would copy
  that region directly, and the *bank* would have to come from elsewhere.
- The CPU->APU writes cannot be captured through `audio_events`: that command
  reads the general event ring, and the DSP's register writes (thousands per
  second) push the transfer out of it within a second. A dedicated port ring
  exists (`AUDIO_TRACE_PORT_RING`, 65536 events, written to `.apu.tsv` by
  `state_dump.c`) but **no debug-server command exposes it**, and adding one
  needs the oracle to build — currently broken by unrelated widescreen work
  (`src/dc_widescreen.c` calls `PpuSetWidescreenLayerRightEdge`, which the
  submodule no longer has).

So the next session has two clean options, in order of preference:

1. expose the port ring (a `port_events <path>` command next to `spc_dump`) once
   the oracle builds again, capture the bank transfer, and drive it with the
   acknowledge-waiting replay that `apu_host --handshake` already implements;
2. or lift the bank directly: diff ARAM before/after the upload (the probe
   already dumps both) and write that region into the embedded APU at boot,
   which skips the protocol entirely but needs the driver's post-transfer state
   to be reachable — that is what the frame-133 ARAM image is for.

## Still to do

1. **Route 1, step 2**: the handshake upload above. Until it works the rendered
   assets are the shipping path, and this is the difference between 5 MB per
   track and 64 KB of ARAM.
2. **SFX**: the seven event triggers still synthesise. On route 1 they come for
   free (same driver, same voices); until then, the capture-window approach or
   reading the BRR sample the driver keyed on are the options.
3. **A track per area**, not per game state: `src/game.c` currently picks
   `MUSIC_TITLE/STAGE/BOSS/ENDING`. `room_names.json` (34 maps) is the table to
   extend, with `music_tracks.json` gaining one entry per area that has its own
   theme. Capturing needs a save state in each area (312 exist in the oracle's
   `build-trace/saves/`).
4. **Format at campaign scale**: one 40 s WAV is 5 MB; 30 tracks would be
   ~150 MB. FLAC keeps the sample-exact loop and halves that; OGG is smaller but
   its encoder delay/padding makes a sample-exact loop harder. If route 1 lands,
   this question disappears.
