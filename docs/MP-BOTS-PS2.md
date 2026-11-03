# PS2 multiplayer (SLUS-20073), full trace

Traced 2026-11-02 from the PS2 executable (Ghidra project `local/ghidra-ps2`,
whole-program decompile `artifacts/analysis/ps2_slus20073/all.c`; EE-only
`lq/sq/por/paddub` patched to MIPS64 equivalents, `$gp` = 0x2b36f0,
non-returning-function discovery disabled). Addresses are PS2 virtual
addresses. Where Ghidra stops at an EE three-operand `mult` (encoded like
`mult` with a nonzero rd: rd = rs * rt), the routine was read from the
disassembly of `local/ps2-iso/SLUS_200.73.patched` (capstone, MIPS64).

Scope: every function that tests the multiplayer flag `2ac530` (about 100),
the match module (1db9d0..1ddf20), the bot roster (272f80..274c50) and the
code they call. Split-screen HUD layout and the second human player are noted
but not detailed; the Xbox has one human player for now.

The Xbox port status is in the last section.

## 1. Mode flags and options

- `2ac534` = multiplayer requested (set by the menu). Level start 165d20 calls
  1dbf20 when it is set; 1dbf20 sets `2ac530` = 1 (multiplayer on) and clears
  `2ac534`. Leaving (1657f0) clears `2ac530`.
- Options at 925490 (1ddaa0 stores, 1ddaf0 returns the block):
  +0 bots, +4 skill (0..3, also copied to the global difficulty `2abeec`),
  +8 kill limit, +0xc time limit in minutes, +0x10 start with gun.
  Defaults 1ddb20: 2 bots, skill 2, 15 kills, 15 minutes, start with gun.
- `2ab9e8` = number of human players (1 or 2); `2ac2b0`/`2ac2b4` the player
  records; `2ac2b8` their count.
- `ps2_multi.tbl` (TABLES.VPP): per arena `$Num bots: A B` (A = most bots in a
  player-vs-player game, B = most in a player-vs-bots game; 4 6 on most
  arenas, 5 6 on one, 0 0 on one). Characters the player can pick: screen
  name, picture, `$EntityType` and `$SkinName`: Miner Parker (miner2/parker),
  Envirosuit Parker (miner1/parker), Scientist Parker (parker_sci), Exec
  Parker (parker_suit), Miner (miner2), Envirosuit Miner (miner1 skin d),
  Ultor Guard 1/2 (guard1, skin c), Envirosuit Guard 1/2 (env_guard, skin b),
  Elite Guard 1/2 (elite, skin e), Riot Guard 1 (riot_guard).

## 2. Match start (1dbf20)

1. Multiplayer on; viewports set (1a5ba0: one player full screen, two players
   split top/bottom).
2. Each human player spawns (1dd930, below); a failure hangs (debug trap).
3. Time left `2ac54c` = minutes * 60 (`2ac550` = no time limit when 0);
   `2ac554` = no kill limit when the limit is 0; match-over `2ac548` = 0.
4. Player stats cleared (1a24a0, 1a6ba0: kills, deaths, etc. at player
   stats +0..+0x18).
5. All 16 roster slots' scores cleared (2740b0: +4..+0x1c).

Level creation (165d20) then runs 2740e0 (bots, section 4).

## 3. Level entities in multiplayer (177010)

While the level's entities are created, in multiplayer every entity whose
class mesh is a skinned character (class field +0x94 == 2, set by the
entity.tbl parser at 35349 for a `.vcm` V3D filename; `.vfx` is 3, other 1)
is created as class `miner1` instead of its authored class (1524e0 gets the
authored class index as an extra argument). Its authored fields still apply:
name, life/armour overrides, AI mode, flags, and its authored weapon
(record weapon name -> +0x66c weapon, +0x670 ammo; 17e050/17e0e0).

So every PS2 bot is the `miner1` model (Envirosuit Miner) with default skin,
holding the arena's authored weapon (m_ps201: Machine Pistol).

## 4. Bots

- 2740e0 (level start): walks the entity list; for each non-human entity
  (188c40 false), the first N (options +0) become bots via 273f70; the rest
  (up to 8 remembered) are destroyed (thunk 184850). Human entities get
  +0x12fc = -1.
