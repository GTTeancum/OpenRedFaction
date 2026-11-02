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
| PS2 menus (next) | First pass in (Xbox only; PC keeps PC menus): main menu, difficulty, load, quit, extras, options (game, controls, sound, buttons), pause and multiplayer setup are drawn from the PS2 art; menu text uses Orbitron Bold. The pause screen shows the paused game darkened behind it. Left: menu sounds are still the PC ones; Training, Credits and Trailer are greyed out; popup and options-row layouts are not traced from the PS2 code |
| Installer | Player-facing installer that requires the player's own PC game files and PS2 disc image, then builds the Xbox disc (texture pack, PS2 menu pack) locally; only our code and the Orbitron font (with its licence) ship |
| Multiplayer | Stage 1 in (PS2-traced, docs/MP-BOTS-PS2.md): the 8 PS2 arenas with their bots and paths, bot fights, respawns, scoring and kill/time limits. Left (stage 2): the PS2 idle path walking and weapon seeking, the "5 kills / 1 kill left" announcements, bots' multiplayer side 3; Player vs. Player needs split screen |
| Flamethrower | Enemy flamethrowers not yet seen on screen; the player's flame starts at an eye-offset point, not the gun's barrel tag |
| Rail Driver | Its impact uses a stand-in spark; the original rail hit effect is not reconstructed |
| Rockets | The rocket body itself has not been seen in flight (the smoke trail has) |
| Driller | Digging loses about 0.4 s to hitches (world rebuild after each bite); the tunnel after loading a save not yet seen on the console |
| Loading | A quick-load takes about 12 s; the rest is weapon textures, glare, the HUD and disc reads |
| Props | Off-screen props are now skipped; whether they pop in at the screen edge has not been checked |

## Test coverage gaps

- No submarine or APC has been seen fully destroyed: the test can't stage it, because the driver dies from their own blasts first.

## Not started

- Multiplayer bots, match rules and split screen (after single-player).
