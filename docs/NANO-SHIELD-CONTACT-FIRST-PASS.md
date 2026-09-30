# Capek firearm shield contact

Player hitscan, precision weapons, NPC direct firearms and NPC shotgun pellets
now consume active nano-shield armor before generic NPC health damage. The
contact that exhausts the shield is consumed; a later contact can damage health.
Railgun contacts debit100 armor, independent of ordinary railgun damage.
Invulnerable actors consume the contact without losing armor.

This uses the existing immunity predicate: positive armor, the entity-class
nano-shield bit and the actor shield-disable bit clear. Event76 still only
changes the disable bit; it does not refill armor. Generic damage immunity stays
read-only, so blast/script damage does not accidentally drain shield armor.
Melee and the Riot Stick retain their existing immunity handling; their contact
rules have not been inferred from the firearm branch.

## Binary evidence and port choices

Original RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Contact branch `0x4c5a90..0x4c5b5e` adjusts the named rail weapon to100, skips
debit for object flag4 and subtracts armor before ordinary damage. Predicate
`0x42cca0` and event handlers `0x4b9b60`/`0x4ba380` provide the existing admission
and toggle behavior. These addresses come from the retained binary research;
no original-game screenshots or runtime comparison were used.

The port clamps exhausted armor to0 for its nonnegative vitals/save contract.
The current rail adapter stops at an active nano-shield contact; full original
rail continuation around shields has not been established. Neither choice is
claimed as an exact reconstruction of every original contact behavior.

## Focused Xbox check

`python tools/xemu_nano_shield.py` uses an isolated snapshot HDD and stock64MiB
XEMU. Authored L8S4 UnHide10358 reveals Capek8359. An opt-in guest fixture stages
clear firearm aim,150 shield armor and finite ammunition. Ordinary player fire
and collision code process rail shots at frames30/60 and a sniper shot at90.
Expected results: armor150→50→0, unchanged health on both shielded contacts,
then health damage with armor0. Every shot must consume exactly one round.

Result: PASS in `artifacts/xemu/nano-shield-20260930-102647/report.json`.
The110-frame run retained4772 free pages (18.64MiB). Both rail contacts left
health100 unchanged while armor fell150→50→0; the following sniper shot reduced
health100→-150 and entered the normal death path. Ammo was2→1,1→0 and2→1.
Contact telemetry recorded exactly two shield contacts and one break. NXDK
compilation succeeded; all disc flags were restored after the run.

The harness restores all disc flags afterward. It neither traverses a campaign
route nor validates interactive aiming, images, animations or audio.

## Remaining work

- Projectile-specific contacts, including remote/grenade zero-debit rules.
- Shield constant/hit/break presentation and associated sound.
- Capek's hover/run and class-speed reconciliation when his shield breaks.
- Live NPC-fire and invulnerable-contact coverage, shield toggles and ordinary
  save/load of the partially depleted shield in an authored encounter.
