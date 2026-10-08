# Eye input validation CPU

Source-written 2026-10-08 after the 18:00 run. No helper build or tests.

The existing Xbox eye.obj contains 20 __fpclassifyf calls before rf_eye_position
starts its transform/transition arithmetic. The parent 18:00 profile records
0.377 ms in the combined NPC eye-update/state-hash phase; this is not a measured
cost for those calls alone or a predicted gain.

Use the existing alias-safe IEEE binary32 finite predicate for these stored
input values, preserving validation order, rejection results and all accepted
finite values. Also use it for the 21 stored input checks in the copy-only
rf_first_person_pose_copy helper. Neither validation block has a live computed
floating-point intermediate. No output guard, transform, transition formula,
camera effect, tag placement, pose frequency or publication order changes.
There are no new allocations, caches or lifecycle requirements.

The three rf_eye_position output classifier calls remain, as do all unrelated
eye/camera arithmetic and their existing x87 boundaries. First-person pose
copy still stages its result before writing, preserving supported aliasing.
Floating-point status flags are not part of the gameplay result contract.

Parent 19:00 target-code inspection and existing eye_probe/model checks remain
pending. Compare the same stock-64-MiB L1S1 scene and actual presented FPS.
