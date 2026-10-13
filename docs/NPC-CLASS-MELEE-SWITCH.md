# Authored class-default close-range melee

## Status and scope

Source-written October 12, 2026, against d15e77c5c2eabd49f1d62e46e73d8d9a669c6c69.
Uncompiled and runtime-unverified. The parent owns the next hourly Xbox
compile/package and short startup smoke; functional coverage belongs to testers.
No worker builds, tests, fixtures, emulator sessions, HDD access, images, campaign
routes, original-input writes or publication were performed.

This slice makes three existing class-default attacks operational: Tankbot
Smash, Rock Snake Smash and Big Rock Snake Smash. It reuses the existing delayed
melee runtime and contact/damage services. Drone's explicit placed Smash primary
is unchanged; no vehicle, GeoMod or monitor code is involved.

## Original evidence

The installed RF.exe SHA256 is
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
The installed tables.vpp SHA256 is
0adafd7196b75a99d10e650e5f8e9f672df7b9fbd1c772df33b33d388896571e.
Read-only VPP extraction locates entity.tbl at256000/374640 bytes and
weapons.tbl at1013760/108890 bytes.

Original primary-AI routine401580 resolves its retained target at401589–401590,
reads actor class+294 then class melee+90 at40159c–4015a2, and returns unchanged
when no melee definition or actor target exists.4015c9 loads that weapon's AI
range;4015cf–4015d8 compares the two actor+3c positions through4faf00.
4015dd–4015fa compares range squared to distance squared: strictly inside the
range selects melee; equality/outside selects class primary.401604–401648 resets
the old weapon, clears the attack/burst waits, selects class+90 and rebinds
through42ab20.401652–401677 resets and returns to class+88 outside range.
This is an actual runtime selector, not merely unused catalog metadata.

Installed class/default/overlay evidence:

- Tankbot: entity.tbl4028,4047–4049; default Tankbot Chaingun/Tankbot Missile/
  Tankbot Smash.4074–4075 overrides fire_stand with tbot_attack_melee.mvf and
  tbot attack melee Foley.
- Rock Snake: entity.tbl4458,4477–4479; default Rock Snake Spit/empty/
  Rock Snake Smash.4494–4495 overrides fire_stand with rsnk_attack_bite.mvf
  and RSnake Bite Foley.
- Big Snake: entity.tbl4678,4696–4698; default Big Rock Snake Spit/empty/
  Big Rock Snake Smash.4713–4714 overrides fire_stand with
  bsnk_attack_bite.mvf and BSnake Bite Foley.

Existing decoded original placements are Tankbot UID10696 in L7S4 and
Rock Snake UID5255/Big Snake UID7007 in L10S4. See
[Tankbot primary](TANKBOT-PRIMARY-FIRE.md) and
[Snake Spit](ROCK-SNAKE-SPIT-FIRST-PASS.md) for exact RFL provenance and selected
ranged loadouts. Existing startup grants already own each class melee weapon;
this implementation does not add inventory or alter original records.

Installed melee profiles:

| Family | Range | Bash damage | Fire wait | Impact offset | Flags |
| --- | ---: | ---: | ---: | ---: | --- |
| Tankbot Smash |5|1200|1.5s|400ms|0x620; Flags2 8|
| Rock Snake Smash |7|1600|2.5s|800ms|0x20; Flags2 8|
| Big Rock Snake Smash |10|1600|2.5s|600ms|0x20; Flags2 8|

Their declarations begin at weapons.tbl2205,2022 and2084 respectively. No
magazine/reload, alternate impact, burst, piercing or explicit AI damage scale
is authored; the existing parser supplies scale1 and one projectile. Snake
flags are melee only, unlike Tankbot's additional underwater/from_eye flags.
The profile loader validates the independent exact flags and values.
Original425b9a exempts melee from ordinary ammunition availability;
426000–426196 owns its delay/action without the ordinary4257c0 debit.
42607d–4260c6 schedules each positive impact offset from accepted game time.

## Live integration

