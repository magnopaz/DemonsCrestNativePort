# Plan — Original *Demon's Crest* Fully Playable

**Status:** planning document, 2026-10-23
**Scope:** take the native port from "campaign loop verified with debug hooks" to
"the original 1994 game can be played start to finish with real navigation, all
bosses, all items, the four endings and no placeholder art in normal play".
**Why now:** the fan-game concept (`docs/demon_s_crest_sequel_game_concept_storyline.txt`)
reuses this engine: a prequel cutscene battle against the living Somulo, then the
original campaign, then the sequel story. That only works if the original is a
solid, complete base.

---

## 1. Where the project is today

Evidence is from the automated checks on the current build (regression PASS).

| Area | State | Evidence |
|---|---|---|
| Engine | C11 + raylib, 256x224 RT, 60 Hz fixed step, deterministic harness | `src/main.c`, `DC_RUN_FRAMES` |
| Rooms | **113/113 load cleanly** | `check_areas.py` |
| Campaign flow | title → Area 0 → clear → overworld → stage travel → boss clear → ending | `check_playthrough.py` 6/6 |
| Boss stages | **7/7 boss entries clear** | `check_campaign.py` |
| Overworld | real Mode 7 world map (1024x1024), rotation, zoom, Firebrand cursor, stage overlay for 66 entries | Step 41/42/43 |
| Text | 518 script strings extracted, BG3 font, paged dialogue, YES/NO conversations, shop/town NPC lines | Steps 38/48 |
| Endings | 4 variants (Bad/Normal/Good/True) with transcribed narration, regression-checked | Step 39 |
| Audio | 4 procedural loops (title/stage/boss/ending), SFX | `audio.c` |
| Save | native save + password codec + pause save | regression |
| Backdrops (BG2) | **14 areas**: 0,2,3,4,6,8,10,12,14,16,34,48,58,74 | `bg2_captures.json` |
| Object placements | 148 enemy placements across only **7 species**; 14 boss objects (`$33` x1, `$A5` x4, `$96` x9); 77 urns; 14 crest pickups; 5 platforms | room JSONs |
| Boss roster | Somulo `$33` (1 room), Arma `$A5` (4 rooms), `$96` (9 rooms — ending scenes/NPCs) | room JSONs |
| Sprite sheets | 11 species with captured sheets (`$07,$08,$0C?`…) — the rest use silhouettes | `species_anim.json` |
| Navigation | only **4 rooms** have explicit `exit` objects; 95 rooms auto-advance via `gate_next_area` after clear | room JSONs |

**Reading of the state:** the *loop* is playable, the *game* is not yet. The port
currently advances through areas through gates and a stage-select overlay rather
than through the level's own doors/exits, most rooms have placeholder enemies or
none at all, most enemies are silhouettes, and only three boss species exist.

---

## 2. Definition of done

A release counts as "original fully playable" when **all** of these hold:

- [ ] **D1 — Navigation:** the player can reach every area of the original game
      from Area 0 using in-game doors/exits and the overworld, with no debug
      hook (`DC_STAGE_SELECT`, `DC_AREA`, `DC_START_X/Y`) involved.
- [ ] **D2 — Encounters:** every room spawns the objects the original does
      (enemies, hazards, urns, platforms) at the right positions, and the
      spawner honours the real spawn records (`$81:A1A9`).
- [ ] **D3 — Bosses:** every boss of the original is present, has its real
      multi-phase behaviour, and can be defeated with in-game attacks.
- [ ] **D4 — Progression items:** all crests, Fire-Crest powers, talismans,
      potions/vellum capacity, gold and upgrades are obtainable and gate what
      they gate in the original.
- [ ] **D5 — Terrain:** water, lava, slopes, breakables, moving platforms,
      wind/ice, hidden passages and boss gates behave as in the original.
- [ ] **D6 — Text/events:** all story beats (intro, crest grants, boss intros,
      NPCs, shop, endings) play from the extracted script at the right events.
- [ ] **D7 — Endings:** all four endings are reachable with the real conditions
      and show their full sequences.
- [ ] **D8 — Art:** no silhouette in normal play; all enemies/bosses/items drawn
      from captured or ROM-derived sheets.
- [ ] **D9 — Audio:** ROM-derived music and SFX (BRR/DSP) replace the
      procedural tracks, with correct track changes per scene.
- [ ] **D10 — Menus:** title, pause/sub-screen, shop, options and password match
      the retail layout and behaviour closely enough to be usable.