- 273f30 clears the roster (0x1edcdd0, 16 x 0x54) and names the slots from
  293ba0: Myles, Willis, Tyler, Abraham, Lance, Jared, Benedict, Butch, Logan,
  Bryce, Tristan, Connor, Mason, Lucas, Sergio, Adrian.
- 273f70 (make bot): first free slot; +0 used, +0x20 entity, +4..+0x1c scores
  zeroed, entity +0x12fc = slot, entity +0x734 |= 0x100000 (bot flag),
  roster +0x24 = character 293be0[slot] (= slot, so bot i uses weapon
  preference row i), roster +0x28..+0x4c copy entity fields 444, 478, 66c
  (weapon), 670 (ammo), 6ec, 6f0, 43c, 664, 34 (life), 38 (armour). Without
  "start with gun", 15d9a0 removes every weapon (0x2d slots) and sets the
  current weapon to none.
- Roster +4 kills, +8 kills of human players, +0xc deaths, +0x10 streak,
  +0x14 longest streak, +0x18 hits landed (1ca3c0), +0x50 name.

### Respawn (2748f0)

Called from the entity death path 148860 when the dead entity is not human
(188c40 false). Picks a bot point (274620), creates a new `miner1` named as
the bot (154340/1524e0), links it to the same roster slot, sets +0x204
affiliation = 3, restores the copied fields (444, 478, weapon 66c, ammo 670,
6ec, 6f0, 43c, 664) and the stored life/armour (+0x34/+0x38; the decompile
shows them written to the dead entity, most likely a register mix-up in the
decompile); bot flag set; without "start with gun" strips weapons
(15d9a0). The corpse stays (ordinary corpse handling). There is no delay:
the new bot exists at once.

### Respawn points (174040, 274280, 274620)

- Section 0x700 records: uid, position, orientation, name, one byte, team
  (int), three flags. Up to 32 points (0x40 bytes at 0x1edd320): +0x3c player
  1, +0x3d player 2, +0x3e bot.
- 274620(entity, out pos, out orient, player): candidates are points whose
  flag matches (player 1, player 2, or bot for a non-human entity) and whose
  nearest live entity (2744f0) is beyond 2.1, then 1.1, then 0.1 units; the
  first pass with candidates picks one at random. None: point 0.

## 5. Human player

- Spawn 1dd930: point via 274620 (player flag), entity created by 1a3000 with
  the class chosen in the character menu (player +0x18) and skin (+0x134,
  158ab0; single player forces `parker`), player flag +0x7c |= 8, given the
  12mm handgun (`2ac464`) with the default ammo 8f4930 (1a2e90/1a2f40).
  Respawn fade state reset (+0x6442c..+0x6443d).
- Death (148860 -> 1a5f00): player entity cleared; in multiplayer the 12mm
  handgun view model is reloaded (1afae0).
- Respawn input (1a51b0): with no entity, primary or secondary fire after more
  than 1.0 s since death starts the respawn (1dd8f0: sound 0x40, fade timer).
  1ddd90 fades to white (alpha up to 165) over 0.15 s, respawns (1dd930), then
  fades back over 0.5 s.
- Dead prompt 1ddc00: the screen darkens to 192/255 over the first second,
  then "Press %s to respawn" (string 1f4c87c with the button name 292560) at
  y 350 (175 in split screen).
- Run speed in multiplayer: the walk/run cap +7e4 is a fixed 8.0 (156980)
  instead of the class value.

## 6. Scoring (274c50, from the death path 14c570)

Skipped once the match is over. For the dead entity: a bot gains a death and
loses its streak (274250). Credit goes to the killer handle +0x12cc:
- killer == victim: bot loses a kill (274230); human: 1a6b40 (suicide).
- killer is a bot: kill and streak (2741f0, longest streak kept); +8 human
  kills when the victim is human (274240).
- killer is a human: 1a6af0 (kill), 1a6b80 when the victim is human.
Hits landed (1ca3c0): each hit on a character adds one to the shooter's
roster +0x18 (bot) or player stats +0x14 (human).

