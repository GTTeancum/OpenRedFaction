# Medic finite persistence

Source-written against clean2122f65d, 2026-10-10. Uncompiled and untested. No
build, syntax check, fixture, gameplay run, original-save modification or active
repository edit was performed by this worker. Parent owns integration and the
hourly Xbox batch. This is the finite-state portion of the actual medic Use
implementation, not a standalone health grant.

## Evidence and scope

Original402ea2 initializes the AI reserve to200. Accepted Use40a670 enters14;
40a540 transfers min(current class-health deficit,current reserve), debits the
reserve and defaults the AI. Exhaustion does not delete the syringe. Constructor
423a85 recognizes medic1 and4104a0 creates real syringe type4 in hand1. Event
Drop_Weapon4b9c7c deletes held clutter and clears its slot. There is no original
serializer claim: RFNC22/RFCH8 are port persistence for those source-owned facts.
The qualification/source contract is MEDIC-USE-SOURCE-CONTRACT.md in the source
research handoff; the sibling syringe/action implementations own actual runtime
construction and transient healing behavior.

## Canonical lane and identity

rf_campaign_medic_state is exactly16 bytes, four LE words:

- +0 reserve_seen:0 or1
- +4 exact binary32 remaining reserve
- +8 child_state:0 constructor/default,1 present residual,2 deleted
- +12 exact binary32 present-child health

Absent reserve_seen0 requires exact positive-zero reserve bits. At runtime it
reconstructs200 only for a source-qualified medic1. reserve_seen1 retains every
finite value from0 through200, including exhausted0. An explicitly seen negative
zero is accepted as numerical zero and preserved bit-for-bit; absence remains
positive-zero canonical. No200-minus-spent reconstruction or decimal conversion
occurs. Child state1 requires finite health greater than0 and at most50; an
explicit full-health50 is legal. Child0/2 require positive-zero health bits.
No epsilon equality or arithmetic is used in the RFNC/RFCH join.

The lanes are independent. A spent medic can retain an untouched constructor
syringe; a never-used medic can have damaged/deleted syringe history; exhausted0
can coexist with present child. Neither lane infers the other. All-zero legacy
lanes reconstruct initial state only after exact medic1 qualification; a nonzero
lane on another current actor is rejected. Other-level history is structurally
validated and becomes source-qualified when that level is loaded.

Keys remain the existing campaign level plus authored actor UID. The dedicated
child field denotes the hand1/syringe role. It contains no new authored UID,
transient registry handle, class-name hash or fabricated pickup identity.

## Wire composition

RFNC22 is selected only when any row has reserve_seen1 or nonzero child_state.
It appends16 bytes after the RFNC21 AI-suppression tail on every row. RFNC1..21
remain readable and decode canonical absence. Every size path is updated:
conditional writer version, total bytes, per-row advancement, read/write offsets,
row-span preflight, version maximum and structural row maximum1732. Existing
whole-component checksum and identity cover the new bytes.

RFCH8 is likewise selected only for residual medic history. Actor row size grows
from88 to104, with the lane at88; earlier versions supply zeros. Private snapshot,
preflight, version selection, writer, restore stage, current-owner merge, exact
level/UID rebind and RFNC/RFCH equality join all carry the lane. No session history
is changed merely to encode a save.

The current-level join rejects any reserve flag/bits or child state/health bits
mismatch before publication, including RFNC22 spent state paired with legacy
RFCH absence and the reverse. A legacy current-level lane and a newer RFCH8
containing only other-level residual medic history are valid when the joined
current row itself is exactly absent. History captures retired owners as well as
living ones so level revisit does not refill/de-delete a retained medic.

## Candidate ownership and publication

scene_medic_state_capture reads finite runtime reserve plus the real child's
capture API. It does not check pending/residual healing; those are export-only
admission owned by scene_medic_save_guard. The current timeline can therefore
have a real accepted heal while a separate load candidate is prepared. The
fresh-loader resident-count call occurs after actual actor/syringe construction
and section history restoration, so its current finite proof stays strict too.
Resource demand reads decoded candidate rows and never requires a current-row
capture. No speculative initialization bypass is added.

RFNC restore stages an independently allocated real syringe candidate, or only
its retained role metadata for saved retired/deleted parents. Required resource
and budget errors occur before live publication. The generated candidate lives
on a private list, is hidden and is not a live gameplay child. Its hand pose is
reconstructed from the saved NPC basis/position and privately evaluated saved
bones; even a legacy no-animation medic row receives a private idle pose rather
than borrowing the current timeline's hand. Failed staging discards candidates.

The nonfresh component API snapshots the complete qualified medic actor and
pose owner in addition to its existing current-row snapshot, preserving exact
pending target/deadline/clip/full-handle identity. A restoring-only exact owned43
predicate permits that current clip to be inspected. The saved candidate codec
still rejects AI14 and its unowned43 admission is unchanged. No current action
is normalized to idle to make a save/load fit.

