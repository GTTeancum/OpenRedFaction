# Capek weapon shield contact

Player hitscan, precision weapons, NPC direct firearms and NPC shotgun pellets
now consume active nano-shield armor before generic NPC health damage. The
contact that exhausts the shield is consumed; a later contact can damage health.
Railgun contacts debit100 armor, independent of ordinary railgun damage.
Invulnerable actors consume the contact without losing armor.

Player rockets, NPC rockets/Tankbot missiles and Fusion flights now use the same
prepass on direct NPC contact. A consumed shield contact retires the flight
before ordinary impact, blast and terrain-cut handling. This prevents a
shield-breaking missile from immediately spilling blast damage into health.

This uses the existing immunity predicate: positive armor, the entity-class
nano-shield bit and the actor shield-disable bit clear. Event76 still only
changes the disable bit; it does not refill armor. Generic damage immunity stays
read-only. A separate [shared radial Nano consumer](RADIAL-NANO-SHIELD.md) now
has independently source-reviewed, parent-integrated positive-falloff shield
debit before generic damage, awaiting 10:00 compilation and runtime.
Scripted direct damage that only calls the generic path still cannot debit Nano
armor; scripted explosions using the shared radial service use its explicit
consumer after integration.
Qualified player Riot primary and ordinary/delayed NPC primary melee now have
independently source-reviewed, parent-integrated direct debit consumers, grounded
in the original melee collision chain. Compilation and runtime remain unverified;
see [Primary melee Nano shield damage](MELEE-NANO-SHIELD.md). Held Riot alternate
retains its provisional immunity path pending contact/cadence reconstruction.

## Binary evidence and port choices

Original RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Contact branch `0x4c5a90..0x4c5b5e` adjusts the named rail weapon to100, skips
debit for object flag4 and subtracts armor before ordinary damage. The early return at
`0x4c5b54..0x4c5b5d` bypasses the later direct/radial/impact requests. The shared
flight step already retires terminal contacts, so shield interception must not
schedule an additional explosion on that path. Predicate
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
clear firearm aim,150 shield armor and finite ammunition. Other NPCs are hidden
in this isolated test setup; ordinary encounters are not verified by this check.
Ordinary player fire
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

The `--rockets` variant substitutes two actual rocket flights and checks armor
against their loaded weapon damage, waits for flight impact before recording
health, then fires the sniper. It requires exactly two launches/two contacts,
no surviving flights and no explosion/terrain-cut count. The fixture checks the
full projectile sweep, including its radius, instead of accepting a clear thin
ray as proof of rocket clearance. Rocket resources are
demand-loaded only for the opt-in fixture. The four-byte UID fixture remains
compatible; an optional second word1 selects rocket mode.

Rocket result: PASS in
`artifacts/xemu/nano-shield-rockets-20260930-104615/report.json`.
Two400-damage rocket flights reduce armor600→200→0, preserving health100 on
both contacts. No ordinary blast or terrain cut occurs; both flights retire.
The final sniper shot reduces health100→-150 through the ordinary death path.
All three shots consume one round. The110-frame stock64MiB run retained
4884 free pages (19.08MiB), restored the test disc, and used no images.
NPC missile and Fusion adapters compile but were not independently exercised.

## Remote-charge interception

Remote actor contacts now run the shield prepass with adjusted amount0 before
publishing a sticky host, impact sound or explosion. Active-shield contact marks
the charge dead/inactive and clears attachment/host state, keeping it out of
ordinary rendering, save capture and detonator selection. It does not debit
armor or health. Shield-OFF actor contacts retain ordinary sticky attachment.

The original contact branch at `0x4c5a90..0x4c5b5d` names Remote Charge as a
zero-amount case and returns before the sticky branch. The scene adapter uses
the existing flight/contact implementation; it does not change saved charge
layout or introduce a second ammunition pool.

`python tools/xemu_nano_shield.py --remotes` stages two releases using the
ordinary launch/ammo service, toggles shield OFF before the second, then sends
one process-local fire edge to the ordinary detonator. Capek is isolated from
other actors, aim and initial armor are staged, and throw-animation timing is
outside this check. Required outcomes: one absorbed charge, one attachment,
one detonation, no remaining active charges, and armor/health unchanged until
the unshielded detonation. No images or campaign traversal are used.

Remote result: PASS in
`artifacts/xemu/nano-shield-remotes-20260930-105746/report.json`.
Two launches produced one shield absorption, one ordinary attachment and one
ordinary detonation, with no remaining active charge. Reserve went3→2→1 and
stayed1 on detonation. The110-frame run retained4699 free pages (18.36MiB), and
the disc flags were restored. This does not prove moving-host save/reload,
ordinary throw animation timing, shield effects or audio output.

## Remaining work

- Other projectile families and shield presentation remain separate from the verified contact paths.
- Normal/alternate grenade contacts now pass the isolated Xbox check documented
  in [Grenade object contacts](GRENADE-OBJECT-CONTACT-FIRST-PASS.md); other victim/source/save combinations remain.
- Shield constant/hit/break presentation and associated sound.
- Capek's exact break-owned fall and persistent run normalization are now
  source-written with explicit owner/history save continuation, owner-local
  shielded8.0 initialization and authored0.3 base restoration in
  [Capek shield movement](CAPEK-SHIELD-MOVEMENT.md), pending Xbox compilation
  and runtime. Original delayed live-speed refresh is retained; the brief
  unresolved break-speed transient remains unsupported for saving.
- Live NPC-fire, Fusion and invulnerable-contact coverage, shield toggles and ordinary
  save/load of the partially depleted shield in an authored encounter.
