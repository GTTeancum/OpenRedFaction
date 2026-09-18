# Reciprocal fragment contact against committed mover geometry

Extended the narrow-static-surface path from5dbecd3f to owned mover polygons. A mover vertex can now hit the interior of a broad fragment face even when the occupancy spheres and fragment corners miss it. This remains an explicit port collision policy, not a claim of a newly reconstructed retail routine.

## Pose and ownership contract

The existing `rf_geometry_collision_body_sweep_textured` adapter establishes the body-query contract: mover origin is `rf_group_attached_pose.position`, basis is `input_matrix`, bounds are the committed minimum/maximum, velocity is `velocity`, and the runtime object handle comes from the collision view. The original-derived49a118 contact adapter preserves material, face token and object/velocity identity. The reciprocal query follows that contract. It does not substitute the distinct ray-facing `public_position` / `output_matrix`, and it does not extrapolate geometry from velocity.

The new query uses the shared mesh-derived rotational bounds, skips mover flag0x40000, applies ordinary0x464 face admission, transforms local vertices/normals through `rf_collision_contact_world`, and invokes the existing four-interval fragment-face crossing helper. It retains the earliest qualifying contact, checks surface orientation, resolves material with the normal scene callback and publishes world point/normal, mover index, source face, object handle and velocity. Errors and misses preserve output. No per-query allocation, checkpoint-schema change or new retained storage is introduced. The existing static-world reciprocal query reuses the same extracted mesh-bound helper.

## Numerical and ownership tests

`fragment_mover_contact` in `tests/scene_detached_sources_tests.c` places the0.04-wide polygon at a translated committed pose, then rotates both mover and fragment90degrees. Both cases detect the expected0.25 contact fraction and normal. Deliberately wrong ray-facing pose fields must not affect the result. Tests preserve handle77, texture11, material7 and the supplied velocity, reject disabled movers and non-solid faces, preserve an existing0.1 nearer hit, and leave outputs unchanged when metadata fails. Moving the committed surface0.25units toward the approaching fragment changes the contact fraction to0.125 in both orientations; no velocity-based prediction is involved.

This extends collision detection against the mover's current committed geometry. It does not establish continuous two-body relative motion, waking sleeping fragments when a mover enters them, velocity-aware debris response, carrying or crush damage. Those behaviors remain separate work. Four angular intervals and vertex/face tests still leave edge/edge and larger-motion coverage open. Native gameplay regression does not itself demonstrate this synthetic narrow mover fixture on Xbox.


## Regression results

All123 PC tests pass (`artifacts/fragment-mover-all-tests.log`). Fresh complete92 and108 connected histories pass first/second cuts, protected-trim rejection and exact saved/uninterrupted continuation (`artifacts/fragment-mover-group92.log`, `artifacts/fragment-mover-group108.log`). First-cut and both-cut audits pass with0.005 penetration and clearance limits; gaps remain0 except the second west piece at0.000100135803 (`artifacts/fragment-mover-floor.log`, `artifacts/fragment-mover-first-floor.log`).

The stock64MiB NXDK build and600-frame native east first-cut regression pass in `artifacts/xemu/render-20260918-022724`:77 checks,3330pages free (13.01MiB), no plugged memory, and exact5300-byte PC/Xbox checkpoint (SHAd27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba). The native framebuffer is byte-identical to the previously inspected `render-20260918-020011` capture, so no duplicate screenshot is posted. The harness restored its disc and closed its owned emulator. This is a native integration/regression result; the translated/rotated narrow-mover demonstration itself remains numerical PC coverage until a dedicated native fixture is added. No performance gain or moving-platform gameplay completion is claimed.
