# Live switches and section state

The recovered Switch dispatcher was present but the gameplay scene supplied no
backend, leaving switches unsupported. The scene now connects trigger and event
lookup to its live registry and uses the existing event dispatcher for effects.
Initial switch link state is applied before automatic startup events, without
consuming an activation or playing sound. Activation follows authored delay,
mode and limit. Named activation sounds and nonspatial rejection slot2 use the
existing bounded audio bank/mixer; failed sound playback is counted without
canceling gameplay.

The inspected83 switches contain119 links:36 to triggers,70 to events and13 to
other objects. The first two target families are now connected. This does not
mean all linked event actions are implemented. Controller, sound-object, light
and renderable-object switch effects remain unimplemented and unmatched links
are counted. Ordinary event dispatch retains its own unsupported-action report.

Authored L1S1 switch9836 begins disabled and blocks linked trigger9840. Its0.1s
activation enables that trigger and consumes the single allowed activation.
The four-frame PC check proves the trigger remains disabled before the delay;
120-frame activation and repeated-request checks prove it enables once.
Switch8687 waits8seconds then routes to Alarm8686. Alarm is type46, not the
special flags-only event17; its gameplay action remains unsupported. The520-frame
case proves delayed switch/event routing only, not working alarm behavior.

Switch snapshots retain disabled, limit, unlimited, activation count and mode by
canonical section name and authored UID. Restore reapplies initial link effects
without replaying ordinary event actions or consuming activation count. Trigger
checkpoints then restore later independent changes to trigger flags. New campaign
initialization clears the store. The bounded owner is40,968 bytes (1024 switch
keys/states and128 section names), with no per-activation allocations or retained
runtime handles. A round trip L1S1/L1S2/back restores3 switches from4 keys;
9836 remains enabled with one used activation and trigger9840 stays enabled.

This is practical session persistence. Pending switch-event deadlines, removed
object identity and full world/controller state remain open. Startup can still
repeat other effects before snapshots restore. No original assets are changed,
no on-disk save/load is added, and these process-local setup/exit fixtures do not
prove walking through a whole mission.

Reproduce PC checks with python tools/replay_live_switches.py. Diagnostics:
SWITCH_RUNTIME counts trigger/event matches, unmatched links, sound requests,
plays/errors, last activated/initialized/restored UID and sound effect (0 for
initialization/restore). SWITCH_DETAIL shows that switch's disabled/count/limit/
mode and first linked UID/kind/flags. SWITCH_HISTORY reports registered/restored/
saved counts and owner bytes. Detail observes final tick state after activation.

The existing routing/initialization service follows original4bc340; loader sound
and switch metadata evidence is at4b83e0. Scene integration is new port code.

The earlier native round trip render-20260914-170603 matched switch runtime,
state and restoration counters, but the overall check failed on PICKUPS[6]
(PC1734 CPU vertices, Xbox0). scene_weapon_submit returns after successful
scene_model_retain on Xbox and bypasses CPU vertex emission, so this field is
backend-specific rendering telemetry. The focused harness now records both
values separately and compares pickup gameplay fields0..5 and7. It does not
claim pixel or mesh-emission parity; the failed report remains preserved.

Final stock64MiB XEMU render-20260914-171145 PASS:240 frames and both handoffs,
with the initial-link setup included. Switch state, history and runtime counters
match PC, together with the selected body, inventory, mission-goal, combat and
animation/audio fields. 9836 returns enabled with one used activation and9840
enabled. This focused gameplay pass excludes backend-specific pickup CPU vertex
counts and does not establish rendering parity or unsupported target behavior.

```powershell
python tools/replay_live_switches.py
python tools/xemu_render_check.py --input artifacts/live-switches/return.bin --spawn --setup-uid 9836 --exit-uid 9019 --return-exit-uid 9346 --seconds 360
```
