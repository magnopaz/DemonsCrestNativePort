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

## 3e. Navigation model: chunks, maps and the overworld root

The level-I/III saves plus the maps settled the data model:

- **A map is a composite of several chunk-areas.** Area 20 (512x256) seams into
  area 21 (512x2048) at x=440; area 21 seams into 23 (1024x2048) at five
  different heights; area 22 seams into 23 too. The community map
  "dark-forest-underground-dyke" covers 1792x469 px, i.e. several of those
  chunks. The ROM grid table describes **one chunk**, not the whole map — which
  is why our worlds look narrow next to the maps.
- **Navigation should therefore be modelled at chunk level**, with map names
  attached to the *set* of chunks a map covers. The first three names are
  resolved (thorn grove = 20, underground dyke = 22, scula cave = 26) and the
  name table (`build_room_names.py`) extends as more named saves arrive.
- **The overworld is the graph root.** Reachability must start at `-1` (the
  overworld), with stage entries leaving it and `to_map` edges returning. With
  that fixed the graph reports **94 of 113 areas (83%) reachable through real
  doors**, 113/113 with the post-boss gates.

Door edges can now come from three sources, all merged in `build_area_graph`:
extracted transitions, door positions observed in saves (`save_observed`), and
the user's level notes (`docs/LEVEL_TRANSITIONS.md` -> `notes`), which are
resolved through the name table and take their trigger position from the
`<room>_before_door` save.

**Remaining gate-dependent areas (need real doors):** 0, 1, 2, 3, 17, 35, 36,
43, 46, 52, 57, 63, ... (19 total). **Chunks with no outgoing edge:** 14, 3, 30,
36, 9.

## 3f. Overworld markers and stage unlocks

`tools/dc_extract/build_map_markers.py` reads mode-7 saves and extracts the
stage icons (objects with species `$94`) plus the cursor (`$6A`). Comparing two
progress points shows the map changing:

| save | markers | note |
|---|---|---|
| `save6ee` (mid-game) | 13 | includes (151, 99) |
| `map_new_stages` (past the bad-ending limit) | 16 | (151, 99) gone; **four new** markers: (78,42), (90,39), (190,140), (202,137) |

