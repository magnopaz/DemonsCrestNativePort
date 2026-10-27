# MVP 2 — Murkland cemetery

Second delivery target: **area 2 (Murkland cemetery)** fully playable, with the
same definition-of-done discipline as `MVP_SOMULO.md`.

## What exists

- Room `area2` loads: world 2048×512, spawn at (64,428), 21 objects
  (enemies `$07`, `$26`, `$2C`, `$67`, one Fire-Crest pickup, urns), and a door
  to area 4 at (1701,300) plus an extracted door to area 3 at (2040,80).
- The player, movement, shooting, urns and pickups all work there today.
- A BW/2-palette backdrop capture exists (`bg2_area2_pal0.png`) but is only
  ~5 KB, and the room's BG layer is almost entirely empty tiles: **the sky is
  black in game**.

## Definition of done

| # | Requirement | Status |
|---|---|---|
| 1 | The cemetery backdrop renders (sky, graves, fog) | **gap** — BG layer is empty, capture is incomplete |
| 2 | Its four enemy species render as real sprites | **gap** — they show as fallback silhouettes |
| 3 | Enemies behave (patrol/shot) and can be killed | partial — generic enemy loop, no per-species AI |
| 4 | Urns give hearts/potions, the Fire-Crest pickup grants its piece | **done** |
| 5 | Walk from the spawn to the door to area 4 and transition | **done** (door present, regression door test) |
| 6 | The area is reachable in the campaign flow | **done** (stage entry / area graph) |

## Blocking work for MVP 2

1. **Re-extract the cemetery background.** The stored BG2 capture is too small
   and the room's tilemap is empty. Either re-run the BG2 capture for area 2
   (VRAM dump at that area) or take the backdrop from
   `docs/sprites/SNES - Demon's Crest - Backgrounds - Cemetery.png`.
2. **Enemy sheets for `$07`, `$26`, `$2C`, `$67`.** `Enemies 1/2.gif` in
   `docs/sprites` has the art but no species labels; the matching tool discussed
   in the plan (`A2`) is the route. `$67` already exists (the ghoul shot).
3. Per-species behaviour for the cemetery enemies (the generic loop is not
   enough for a "fully playable" claim).

## Why it is the right second delivery

It is small (one 2048-wide room), it reuses everything MVP 1 built (urns,
pickups, doors, shooting), and it forces the two asset gaps (backgrounds and
enemy sheets) that every later stage needs anyway.


## Status after the sprite and background work

Playable today: the room loads with its sky/trees backdrop and the gravestone
terrain, the HUD shows the dot row, skeletons (`$2C`) and ghouls (`$67`) walk the
graveyard with sheet-based art, the Hippogriff mid-boss fights with its statue
phase, and dying restarts an arena.

Open items found while running a scripted traversal (`DC_DEMO=15`):

1. **The player gets stuck at world x ~266** in a straight walk-right: the terrain
   needs a jump the demo does not time. Not a collision bug as far as we can tell,
   but the first gap/ledge of the room should be checked against a recording.
2. **A second Firebrand-like figure** appears at the left of the player around
   x ~238. It is not an object (the debug print of every drawn enemy/boss/NPC
   shows only `$2C/$67/$07/$26`) and `DrawPlayer` is called once, so the source is
   still unknown - the debug hook `DC_DEBUG_OBJ=1` prints what `DrawObjects` draws
   and is the tool for chasing it.
3. The room's exits sit at y 300 (area 4) and y 80 (area 3); a straight walk never
   reaches them, so an end-to-end run needs a proper jump script or a human.