## 7. Match update (1dc1a0, every frame) and end

- Scores 1db9d0: every human and every bot score, sorted descending (9254b0,
  count 2ac558). Leader = the top score (1ddbb0); rank of a score = 1ddb60.
- Time: when not paused, time left -= frame time; below zero ends the match
  (unless no limit).
- Kill limit: any human or bot at or above the limit ends the match.
- Announcer (1dbb60 -> 26e210, flat 2D voice; lines loaded in 1dbc50 from
  28f510: 0 mp_ann_01, 1 _02, 2 _03, 3 _04, 4 _05, 5 _07, 6 _08, 7 _09,
  8 _10, 9 _11, 10 mp_ann_allyourbase):
  - leader score changes to limit-5 -> line 8 (mp_ann_10); to limit-1 ->
    line 9 (mp_ann_11);
  - under one minute left -> line 0 once; under 31 s -> line 1 once; under
    15 s -> mp_ann_03 once (26e180, the long countdown); under 11 s -> line 2
    once (when not already played);
  - match over -> line 4 (mp_ann_05), then after it: one player ranked first
    -> line 5 (mp_ann_07); two players: both first -> allyourbase, player 1
    first -> line 6, player 2 first -> line 7.
- Match over: players' weapons put away, zoom and scanner off.
- HUD while running: clock at bottom left (multi_hud_time.tga at (17,400),
  m:ss at (47,402) and right-aligned at 41); split screen draws the divider
  bars at y 0xde..0xe2.
- End screen: fades in over 0.7..1.0 s; per player: name (+0x144) at
  (240 or centred, 40), labels 1f4ce70/1f4ce74 at (240,62)/(240,84), kills
  at (340,62), "rank/total" (`%d/%d`, total = humans + bots) at (340,84);
  winner art (winner_trim.tga, winner_win.vbm) when ranked first. Continue
  prompt 1f4ce78 at y 350; the continue button (pad 0xe) fades out over 1 s,
  then leaves to the multiplayer menu (163b30 0xc). Pause/quit in a match
  also returns to that menu (1b2b20 case 0xc -> 0x1a).
- Debug roster list 274a80 (flag 2acd14): name, kills, deaths per bot.

## 8. Items

- Touch pickup 16c2f0: outside multiplayer only the human picks items up; in
  multiplayer any character (type 0) does (16cba0).
- 16cba0: skips items flagged +0x28c bit 0, entities in vehicles/turrets and
  pickups blocked by geometry; runs the item's handler (class +0x38, default
  16e0e0). A non-human picker runs 16cb00: a bot in AI mode 7 or 4 whose
  life and armour are both above 95% returns to fighting (133d80).
- Respawn: item +0x27c (ms; the level loader 178220 stores -1 for an authored
  0 and seconds * 1000 otherwise): negative = removed when taken; positive
  = hidden (189ff0) and shown again when the timer
  +0x280 expires (16ce10 checks every 100 ms, sound 1).
- Spin: in multiplayer, items whose class has flag 2 rotate 0.75*pi rad/s
  (16c5e0).
- Dropped items (+0x28c bit 8): every 1.5 s, if more than 2 (multiplayer) or
  6 (single player) exist, the oldest fades out over 1 s (16d000/16d080/
  16c560).
- Handlers: weapons/ammo 16d970 (adds to inventory 1327e0; a bot then picks
  its preferred usable weapon, 154a20 -> 12a700); health 16dfb0 and armour
  16de80 add the item amount, scaled by difficulty only outside multiplayer;
  envirosuit 16dde0.

## 9. Damage

- 1888d0: in multiplayer, damage from a non-human attacker is scaled by
  28e980[skill] = 0.5, 0.6, 0.8, 1.0 (single player scales damage *to* the
  player by 28e970 instead).
- Weapon damage (1ce890): weapons.tbl `$Damage:` has two values (single
  player +0xf0, multiplayer +0xf4; `$Alt Damage:` +0xf8/+0xfc). Multiplayer
  uses the second value; an AI shooter's damage is also multiplied by the
  weapon's +0x108 scale.