There is one explicit legacy initialization migration, separate from that current
identity capture: a saved living (health>0, nonretired, non-dead-pose) exact medic1
with raw AI0, authored AI byte1 and reconstructed default2 stages effective mode2.
The prior port lacked placed medic-AI initialization and serialized calloc0.
Source audit found no explicit living0 setter: normal mode commands admit
1/2/4/11/-1, event34 maps1/2/4/5/11/-1, seat/turret enter13/exit2 and teleport2.
The real seat-release0 writer is limited to dead actors and is excluded. Saved
-1, all explicit modes, dead/retired0 and other authored defaults stay unchanged.
The raw decoded row and joins remain immutable; separate effective_ai_mode drives
assignment. rf_scene_medic_ai_restored reports count/UID/raw/effective, while the
existing mirror/expected-mode mismatch counters compare the admitted effective
mode. No migration depends on reserve/child absence or current health inference.

scene_npc_checkpoint_restore_assign does not assign the new finite lanes.
scene_npc_checkpoint_restore_finish is the sole same-level success boundary:
retire old transient healing without changing new AI/playback, publish staged
child, assign exact finite reserve, assign the already-rebound history lane, then
perform the existing suppression/death-item finish. Full-world callers reach it
only after every fallible publication and final storage close succeeds. The
standalone component commit reaches it after its validation/assignment phase.

Ordinary quickload first reads/preflights/closes storage in
scene_world_quickload_request before publishing a level replacement. Failure
there leaves the current scene's heal/reserve/child intact. The sole
scene_world_load call is guarded by !frame in a fresh scene; the action owner
must prohibit frame0 acceptance/transfer. An accepted transition closes the old
scene's transient owners. On the new scene's final-close failure, new medic
reserve/child/history are not published. This patch does not claim or implement
whole-world rollback of the preexisting vehicle/player loader, and does not move
storage close or hide its original failure diagnostic.

Section history restoration applies parent visibility/retirement before its
child. It deliberately stages/replaces the fresh constructor child even for an
unchanged default lane. This bounded port staging consumes a temporary runtime
registry generation/UID; it does not invent or modify an authored/persistent UID.
Section capture preflights all finite medic states before committing any
actor history and propagates failure to its sole caller. The action owner must
hold section exit while its bounded transaction/presentation resolves; no save
veto, early health transfer or constructor refill is used to end that wait.

## Storage and Xbox bounds

- Live campaign medic history:16*2048 =32768 additional bytes.
- Each private RFCH snapshot/restore stage:32768 additional bytes; existing
  sizeof-based stage accounting includes them.
- Every rf_npc_checkpoint_record:16 additional bytes. Restore embeds saved and
  before records, therefore32 bytes per entry, plus the explicit syringe
  candidate (64 bytes on32-bit Xbox),4-byte snapshot marker and4-byte effective
  AI mode:104 additional
  bytes per entry before the runtime-owner growth. The already-existing owner_before also
  grows by the action/syringe runtime fields; their sizeof-based allocation
  includes that growth.
- RFNC21-to22 wire delta:16*current_count, at most32768 bytes structurally.
  Row maximum1716->1732; structural component bound3514432->3547200. These are
  structural maxima, not permission to exceed the composed save cap.
- RFCH7-to8 wire delta:16*history_actor_count, at most32768. Structural maximum
  297832->330600 bytes. Promotions from older versions also retain all intervening
  tails under their established rules.
- A medic restore without a saved animation additionally stages50*B rounded to
  four-byte alignment for B bones, at most2500 bytes for50 bones (48B matrix plus
  2B generation per bone). Existing saved-animation staging already accounts for
  that memory, so it is not allocated twice.
- Candidate child allocation/peak must fit the remaining composition budget
  before construction; allocated child bytes are added to NPC stage->bytes,
  which flows into world->allocated_bytes. Shared syringe resources are owned
  and bounded separately by the syringe worker (256KiB shared resource plus
  128KiB aggregate live/candidate bodies), not hidden in this estimate.
- The existing2MiB loader stage budget and110524-byte RF_CHECKPOINT_FILE_MAX are
  unchanged. Capacity failure remains a bounded refusal, never a partial save.

## Source audit and remaining integration

A source-only search found no second production RFNC/RFCH reader. The production
row/span/version/hash paths are updated here. Existing targeted tools intentionally
assert fixture-specific RFNC10/11/12 or RFCH4; tools/xemu_frozen_npc_save.py is an
immutable RFNC1..14 structural reader and still fails closed on22. It was not
broadened into a new medic fixture. No tooling was executed. Parent can select
appropriate bounded validation in the coordinated batch.

Required sibling APIs and ordering are listed in INTEGRATION.md in this staged
handoff. Action and syringe behavior, resource availability on actual medic1,
new-format save/load/revisit behavior and stock64MiB runtime remain unverified.

The separate live AI default is reconstructed from the source-qualified authored
setup; it is not a fifth word in this finite lane and is never inferred from the
saved current action. Actual UID783 has authored default2. The source inspector
confirmed Set_AI_Mode4bc6c0 invokes current-action setter407e20, never stored-default
setter407eb0. Therefore an explicit later current mode remains independent of the
constructor-derived default, and this setter does not require another persistence
lane or a speculative default-history save guard.
