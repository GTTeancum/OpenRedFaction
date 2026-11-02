# PS2 multiplayer bots and match rules (SLUS-20073)

Traced 2026-11-02 from the PS2 executable (Ghidra project `local/ghidra-ps2`,
whole-program decompile `artifacts/analysis/ps2_slus20073/all.c`; EE-only
`lq/sq/por/paddub` patched to MIPS64 equivalents, `$gp` = 0x2b36f0,
non-returning-function discovery disabled). Addresses are PS2 virtual
addresses. The Xbox port uses these as the reference for one-player
multiplayer against bots.

## Maps

- The PS2 arenas are the PC multiplayer maps with authored bot data added.
  m_ps201 = dm01 and m_ps202 = dm02 have byte-identical respawn sections;
  m_ps203 dm03, m_ps204 dm04, m_ps205 dm05, m_ps207 dm06, m_ps208 dm07 and
  m_ps210 ctf02 share most respawn points (edited for the PS2).
- Each adds a navigation section (0x20000, same layout as PC v180) and a small
  entity section (0x30000) holding the bot characters (e.g. m_ps201: `elite`,
  `guard1` with `Machine Pistol`).
- Respawn points (section 0x700, reader 174040): uid, position, orientation,
  name, one byte, team (int), then three flags. 274280 stores up to 32 points
  (0x40 bytes each at 0x1edd320): flags at +0x3c (player 1), +0x3d
  (player 2), +0x3e (bot).

## Respawn point choice (274620)

Candidates are points whose flag matches the spawner: +0x3c for player 1,
+0x3d for player 2, +0x3e for a bot (an entity for which 188c40 is false,
i.e. not a human player). Passes with a distance threshold of 2.1, 1.1, 0.1
(274a40/2744f0 measures the point against nearby objects); the first pass with
any candidates picks one at random (25b740). With no candidates, point 0.

## Bots

- Match start 2740e0: walks the entity list; every non-human entity becomes a
  bot (273f70) until the chosen bot count (options +0, 1ddb00) is reached; the
  rest are destroyed (thunk 184850).
- Roster 0x1edcdd0, 16 slots of 0x54: +0 used, +4 kills, +8 kills of human
  players, +0xc deaths, +0x10 kill streak, +0x14 longest streak, +0x20 entity,
  +0x24 character (table 293be0), +0x28..+0x4c copies of entity fields
  444/478/66c/670/6ec/6f0/43c/664/34/38 (skill settings), +0x50 name (table
  293ba0, starting "Myles"). 273f70 also sets entity+12fc = slot and
  entity+734 |= 0x100000 (bot flag); in multiplayer without "start with gun"
  it calls 15d9a0.
- Respawn 2748f0, called from the entity death/cleanup path (~1bxxxx, after
  corpse handling) when the dead entity is not human and multiplayer is on:
  picks a bot point (274620), creates a new entity of class `miner1` (154340,
  1524e0) named as the bot, links it to the same roster slot, sets
  entity+204 = 3, restores the roster's copied fields and the bot flag.

## Scoring (274c50, per death)

- Victim is a bot: roster deaths +1, streak reset (274250).
- Killer (entity+12cc) is the victim: suicide; a bot loses a kill (274230), a
  human player's record loses one (1a6b40).
- Killer is a bot: kills +1, streak +1, longest streak updated (2741f0); if
  the victim is human, kills of humans +1 (274240).
- Killer is a human player: player record kills +1 (1a6af0); victim human:
  1a6b80.

## Match (1dbf20 setup, 1dc0e0 per frame)

- Options (925490, 1ddaa0): bots, skill (also global difficulty 2abeec), kill
  limit, time limit in minutes, start with gun. Defaults 1ddb20: 2, 2, 15, 15,
  yes.
- Time: minutes x 60, decremented by the frame time when not paused; below 0
  ends the match. 0 minutes = no limit.
- Kill limit: ends the match when any human player or any bot reaches it;
  messages at limit-5 and limit-1 (1dbb60 8/9). 0 = no limit.
- End: ranks the player (1ddb60) and shows the end messages (28f54c/28f588).

## Bot AI (ordinary AI plus multiplayer branches)

There is no separate bot brain. Entity+204 is the affiliation; bots get 3.
The AI (0x128000-0x138000) has ~25 functions with multiplayer
(`DAT_002ac530`) branches. Traced so far:

- Target choice 129410: walks every entity except itself; skips dead,
  flagged (+7c bit 2), state 440 == 1 and non-hostile (1291f0) entities;
  weights distance by 1 - skill/5 for human players in multiplayer; picks the
  nearest that passes the visibility/route checks. In multiplayer, if no
  human was picked and a previous target (+480) exists, keeps it 2 times in 3.
- Hit by an attacker 132fb0: in multiplayer a 1-in-3 chance to switch to the
  attacker at once; above 25 health a 1-in-5 chance to set flag 0x1000000
  (seek); the same-affiliation exclusion applies only outside multiplayer.
- Ally alert 133d80: alerting same-affiliation allies is skipped in
  multiplayer.
- Idle in AI mode 4 (12e520): in multiplayer searches every live entity for
  the nearest target.
- Mode change 132de0: in multiplayer mode 2 becomes 3 and bits 0x0f800000 of
  +484 clear.
- Seek / wander 12f5f0, 12f750, 12f8e0, 131ac0, 132ba0, 137aa0: multiplayer
  goal selection (random roams, item seeking); 131b70 sets a seek flag with
  probability 1/(8 - skill) when hit by someone other than the current target.
- Skill (options +4) also sets the global difficulty (2abeec).

Not yet traced: the goal choice inside 12f5f0 (paths or items) and the
remaining branches (12a960, 12ae50, 12b830, 12bbb0, 12d290, 12fc60, 12ffd0,
1373c0).
