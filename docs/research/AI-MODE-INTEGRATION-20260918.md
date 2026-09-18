# AI mode event gameplay integration

Set_AI_Mode event34 now binds to the registered scene NPC adapter. Authored modes0/1/4/5 map to runtime catatonic1, waiting2, motion-detect11 and default-1. The existing event mapping comes from factory4b84c0/table59c014; runtime labels are corroborated by the local Dash Faction reference ai.h. Scheduler details below are explicit first-playable port policy.

Catatonic stops combat pursuit, clears prior acquisition and suppresses later combat scheduling, gunshot acquisition, scripted movement and stationary aiming. A later damage or Attack request may record an alert, but cannot bypass those execution gates. Waiting/default clears stale combat and allows ordinary acquisition again. Motion detection gates initial acquisition on player velocity; existing range, facing and cover still apply. Once acquired, combat can continue when the target stops. Explicit scripted targets and retaliation remain supported.

The adapter rejects waypoint4, collecting5, turret13 transitions and linked occupants rather than claiming ordinary combat implements those behaviors. Route/item integration, vehicle propagation, durable mode persistence and fuller original arbitration remain open. The old untracked scene_ai_mode.inc is not compiled or used.

## Validation

- event_ai_mode passes immediate/delayed runtime event mapping, OFF handling and propagation.
- scene_ai_mode_gameplay passes registered-adapter transitions, actual combat suppression despite explicit Attack/alert, actual script-step movement/aim suppression, hearing suppression, and waiting-mode acquisition at frame30 with deadline60. Motion gate and unsupported-mode nonmutation are also checked.
- PC and stock-target Xbox builds pass.
- No new native authored Set_AI_Mode encounter or visible campaign scenario was run; these checks establish code integration and focused scheduler behavior, not full campaign scripting coverage.