- Friendly rules: collision filter 18bd00 ignores the 0x4000 no-collide
  pairing in multiplayer and character-vs-type-1 contacts are off; same
  affiliation does not protect anyone (132fb0, 133d80).

## 10. Bot AI

There is no separate bot brain: bots run the ordinary AI with multiplayer
branches. The AI struct is entity +0x270; fields below are AI-relative
(mode +0x1d0, target +0x210, weapon +4, flags +0x484, route list +0x358,
route cursor +0x35c, route mode +0x364).

### Targets

- 129410 (target choice, called by 12b580 every 28e9b0[skill] = 600, 450,
  300, 200 ms): every other live, visible entity; distance to human players
  weighted by 1 - skill/5 in multiplayer; nearest passing the line-of-sight
  check wins. If no human was chosen and a previous target (+0x480) exists,
  it is kept 2 times in 3.
- 12b830: in multiplayer "no human players" means none of the two player
  records has an entity.
- Hit by an attacker 132fb0: in multiplayer a 1-in-3 chance to react to the
  attacker at once (1339e0); otherwise, above 25 life, 1 in 5 sets +0x484
  bit 0x1000000 (seek next update, 131ac0). Same affiliation never protects.
- 131b70: hit by someone other than the current target: 1 in (8 - skill)
  sets the seek bit 0x1000000.
- 1339e0 (react to attacker): single player switches to mode 7 (charge the
  attacker); multiplayer instead runs to a `health` route (12e3f0).
- 133d80 (resume fighting): in multiplayer allies are not alerted; mode 3.
- 12bbb0 (mode 2): multiplayer goes straight to mode 3; 132de0 turns mode 2
  into 3 and clears +0x484 bits 0x0f800000 on every mode change.

### Routes (named waypoint lists)

- PS2 arenas carry named lists in RFL section 0x10000 (m_ps201 has 20; the
  PC maps have none): `weapon...`, `ammo...`, `health...` and free names. They
  index the PS2 path nodes (section 0x20000).
- 12e2a0(ai, prefix): lists whose name starts with prefix (up to 32), one at
  random; 12e210 sets list, cursor 0, mode 0 (one way); 136900 plans a path to
  the list's current node (fails when the plan has fewer than 2 nodes); on
  success mode 4 and +0x484 |= 0x800008. Wrappers: 12e3e0 `ammo`, 12e3f0
  `health`, 12e400 any list, 12e410 `weapon`.
- Mode 4 (12eed0): while bit 0x800000 is set no new target is looked for.
- Node reached (12e520): next node; at the end of a one-way list in
  multiplayer, if any other live entity is within 3 units the cursor goes
  back to node 1 (walk the list again), else the bot resumes fighting its
  last target (133d80, mode 3, which clears the bit).
- 1373c0 (path step) in multiplayer re-runs target choice (12b580) and stops
  following when it fires.

### Attack update (12ffd0, mode 3, every frame)

In order:
1. Target gone: multiplayer roams at once (12f8e0 forced).
2. Multiplayer: unless +0x484 bit 0x2000000, 12f5f0 (1 in 64): move to the
   path point near the target whose straight route keeps farthest from the
   target (12f460 over the list 7aac40, 2abee4 points); on success sets
   0x2000008 and skips the rest of the frame; while 0x2000000 is set the move
   is stepped (1373c0) until done.
3. Multiplayer: when the target is visible (12fa00) and timer +0x180 fires,
   1 in 4 reacts as if hit (1339e0 -> `health` route).
4. Ordinary movement/attack positioning (12b310, 128f60, 136bb0).
5. 12f810: weapon record 8f41dc/8f4240 + weapon * 0x530 gives the ammo type
   and its maximum; below a third of it, 1 in 64 runs an `ammo` route.
6. 12f750: 1 in 4, then rank = position of the current weapon in the bot's
   preference row (12a5d0; -2 when absent): rank -2 runs a `weapon` route;
   rank >= 0 runs one when rand % (rank + 2) == 0.
7. 12f8e0: 1 in 64 roams any list.
8. Fire (12ae50/12adc0) in movement states 2, 3 and 0xb, the same frame.
   12ae50 in multiplayer also flags +0x6f4 bit 0x8000000 when the target is
   within 3 units (12f960, melee range).

