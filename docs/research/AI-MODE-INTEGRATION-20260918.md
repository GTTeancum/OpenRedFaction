# AI mode event gameplay integration

Set_AI_Mode event34 now binds to the registered scene NPC adapter. Authored modes0/1/4/5 map to runtime catatonic1, waiting2, motion-detect11 and default-1. The existing event mapping comes from factory4b84c0/table59c014; runtime labels are corroborated by the local Dash Faction reference ai.h. Scheduler details below are explicit first-playable port policy.

Catatonic stops combat pursuit, clears prior acquisition and suppresses later combat scheduling, gunshot acquisition, scripted movement and stationary aiming. A later damage or Attack request may record an alert, but cannot bypass those execution gates. Waiting/default clears stale combat and allows ordinary acquisition again. Motion detection gates initial acquisition on player velocity; existing range, facing and cover still apply. Once acquired, combat can continue when the target stops. Explicit scripted targets and retaliation remain supported.

Waypoint4 now resumes only an existing Follow_Waypoints binding retained by the actor: it validates the path/cursor, resolves the current authored node and restores route movement while preserving loop/ping-pong mode and direction. No destination is invented for an unbound actor. Autonomous combat and noise acquisition cannot take over the active route; an explicit Attack order can override it (first-pass policy). The adapter still rejects unbound waypoint4, collecting5, turret13 transitions and linked occupants. New route assignment, item collection, vehicle propagation, durable mode persistence and fuller original arbitration remain open. The old untracked scene_ai_mode.inc is not compiled or used.

## Validation

- event_ai_mode passes immediate/delayed runtime event mapping, OFF handling and propagation.
- scene_ai_mode_gameplay passes registered-adapter transitions, actual combat suppression despite explicit Attack/alert, actual script-step movement/aim suppression, hearing suppression, and waiting-mode acquisition at frame30 with deadline60. Motion gate and unsupported-mode nonmutation are also checked.
- PC and stock-target Xbox builds pass.
- No new native authored Set_AI_Mode encounter or visible campaign scenario was run; these checks establish code integration and focused scheduler behavior, not full campaign scripting coverage.

Waypoint extension: focused adapter checks additionally pass resuming the second node after catatonic, retaining ping-pong/reverse state, target-coordinate publication, autonomous combat suppression and invalid-cursor nonmutation. PC and Xbox builds pass. This verifies route admission into the existing movement implementation, not a new live traversal capture.

## In-memory revisit retention

The normal actor capture/restore lifecycle now stores stable AI actions beside the existing actor slot and level/UID key. Bound routes store section-relative offsets, count/cursor, loop mode/direction and a section-content hash. Revisit validates the newly loaded section and node bounds, then rebinds the route to the new buffer; no process pointers, old handles or old timestamps are saved. New-campaign reset clears the parallel store. Unsupported or mismatched records are not restored. Retired actors retain existing retirement behavior.

Fixed storage is32bytes for each of2048 actor slots (64KiB). The focused helper check passes mode retention, route reconstruction at a different buffer address, cursor/loop/direction preservation, changed-section rejection, mismatched actor identity and reset. PC/Xbox builds pass. No new live two-level round trip or disk-save support is claimed; that runtime route remains to be exercised after the core testbed work.
