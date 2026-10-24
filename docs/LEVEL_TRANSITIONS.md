# Level transitions (observed in game)

Strict table: `from | door | to | note`. `from`/`to` are map names from
`docs/FullMaps/images` (as used in the save names). `to` can be `map` for a
return to the overworld. The tooling turns each row into a door edge and takes
the trigger position from the `<from>_before_door` save when one exists.

| from | door | to | note |
|---|---|---|---|
| dark-forest-thorn-grove | right door | dark-forest-underground-dyke | second door in the room is not used here |
| dark-forest-underground-dyke | up (Air Crest / floating platform) | dark-forest-scula-cave | door works going up |
| dark-forest-scula-cave | right door | boss | boss room; the other door is underwater (needs Water Crest) |
| dark-forest-scula-cave | boss defeated | map | taking the life up returns to the map |
| towers-of-doom-gryphon-spire | right door | towers-of-doom-misty-skies | |
| towers-of-doom-misty-skies | right door | boss | boss room (the fabled recomp bug; kill fast) |
| towers-of-doom-misty-skies | boss defeated | map | boss leaves the Claw piece |
| poseidonia-aquatica | right door | poseidonia-temple-and-hidden-chamber | |
| poseidonia-temple-and-hidden-chamber | boss defeated | map | boss leaves the Water Crest |
| mountains-valley-of-the-grewon | right door | mountains-ice-spire | |
| mountains-ice-spire | up door 1 | area:64 | side room with a life up; only way back |
| mountains-ice-spire | up door 2 | mountains-ice-palace-corridor | |
| mountains-ice-palace-corridor | right | mountains-ice-palace-corridor | the map's extreme right; continues to the inside door |
| mountains-ice-palace-corridor | inside door | area:68 | boss chamber antechamber |
| area:68 | boss door | boss | palette-swapped bug boss |
| murkland-cemetery | right door (mid boss) | murkland-slope | mid boss must die first |
| murkland-slope | right door | murkland-abyss | |
| murkland-abyss | right door (Arma 1st) | map | Arma defeated returns to the map |
| ruined-city-dark-avenue | right door | ruined-city-reservoir | |
| ruined-city-reservoir | right door | ruined-city-boneyard | |
| ruined-city-boneyard | right door (boss) | map | boss defeated returns to the map |
| area:0 | after boss | area:34 | second colosseum room, same map; the on_the_righ save lands there |