Steps 5-7 are skipped while bit 0x800000 is set. The random function 25b740
is newlib `rand` (31-bit; disassembled: seed * 0x41c64e6d + 0x3039), so the
rates above are per frame. A bot therefore fights in short bursts: it fires
on the first attack frame and usually leaves for a route within a fraction of
a second, picking items up along the way.

### Weapon preference (12a5d0, 12a700, 12a680)

Rows 28e760, 8 weapons per row, row = bot slot:
0-1 Shotgun, Machine Pistol, Sniper Rifle, Assault Rifle, Flamethrower,
Rocket Launcher, rail_gun, 12mm handgun; 2 Sniper, rail, Assault, MP,
Shotgun, Rocket, Flame, 12mm; 3 Assault, MP, Flame, Shotgun, Rocket, Sniper,
rail, 12mm; 4 MP, Assault, Shotgun, rail, Rocket, Flame, Sniper, 12mm;
5 Shotgun, Assault, Flame, Rocket, MP, Sniper, rail, 12mm; 6 Assault, Sniper,
rail, Rocket, Shotgun, Flame, MP, 12mm; 7 Assault, Flame, Rocket, Shotgun,
rail, Sniper, MP, 12mm; 8 Flame, rail, Rocket, Assault, Sniper, Shotgun, MP,
12mm; 9-10 rail, Rocket, Flame, Assault, Sniper, Shotgun, MP, 12mm; 11 rail,
Shotgun, Rocket, Flame, Assault, Sniper, MP, 12mm; 12 Rocket, rail, Flame,
Assault, Sniper, Shotgun, MP, 12mm; 13-15 Rocket, rail, Shotgun, Flame,
Assault, Sniper, MP, 12mm.

12a700 (choose weapon; 1 in 8 unless forced): first weapon of the row the
bot owns (1327e0 inventory) with clip or reserve ammo (12a680); switches to
it. No usable weapon: forced retry. A bot with no weapon gets the 12mm
handgun (12a590). After any weapon pickup in multiplayer the choice is forced
(154a20).

### Other multiplayer branches

- 185e20: entities never sleep by distance in multiplayer (single player
  idles AI beyond 900 units of the player).
- 1d8fb0 (aim assist candidates): 100 units in multiplayer (30 single
  player), any affiliation.
- 137aa0 (path step): in multiplayer, at each node reached, 1 in 2 flips the
  walker's +0x6f4 bit 8 (= AI +0x484 bit 8), which 1373c0 uses to face the
  target (bit set) or the path (bit clear) while walking.
- Movement animation (149cd0/149830): an armed fighting entity (149160) uses
  attack_stand/attack_walk/attack_run, and multiplayer always picks
  attack_walk/attack_run (3, 5); otherwise stand/walk/run (0, 2, 4). Walk or
  run follows the speed mode +0x7e8 (156980: 0 walk at class +0x50 * +0x54,
  1 run; in multiplayer the run maximum is 8.0, 2ab7bc). 12e210 starts a
  route running for bots (+0x734 bit 0x100000, set by 273f70/2748f0) and
  walking otherwise; 133d80 resumes fighting running.
  Port: scene_mp_locomotion_state, scene_mp_bot_speed, scene_mp_face_target.
- 149830/149cd0: multiplayer forces the movement animation set (3, 5).
- 176590: multiplayer traps (hangs) on malformed authored objects (debug).

## 11. Not traced

- Split-screen HUD/viewmodel offsets (1a9c20, 1abd00, 1acc70, 1af170,
  160f40 multiplayer parts) and the second player's controls.
- The multiplayer menus' character and arena screens (the Xbox already draws
  them from the PS2 art; the character choice is not wired).
- Exact meaning of entity +0x66c/+0x670 beyond weapon and ammo.

## 12. Xbox port status (2026-11-02)

