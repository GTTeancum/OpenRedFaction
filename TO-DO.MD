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
| Multiplayer | PS2 multiplayer traced (docs/MP-BOTS-PS2.md). In from the PS2 code: arenas, bots (`miner1`, guns shown, instant respawn with the body left behind), PS2 target choice and teams, first-shot timing and fire facing, fighting on routes, weapon/ammo/health runs and roaming, hit reactions and step-away, item pickups by touch, item respawn and spin, dropped-item cap, PS2 multiplayer damage from the player's PS2 disc, skill damage scaling, announcer lines, respawn fade, 8.0 run speed, PS2 random numbers. Game clock no longer runs in slow motion. Also in: PS2 multiplayer weapon values (clip, ammo, ranges, spread), per-frame stand/run animation. Also in: bot upper-body aim (clamped against the animated pose; seen on XEMU with a bot leaning 9 degrees, bodies normal), bot sight state for attack step 3, the dropped-item fade (drawn; not yet caught in view), the menu character's class (no visible model with one player). Left: the attack-step-3 terms for the untracked AI movement sub-state, and seeing a fading drop on screen. PS2 end screen in (GAME OVER, kills, rank, continue), plus the "Press Fire to respawn" prompt and the in-match health/kills/rank block and clock, then the MP STATS screen (kills, deaths, longest spree, accuracy; Continue / Replay Level / Main Menu; piercing rounds count per body; the flamethrower never counts, as on the PS2), the PS2 respawn loadout (handgun only, full) and the winner art. The main menu entry reads BOT MATCH and opens Player vs. Bots directly (two-player is shelved) |
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

- Multiplayer split screen: shelved (about 8 fps, below the 30 fps bar); code kept, unreachable.

## Multiplayer: open PS2 items

Listed November 3; each is one line, details in `docs/MP-BOTS-PS2.md`.

| # | Item |
|---|---|
| 21 | Split-screen two-player: shelved November 6 at about 8 fps (30 fps bar); code kept but unreachable, the menu entry reads BOT MATCH. Notes in `docs/SPLIT-SCREEN-2P.md` |
| 24 | Waste Disposal Plant bots stuck about 7-10%: PS2 route link node 12 to 14 climbs a 4 m wall at the end of a trench. The PS2 map has the same wall (geometry checked), so it cannot be walked uphill on either; bots give up after 1 s and later try it again |
| 26 | Find whether the PS2 has a no-progress give-up for routes; if so, replace the port's one-second give-up with it |
| 30 | PS2 route-start selection (113570: standing node from 138500, else 113380 nearest node with a clear sweep) is written but parked in `local/mp-sweep/ps2-route-start.patch`; its first form left Waterlogged bots stuck 25% of the time, the full form is untested |
| 28 | Rare game error in the bot animation update (seen twice on The Lobby, once on The Badlands); cause being recorded |
| 29 | XEMU closed 30 s into one Waterlogged run (a re-run was clean) |
| 27 | Arenas use PC map geometry with PS2 bot routes; check the PS2 maps for differences (or load the PS2 maps) |

### Bot sweep after the movement fixes, November 5 (XEMU, same matches)

| Arena | Bot time stuck before | After |
|---|---|---|
| The Lobby | about 30% | 0% |
| Installation XJ5 | about 80% | 4% |
| Waste Disposal Plant | about 65% | 7% |
| The Badlands | about 50% | 3% |
| Hangar 18 | about 35% | 3% |
| Waterlogged | about 90% | 1% (bots still often pick routes with no path, so fewer fights) |
| Mine Warfare | about 45% | 3% |
| Warlords | about 30% | 4% |

### Bot sweep, November 3 (console, 5-minute matches, 4 bots, details in `docs/MP-BOTS-PS2.md`)

| Arena | Bot time spent stuck | Longest spell with no bot firing |
|---|---|---|
| The Lobby (dm01) | about 30% | under a minute |
| Installation XJ5 (dm02) | about 80% | last 3 minutes |
| Waste Disposal Plant (dm03) | about 65% | last 1.5 minutes (all four bots on one spot) |
| The Badlands (dm04) | about 50% | last 2 minutes |
| Hangar 18 (dm05) | about 35% | under a minute |
| Waterlogged (dm06) | about 90% (mostly no path found) | from 40 s in to the end |
| Mine Warfare (dm07) | about 45% | over a minute |
| Warlords (ctf02) | about 30% | over a minute |
