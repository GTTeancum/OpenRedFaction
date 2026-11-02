# Red Faction Xbox — to-do

Updated November 2. Open items only, one line each. Finished work and the full working notes are in `docs/TODO-HISTORY.md`.

---

## Waiting on you (USER)

| # | What to check | Where |
|---|---|---|
| 1 | Turn on 480p in the Xbox dashboard (the cable is component; 480p and 720p are off there) | dashboard |

## Open work

| Area | What is left |
|---|---|
| Flamethrower | Enemy flamethrowers not yet seen on screen; the player's flame starts at an eye-offset point, not the gun's barrel tag |
| Rail Driver | Its impact uses a stand-in spark; the original rail hit effect is not reconstructed |
| Rockets | The rocket body itself has not been seen in flight (the smoke trail has) |
| Driller | Digging loses about 0.4 s to hitches (world rebuild after each bite); the tunnel after loading a save not yet seen on the console |
| Loading | A quick-load takes about 12 s; the rest is weapon textures, glare, the HUD and disc reads |
| Props | Off-screen props are now skipped; whether they pop in at the screen edge has not been checked |
| 30 FPS smooth mode | Optional: the in-between pictures are not drawn yet (it behaves like steady 30, the shipped default) |

## Test coverage gaps

- No submarine or APC has been seen fully destroyed: the test can't stage it, because the driver dies from their own blasts first.
- 28 older PC tests fail because their fixtures are out of date (listed in the October 25 notes in the history file).

## Not started

- Multiplayer (after single-player).