- [ ] **D11 — Regression:** the suite covers every milestone's acceptance case
      and stays green; a fresh build passes.

---

## 3. Gap analysis

Severity: **S1** = blocks a normal playthrough · **S2** = blocks fidelity ·
**S3** = polish.

| # | Gap | Severity | Notes |
|---|---|---|---|
| G1 | **Area-to-area navigation**: only 4 rooms have exits; 95 auto-advance after clear; no doors between rooms in a zone | S1 | The single biggest blocker to "playable like the original" |
| G2 | **Object placement coverage**: 7 enemy species placed, 148 placements; extractor only reads the area's object list | S1 | Scripted/spawned enemies (probe-visible) are missing |
| G3 | **Boss roster**: only `$33` (Somulo), `$A5` (Arma) and `$96` (NPC/ending) objects exist; no multi-phase boss state machine beyond a subset | S1 | No Phalanx/final boss fight, no statue boss logic |
| G4 | **Sprite coverage**: 11 sheets of ~40 species; `$33` and `$2C` explicitly rejected as fragmented | S2 | Silhouettes are honest but not shippable |
| G5 | **Terrain**: slopes/water partially ported; lava, ice/wind, breakables, moving platforms, boss gates incomplete | S1/S2 | Blocks some rooms and all late-game zones |
| G6 | **Crest forms**: 5 forms exist as state; per-form animation sets, OAM and abilities (Tornado platforms, Claw lava-freeze) missing | S2 | Needed for D4/D5 |
| G7 | **Items/shop/NPCs**: shop works with 3 items; talismans, spells, minigames, NPC conversations only partly wired | S2 | City NPC conversation is done; the rest is not |
| G8 | **Events/cutscenes**: dialogue system works, but boss intros, crest grant texts, ending sequencing and scene cameras are not wired to events | S2 | Data exists (script strings) |
| G9 | **Endings**: narration transcribed (not from ROM plain text), conditions approximated by crest count | S2 | Conditions need decoding from the ROM |
| G10 | **Overworld landing**: stage overlay works; the 6 real map castles aren't tied to stages | S2 | Map art is real; mapping is not |
| G11 | **Audio**: procedural replacement; no BRR/DSP extraction | S3 | Authenticity only |
| G12 | **Menus/options**: pause/subscreen basic, options minimal, no localization hooks | S3 | |
| G13 | **QA depth**: regression covers demos, 7 boss entries, 12-area subset; no full-route test | S2 | Needed to protect milestones |

---

## 3b. Progress log

- **2026-10-23 — save batches b/c/d.** Captures now cover **22 areas**
  (0,2,3,4,6,8,10,12,14,16,18,20,24,28,32,34,48,58,74,96,100,102). The probe
  pipeline harvested areas 18 (skull piles/hands), 20, 24, 28 (burning forest),
  32 (hidden room), 96 (potion store), 100 (burned state) and 102 (boss).
