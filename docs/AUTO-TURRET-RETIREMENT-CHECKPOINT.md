# Auto Turret removal persistence

Implemented source, 2026-09-30; parent integration and Xbox verification are
pending. The primitive, companion and runtime-accessor changes are frozen for
the parent's serial build/native checks. No PC build or runtime was performed.

## Format and transaction

Use RFTU version3 as an alternative to version2, not another nested generated
layer: `RFTU3(RFTU1(RFNS(RFVA(vehicle + route))))`. Version3 retains the16-byte
header and includes every generated binding, bounded by128. Its224-byte rows
contain the existing216-byte RFTU2 row followed by removal reason at216 and the
RFNC base-retired fact at220. Reason0 is ordinary live/dead ownership, reason1
is base removal, and reason2 is head-only removal. Base UID plus role1 remains
the identity; neither runtime handle is serialized. RFTU2 remains readable and
keeps its existing216-byte wire format and strict nonretired admission.

The new `scene_turret_generated_retirement_checkpoint.inc` owns version3
capture, prepare, admission and assignment. It uses the existing stage shape
with one new in-memory `removal_reason` field per row; `base.retired` already
exists. Ordinary target admission can therefore keep using the same generated
stage pointer. Removed rows already have both hidden/removed head flags, so the
existing pending generated-target callback rejects them as living targets.

All prepare/admit work runs before any publication. Fresh-scene admission
requires both real factory registrations and the correct authored base slot.
NPC/RFNC assignment runs first and may unregister/close the base. Generated
assignment then restores the head scalar state and terminal sidecar through
the admitted stable array slot; it does not resolve an already unregistered
base as if it were alive. The head remains registered at its existing owner
address, hidden and detached. No damage, death event or presentation callback
runs during assignment.

## Minimal primitive changes

Shared primitive changes are limited to `scene_turret_generated_checkpoint.inc`.
The runtime retirement file adds two bounded sidecar accessors and native
telemetry. Parent owns scene/world/adapter integration.

1. Add `removal_reason` to the in-memory generated row only. RFTU2 decode sets
   it to0. Keep its wire encoder and216-byte row offsets unchanged.
2. Extract `scene_turret_generated_checkpoint_capture_head(binding, frame,
   row)` from capture-row's scalar/head section. Keep common registration,
   unsupported possession/burning, selected-model and combat-owner checks.
   Move exact live/dead link admission and the live-base lookup into the old
   capture-row wrapper. The new version3 capture supplies the real retirement
   fact and verifies its detached link separately.
3. Extract `scene_turret_generated_checkpoint_valid_common(row, frame)` from
   the existing validator. Retain scalar bounds, stable identities, weapon,
   health/dead coherence, death-dispatch coherence, target/cadence, finite pose,
   eye limits and orthonormal mount checks. Only the rejection of retired base
   and dead-base/live-head combination belongs in the old lifecycle wrapper.
   The old validator also explicitly rejects nonzero removal reason.
4. Extract assignment of one row as
   `scene_turret_generated_checkpoint_assign_row(row, frame, linked_handle)`.
   This retains all scalar, combat, pose, base-readiness mirror and diagnostic
   assignments, without fallible lookup/callback work beyond the already
   admitted stable binding. Old RFTU2 assignment passes its existing
   dead?-1:base link. New removal assignment passes-1 and publishes the marker.

Version3 capture uses the same live base helper for ordinary/head-only rows.
For a removed base it reads the stable base array slot only after validating
its authored class/UID, absent registration, persistence registration/slot and
the matching retired campaign record. The base's actual post-removal health
and AI flags are retained. It must not rewrite a living removed head as dead
to satisfy version2 constraints.

## Removal-specific admission

- Reason0 requires base.retired0 and retains all version2 life/death rules.
- Reason1 requires base.retired1 and an independently matching retired RFNC
  row. Ordinary removal currently sets base health0/deathbit while preserving
  the head's real health/model/death reason. That asymmetry is legal only in
  this explicit removal state.
- Reason2 requires base.retired0. It may retain a living or previously dead
  base/head. A later base removal supersedes reason2 with reason1, matching the
  runtime retirement helper.
- Both removal reasons require hidden/removed head flags2|0x4000, no target,
  no cadence or burst, detached link, cleared base ready bit and original
  head health/dead/death-dispatch coherence. No possession/burning exception
  is introduced. Surviving base AI is restored by its ordinary RFNC row.
- Destination preparation requires fresh factory owners; it cannot resurrect
  a base whose current runtime physics owner was already freed. The existing
  NPC admission policy remains authoritative for in-session restoration.

