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

This is a practical event/trigger implementation. NPCs, clutter, effects, full
original deletion side effects and cross-level removal persistence remain open.
