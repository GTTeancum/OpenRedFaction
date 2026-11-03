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
| Multiplayer | PS2 multiplayer traced (docs/MP-BOTS-PS2.md). In from the PS2 code: arenas, bots (`miner1`, guns shown, instant respawn with the body left behind), PS2 target choice and teams, first-shot timing and fire facing, fighting on routes, weapon/ammo/health runs and roaming, hit reactions and step-away, item pickups by touch, item respawn and spin, dropped-item cap, PS2 multiplayer damage from the player's PS2 disc, skill damage scaling, announcer lines, respawn fade, 8.0 run speed, PS2 random numbers. Game clock no longer runs in slow motion. Also in: PS2 multiplayer weapon values (clip, ammo, ranges, spread), per-frame stand/run animation. Left, from the PS2 code (section 12 of the doc): upper-body aim (traced), character choice, bots' poses not yet seen on screen. PS2 end screen in (GAME OVER, kills, rank, continue), plus the "Press Fire to respawn" prompt and the in-match health/kills/rank block and clock, then the MP STATS screen (kills, deaths, longest spree, accuracy; Continue / Replay Level / Main Menu; flamethrower and piercing rounds not yet counted for accuracy), the PS2 respawn loadout (handgun only, full) and the winner art. Player vs. Player needs split screen |
| Flamethrower | Enemy flamethrowers not yet seen on screen; the player's flame starts at an eye-offset point, not the gun's barrel tag |
| Rail Driver | Its impact uses a stand-in spark; the original rail hit effect is not reconstructed |
| Rockets | The rocket body itself has not been seen in flight (the smoke trail has) |
| Driller | Digging loses about 0.4 s to hitches (world rebuild after each bite); the tunnel after loading a save not yet seen on the console |
| Loading | A quick-load takes about 12 s; the rest is weapon textures, glare, the HUD and disc reads |
| Test room | The developer-room test character's shotgun is handed over after gun models load, so it is never drawn |
| Props | Off-screen props are now skipped; whether they pop in at the screen edge has not been checked |

## Test coverage gaps

- No submarine or APC has been seen fully destroyed: the test can't stage it, because the driver dies from their own blasts first.

## Not started

- Multiplayer split screen (after single-player).