In (scene_mp.inc, main.c): arenas with PS2 path, bot and route data; bot
count, roster and names; bots created as `miner1` (skinned-character classes
swapped as in 177010, authored weapon kept; scene_mp_bot_seeds); held guns
shown after a bot respawns (the drop record of its previous life no longer
hides the live gun); bot and player respawn points; scoring; kill and
time limits; respawn on Fire; results screen; route picking and the
target-lost roam; the kill-limit announcer lines (but keyed to the player's
score, not the leader's).

Differs from the PS2 or missing:
1. Bot skin: `miner1` uses its default skin as on the PS2 (no change).
2. Respawn: as on the PS2 the bot is back the moment its corpse forms and the
   corpse stays (it fades by the ordinary corpse rules). Port mechanism: the
   corpse's model moves to a spare model owner (multiplayer adds one per
   corpse slot) and the bot's slot is restored and registered again, rather
   than a new entity being created. Respawned bots keep their authored
   affiliation (PS2: 3).
3. Combat timing: ported. Multiplayer bots look for targets every 600, 450,
   300 or 200 ms by skill (12b580 / 28e9b0) and fire on the first frame they
   face the target (no half-second wait; scene_mp_first_shot). The PS2 facing
   test in 12a960 (12ac60: dot of the bot's facing +0x71c and the unit
   direction to the target below 0.9 refuses the shot) is close to the
   port's 30-degree horizontal cone, which is kept.
   Fighting on a route: ported. The PS2 path step 1373c0 in multiplayer
   runs 12b580 each step: the target choice on its skill timer (setting
   +0x484 bit 8 when the target is seen; entity +0x6f4 is the same word) and
   then 12ae50, the ordinary fire, on the current target; it stops following
   only when 12ae50 returns 1 (melee range). While the bot has a target
   (+0x6f4 bit 8, entity +0x480 = AI +0x210) it faces it as it walks (138e60
   with the target's position), so the fire check passes. Port: a weapon
   or roam route keeps the bot's target (scene_mp_route), multiplayer bots
   may acquire targets on a route (campaign_enemy_mode_admits), and the
   route steering faces the target (scene_mp_route_aim, as tactical routes
   do). Attack-update steps 6 and 7 (weapon seek, 1-in-64 roam) are now on
   (`SCENE_MP_WEAPON_SEEK` 1). dm01, 4 bots, 150 s: 434 bot shots and 25 bot
   kills with 22 weapon and 39 roam routes (207 and 13 with the seeks off;
   38 and 2 with them on before route fighting). Still missing: steps 2 and
   3 (the 1-in-64 move away from the target, the 1-in-4 `health` reaction)
   and the hit reaction's `health` route; the melee-range stop.
4. Items: bots pick up health, armour, weapons and ammo (scene_mp_pickups.inc:
   16dfb0/16de80 class-maximum rule, 16d970 inventory add, then the forced
   12a700 preferred-weapon choice; 16cb00 back to fighting above 95%). dm01,
   4 bots, 150 s: 2 health, 5 armour and 18 weapon/ammo pickups, 1 weapon
   switch; 527 bot shots and 34 bot kills (434 and 25 before). Port touch
   test is the player's 2-unit radius; remote charges and the riot shield are
   left to the player; the unforced 1-in-8 weapon choice inside 12a960 and
   pickup sounds are not ported. Items respawn and spin in multiplayer
   (scene_mp_pickups.inc): the authored seconds (178220 turns 0 into -1, else
   seconds * 1000 ms) hide a taken item until the timer runs out; items
   with items.tbl `spins_in_multi` (flag bit 2) turn 0.75 * pi rad/s about
   the vertical. dm01, 150 s: 80 items reappeared, 46 spinning at the end,
   75 bot weapon/ammo pickups and 12 bot weapon switches; two captures half
   a second apart show a spinning item turned. Reappear sound and the
   dropped-item cap are not ported.
5. Damage: bot damage is not scaled by skill; multiplayer weapon damage
   values are not used.
6. Player: no character choice (always the single-player Parker), the run
   speed cap is not 8.0, no respawn fade or "Press Fire to respawn" prompt,
   1 s respawn delay not enforced.
7. Announcer: only the kill-limit lines, keyed to the player; time callouts,
   match-over and winner lines missing.
8. End screen: the port shows its own results list, not the PS2 layout
   (rank/total, winner art, continue fade).
9. Bot weapon choice: the forced choice after a pickup is ported; the
   unforced 1-in-8 choice inside 12a960 and the hits-landed stat are not.

### Port stand-ins to replace with PS2 behaviour (audit, 2026-11-02)

Rule: every multiplayer behaviour comes from the PS2 code; port inventions
are removed or replaced. Status after the overnight pass (2026-11-03):

Done (now from the PS2 code):
- Random numbers: PS2 rand 25b740 (seed * 0x41c64e6d + 0x3039, low 31
  bits), seeded at level start from the port's update count since boot
  (PS2 282380: frames since boot); chance tests use the PS2 comparisons
  (rand < 0x2aaaaaaa for 1 in 3, < 0x1fffffff for 1 in 4, < 0x1ffffff for
  1 in 64, & 0x3f). Port note: the PS2 shares one sequence with every
  system; the port's multiplayer draws have their own.
- No idle mode: without a target a bot roams a random list at once
  (12ffd0 step 1 -> 12f8e0 forced; 12bbb0 turns mode 2 into 3 in
  multiplayer); an arena without lists leaves it standing. The port's
  interruptible "idle walk" and the nearest-to-player seek are removed.
- Attack-update steps 5-7: ammo route (12f810: reserve of the weapon's ammo
  type below a third of its maximum, rand < 0x1ffffff), weapon route
  (12f750), roam (12f8e0), in PS2 order.
- Pickups: physics touch (item sphere 0.25 against the character's
  collision spheres; 16c760, 189d50 -> 16c2f0), line of sight from the item
  to the picker's eye (194fd0), no_pickup flag; every item the default
  handler serves (remote charges and the riot shield included), envirosuit
  (16dde0: armour to the class maximum); pickup sound for any picker;
  reappear sound 1; respawn timers checked when more than 99 ms passed
  (16ce10). Port note: the doctor uniform's handler is not traced, so bots
  leave it; sound slots 1 and 0x40 assume the PC sounds.tbl order.
- Fire facing: 12a960's 0.9 dot of the full eye facing replaces the port's
  30-degree horizontal cone in multiplayer.
- Damage: weapons use the PS2 weapons.tbl's second `$Damage:` /
  `$Alt Damage:` value (1ce890), read from the player's PS2 disc into
  ps2menu.vpp (ps2_damage.txt, tools/build_ps2_menu_pack.py); the PC
  `$Damage Multi:` is only a fallback without the pack. The PS2 values
  differ a lot from the PC ones (12mm 40 vs 25, rail 2550 vs 587, heavy
  machine gun 100 vs 45). Damage from bots is scaled by 28e980[skill] =
  0.5, 0.6, 0.8, 1.0 (1888d0); the single-player 28e970 scale to the player
  is off in multiplayer.
- Announcer: leader score (1ddbb0) for lines 8/9; time callouts in the last
  minute (line 0, under 31 s line 1, both waiting for other lines; under
  11 s line 2); match over line 4 stopping others, then the winner line 5
  queued until line 4 ends when the human ranks first (1dc1a0, 1dbb60).
- Player: respawn on primary or secondary fire more than 1 s after death,
  sound 0x40, fade to white (alpha 165) over 0.15 s, respawn, fade back over
  0.5 s (1a51b0, 1dd8f0, 1ddd90); run maximum 8.0 (156980).
- Movement: bots run at 8.0 with attack_run/attack_stand while fighting
  (149cd0/149830), face the target on a route only while the facing bit is
  set (12b580, flipped 1 in 2 per node by 137aa0).
- Game clock: each update covers the real time owed (PS2 1fd9e0 frame time)
  instead of dropping it; port cap 2/60 s per update (several port weapon
  updates reject steps above 0.05 s; the PS2 cap is 0.25 s).

- Target choice (129410 on the 12b580 skill timer) replaces the port's
  single-player sight and opposed acquisition for multiplayer bots: range
  25, human distance weighted by 1 - skill/5, hostility table 28e750
  [0,0,1,1 / 0,0,0,0 / 1,0,0,1 / 1,1,1,1] on friendliness plus the
  attacker list (+0x3d4, filled from hits), field of view from the
  instance FOV (128eb0, +0x764 = cos(FOV/2), 177010), line of sight to the
  candidate's eye from the body origin or eye alternately (194fd0), keep the
  previous target when rand % 3 != 0 and no human was chosen; a found
  target replaces the current one, none found leaves it. The player is
  friendliness 2 (PS2 1a2fb0), so the arena bots (0) start hostile to the
  player but not to each other; respawned bots take 3 (2748f0), hostile to
  everyone. The facing bit comes from this scan seeing a target (12b580).
  Port note: the stealth/hidden-player and mode 0xb clauses are left out.
- Dropped-item cap (16d000/16d080): every 1.5 s, more than 2 drops starts
  the oldest's 1 s fade, then it is removed. Port note: the fade is not
  drawn (the drop draw has no alpha); one drop record per bot.

- Step-away start node: 138b30 -> 138500 (node the bot stands in, else
  the best edge pair, taking the later of the pair as 138b30 passes one
  address for both, else the nearest node within 25 seen with a 2.5
  sweep), via the port's same PC routine rf_entity_navigation_select,
  mode 1. Port note: the second node +0x5c0 is not kept; no fallback to
  other candidates when the best cannot be planned.
- Hit reaction's 1-in-4 `health` branch: a humanoid (class flag 0x20000,
  15a3b0) whose weapon is not melee-only (134100: weapon record +0x244 bit
  0x20 `melee`, no secondary) runs a `health` route when rand & 3 == 0.
