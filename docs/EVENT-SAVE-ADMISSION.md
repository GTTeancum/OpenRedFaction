# Ordinary event continuation admission

Written 2026-10-08 on isolated base
`0c20b01030ee69d1e673c19e914400c649144569`. No builds, tests, new fixtures,
original-game execution or campaign traversal were performed. This is
post-14:00 source work awaiting the next parent verification batch.

## Concrete blockers

Whole-world capture encodes every authored event. RFEC's stale type whitelist
rejected even dormant Item_Pickup_State54, Mover_Pause72, Ignite_Entity82 and
Defuse_Nuke86 records, blocking saves in their containing levels. Other known
but unimplemented dormant event classes had the same common-header problem.
Separately, outdated capability bits rejected pending delayed effects already
implemented by the scene: actual examples include L12S1 Teleport9711 (2.6s),
L13S3 Shoot_At8858 (0.5s), L15S1 Shoot_At9489 (1.5s), and L14S3 Endgame10209
(2s), plus ten delayed Look_At records across the installed SP archives.

RFEC now admits the common header/scheduler for all 90 known original event
types. This is not permission to run an unsupported effect: the exhaustive
capability switch defaults to UNIMPLEMENTED, and existing snapshot and staged
restore gates still reject a pending action carrying that bit. Unknown type
numbers remain rejected. Wire layout, checksums, remaining-time arithmetic,
source/actor UID rebinding and assignment-only restoration are unchanged.

## Owned effects and safe limits

Teleport/Look_At/Shoot_At use their existing RFNC actor/order/pose state and
RFNS seat transaction. Turn_Off_Physics uses existing physics-owner admission;
frozen NPC snapshots still fail their separate explicit guard. Ignite_Entity
uses existing RFAP4 burn-owner/cadence/source state. Mover_Pause uses the mover
checkpoint. Endgame and Defuse_Nuke may retain a queued request, but capture now
explicitly rejects an already active terminal/modal owner because that live
state is not serialized.

The pickup override array is not in RFPU. Its nonzero values now explicitly
reject capture instead of becoming silently lossy after the event whitelist
is widened. The admitted RFPU profile means inherited pickup policy (zero);
world mission staging validates the current override owner and atomically
restores that baseline even during an in-session load. Actual nonzero override
persistence remains a separate implementation task. Bolt_State and vehicle
exit-lock capability stay conservative because this slice does not add their
missing external-state representation. Explode's existing admission remains.

Four implemented callbacks omitted by the delayed runtime whitelist now have
their actual backend gates: Bolt_State43, Detach58, Mover_Pause72 and
Defuse_Nuke86. No installed nonzero-delay record was found for those four;
this closes dispatch consistency, not a newly claimed campaign encounter.

Three existing checkpoint test expectations were aligned with the capability
contract (including replacing an already stale type89 rejection by unknown90).
The tests were not executed. No new test program or exhaustive fixture exists.

Overall implementation remains approximately 88%. Objective/event progression
has new source fixes but remains runtime-unverified; no percentage uplift is
claimed from these admission changes alone.

## Pending scripted explosions

A follow-on source check closes the old Explode10 capability rejection too.
There are73 authored nonzero-delay Explode records, including L1S1/9456 at0.75s,
L9S3/137 at1s, L20S1/12436 at2s and train02/9059 at300s. The scene already binds
`scene_script_explode`, preloads the level's actual named clips and uses the
ordinary delayed-event tick gate. The callback performs its optional admitted
terrain mutation, starts presentation, then applies radial damage synchronously;
no separate delayed gameplay owner is created before the event fires.

RFEC now retains that queued deadline without the obsolete UNIMPLEMENTED bit.
WORLD, DAMAGE and VISUAL requirements remain. Candidate world/terrain, actor,
vehicle, clutter and player admission still run, and restore never invokes the
effect early to reconstruct it. The pending event later follows the same callback
and outgoing links once its rebound clock expires. Already-running particles
remain the existing explicitly deferred presentation state; no new visual
continuation claim is made. This does not broaden GeoMod eligibility, invent
missing terrain edits or bypass any material world-state gate. Source arithmetic
and callback provenance remain in SCRIPTED-EXPLODE-FIRST-PASS.md.

No tests or builds were run for this change; it joins the parent15:00 batch.

## Pickup policy follow-on

The temporary nonzero pickup-state save veto above is superseded by the
integrated RFIP2 owner in ITEM-PICKUP-STATE-SAVE.md. Ordinary snapshots now
encode actual inherited/enabled/disabled policy, stage exact UID rebinding and
publish it atomically with retirement state. Pending Item_Pickup_State54 uses
WORLD/INVENTORY admission. The startup-grant ledger remains RFIP1. Queued
Explode and camera changes documented above remain intact.
