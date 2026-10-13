# Queued robot-death save continuation

October 12, 2026. Source-written and source-reviewed only; compilation and
runtime behavior remain unverified. No builds, tests, fixtures, XEMU, HDD,
images, publication or real-repository edits were performed by this helper.
Parent owns hourly compile/package rollups and startup/run smoke only.

## Scope and source ownership

This increment follows integrated RFAP12 delayed-melee continuation on
`0454c8db62c187011e32007109802e953e0f6f0f` (original inspection `a094722d`).
It preserves up to 32 **queued, still-registered** clipless finalizers for the
exact existing Tankbot, Drone and Spike class/profile admissions. This is
port-owned save syntax, not reconstructed original RF.exe save bytes.

The original source evidence and implementation remain in
[ROBOT-DEATH-BLAST.md](ROBOT-DEATH-BLAST.md): 41ee40 accepts absent action -1;
418f80/419420 retire before effect/radial work; 4364da/4364e7 multiply clip
damage by authored radius. Tankbot is radius 10/damage 1000, Drone radius
4/damage 400, and Spike radius .9/damage 0. No broad robot flag, inferred
weapon family, GeoMod path or positive Spike damage is introduced.

Runtime queue capacity is unchanged: each allocated actor owns its receipt.
The checkpoint cap is 32, matching one existing outer-drain budget. More
pending receipts, sequence exhaustion, in-progress drain, sticky partial
fault, stale identity, unsupported embedded effects and a queued source
already removed by an intervening event remain explicit save refusals. A
refusal never drains work to make it fit and never changes the old save.
Completed receipts require no record because ordinary retirement is terminal.

## Wire format

RFAP13 preserves the 16-byte header and existing 112-byte row stride. Its
kind27 row is:

| Byte offset | Value |
| --- | --- |
| 0 | kind27 |
| 4 | contiguous queue ordinal, starting at zero |
| 8 | positive authored source UID |
| 12 | exact profile index: Tankbot0, Drone1, Spike2 |
| 16 | authored class index |
| 20,24 | low/high halves of exact nonzero uint64 queue sequence |
| 28,32 | low/high halves of common uint64 admission high-water |
| 36..47 | three retained binary32 blast-center components |
| 48 | retained fatal health |
| 52 | raw profile radial damage |
| 56 | profile radial radius |
| 60..111 | reserved zero |

Rows are emitted in strictly increasing sequence order. Decode requires
contiguous ordinals, unique UID/slot/full handle, increasing sequence and a
common finite high-water no smaller than any sequence. No runtime handle,
persistence slot, resource pointer, clip index or Foley index is on the wire.
Destination metadata resolves the optional presentation indices; the exact
profile still validates raw damage/radius, class and absent corpse model.
The paid blast center is retained directly, never recalculated from a newer
NPC pose or current profile offset.

RFNC29 adds no bytes or fields. Existing `dead_pose` value2 identifies a
still-registered, fatal-health, action-1 pending finalizer with no skeletal
animation, movement, attack, support or controller continuation. Vitals,
inventory, selected weapons, persistent drops/configured item, affiliation,
AI suppression, route history and medic history remain ordinary RFNC/RFCH
state. Its inert body is placed at the saved published position with yaw;
active steering, dead-body velocity, pitch and leftover animation are not
resumed. The independently retained RFAP center drives the actual effect.
The receipt exists only after death selection and loot publication already
succeeded, so none of those effects is replayed.

Older RFNC1–28 reject marker2; existing RFNC1–28 and RFAP1–12 decode paths
remain supported. The writer selects RFNC29/RFAP13 only when needed.
Standalone NPC encode/decode and component restore reject the new marker;
component export continues to refuse pending robot work.

## Staging, pairing and publication

Each private stage entry is 72 bytes: the unchanged 64-byte runtime receipt
plus resolved actor slot and prepublication persistence binding. The fixed
stage is exactly 2,320 bytes: a 16-byte sequence/count header and 32 entries.
A source-level size assertion enforces this layout. RFAP's private stage
therefore grows by 2,320 bytes; no per-NPC entry or live queue is enlarged.
The existing NPC `unowned_motion_mask` bit31 is reserved as the RFNC29
finalizer obligation; the established low16 animation bits are unchanged.
Maximum new wire payload is 3,584 bytes, plus the existing 16-byte RFAP
header when this is the only RFAP component.

Capture qualifies the complete registry/entity/damage full-handle identity,
authored UID/class, exact profile and current-level persistence key. It refuses
owned corpse models, active firing tokens, attachments, seating, scripted
animation, pending reload, burn ownership and configured-item faults.

Prepare never calls the live save-busy guard. It resolves each portable UID
to a complete destination identity. RFNC staging checks the exact supported
profile and leaves bit31 unresolved. Composed admission requires exactly one
RFAP27 for each RFNC29 marker and vice versa, matching UID, class, slot,
full handle, registered state and fatal/action-1 candidate. Missing, duplicate,
legacy/absent, living, retired and class-mismatched joins refuse. Only a complete
join clears bit31. The existing history join additionally requires the rebound
key to remain unretired and agree exactly on saved vitals, affiliation, flags
and drops, alongside its existing configured-item/suppression/route joins. The ordinary NPC publication validator rejects unresolved
bits, including callers that bypass the composed admission.

Pending publication happens only in the existing final-success section,
after native storage close, history rebinding and NPC finish. It resets the
old epoch/receipts, copies the staged queue and high-water, and binds each
receipt to the actor's newly assigned history slot. It performs no allocation,
retirement, audio or radial callback. Failed prepare/admission/close does not
publish the new pending queue. Legacy or absent rows clear old receipts only
at that same successful boundary.

The next ordinary outer drain retains the unchanged state1-to-state2-before-
callback rule, full owner checks, removal-before-radial ordering, epoch guard,
32-receipt budget and sticky partial-failure latch. Newly chained deaths get
sequences after the restored high-water. A half-applied retirement/blast is
never encoded as fresh work and cannot be replayed by this extension.

The whole-save limit remains 110,524 bytes and composed staging remains
2 MiB; existing size admission includes this private stage. Optional visuals
use the existing shared four-clip/512 KiB budget. No new resources, fixture,
monitor RFEN8 change or larger storage envelope is introduced.

## Deferred implementation limits

- A queued source removed by an intervening event has no admitted pending
  tombstone lane here and still refuses until the ordinary drain consumes it.
- Unrelated live attacks may independently refuse the same save if their
  source/target is already dead; this increment does not retarget paid work.
- Larger pending chains refuse rather than truncate, drain or enlarge limits.
- Inert terminal actor presentation is intentionally canonical; no replacement
  corpse, physics continuation or original-game save-format claim is made.