- Movement animation state every frame (149cd0): stand while not moving,
  else walk or run by the speed mode, the attack set while fighting
  (multiplayer 149830). Port note: "not moving" is the body not having
  moved in the last update (PS2: velocity +0x634 exactly zero).

- Attack-update step 3 (12ffd0): target out of attack range (12fa00:
  distance over 132b10 -> 12a520's range: the authored combat range
  override, else the weapon's AI attack range +0x4f4, 0.5 without a
  weapon) with timer +0x180 expired (12b310 sets it 500 ms ahead while the
  target is in range; 1ffa60 treats an unset timer as not expired) and
  rand < 0x1fffffff -> `health` route. Port note: 12fa00's not-seen clause
  (timer +0x188), its 3-unit allowance for one target flag, the 1.5 range
  factor and 12b310's zero reset for AI movement sub-state +0x204 are left
  out (the port does not track that sub-state).
- Multiplayer weapon values (1c6e10, run at match start): every weapon
  takes the second value of $Max Ammo (+0x240), $AI Attack Range (+0x4f4),
  $Clip Size (+0x80), $Spread Degrees, $Alt Spread Degrees, $AI Spread
  Degrees, $AI Alt Spread Degrees and $AI Damage Scale (+0x108). They come
  from the PS2 weapons.tbl in ps2_damage.txt and are applied to the port's
  primary definitions and supply table (scene_mp_weapon_damage,
  scene_mp_weapon_supply). Port note: AI alt spread has no port field.

