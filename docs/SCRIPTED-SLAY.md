# First-pass scripted NPC deaths

Type1 `Slay_Object` dispatches an on request to the scene's registered NPC
damage/death path. Off is a no-op. Authored delay, source and execution time
are preserved. Unsupported target families return NOT_FOUND.

The practical scene implementation applies forced lethal damage using current
health plus nonnegative armor plus one, bypassing ordinary invulnerability and
damage multipliers. It then calls the existing death-entry and death-animation
services. It does not unregister the actor or discard corpse physics. An actor
already dead is left alone. This is a playable-first implementation, not full
original Slay_Object parity; player/clutter targets, complete death presentation
and attribution remain open.

The contained L7S2 event5331 test covers off suppression, immediate dispatch,
delayed dispatch and source/time forwarding. `tools/replay_script_slay.py`
compares a control with L1S1 event9362, which targets NPC8432, at frames0 and60.
The90-frame repeated case records one death entry and health-1; control records
none. Both builds and36 tests pass. Native repeated-slay verification is running
in `artifacts/slay-xemu.log`; do not treat the launch as a passed result.

Update: stock64MiB XEMU90-frame repeated-slay test passes in
`artifacts/xemu/replay-20260914-081317/report.json`, matching PC's single death
entry for NPC8432 with health-1 and no status error. Native framebuffer was
inspected; its starting camera does not demonstrate the corpse presentation.