That is the retail progression in data form: the final-stage marker disappears
when the bad-ending path closes, and the remaining stages open. `map_markers.json`
now carries each snapshot and the union of positions, so the port's overworld can
draw and land on the real markers instead of the procedural node list (M1's
landing task and M8's unlock states both consume it).

## 3g. Levels V/VI named; entries instead of guesses

Poseidonia and Mountains saves resolved seven more rooms:

| area | level | map | roles |
|---|---|---|---|
| 48 | V | poseidonia-aquatica | entrance, e1, e2, before_door |
| 54 | V | poseidonia-temple-and-hidden-chamber | entrance, passage, persecution, persecution_trigged, boss, dead |
| 58 | VI | mountains-valley-of-the-grewon | entrance, e1, before_door |
| 62 | VI | mountains-ice-spire | entrance, e1, before_door_1, before_door_2 |
| 64 | VI | (ice spire side room, no map) | inside_door_1 |
| 66 | VI | mountains-ice-palace-corridor | entrance, e1, before_door |
| 68 | VI | (boss chamber, no map) | inside_door, inside_door_before_boss, inside_door_boss |

Edges added from the prose, including **`area:N` destinations** for sub-rooms
without a community map (the ice-spire life-up room `area:64`, the corridor boss
chamber `area:68`). The notes table is now 14 edges.

Entry inference was refined: a named room is an overworld entry **only if no
other named room of its level leads into it**, so mid-level rooms (temple,
corridor, boss chambers) are not treated as map entries. Level entries resolved
so far: **20 (III), 36 (IV), 48 (V), 58 (VI)**.

Graph: 14 note edges, 59 doors total, **95/113 (84%) reachable through real
doors**, 113/113 with gates, 18 gate-dependent areas, 4 chunks with no outgoing
edge (14, 3, 30, 9).

## 3h. Levels I/II resolved; 87% reachable by real doors

The level I/II named saves also **corrected an earlier assumption**: level I is
not the colosseum room. The names resolve as:

| area | level | map |
|---|---|---|
| 2 | I | murkland-cemetery |
| 4 | I | murkland-slope |
| 6 | I | murkland-abyss |
| 8 | II | ruined-city-dark-avenue |
| 10 | II | ruined-city-reservoir (item1 = potion, item2 = talisman passage) |
| 18 | II | ruined-city-boneyard |

Chains added from the prose: cemetery -> slope (after the mid boss), slope ->
abyss, abyss -> map (Arma 1st), dark avenue -> reservoir -> boneyard, boneyard ->
map (boss). The notes table is now **20 edges**.

Two model fixes this round:

1. **Area 0 is the game start**, not an overworld entry; it now has its own
   `game_start` edge. That immediately made area 17 reachable through its
   transition.
2. The old `save_lvl_I_room0..2` saves are the same rooms as the new named ones
   (2/4/6), so they only add unmatched duplicates now.

Graph after the round: **99/113 (87%) reachable through real doors**, 113/113
with gates, **14 gate-dependent areas** left (1, 35, 43, 46, 52, 57, 63, 76, 77,
83, 88, 94, ...) and 4 chunks with no outgoing edge (14, 3, 30, 9).

## 3i. Level 0 named; room names in the UI; note edges walkable

- Level 0 resolved: **murkland-colosseum = area 0**, and the level advances to
  **area 34** (the second colosseum room, same map) after the boss — not to the
  extracted target 17. Both rooms are boss fights; the notes carry the edge
  (`area:0 -> area:34`), and the notes table is **22 edges**.
- That advancement is a **post-boss gate**, not a walk-through door. Fixing it
  surfaced two real bugs: area 0's `gate_next_area` pointed at 1 (wrong target),
  and the gate's x-threshold (`world_w - 8`) was unreachable because walls keep
  the player ~32 px inside the edge. Both are fixed; regression now proves
  area 0 -> 34, and `DC_BOSS_CLEAR` applies after the level load (the boss
  object spawns after `Game_Reset`, so the old hook was overwritten).
- `merge_observed_doors.py` now also merges note edges into the room JSONs and,
  with `--trust-notes`, **drops extracted transitions that contradict the
  notes** (2->3, 4->5, 8->7, 10->12, 20->21, 22->23, 38->39, 48->4 were dropped;
  2->4, 4->6, 8->10, 10->18, 20->22, 22->26, 36->38, 48->54, 58->64 added). This
  makes the named chains walkable in the port instead of the old gate shortcuts.
- `asset.c` loads `room_names.json`; the **area banner** now shows the map name
  (e.g. "MURKLAND CEMETERY") and the **stage-select list** shows names for the
  named areas instead of bare numbers.
- Room-mapping validation attempt: comparing live BG1 tilemaps against our room
  JSONs was inconclusive (many rooms draw on BG2, and probes can catch a
  transition), so the mapping rests on the user's observations.

## 3j. Shared map names and the level-0 gate

`murkland-colosseum` is **two areas** (0 = entrance/boss, 34 = second room/boss).
The notes lookup now keeps a name -> areas list and prefers the area saved with
the `entrance` role, so a bare name resolves to the level start; ambiguous names
are reported (`map 'murkland-colosseum' spans areas [0, 34]; using 0`).

The same happens for `mountains-ice-spire` (62 + 64) and
`mountains-ice-palace-corridor` (66 + 68), which is the expected chunk pattern.

## 3k. Endgame mapped (VII and VIII); merge policy fixed

The final-stage saves resolved the whole endgame:

| area | level | map | roles |
|---|---|---|---|
| 74 | VII | phalanxs-fortress-front-hall | entrance, e1, before/after_door, boss |
| 76 | VII | phalanxs-fortress-western-bell-tower | entrance, before_door |
| 80 | VII | phalanxs-fortress-eastern-bell-tower | entrance, before_door |
| 82 | VII | phalanxs-fortress-hall-of-agonized-souls | entrance, e1, boss |
| 84 | VII | phalanxs-fortress-phalanxs-chamber | boss talk / boss / cutscene / boss2 / boss2_talk / boss2_cutscene |
| 86 | VII | phalanxs-fortress-demon-realm | entrance, boss_talk, boss |
| 88 | VIII | demons-hideout-tunnel | entrance, before_door |
| 90 | VIII | demons-hideout-the-dark-demon | entrance, final_boss_second |

Spiny balcony turned out to be **area 78** (my map-grid inference of 39 was
wrong — the save corrected it). The notes now carry
`western bell tower -> spiny balcony -> eastern bell tower` with real areas. Boss part species seen: `$09/$11/$13`
(mid bosses), `$86/$88/$8B/$8C/$8D` (Phalanx phases), `$58/$5C/$5D/$5E/$5F`
(demon realm), `$89/$8A/$BD` and `$BC` (true final boss forms).

**Merge policy bug found and fixed.** `--trust-notes` used to drop *every*
extracted transition whose target differed from a note edge, which removed real
side doors (reachability fell from 111 to 98 with gates). Now a transition is
only replaced when it sits at the **same door position** as the note edge, and
`tools/dc_extract/restore_transitions.py` re-adds any extracted transition that
had been dropped (18 restored across areas 5, 8, 13, 20, 22, 23, 25, 27, 29, 38,
39, 48, 59).

Result: **101/113 (89%) reachable through real doors**, 113/113 with gates,
**12 gate-dependent areas** left (1, 35, 43, 46, 52, 57, 63, 77, 83, 94, 99,
109) and 4 chunks without outgoing edges (14, 3, 30, 9).

## 3l. What the recomp savestates actually are (important)

While trying to capture boss frames it became clear that the user's `.sav`
save states **restore WRAM/VRAM/CGRAM/OAM but boot the CPU from reset**:

- the probe reads coherent game data (area `$008D`, player `$1030`, object pool,
  live OAM) for the first ~100 frames after resume;
- but every exact-frame screenshot shows the **Capcom copyright screen**, i.e.
  the game's boot path, not the saved scene;
- `run_to_frame`/`dump_frame_raw`/`screenshot` therefore cannot reproduce or
  drive the saved gameplay.

Consequences for the project:

1. The saves remain excellent **data snapshots** (placements, tilemaps, CGRAM,
   OAM, text, map markers) — everything extracted so far is valid.
2. They cannot be used to **drive** gameplay or capture composited frames, so
   boss sprites cannot be cut out of a frame.
3. Navigation truth must come from the user's descriptions + the ROM tables,
   which is how `LEVEL_TRANSITIONS.md` and the area graph work.

This is why `tools/oracle/extract_boss_sprite.py` is kept only as a documented
dead end: it needs a resumable state to produce frames.

**Also fixed:** `bake_object_sprites.py` painted the merged OAM index image with
a single palette (palette 0), so multi-palette bosses were mis-coloured even when
their geometry was right. It now composes each sprite with its **own palette**
(and masks the low tile bit for 16x16 sprites). Boss composition remains
fragmented for Somulo/Arma because the OAM part-to-object attribution is still
unsolved; that is recorded as the top M4 task.

## 3m. Doors are chunk-local; notes correct destinations

The save coordinates are **level-space** (a whole map spans several chunk-areas),
so most note doors fall outside their chunk's extracted world. Adding such a door
produced an unreachable trigger. The merge now does the right thing:

- if the note door's position fits the chunk → add/merge it there;
- otherwise **re-point the chunk's ROM transition** (which is already in
  chunk-local coordinates) to the destination the notes give.

