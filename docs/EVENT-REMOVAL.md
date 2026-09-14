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
