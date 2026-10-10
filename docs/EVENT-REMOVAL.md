# First-pass mission event removal

Type2 `Remove_Object` now retires linked runtime events and triggers on its on
action. Other object families are reported as unsupported. Off is a no-op.
Event retirement cancels the common delay, UnHide requests and death polling.
Trigger retirement disables automatic and contact activation. Removing the
registry handle prevents stale links from finding the object again.

Storage remains allocated until scene teardown because the active dispatch
stack can still reference it, including a self-removing event. The event loop
checks an explicit retirement field; it does not assume that every event passed
to the runtime was created through the registry factory. This preserves the
supported synthetic/embedded event test callers.

The actual L7S2 Remove_Object5326 links Delay5324, which otherwise activates
security-door controllers after three seconds. The contained test starts that
timer, confirms an off action leaves it intact, removes it, then advances beyond
the deadline and checks that no event dispatch occurs. It also covers repeated
removal, a trigger target and self-removal without freeing active storage.

The scene backend now also removes registered NPCs: it unregisters their entity
and object handles, stops scripted movement, releases the physics body, marks
render exclusion and records retirement in the existing campaign actor store.
It does not synthesize a combat death sequence. This store preserves the removal
on section revisits; complete world serialization remains open. The scene's
retirement count now includes both restored exclusions and live scripted removals.

L1S3 authored event9602 removes NPC9436. Its90-frame PC replay completes with
25 registered-at-load actors and one excluded actor, with no retirement error.
Fixture setup dispatch accepts Delay or Remove_Object and can run without a
Goto command. Native verification is running in `artifacts/npc-remove-xemu.log`.

Death watchers distinguish a previously resolved handle that no longer exists
from an unresolved authored UID: the former is known missing, while the latter
still defers. Tests cover ordinary all-dead versus authored any-missing behavior.
Clutter, effects, full original deletion side effects and general cross-level
object removal persistence remain open.

## Retained NPC order cancellation (2026-10-09)

Ordinary NPC removal now shares `scene_npc_retirement_cancel_orders` with
expired-corpse source retirement. After successful entity unregister, it drains
the bounded single-shot queue through the existing non-fired pop, clears borrowed
movement/look/shoot/animation orders and controller/route references, cancels
combat/reload deadlines and publishes catatonic mode. Before this repair,
removing an alerted actor retained `combat_alert`, which the unchanged RFNC
capture correctly rejects for retired rows; waypoint and scripted orders could
also retain requirements on an actor that no longer exists.

Static original evidence: RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`;
type2 dispatch `4b9070` selects `4b9a10`, whose registered-object branch calls
`48ab40` at `4b9a45`. That callee only sets object+7c bit2. It does not dispatch
damage, death or a weapon drop. The shared cancellation is the port's retained
owner policy, already used for expired corpses, not a reconstruction claim for
the complete original destructor.

The helper does not clear the actor, weapon firing/effect handles, damage source handle,
inventory, persistence row, drop history or allocated physics/model resources.
Existing seat/generated-child/physics teardown remains authoritative. The queue
count is admitted before NPC unregister; repeated corpse cleanup is inert.
NPC `firing.active` has no ordinary live producer in current scene source:
construction zeros it, and reset/disarm paths only clear it. It is intentionally
untouched, including its existing RFNC veto; a future live firing owner must use
real sound/effect teardown rather than masking that bank. No codec change,
death/drop/grant/event replay or new allocation is introduced.

Removal also extinguishes the removed target through the existing
`scene_burning_extinguish(handle)` after successful unregister, then clears
that retained actor's `damage.effects.burn` and `damage.burn_source`. Previously,
the next burn tick retired the slot after target lookup failed, leaving the
unregistered actor's burn backlink nonzero and permanently vetoing RFNC.
Both cleanup steps are non-failing and occur only after unregister succeeds.
Extinguish matches the record's target, not its ignition source: other actors'
burns and historical source UIDs remain intact. The removed actor's own
`damage.effects.handle` is preserved for those historical attribution checks.
This requests no damage pulse, death, visual/audio effect or event.

Source-written and unverified until the parent-coordinated 16:00 Xbox batch.
No helper build, runtime test or new fixture was run.

The terminal removal branch also clears scripted_physics and scripted_physics_seen only after rf_physics_body_close. An unregistered, deallocated body has no remaining freeze/wake owner; keeping that marker would falsely trigger RFNC14's live frozen-death refusal. The live frozen-death restriction remains unchanged, and no wake callback or new save field is introduced. This is source-written and untested.
