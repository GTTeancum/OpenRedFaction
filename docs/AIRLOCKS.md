# First-pass airlock door interlocks

L2S1 exit3218 was not an ordinary walking contact. Its trigger3226 requires
Use and references room UID3229. The old live adapter skipped every trigger
with that field populated. Walking alone must still leave this exit inactive.

Evidence from installed RF.exe SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836:
465510 reads fields0 into constructor+1c;4bf970 copies it to trigger+2c0.
4bfc60 resolves that reference with45e7c0, searches other triggers sharing it,
and checks linked type8 controllers before dispatch. Geometry loader4ed520
reads room UID into runtime+24 and serialized byte30 into runtime+42.
Room3229 is geometry room106. L2S2a room7650 has byte30 clear, so that
byte is not used as an airlock-presence test. Chamber ownership is established
by the room UID and peer trigger/controller references. Triggers3224 and3226 refer
to the same chamber and link the opposite two-panel door groups.

The practical shared adapter supports recognized chamber rooms and opposite
translation doors with two keys. It allows contact dispatch only when every
linked opposite controller is stationary at key0 (the closed authored pose
for these doors). Moving or open peers, unsupported controller layouts, and peers with no
resolved door controller block it. Non-controller links follow existing dispatch rules.
Use, contact bounds, cooldown, activation count and ordered event/controller
links still pass through the existing shared runtime. It adds no allocation.

This is a first-pass interlock, not exact4bfc60 behavior. Pressure state,
equalization delay/sound, room-atmosphere mutation, multi-key/rotating doors,
and section-revisit restoration remain open. Unsupported rooms are counted
and blocked rather than treated as ordinary doors. Missing references are
conservatively blocked, whereas the original has a direct-dispatch fallback.

rf_scene_airlock / AIRLOCK records polls, allowed, blocked, unsupported,
last room UID and last trigger UID. Counters reset on section initialization;
PC LEVEL_EXIT_AIRLOCK retains the departing section observation in its log.
The borrowed geometry reference is cleared when the scene closes.

`python tools/replay_airlock.py` stages once outside airlock B and walks for
360 frames, with or without held Use. It does not force any event or exit.
The Use case must first be blocked by the peer door cycle, then cross exit3218
into L2S2a at frame274. The no-Use control must finish without a transition. The reverse L2S2a exit7639
fixture also succeeds at frame272, after216 blocked polls followed by one
allowed request. This chamber has serialized byte30 clear.
This does not prove a complete walk through the chamber or campaign encounter.

The intermediate stock64MiB run render-20260914-181743 passed360 frames
and the outbound transition, but still restricted room byte30. Final run
render-20260914-182149 passes360 frames after removing that restriction. Both
PC and Xbox cross exit3218 at frame274 into L2S2a. Native samples record217
blocked polls by frame272; destination samples show an allowed room7650
request. All selected gameplay checks pass, including final AIRLOCK words
[1,1,0,0,7650,7648]. The final scene has4383 free pages (17.121MiB).
These memory samples do not establish pixel parity or inspect every frame.

```text
python tools/xemu_render_check.py --input artifacts/airlock-replay/use.bin --spawn --level L2S1.rfl --exit-start-uid 3218 --seconds 600
```