## Parent adapter and wiring changes

1. Include retirement checkpoint after the primitive and runtime retirement
   helpers, before world checkpoint adapters. Keep old prototypes until new
   dispatch hooks replace them.
2. Dispatch RFTU3 prepare to the new companion, RFTU2/legacy to the existing
   primitive. Charge the selected stage's `allocated_bytes` exactly once;
   common stage close remains usable.
3. Change `scene_turret_generated_world_base` to return `saved.retired` instead
   of rejecting that flag. Version2's own admission still rejects it. Keep
   authored base-class validation and saved-health extraction.
4. Snapshot's decoded RFNC cross-check must compare actual retired facts and
   health for removed bases instead of calling the live-only base lookup.
   Use the companion capture-base function for every row, then select RFTU3
   only if any explicit retirement exists. Remove the blanket retirement
   checkpoint rejection only after this path is connected.
5. World admission dispatches based on the stage's explicit removal fields;
   version3 admission must independently compare RFNC retired status for every
   row. The unchanged pending-target callback rejects hidden removed heads.
6. After all stages admit: assign NPCs, authored turrets, then the selected
   generated stage. Removal assignment populates markers with fresh handles
   and clears activation/pending death slots for removed rows. Ordinary rows
   clear any stale retirement marker only where prior NPC admission permits
   restoration. Existing NPC seat assignment follows the generated assignment.
7. Continue skipping retired heads in tag publication, activation, death joins
   and damage. Full level teardown alone unregisters the head tombstone.

The focused check should cover both base removal and head-only removal through
ordinary save/reload, comparing removal reason, RFNC retired fact, registration,
health/dead state, readiness, target/cadence and zero repeated death effects.
No campaign traversal or PC runtime is required.

Delivered parent entrypoints, with the original argument order retained:

- `scene_turret_retirement_checkpoint_capture_base(context, uid, out)` supplies
  actual live/removed base facts for RFNC capture cross-checking.
- `scene_turret_retirement_checkpoint_capture(buffer, capacity, inner_bytes,
  frame, budget, written)` selects RFTU2 when no removals exist, otherwise RFTU3.
- `scene_turret_retirement_checkpoint_prepare(data, bytes, frame, budget, out,
  inner, inner_bytes)` dispatches both generated versions and legacy handling.
- `scene_turret_retirement_checkpoint_admit(stage, read_base, read_target,
  context)` keeps strict RFTU2 admission or admits explicit removal state.
- `scene_turret_retirement_checkpoint_assign(stage)` restores either format and
  updates every marker, including clearing stale head-only removal markers.

Include the new companion immediately after the generated checkpoint primitive.
It forward-declares `scene_turret_generated_retirement_read(index, reason)` and
`scene_turret_generated_retirement_assign(index, reason)`, whose definitions
stay in the later runtime retirement include. Keep the existing common stage
close and allocation accounting. Remove the obsolete blanket
`scene_turret_generated_retirement_checkpoint_admit` snapshot gate when wiring
the new capture path.

`rf_scene_turret_retirement_probe[8]` contains base UID, base registration
present, head registration present, head flags7c, head linked handle, head
health float bits, head dead and total turret shots. It resets at factory
retirement reset, updates after successful removal before the caller's NPC
unregister, and updates after checkpoint assignment. Therefore a base-removal
runtime snapshot reports registration1/1 before unregister, whereas a restored
base-removal snapshot reports0/1. `rf_scene_actor_retirement[1]` separately
proves that the ordinary NPC unregister completed during the runtime removal.

Implementation details for the next patch:

- A head-only removed base can subsequently die normally; a removed child
  remains hidden with its actual pre-removal health and death reason. Therefore
  both explicit removal reasons allow dead-base/live-head asymmetry, whereas
  reason0 never does. A later scripted base removal changes reason2 to1.
- Capture selects version3 only when at least one explicit removal marker
  exists. Version3 prepare requires such a row; all-live files remain RFTU2.
  This lets the unchanged common stage identify its assignment/admission path
  without adding a version field or changing existing callback signatures.
- The new assignment dispatcher also clears obsolete retirement markers when
  restoring an admitted RFTU2 stage. Otherwise an in-session reload of an
  earlier active save over a head-only removal would remain silently inactive.
  Generic NPC admission still rejects resurrecting a physically freed base.
- Reads of a retired base use the real stable slot, authored UID and retained
  campaign persistence key; they never accept only a stale registration handle.
- The parent may use one new public capture-base helper for both RFNC snapshot
  cross-check and generated row capture. This keeps retirement fact extraction
  in one place and avoids supplying pre-load live values during admission.
