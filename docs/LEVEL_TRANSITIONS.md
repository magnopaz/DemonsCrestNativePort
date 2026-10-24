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
