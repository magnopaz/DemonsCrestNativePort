# Named rooms from the level saves

Built by `tools/dc_extract/build_room_names.py` from saves named
`lvl_<level>_<map-name>[_role]`. Each row is an area id observed in a save
plus the map it corresponds to.

| area | level | map | roles | our grid | map grid | note |
|---|---|---|---|---|---|---|
| 20 | III | dark-forest-thorn-grove | before_door, entrance | (2, 2) | (5, 3) | our grid is smaller/larger — see below |
| 22 | III | dark-forest-underground-dyke | before_door, e1, e2, entrance | (2, 2) | (7, 3) | our grid is smaller/larger — see below |
| 26 | III | dark-forest-scula-cave | before_door, boss, boss_dead, entrance | (8, 2) | (6, 3) | our grid is smaller/larger — see below |

## What the mismatch means

The player coordinates in the saves fit inside the map dimensions but often
fall outside our extracted world (thorn grove: save x=909 vs our 512 px;
underground dyke: x=1687 vs our 512 px). The ROM grid table
(`$81:A6D8`, read by `area_info`) reports 2x2 for those areas, while the
community maps show 5x3 and 7x3 — and the maps agree with the save
coordinates. So the grid table does **not** describe the full room for
multi-screen areas; the room is a chain/stream of more screen data than that
table holds.

Consequence for M1: room extents must come from the maps (and runtime
observation), not from the static grid table. The next extraction step is to
compose rooms from the map-driven screen lists instead of `area_info`.