- **Species sheets** grew to 16 species: `$23 $24 $25 $26 $28 $29 $2A $2D $2E
  $48 $49 $57 $63 $65 $67 $6C $79 $96 $97` (some sheets are multi-part boss
  clusters and are marked in the JSON's `source`).
- **Shop decoded from the game itself**: the potion-store saves' BG3 layer gave
  the real greeting (`$85:87D5` "WELCOME TO "THE BLACK LOTUS" WE CARRY ONLY THE
  FINEST POTIONS"), the stock menu (`$85:88A0`) and the item descriptions:
  Mercury 6 GP (escape an enemy), Sulfur 10 GP (flee a battle), Herb 25 GP
  (restore some vitality), Elixir 100 GP (extra life), Ginseng 140 GP (restore
  all vitality), plus "CHEAPSKATE! YOU DON'T HAVE ENOUGH MONEY" (`$85:8B17`).
  The shop UI now uses this stock and these prices.
- **NPC merging is part of extraction** (`merge_npc_objects.py`, with
  `--x/--ground-y/--clamp` placement options) and regression covers the city
  conversation end to end.
- **Savestate driving** (`tools/oracle/drive_save.py`): `set_controller` +
  `screenshot` let a single savestate be replayed forward with synthetic input.
- Pending from this batch: `save11d` was listed but not present; the burning /
  burned stage variants are captured but the stage-state switch is not modelled.
- **Guides integrated** (`demon_s_crest_item_locations_guide.txt`,
  `demon_s_crest_crests_and_crest_fragments_guide.txt`,
  `demon_s_crest_endings_guide.txt`): the ending rule is now completion-based
  (<50% bad, 50-99% normal, 100% good; true needs the Dark Demon), replacing the
  crest-count approximation. The item guide lists every crest/Fire shot (with
  the boss that grants it), the 16 life extensions, and the urn/vellum/talisman
  locations — the checklist for M7/M8.
- **2026-10-24 — save batch e / ee.** Captures now cover **32 areas** (adds 36,
  38 wind room, 40 mid-boss, 42, 46 lake, **84 final-boss stage**, 92 library,
  98 spell shop, 104 minigame parlour, 110 second Black Lotus). Species sheets
  reach **29** (adds `$06 $09 $12 $15 $41 $87` and an `$A5` Arma cluster).
  Text read from the shops/NPCs:
  - second Black Lotus (`$85:...` menu without Ginseng): Mercury 6, Sulfur 10,
    Herb 25, Elixir 100, EXIT — implemented as a per-area stock;
  - spell shop: "I CANNOT HELP YOU UNLESS YOU BRING ME A VELLUM FIRST"
    (`$85:8370`) — shown when the player has no vellum;
  - minigame parlour: "IT HAS BEEN A LONG TIME SINCE A CUSTOMER HAS VISITED MY
    SHOP";
  - library talisman descriptions `$85:8C51` Crown, `$85:8CB3` Skull,
    `$85:8D17` Armor, `$85:8D79` Fang, `$85:8DD0` Hand — to wire in M7.
  - The final boss stage (area 84) shows the Dark Demon's face backdrop; the
    bad-ending cutscene lives in area 192 (mode 7).
- Remaining for the plan: M1 area graph, M2 placements for the new areas, M3
  boss AI (the area 18/102 bosses are captured but not ported), M4 sheets for
  `$28/$29/$2A` (currently multi-part clusters), M5 stage-state variants
  (burning/burned).

## 3b-2. Full-stage maps (docs/FullMaps)

The community full-stage maps in `docs/FullMaps/images` are 1:1 stitches of the
original rooms, so each one gives the room's **true screen grid** and the door
layout. `tools/dc_extract/index_full_maps.py` indexes them into
`docs/FullMaps/index.json` (43 maps, grouped by level).

How they help M1/M2:

1. **Room width ground truth.** Our extracted rooms are sometimes narrower than
   the real level (area 4 is 1024 px against a save at x=2025; the city is 256 px
   against x=604). The maps give the intended grid, e.g. **Murkland Colosseum is
   4 screens wide (1024x197 px)** while our area 0 is 3 screens — one missing.
2. **Name -> area identification.** Visual matching already confirms
   **area 0 = Murkland Colosseum** (the arched purple walls and torches match our
   screenshots). A `match_maps.py` (render each room's BG1 from its tilemap +
   atlas, correlate against the maps at reduced scale) would identify the rest
   automatically.
3. **Door positions.** Exits and door mouths are visible in the maps, so
   observed save doors can be cross-checked and the missing bboxes filled in.

**Next sub-tasks:** (a) write `match_maps.py` and produce a name<->area table;
(b) re-extract the mismatched rooms with the map-derived screen grids;
(c) feed map door positions into the area graph as candidate edges to verify with
saves.

## 3c. M1 progress — area graph and the first real doors

**Built:** `tools/dc_extract/build_area_graph.py` (nodes = areas with world size,
spawns and boss gate; edges = extracted doors, post-boss gates, overworld stage
entries, plus **doors observed in the level saves**) and
`tools/check_navigation.py` (reachability tiers + data-quality reports).
`merge_observed_doors.py` writes observed doors into the room JSONs so the port
can walk them.

**Numbers today:**

| Metric | Value |
|---|---|
| Areas | 113 |
| Extracted doors | 53 (36 areas have at least one) |
| Post-boss gates | 94 (94 areas have one) |
| Stage entries | 66 |
| Reachable from area 0 **with real doors only** | **2 areas (1%)** |
| … plus gates | 11 (9%) |
| … plus stage entries | 11 (9%) |
| No outgoing edge at all | 6 areas (14, 26, 3, 30, 36, 9) |
| Level-I doors derived from the saves | 2 (`2 -> 4`, `4 -> 6`) |

**Runtime:** the port already handles area doors and boss gates
(`Game_CheckTransitions`), so an observed door is walkable as soon as it is in the
room JSON. Regression now starts on the area-2 door and asserts the arrival in
area 4, and `DC_BOSS_CLEAR=1` marks a room cleared for revisits/tests.

**Blocker found:** the extracted rooms are sometimes **narrower than the real
level** — area 4 is 1024 px wide but the save shows the player at x=2025, and the
city (area 8) is 256 px against a save at x=604. Observed doors beyond the
extracted width are reported by `check_navigation.py` and skipped by the tests
until the rooms are re-extracted with their full multi-screen width. This is now
the top M1 task, ahead of collecting more save pairs.

**Also found:** area 4 has a transition with `next_area = -1` at its left edge,
which is almost certainly the "leave the level / back to the map" door; the
runtime should treat `-1` as a return to the overworld.

## 3d. Room sizes: what the ROM grid actually says

`extract.py` gained a guarded **`--full-grid`** mode: with `--full-room` it now
composes *every used row* of the area's screen grid instead of only the row that
contains the reference slot.

Result on the two suspect areas:

| area | grid (ROM) | world | note |
|---|---|---|---|
| 0 (colosseum) | 3 x 1 | 768 x 256 | the community map's 4th screen belongs to the adjacent room |
| 4 | 4 x 2 | 1024 x 512 | grid is genuinely 1024 wide |

So the "rooms are extracted too narrow" theory is **wrong**: area 4's ROM grid
really is 1024 px, yet the level-I save has the player at **x = 2025**. The
likely explanation is that the player's coordinate (`$1030`) lives in a
**level-wide space** shared across connected rooms (with a per-room origin
subtracted on entry), or the `$008D` value in that save is not the grid id.
Deciding this is the next M1 experiment: read the room-origin variable alongside
`$008D`/`$1030` in the level-I saves and compare.

Whatever the answer, the graph's door edges stand; only the mapping from door
positions to world coordinates depends on it.

## 4. Milestones

Each milestone ends with committed code, green regression, updated docs and a
concrete acceptance test. Sizes are estimates in "sessions" (one autonomous
working session), to be revised as data comes in.

### M1 — Canonical area graph and real navigation (S1, ~3 sessions)

**Goal:** walk the game without debug hooks.

1. Extract the real room connections: `transitions` (54 today) plus per-room
   `exits`/doors from `$81:981A` and the stage-entry table (66 entries).
2. Build `area_graph.json`: nodes (area, screen, spawn point), edges (door,
   one-way, gate, overworld entry) with requirements (crest, item, boss flag).
3. Replace `gate_next_area` auto-advance with real doors; keep gates only where
   the original uses them (boss rooms).
4. Wire overworld landing to the 6 campaign nodes + stage entries instead of the
   current shortcut list.
5. Acceptance: `check_playthrough.py` walks Area 0 → Arma → overworld with only
   controller input; new `check_navigation.py` proves every area is reachable
   from the start in the graph (and the graph matches runtime transitions).

**Risk:** transition data is RRM-derived; may need oracle reads per door.

### M2 — Full object/spawner coverage (S1, ~4 sessions)

**Goal:** every room spawns what the original spawns.

1. Batch-probe every save the user provides *and* every area via the recomp
   (`probe_saves_batch.py`) to harvest live object lists (slot, species, x, y,
   flags) per area.
2. Merge harvested placements into room JSONs (`merge_npc_objects.py` generalised
   to all species), keeping extractor data as the base.
3. Port the `$82:8611` object loop paths that placements rely on (primary/secondary
   update, activation windows).
4. Acceptance: new `check_spawns.py` compares runtime pool vs the harvested list
   per area (species+position within tolerance) for 20+ areas.

### M3 — Boss roster and AI (S1, ~6-8 sessions)

**Goal:** every boss is present, multi-phase and beatable.

1. Inventory the boss roster from object lists + oracle probes (expected:
   Somulo, Statue, Arma, Phalanx forms, final boss).
2. Decode per-boss state machines from the dispatch table (`$82:8000` handlers),
   starting with the two known ones (`$33`, `$A5`).
3. Capture boss sprites with the OAM base/count field (see §6) — unfragmented.
4. Wire boss intro dialogue, arena gates and defeat events.
5. Acceptance: a scripted bot defeat for each boss + `check_campaign.py` extended
   to all bosses.

### M4 — Sprite coverage programme (S2, ~4 sessions)

1. Make `bake_object_sprites.py` production-grade: OAM base/count decoding,
   cluster + palette attribution, per-frame animation sampling (2-4 frames).
2. Target list: every species with placements, by area coverage.
3. Drop silhouettes from the target list as sheets land; keep a "no-sheet" report.
4. Acceptance: a `species_coverage.py` report; regression asserts no species in
   the main route uses a silhouette.

### M5 — Terrain and mechanics fidelity (S1/S2, ~4 sessions)

1. Terrain: lava (damage), ice (friction), wind, quicksand, spikes, currents.
2. Breakables: Buster/Earth blocks, headbutt doors, hidden passages.
3. Moving platforms with rider physics (exists) + crumble/cycle timings.
4. Water: swim/jump constants per form; air pockets.
5. Acceptance: per-mechanic demos (like the physics oracle replays) with
   comparisons against recomp traces.

### M6 — Crest forms and abilities (S2, ~3 sessions)

1. Per-form movement/attack: Fire, Earth, Air, Water, Time, Ultimate.
2. Tornado platforms, Claw lava-freeze, Buster walls, Demon Fire.
3. Real form OAM/animation sets and HUD icons.
4. Acceptance: form-specific demos (fly through a shaft, freeze a lava pool,
   smash a wall) in regression.

### M7 — Items, shops, NPCs, events (S2, ~4 sessions)

The user-supplied guides in `docs/` are the reference for this milestone:
`demon_s_crest_item_locations_guide.txt` (crests + 4 Fire-Crest shots, 16 life
extensions, urns/vellums/talismans with locations and boss rewards),
`demon_s_crest_crests_and_crest_fragments_guide.txt` and
`demon_s_crest_endings_guide.txt`.

1. Talismans (Crown/Skull/Armor/Fang/Hand) with real effects and icons.
2. Spells/vellum and potion use from the sub-screen.
3. Shop: full stock, prices, refusal lines; city NPC conversations (one done).
4. Minigames (headbutt) and other NPC interactions where data allows.
5. Event wiring: crest grants, boss intros, area-entry story beats.
6. Acceptance: itemisation test (`check_items.py`): acquire each item via play
   and assert its effect.

### M8 — Endings and final progression (S2, ~3 sessions)

The endings guide gives the retail rules, which are now implemented as a
completion percentage (5 crests + 4 Fire shots + 5 talismans + 5 urns + 5
vellums + life extensions): **<50% bad, 50-99% normal, 100% good**, and the
**true** ending needs the Dark Demon fight (not implemented yet). The secret
epilogue is the post-credits password (`QFFB KNST BDR2 SA22` / `AR3S J42S G743
ML14`), which our password codec can accept once the format is confirmed.

1. Decode the real ending conditions — partly done via the guide; still verify
   the exact 100% gate in the ROM.
2. Ending sequences: artwork/captures per ending, scene timing, credits.
3. Final boss and last-area polish; verify all four endings from real play.
4. Acceptance: `check_endings.py` reaches each ending through the route (not
   `DC_ENDING`).

### M9 — Audio, menus, localization, release QA (S3, ~5 sessions)

1. BRR/DSP extraction: samples, instruments, sequences; replace procedural
   tracks; port SFX triggers (`$80:BD57`).
2. Retail-accurate pause/sub-screen layout, options, password screen.
3. Localization hooks and text encoding.
4. Release QA: full-route automated playthrough, fresh build, packaging notes.

**Total estimate:** ~36-44 sessions. M1-M3 are the critical path to a real
playthrough; M4-M6 make it look and feel right; M7-M9 complete it.

---

## 5. Sequenced backlog (first ten tasks)

1. `tools/dc_extract/build_area_graph.py` — parse transitions + stage entries into
   `assets/generated/area_graph.json`; add `check_navigation.py`.
2. Replace `gate_next_area` auto-advance in `Game_CompleteArea` with door/exits
   where the graph says so; keep gates for boss rooms.
3. Extend `probe_saves_batch.py` to dump *all* pool sectors and merge placements
   for every area, not just NPCs.
4. Decode the object sprite base/count field (`$0D/$0E` candidates) and feed it
   to `bake_object_sprites.py`; retry Somulo `$33` and the big eyeball `$6B`.
5. Port boss `$33` phase 2 (head-hit → stagger → retract) and wire its intro.
6. Add `$A5` (Arma) full behaviour: dialogue gate, projectile patterns, defeat.
7. Terrain: lava damage + Claw freeze; ice friction; wind zones.
8. Water form: swim physics + underwater currents.
9. Item pass: talisman effects + sub-screen use of potions/vellum.
10. Ending conditions: decode the crest/talisman gate from the ROM and replace
    the crest-count approximation.

---

## 6. Data and tooling needs

**Saves that unblock the most (in priority order):**

1. A save **entering each area** from its real door (to validate M1 transitions).
2. A save **at each boss** (phase 1 and 2 where applicable) — Somulo, statue,
   Arma, Phalanx forms, final boss.
3. Saves **with each enemy type on screen** (for M2/M4 coverage).
4. A save **with a dialogue box** for every NPC/story beat (the BG3 reader maps
   text to events — already proven with the city NPC).
5. A save **at each ending** (for M8 artwork/sequence).

**Tooling to build or extend:**

| Tool | Purpose |
|---|---|
| `build_area_graph.py` | transitions + stage entries → graph with requirements |
| `check_navigation.py` | graph reachability + runtime door assertions |
| `probe_area.py` | probe an arbitrary area (not just saves) for placements/sprites |
| `bake_object_sprites.py` (extend) | OAM base/count, multi-frame animation, palettes |
| `species_coverage.py` | report which species in the route use silhouettes |
| `check_spawns.py` / `check_items.py` / `check_endings.py` | milestone acceptance |
| `bg2_captures.json` (grow) | one capture per area for backdrops |

**Docs to keep current:** `PROJECT_STATE.md` (append per step),
`FULL_CAMPAIGN_PLAN.md` (tick items as milestones land), this plan (revise
estimates), `README.md` (hooks + status table).

---

## 7. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Recomp debug server instability / port collisions | Blocks capture sessions | Dynamic ports (done), retry with a fresh process, batch probes |
| Object sprite attribution (multi-part bosses) | Blocks D3/D8 | Decode OAM base/count from the object record; validate with two saves per boss |
| Room data narrower than the real area (e.g., city 256 vs 1024+) | Wrong navigation/placements | Re-extract with multi-screen room sets before M1 acceptance |
| Ending conditions unknown | D7 | Probe the ROM's ending branch; compare save flags at each ending |
| ROM text vs transcribed assets | Legal/accuracy | Keep extraction reproducible from the user's ROM; document provenance |
| Scope creep from the sequel concept | Delays D1-D11 | Keep the port plan first; the sequel layers on the same data formats |

---

## 8. Test plan

- **Per milestone:** add a `check_*.py` and a demo (`DC_DEMO=<n>`) that exercises
  the acceptance case; all run in `run_regression.py`.
- **Continuous:** `check_areas` (113), `check_campaign` (all bosses as they land),
  `check_playthrough` (full route once M1 lands), `check_navigation`,
  `check_spawns`, `check_items`, `check_endings`.
- **Visual:** screenshot captures per milestone stored under `dcbuild/out` (not
  committed) with a short note in the docs.
- **Release gate:** fresh configure+build in an empty directory, all checks green,
  and a manual smoke pass of the first 30 minutes.

---

## 9. How this supports the fan game

- **Prequel Somulo battle:** M3/M4 give a real Somulo sprite set and a ported
  fight; the prequel can reuse the boss framework with a new phase script.
- **Original campaign:** M1/M2/M5/M6 are exactly what "play the original" needs;
  the same data (rooms, placements, objects) is reusable by new stages.
- **Sequel (`Reign of the Red Blaze`):** the engine should expose:
  1. a **data-driven stage format** (room grid, objects, transitions, script
     events) that the concept doc can feed;
  2. a **cutscene/event runner** (camera, waits, dialogue, spawns) instead of
     hard-coded state transitions;
  3. a **boss framework** (phases, patterns, weak points) parameterised per boss;
  4. **asset slots** for new bosses/enemies using the same species-sheet pipeline.
  Those are small generalisations of M1/M3/M8 and are noted in their tasks.

---

## 10. Immediate next actions

1. Land M1 task 1-2 (area graph + real doors) — highest impact on "playable".
2. Extend the probe/harvest pipeline for all placements (M2 task 1-3).
3. Decode the boss OAM base field and retry Somulo (M3/M4 shared task).
4. Keep asking for the specific saves listed in §6 as each milestone starts.

*This plan is a living document: update estimates and status as milestones land.*
