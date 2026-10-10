# Authored creature delayed melee contacts

Post-02:00 source implementation, October 10, 2026. Uncompiled and
runtime-unverified until the parent-owned consolidated Xbox batch. No build,
test, fixture, route, emulator run, generated asset or original-input change
was performed for this slice.

## Original timing and weapon evidence

Read-only `Installed_Game/tables.vpp`, `weapons.tbl` at archive offset
1013760, size 108890, declares:

- Reeper Claw: bash damage 40 at line 1956, range 3.8 at 1951, Fire Wait
  2.0 at 1954, and primary impact offsets 0.4/0.8 seconds at 1962/1963.
- Baby Reeper Claw: bash damage 10 at 1996, range 2.5 at 1991, Fire Wait
  2.0 at 1994, and primary offsets 0.3/0.6 seconds at 2002/2003.
- Mutant Attack 1: bash damage 40 at 2249, range 2.5 at 2245, Fire Wait
  1.5 at 2248, and primary offsets 0.15/0.5 seconds at 2255/2256.
- Mutant Attack 2: bash damage 40 at 2281, range 2.5 at 2277, Fire Wait
  1.5 at 2280, and one primary offset 0.15 seconds at 2287.

All four have no alternate impact delays. Their real original L10S1 actor
loadouts, ownership and ammo-free melee evidence are recorded in
`REEPER-CLAW-MELEE.md`. Admission still uses those exact names and actual
owned primaries; no inventory grant or fallback is added.

Read-only RF.exe inspection establishes that these are independent offsets
from swing scheduling, not intervals after a preceding impact:

- `0042606f–00426075` requests action 2.
- `0042607d` points to actor timer pair `+4c0/+4c4`; `00426083` points
  to the first primary impact float. `00426089` initializes two iterations.
- Each iteration loads its own primary float at `004260a0`; alternate mode
  instead loads the corresponding float eight bytes later at `0042609b`.
- `004260a2–004260ae` converts with `int(seconds * 1000 + 0.5)`.
  Read-only constants at `00589410` and `005893c0` are 1000.0 and 0.5.
- A positive converted value is passed to timer setter `004fa360` at
  `004260ba`. `004260c3/004260c6` advance both delay and timer pointers
  four bytes before the next iteration.
- `004fa369` reads current game time from `005a3ed8`; `004fa382`
  adds the supplied offset. It does not read the other impact's deadline.

Consequently the reconstructed deadlines are swing-start +400/+800,
+300/+600, +150/+500, and +150 milliseconds respectively. A delayed first
service never moves the second deadline. This is source evidence for the
timer relationship, not a claim of retail contact-volume or animation parity.

## Shared bounded owner

`scene_ai_reeper_claw.inc` atomically admits the four primary definitions and
their exact independent delay metadata. Its
`campaign_enemy_reeper_claw_impacts` returns the bounded primary count and
milliseconds only for a fully published exact ID.

`scene_ai_drone_smash.inc` retains the existing 64-entry runtime-only bank
and lifecycle function names but now admits those four creatures as well as
Drone Smash. Each entry owns a swing, not a single hit: full source/target
handles, qualified slots, weapon, accepted order/target, AI mode, unique
ticket, up to two absolute deadlines and a remaining-hit bitset. One source
cannot own overlapping swings. No raw NPC pointer or serialized field is
added. The extra per-entry deadline/mask is bounded, without allocation.

The new `campaign_enemy_delayed_melee_weapon` is the scene interception
predicate. `campaign_enemy_drone_smash_weapon` remains strictly Drone-only
because Drone Missile admission depends on that exact identity. The public
Drone diagnostics retain their meaning; creatures use separate private,
process-lifetime counters with the same compact layout. Creature counters
are telemetry only and are not reset by level/timeline publication.

The parent scene branch must intercept genuine creature actor targets before
the common immediate cadence/presentation/contact path. It leaves the old
creature point/once presentation-only path intact, including queue retirement.
Vehicle contact remains excluded. The existing service hook stays before
ordinary combat cooldown, pain and per-owner early-outs.

One accepted swing advances existing authored cadence once: 120 frames for
the two Reepers, 90 for the two mutants. It requests existing primary action
presentation/Foley once. Each admitted impact makes a fresh primary damage
request through `combat_enemy_primary_damage`, then existing target-specific
damage/death handling. Damage is neither divided between hits nor charged
at admission. Two valid contacts can therefore apply two authored damage
requests, unlike the old immediate single-contact approximation. No firearm
readiness, ammo debit or reload is introduced.

## Consumption, cancellation and callback safety

Phase 1 reserves a source during presentation; phase 2 is armed; phase 3
retains its reservation during impact dispatch. Each due bit is consumed
before any contact/rubble/damage/death callback. A miss consumes only that
impact; a later independently scheduled contact can still succeed.

Every impact requalifies the accepted source and target, current held
ownership, order/mode, live registry generation, range and aim. It cannot
retarget. Source eye/basis and target eye/body state are copied before the
contact query and compared after it; a callback-mutated transform becomes a
miss, without repeating a potentially damaging rubble query. Damage callbacks
are followed only through freshly resolved full handles.

After dispatch, only the unchanged ticket in phase 3 can return to phase 2
for a remaining hit. Cancellation/reset/replacement cannot resurrect an old
mask or deadline because the copied strike is never restored. A fatal contact
error consumes the active hit and clears its surviving swing rather than
retrying a possibly partially applied contact.

Both service and presentation have stack-duration guards. New admission is
blocked while either guard is active, including after a callback cancels or
resets the pending record. Save admission also observes both guards. Neither
guard is cleared by timeline reset; each is released by its own returning
call, preventing callback reentry from replacing the same source's reservation.

Both overdue positive authored contacts are serviced in authored order in a
single ordinary tick when necessary, independently requalifying each. This is
a bounded port catch-up policy, not a claim about every original low-frame-rate
coalescing path.

## Geometry and persistence limits

All contacts retain full-three-dimensional authored reach and the existing
intended-body/world/rubble contact query. Drone retains its conservative 3D
30-degree cone. Ground creatures use their preexisting horizontal 30-degree
aim rule at admission and contact; adopting the Drone pitch restriction would
newly reject some close attacks across different eye heights. Existing pursuit
remains horizontal. Shields, swept limbs, animation markers, vehicle/rider
contact, flesh-contact audio and new creature attacks remain outside scope.

Existing source cancellation hooks cover accepted order, selection/reset,
mode, disarm, holster and control-owner changes. Death, hiding and removal
cancel swings in which the full handle is either source or target. Target
weapon/order changes do not cancel an incoming swing. Pure pursuit stopping
does not cancel a swing.

The existing save-only guards now cover the shared bank, including the gap
between first and second contacts. Ordinary NPC export, composed player save
and world snapshot cannot silently omit pending damage. Shared NPC row capture
and load preflight remain unchanged. Failed save/preparation does not clear or
postpone contacts. Successful component/player/world timeline publication,
teardown and restart use the existing reset hooks. No save layout changes and
no pending damage is inferred from an animation row. Existing world-load
rollback limitations described in `DRONE-DELAYED-STRIKE.md` remain unchanged.

Original timer-consumer distinction: NPC409399–4093aa clears one timer and
40956b calls the factory inside the loop before40959a–4095a1 advances. Two
overdue NPC impacts can therefore dispatch twice. Player4a281b–4a2848 instead
aggregates a boolean and calls once at4a28fa. This source distinction supports
the NPC per-bit loop; it is not a new runtime result.