Nine doors were re-pointed this way (`20→22`, `22→26`, `38→boss`, `48→54`,
`66→boss`, `4→6`, `8→10`, `10→18`, plus area 0 earlier), keeping the real trigger
positions while fixing the destinations.

Regression now walks five doors with only synthetic input:
`0→34`, `2→4`, `8→10`, `10→18`, `20→22` (`DC_AREA` + `DC_START_X/Y` +
`DC_CAMPAIGN` + `DC_BOSS_CLEAR`, then the walk demo).

## 3n. Interiors: library, Black Lotus, wise man

The three up-doors inside the ruined city's dark avenue resolved to their rooms:

| room | area | what it is |
|---|---|---|
| library | 92 | NPC describes talismans (`$85:8C51..8DD0`) |
| black_lotus | 96 | potion shop (Mercury/Sulfur/Herb/Elixir/Ginseng) |
| wise_man | 94 | spell shop; asks for a vellum first (`$85:8370`) |

They are recorded in `docs/INTERIOR_DOORS.md` rather than the parseable transition
table: their save coordinates are level-space and would clamp to the same spot in
the chunk, producing ambiguous triggers. The **names** are still used by the UI —
the area banner now shows **LIBRARY** for area 92 (verified).

The save-name parser was also hardened: roles are matched from the end against a
known set, so names containing underscores (`wise_man`, `black_lotus`) are no
longer split into a bogus name + role.

Still needed to make them walkable: a level-space → chunk-space mapping for level
II's dark avenue (the avenue is a chunk composite; saves reach x≈990 while the
extracted chunk is 256 px wide).

## 3o. Merge policy: never drop, order instead

Re-pointing a door a second time showed the risk: area 76 had a real extracted
door to 39 only ~15 px from the note door to 78, and dropping the "overlap"
deleted a whole branch (areas 21/23/41/42 became gate-only). Final policy:

- **never delete** an extracted transition;
- a note/clamped door is **inserted at the front** of the room's transition list,
  so when two triggers overlap the note door wins the runtime check order;
- a room with exactly one extracted door still has its destination re-pointed
  (that case is unambiguous).

Result: **102/113 (90%) reachable through real doors**, 113/113 with gates,
**11 gate-dependent areas** (1, 35, 43, 46, 52, 57, 63, 77, 83, 99, 109) and 4
chunks with no outgoing edge (14, 3, 30, 9). Regression proves six doors:
`0→34`, `2→4`, `8→10`, `10→18`, `20→22`, `76→78`.

## 3p. Poseidonia side paths, limestone, and a real area-id limit

New rooms resolved from the latest saves:

