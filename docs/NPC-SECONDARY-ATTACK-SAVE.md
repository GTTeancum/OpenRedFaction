# NPC secondary-attack checkpoint ownership

Source-written against `1f3eb0b0ee5ddc76d3aa4320978e1eb0b9063d50` in the external
implementation directory. Compilation and gameplay are unverified. No builds,
syntax checks, tests, fixtures, emulator runs, images, HDD operations or
real-repository edits were performed. Parent owns integration and the hourly
compile/package rollup. Functional coverage belongs to testers.

## Implemented scope

Ordinary composed saves now retain the implemented Drone/Tankbot secondary
Missile owner, independently of primary-fire state:

- Drone's existing 64-slot phase-2 cooldown bank, preserving exact bank slots
  and the remaining positive part of its four-second millisecond deadline.
- Tankbot's independent per-NPC cooldown, including accepted forced Shoot_Once
  launches, with a bounded 64-receipt checkpoint admission limit.
- Drone Missile straight flights in the existing shared eight-slot NPC rocket
  pool, retaining their position, velocity, basis, lifetime, raw accepted damage,
  source and liquid query state.
- Tankbot Missile flights use the same strengthened RFAP9 source, candidate and
  exact-profile checks, whether or not the independent cooldown has expired.
- Current paid reserve values join the complete candidate RFNC inventory.
  Publication never debits, reloads, grants ammunition or replays launch logic.

Cooldowns are independent of target, combat order, current primary/secondary
selection, alert, holster, pain, visibility and source life. A registered corpse
or switched source may retain a paid wait/flight if the existing RFNC and history
owners admit it. Removed/retired sources still have no historical-source receipt
and refuse. Forced Tankbot ownership keeps the existing registered-eye/ammo
contract; an allocated physics body is not newly required for that paid owner.
The current RFNC's other body, combat, movement and animation limits still apply.

The live Tankbot implementation stays in each allocated NPC; the checkpoint
limit does not introduce a live cooldown bank or stop eligible shots. A save
with more than 64 unexpired Tankbot owners refuses without modifying anything.
Drone's live 64-slot bank, shared eight-flight pool, finite ammo and paid-only
acceptance remain unchanged. Expired waits are omitted read-only, matching the
next ordinary service; no save, failed preparation or failed storage close
clears or postpones a live wait.

## Provenance and unchanged gameplay

See `DRONE-SECONDARY-FIRE.md`, `DRONE-MISSILE-AUDIO.md`,
`TANKBOT-SECONDARY-FIRE.md` and `TANKBOT-PRIMARY-FIRE.md` for the reconstructed
consumers and existing approximations. RFAP is this port's checkpoint format,
not a claim about original RF.exe save bytes.

Original ordinary combat405af0 invokes secondary406330/406390/426ca0 after
primary dispatch. Secondary admission owns its separate deadline, range,
blast-plus-body clearance, alignment and LOS. Shoot_Once4badd5..4baddd overrides
the old deadline before the forced factory call; the implemented owner refreshes
only after successful finite-ammo publication. No queue, dry-fire, pool-refusal
or target-suppression behavior changes here.

The installed Drone profile is explicitly not homing. Its exact metadata loader
admits DrillMissile01.VFX, 15cm rocket reserve12, damage15/explosive3, speed20,
lifetime5, radius0.15, blast2, crater5, range30 and four-second cadence. Tankbot
uses reserve36, damage25/explosive3, speed15, lifetime6, blast3, crater5, range150
and four-second cadence. Its loaded collision radius remains authoritative.
Both retain current eye-origin straight aim. No homing target, predictive aim,
secondary muzzle substitute or primary-animation change is introduced.

## RFAP9 format

RFWC3, the 16-byte RFAP header and 112-byte row size remain unchanged. Version9
is produced whenever any secondary cooldown or Tankbot/Drone flight is present.
Otherwise capture keeps the existing conditional RFAP1–8 version selection.

Kind21 is one Drone cooldown:

- +0 kind21; +4 exact Drone bank slot0–63; +8 positive authored source UID
- +12 reserved zero
- +16 positive remaining milliseconds1–4000
- +20 signed nonnegative current reserve snapshot for RFNC equality
- +24..111 reserved zero

Kind22 is one Tankbot cooldown with the same payload, using a compact contiguous
checkpoint ordinal0–63 instead of a live bank index. The authored UID rebinds to
the complete candidate NPC slot; Tankbot timers remain actor-local at runtime.

Duplicate per-family source UIDs, duplicate slots, holes in Tankbot ordinals,
invalid deadline/reserve fields, nonzero reserved fields, ticket exhaustion and
unsupported identities refuse. A source may independently retain one owner in
each family after genuine selection changes; the two waits are never merged.
No registry handle, pointer, deadline absolute clock or runtime ticket is saved.