The existing finite creature registry appends three profiles after its four
legacy Reeper/Mutant entries. The same selection chain now admits these exact
owned weapons; no generic unknown-weapon fallback is added. An immutable-pair
helper verifies exact class name, class default ranged/melee IDs, current
selection in that pair, and ownership of both weapons. Supplied inventory
allows the same helper to validate a saved candidate independently of the
outgoing live selection.

The switch helper runs for an already-alerted living on-foot actor with a
retained player/NPC target. It uses the current body's XYZ position distance,
strictly compares against the authored melee range, and selects the matching
class primary outside range. Full registry/entity/body identity is required for
both endpoints. Point/once orders, mounted targets and unrelated weapon
selections do not acquire a new approximation. Acquisition and affiliation
remain under existing owners.

Switching waits while an accepted Spit windup, an armed/callback melee slot,
script animation, reload, burn, pain lock or active nonloop clip still belongs
to the outgoing selection. A frame-local wait suppresses new primary acceptance
after independent secondary and ammo/reload service; it cannot perpetually
restart the outgoing fire clip. The ordinary global windup/impact services
continue before this gate. No wait flag is saved because each next frame derives
it from real current owners and target distance.

Once safe, motion selection is staged through the existing class/weapon overlay,
the new fire clip is demanded within the existing cache budget, and retained
weapon reset runs before selection publication. Actual authored bite/Smash
animation and action Foley are consumed by the existing fire presentation.
The old burst count and navigation refresh are cleared. Already-paid primary
cadence is retained across both directions, preventing boundary oscillation
from obtaining free attacks. Secondary inventory, independent Missile wait,
released Spit flights, target/order and actor physics are not reset.

The preserve-cadence/defer-until-owned-work-finishes policy is a bounded port
choice. It deliberately does not reproduce the original immediate timer
clearing at401611–401634. This avoids dropping paid work or mislabeling an
outgoing action under the new RFNC mapping.

## Save compatibility

RFNC already persists selected weapon, inventory, playback and combat state.
No NPC row or restore-stage layout is enlarged.
RFAP14 appends semantic families to the existing kind26 receipt:

- 0 Drone Smash and1–4 Reeper/Mutant keep their prior meanings.
- 5 Tankbot Smash,6 Rock Snake Smash,7 Big Rock Snake Smash.

The header remains16 bytes, every row112 bytes, the controller and live pending
banks64 slots, and the whole-save/storage/restore budgets unchanged. The
profile ceiling admits the independently validated2.5-second Snake cadence;
impact delays remain at most800ms. No resource mask or new render pool is added.

The writer emits14 only when a new-family receipt is present. Existing-only
saves retain the prior smallest required version through13. All readers accept
RFAP1–13, while new families under an older version are rejected in common
preflight, resource-demand preflight and full receipt decoding. Old binaries
reject version14 instead of interpreting the new families.

Capture and candidate admission additionally join each new family to its exact
class/default pair and supplied owned inventory. The previous full-generation
source/target, order, cadence/RNG, pending-slot, duplicate/remaining-mask and
candidate life/seat joins remain in force. Released Spit flights continue to
join their authored source even when that source currently holds melee;
they do not require the outgoing selected Spit controller.

Only final successful storage closure publishes restored pending slots with
fresh tickets and exact deadlines. No attack, sound, animation, damage or ammo
is replayed. Failed save/load preparation does not cancel pending work. Callback
reservations and unsupported owners still refuse, and capacity errors are not
relaxed. Existing snapshot/export call sites retain their two-argument
count-only capture interface through a local wrapper.

## Limits

The shared first-playable intended-body/ray contact, aim, eye-distance reach,
cover and shield handling are reused. This does not reconstruct swept limbs,
exact bite volumes, every original scheduler transition or retail melee effects.
Switch distance uses body origins while actual contact retains the existing
melee geometry; these are distinct gates. Suspended nonloop playback can defer
a switch until its ordinary animation service resumes. Broader functional and
campaign coverage remains with testers.