Still open (PS2 behaviour traced or located, not ported):
1. Upper-body aim (151110 -> 150c30, traced): while 14f210 holds (target
   present, at least 1.0 higher or lower, weapon not the riot shield, and
   more), the eye facing (+0x71c) is taken into the entity frame (+0x48)
   and each spine bone's frame (class +0x1028, +0x102c; 1fb210 rows dot);
   the pitch atan2(y, z) is halved for the first bone, used whole for the
   second, each clamped to -0.785..0.45 rad, and applied as a bone override
   (basis rotated about the bone's X axis by 1f86f0 -> 1fbba0, weight
   ramping 0..1 over 0.75 s, entry +0x1394/+0x13b8/+0x13bc), the same
   override the port's rf_model_override_pose applies. Not ported: the
   rotation sign convention of 1fbba0 is not established and the result
   cannot yet be checked on screen.
2. Player: character choice from the menu; the respawn loadout (1a2e90/
   1a2f40 give the 12mm handgun with 8f4930, which reads as weapon record 1
   +0x240, the remote charge detonator's max ammo in the PS2 table order,
   so the field or index is not yet understood); the "Press Fire to
   respawn" prompt (strings.tbl 3; draw site not found yet).
3. End screen in the PS2 layout (rank/total, winner art, continue fade).
