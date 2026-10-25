# MVP 1 — Somulo, the first fight

The first delivery target: the colosseum fight from the start of the game,
playable end to end, without cheats or stage select.

## Definition of done

| # | Requirement | Status | Evidence |
|---|---|---|---|
| 1 | Start a new game and walk into the colosseum (area 0) | **done** | title flow test + `demo 14` |
| 2 | Somulo appears at the arena, body on the floor, head above | **done** | `enemy_33_*` cut from the recorded fight |
| 3 | The head lowers so a grounded Firebrand can hit it | **done** | head bob in the state machine |
| 4 | He breathes low and high fireballs that track the player | **done** | low/high aim + firewall on impact |
| 5 | **Six** hits kill him; each hit gives a **32-frame** white flash that is invulnerable | **done** | measured in `rec_somulo` (hits at 530/684/834/985/1138/1299) |
| 6 | Defeat leaves the player in control and the gate leads to area 34 | **done** (opt-in `DC_MVP_KEEP_CONTROL`) | `demo 14` reaches area 34 |
| 7 | No placeholder art for the boss, his fireballs or the firewall | **done** | recorded frames + sheet fire art |
| 8 | Beat it as a player with the normal form, no debug flags | **check** | `DC_AREA=0` plain run; the demo's six hits prove reachability |
| 9 | **No HUD during the fight** (the original hides it for the cinematic), and life elsewhere is shown as **dots**, not a bar | **done** | measured from the user's captures; area 0 has no HUD, area 34 shows the two dot rows |

### How it was verified

- `demo 14` walks to the dragon, lands six shots 40 frames apart, and reaches
  area 34 in campaign mode. The regression asserts the HP sequence
  `6 → 5 → 4 → 3 → 2 → 1` and the area transition.
- The hurt rhythm and hit positions come from the recorded take, not guesses.
- `DC_BOSS_NO_ATTACK` is a **test hook** only: it keeps Somulo from firing so
  the loop can be checked without dodging. A human plays against the full
  fireballs.

## Known gaps inside "complete"

1. **Intro roar** — in the original he rears and roars before the fight; the
   pose exists in the sheet but is not staged yet.
2. **Second fight (area 34)** — the gate leads there, but area 34 is not staged
   as the phase-2 encounter.
3. **Mouth flame / breath effect** at the moment of firing.
4. The head bob is a gameplay affordance; the original's exact cadence still
   needs a RAM replay of the take.

## Try it

```powershell
# in the native port, from the title:
#   NEW GAME -> walk right into the colosseum -> shoot the head six times
# or scripted:
$env:DC_AREA="0"; $env:DC_DEMO="14"; .\demonscrest.exe --assets assets\generated
```

## Next delivery

MVP 2: **Murkland cemetery** (area 2) fully playable — its rooms, enemies, urns,
doors and the stage's boss, with the same definition-of-done table.


## The entrance (how the fight actually opens)

Explained by the user from the recorded fight — it is scripted, not a cutscene
you can skip:

1. Firebrand is shown in the colosseum; **Somulo is on the far side of the gate**
   and looks at him (his eyes glow between the bars).
2. The steps shake the screen as he comes to the **other gate, which opens** for
   him.
3. He passes through and **screams**.
4. **Only after the scream does the player get control.**
5. He then **walks toward the player with his eyes closed** — he cannot see
   Firebrand, so he does not react; standing in his path is fatal.
6. When he stops and **opens his eyes**, the fireball duel begins.

Implemented so far: the three phases (distant steps, walking in from the left,
the roar), control lockout until the roar ends, screen shake on the steps and the
roar, the deadly blind walk (state 7), and `DC_BOSS_INTRO=0` to skip it in tests.

Still missing: the **gate opening** animation, the glowing-eyes-behind-the-bars
shot, and the eyes-closed / eyes-open / scream sprites (the entrance currently
reuses the single full-body frame).
