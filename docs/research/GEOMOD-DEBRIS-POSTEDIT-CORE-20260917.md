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


## Live integration (supersedes the earlier pending status)

Rocket cuts now invoke cleanup after successful render/collision publication
and new-fragment spawning. The effective radius is hardness.scale multiplied
by template radius; center is the adjusted cutter center. Original466b1e..466b2c
copies descriptor+8 to6485a0, the same center subsequently passed to4de530 and
490900. Newly born chunks have positive bounce counts and are excluded.
All 80 possible ages are evaluated before applying any age changes; the routine
uses 320 bytes of stack scratch and no allocation. Reset clears telemetry.
Diagnostic cuts/other future geometry producers still need explicit notification
integration; actor, prop and glass effects are not implied by this debris pass.

`python tools/replay_debris_cleanup.py` reproduces ordinary fire frames316/430
with a small second-shot aim change. Two committed cuts examine41 active chunk
states (37 moving,4 settled) and change one settled age. Every live input/output
matches full original490900 through `probe_debris_postedit.py --live-log`.
The map-exhaustion control rejects both edits and leaves cleanup telemetry zero.
This proves cleanup is suppressed on those failures, not full rollback of all
admission/debris-preparation RNG, which retains its existing separate semantics.

Stock64MiB native run artifacts/xemu/render-20260917-015632 passes67 comparisons
at530frames with8,322 free pages (32.508MiB). Exact PC/Xbox cleanup state:
`[2,41,37,4,1,1408358166,0,0]`. Native input digest is checked by the same replay
tool's --native-report mode. Disc restored; owned emulator exited. The endpoint
shows textured crater, smoke, launcher and HUD; no full fade-animation visual
acceptance is claimed. All115 PC tests pass (postedit-full-ctest.log).