Existing kind2 adds variant3 for Drone Missile only in version9. Variant1 stays
ordinary Rocket Launcher, variant2 stays Tankbot Missile. Its existing position,
velocity, radius, liquid flags, double lifetime, raw damage and 9-float basis
layout stays unchanged; +60/+64/+68/+108 must be zero for RFAP9 secondary rows.
Both RFAP9 variants2/3 require the exact admitted loaded family and raw damage,
original speed/lifetime/radius, finite bounded coordinates, right-handed
orthonormal basis and velocity/basis agreement within float-normalization
roundoff. Actual float/double bits survive. No source weapon/life predicate is
applied to an already-paid flight.

RFAP1–8 remain readable with their previous payload semantics and no invented
cooldown. Variant3 and kinds21/22 are rejected in older versions. The early
shared-charge preflight and boot-resource preflight accept version9 without
changing legacy Flame/debt/ignition checks. Successful old-format loads retire
outgoing secondary timers and Drone flights before publishing their own pool.

## Source joins, publication and callback boundaries

Each new cooldown and RFAP9 secondary flight proves exactly one full registered
source: registry wrapper, entity view, damage handle, authored positive UID,
nonretired persistent slot and current level all agree. On restoration it joins
exactly one complete candidate RFNC entry with that UID, registered handle and
authored slot. Constructor identity alone cannot stand in for a removed or
retired candidate. Each cooldown's saved reserve must equal that candidate's
real inventory; it is never an ammunition grant.

Phase1 Drone reservations, either family launching and shared NPC missile
launch/update callbacks refuse save/export. The stack-owned service depth is
never reset by world teardown; even the last retiring missile still owns the
callback boundary. Load preparation has no new live-state veto. Legacy RFNC,
RFCP and old composed-export paths retain their unrepresented-state refusal.
Robot-death, Drone Smash/delayed melee and all unrelated refusal guards remain.

The shared NPC rocket pool is now assigned only at final world/storage success,
after outgoing Drone flight and both secondary cooldown resets. This includes
old-format ordinary Rocket/Tankbot rows, retaining slot order and count. New
cooldown publication is assignment-only, rebases remaining milliseconds onto
the restored clock and allocates fresh monotonic Drone tickets budgeted during
preparation. It performs no callback, debit, target acquisition or launch.
Failed preparation/publication/storage close never installs the new rocket pool
or new cooldowns. This is the new owners' transactional boundary, not a claim
of complete rollback for all older world-load components.

## Resource and audio continuity

NPC-only demand bit24 selects Tankbot metadata, bit25 Drone metadata; both also
demand shared Rocket visual/effect bit4. Demand comes from cooldown rows and
paid flight family, independently of constructor loadout or later weapon
switches. These flags never enter first-person view demand or grant ownership.
Old Tankbot variant2 saves also gain explicit metadata demand without changing
their wire semantics. Existing resource budgets and loaders remain in place.

All flight audio resumes only after final successful pool publication. Ordinary
Rocket loops retain their existing path. Drone resumes only its copied authored
10/.9 flight-loop profile at the saved endpoint; its launch one-shot and accepted
attack counter are not replayed. Native audio handles, cursors and transient
cosmetic history are not serialized. Tankbot retains its existing presentation.

## Capacity and remaining implementation

The RFAP structural maximum grows from490 to618 rows. Maximum component bytes
grow from54,896 to69,232, adding14,336 bytes of possible serialized receipts.
The actual RFAP restore stage grows by2,600 declared bytes:128 ×20-byte owners,
two4-byte counts and eight4-byte secondary-flight source UIDs, before any
whole-structure ABI alignment. There is no new live attack pool; the only new
permanent control field is a four-byte stack-service depth. Actual Xbox layout
and peak resident use have not been measured in this source-only pass.

The whole-file cap stays110,524 bytes and restore-stage cap stays2MiB. One
cooldown row adds112 bytes, or140 including a new RFAP header and RFWC3 section
prefix; one new flight plus cooldown adds252 bytes including that overhead.
The previously reported actual18 fresh world was107,936 bytes, leaving2,588.
Dense simultaneous state may correctly refuse rather than exceed that cap.

Removed-source history, other guarded delayed-melee/death/world owners, broad
ordinary primary-controller persistence and dense-world save capacity remain
separate implementation work. This slice makes no claim that every Drone or
Tankbot encounter is currently saveable under every other world-state guard.
Functional behavior remains for testers; parent owns the hourly stock64 build
and only the permitted short startup smoke.