| area | level | room |
|---|---|---|
| 50 | V | poseidonia-underwater-grotto |
| 52 | V | poseidonia-lair-of-holothurion (boss leaves a life up) |
| 30 | III | dark-forest-limestone-cave |

Links added: aquatica → grotto, grotto → temple (up), grotto → holothurion lair
(right), lair → boss, scula cave → limestone cave (up).

**Area ids above 112 are not real.** The `temple-and-hidden-chamber_from_grotto`
save reports `$008D = 118`, but `area_info` for 113..127 returns garbage (invalid
tilesets, grids like `241x2`), so the game's room id is not the extraction index
for that one save; it is the temple (54) reached from the grotto. The name tool
now ignores areas above 112.

**Merge policy settled after three regressions:** extracted transitions are never
modified or deleted; note/save doors are inserted **first** (they win overlapping
trigger checks) and clamped into the chunk when their save-space position is
outside it. That restored the branches lost earlier (21/23/41/42).

Result: **103/113 (91%) reachable through real doors**, 113/113 with gates,
**10 gate-dependent areas** left (1, 35, 43, 46, 57, 63, 77, 83, 99, 109) and 4
chunks with no outgoing edge (14, 3, 30, 9 — note 30 is now named, its exit is
the up-door to the riverbank, whose save is still missing).

## 3q. Somulo ported (M3's first boss)

The user supplied fight screenshots plus the pattern, which made this possible
without any drivable save:

**Animated reference folder.** `docs/sprites/animated/` holds motion material:

- `dragons-crest-somulo-animation.gif` — 22 frames (50 ms each ≈ 66 game frames)
  of the real fight: two fireballs in 1.1 s, so an attack roughly every 33
  frames. The AI cadence was tuned to that (idle 22 → windup → fire → recover
  12), and Somulo's draw scale is 1.3x to match the reference dragon's ~195 px.
- `somulo-walking.webp` (16 frames, 241x181), `Arma_laugh.webp` and
  `fireboss_animated.webp` are **hi-res fan animations**, not ripped sprites —
  they do not match the SNES pixel style, so they are reference only (the walking
  pose still confirmed the leg cycle).
- `demon's-crest-bad-ending-animation.gif` (3 frames) and
  `map_mode7_animated.mp4` are material for M8's ending sequence and the
  overworld presentation.
- Gameplay captures (`demon's-crest-mid-boss-gameplay.gif`,
  `animated_gameplay_skullpile.gif`, `demons-crest-firebrand-forest-gameplay.gif`,
  `air-crest-firebrand-gameplay.gif`) are useful for timing checks.

**Sprites from the user's sheet.** `docs/sprites/` contains a labelled Somulo
sheet (Spriters Resource, ripped by Lord Zymeth): six head poses, body, walking
and defeated legs, fireball and firewall frames, and a darkened palette. The port
composes full poses from **body + head** (the original draws them as separate
objects: `$33` head, `$35/$36/$37` parts) and mirrors them to face right:

- frames `enemy_33_0..4.png` = idle, attack high, attack low, hurt, defeated,
  selected by the boss state (and `substate` = high/low) rather than by cycling;
- `boss_fireball_0..2.png` and `boss_flame_0..6.png` are the sheet's fireball and
  firewall frames, used for species `$34` (flames are bottom-anchored and grow).

**Fight, as implemented from the reference**

- Idle → windup → **fire** → recover loop; the fireball is aimed at Firebrand and
  alternates **low and high** (tracking his feet or his head at fire time).
- The fireball **spawns a firewall on impact** (three standing flames that burn
  for 90 frames and hurt on contact).
- **Six head hits** kill him (the object is the head, which is the weak point);
  a hit interrupts his cycle into a stagger.
- His sprite comes from the reference screenshots: a colour-key cut (the
  colosseum is purple, the dragon gold/bone), trimmed to the dragon and anchored
  at the head.

**Engine fixes this surfaced**

1. `DC_SPECIES_MAX` was **16** while `species_anim.json` now has 30 entries — half
   the captured sheets were silently never loaded. Raised to 64: this alone
   explains most of the remaining "silhouette" enemies.
2. `species_anim.json` is loaded once; the boss's 1.5x draw scale is gone (the
   cut sprites are already 1:1), so bosses no longer balloon.
3. The player's fireball now leaves from the **mouth** (x+10, y-18) as in the
   reference frames, instead of the feet; urn hit tolerance was widened so
   ground urns still break.
4. Rooms can hold **several boss objects** for the same species (extractor plus
   probe placements). They are deduplicated, preferring the observed fight
   position.

Remaining for Somulo: an animation frame (the sheet is a single pose), the
mouth-flame idle, and tying his defeat to the phase-2 room (area 34) beyond the
existing gate.

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
