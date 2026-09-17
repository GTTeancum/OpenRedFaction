# Settled debris cleanup after geometry changes

Added rf_geomod_notify_debris to the shared C notification module. It reconstructs
original490900's per-fragment operation: when signed bounce count is <=0 and
center distance squared is strictly below the stored float radius squared,
copy lifetime to age. Other fragments retain age. This begins normal timed fade;
it does not delete fragments, clear support, reset orientation or relaunch them.
There is no detail-marker test. Finite negative radii retain the original
squared-radius behavior; nonfinite/overflow inputs return an error and preserve
output instead of reproducing undefined gameplay state.

`python tools/probe_debris_postedit.py` executes complete original490900 with
real4faf00/409fa0/40a180 math, a supplied one-node list and patterned record.
All bytes outside age are checked unchanged. 168 cases cover signed gates
-1/0/1, two centers, seven offsets including exact/inside/outside boundary and
diagonal positions, and four radii. Generated raw-word fixtures feed the actual
C helper in geomod_notify_tests. All cases and invalid-input preservation pass.
RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

Evidence lives in artifacts/debris-motion/postedit-original.json and
postedit-build.log. The helper is not yet called by the live scene. Integration
must use the completed edit's actual center/effective radius, run after complete
collision/render publication, and preserve original spawn/cleanup ordering.
New fragments normally have positive bounce counts and are excluded. Rejected
transactions and checkpoint validation must not trigger successful-edit cleanup.
Player/NPC wake, prop retirement and glass cleanup remain separate open work.

Stock-profile NXDK build also succeeds (postedit-xbox-build.log). No emulator or live cleanup acceptance is claimed before scene integration.
