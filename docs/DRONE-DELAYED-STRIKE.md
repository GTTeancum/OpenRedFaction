# Authored Drone Smash delayed strike

Integrated after the 2026-10-10 02:00 Xbox build. Source-reviewed staging is now
wired into the ordinary gameplay owner; independent integrated review found
no concrete blocker. This code
is uncompiled and runtime-unverified until the next hourly batch.

## Original evidence

Original levels1.vpp/L3S4.rfl Drone UID844 selects Drone Smash / Drone Missile.
Installed weapons.tbl:2175–2201 gives Smash melee/underwater/from_eye flags
0x620, no_fire_through flags8, bash1600, AI range5, Fire Wait1.5 and one primary
Impact Delay0.4 at2194, with no alternate delay. Original RF.exe4c3d98–4c3e24
reads independent primary/alternate delay pairs.42606f–4260ce requests action2
then schedules positive primary delays using int(seconds*1000+0.5).
Melee availability/debit exceptions are425b9a and426000–426196.

Existing action presentation selects drone_attack_melee.mvf and Drone Attack
Foley; separate weapon Launch names Sea Creature Attack, unresolved in the
installed Foley catalog. No replacement sound is invented. Existing optional
missing-action RF_NOT_FOUND behavior remains nonfatal.

## Implementation

A separate exact Drone selector keeps the immediate Reeper/mutant registry
unchanged. It validates the real held and owned primary plus exact catalog,
flags, ammo-free shape and authored delay. Startup reads the existing primary
and independent delay parsers; no per-shot reads or new save format.

A 64-entry runtime-only pending bank retains source and target full handles,
qualified source/target slots, weapon, accepted order, AI mode, unique ticket
and millisecond deadline. One pending strike is allowed per source. Admission
reserves before presentation callbacks, retains ordinary90-frame cadence,
then arms the400-ms impact only if the reservation remains valid. No firearm
readiness, ammunition debit, reload or fallback grant occurs.

Service runs before every ordinary enemy gate, including cooldown and pain,
and consumes a due entry before contact/damage callbacks. It resolves only the
accepted live pair, checks held ownership/order/mode, current full3D range and
conservative30-degree cone, and uses shared body/world/rubble contact. It does
not retarget. Tickets and repeated identity/geometry qualification prevent a
callback from damaging a replacement or reviving a cancelled strike. Original
AI scaling and shared damage/death own actual damage;1600 is not applied early.

Linked/seated sources and targets are excluded. Existing ordinary pursuit
remains horizontal, so elevated targets may fail the conservative3D cone.
Point orders, vehicle/rider melee, physical-shield parity, swept limb volumes,
altitude steering, Drone Missile and animation-marker fidelity are deferred.

## Lifecycle and persistence

Source cancellation handles accepted order, selection/reset, mode, disarm,
holster and control-owner changes. Source-or-target cancellation handles death,
hide and removal. Removal uses the original handle after unregister, which
zeroes its wrapper. A victim changing its own weapon/order does not cancel an
incoming strike. Ordinary pursuit stopping at range does not cancel a swing.

Pending/in-service strikes reject save/export at save-only boundaries. Shared
NPC row capture is also used by load preparation and remains untouched. Failed
save/preparation does not clear, postpone or mutate strikes. Successful NPC
component commit, composed player checkpoint publication and final world load
publication/storage close reset the replaced timeline; teardown/restart also
reset it. No pending damage is inferred from saved animation state.

Existing world loading is not a general rollback transaction after assignment.
Preserving the pending bank on a later load error does not promise restoration
of all old world owners. This slice does not alter that pre-existing boundary.

## Evidence limits

No build, runtime strike, animation/audio output, damage, save/load or timing
result is claimed for this post-02 source. No synthetic inventory, forced event,
changed spawn, route or campaign traversal was used. Parent owns the next
consolidated stock-64-MiB Xbox batch.
