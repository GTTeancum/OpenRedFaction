# Bounded detached-solid simulation step

`rf_physics_solid_step` now joins translation and angular prediction, swept-bound
preparation, caller-supplied earliest collision/material lookup, pose acceptance,
contact response and final position publication. It works on a private body copy;
a failure on any substep leaves the caller's body, published pose, object flags
and report unchanged. Query callbacks are read-only and must not publish changes.

Per-frame acceleration scratch survives contact retries. Repeat preparation is
cleared only at frame entry; force and torque are integrated on the first pass,
then cleared through the recovered contact preparation. Sleeping bodies retain
ownership and skip further work until an external wake restores80000000.

The scheduler is an explicit bounded port policy, not a claim of exact original
487770 scheduling: maximum ten passes, immediate stop on a settled response,
and a reported positive time remainder when the pass cap is reached. The
original's additional pass-count/remaining-time cutoff is not reproduced here.
No arbitrary time or position nudge is used to hide zero-time contacts.

The query provider must resolve the actual body spheres and composed world,
including materials. The core does not invent a bounding-sphere substitute.
The test fixture intentionally represents one spherical body and calls the
shared recovered sphere-plane collision routine. It verifies a multi-frame
fall, multiple impacts, rebound, no floor penetration, settling and subsequent
inactive retention. It also injects failure on the second query to prove
whole-step rollback after a successful first contact, and a pathological
separating zero-time contact to prove the ten-pass bound.

Validation: physics_solid_propose CTest plus the full PC suite; NXDK build.
There is no new original-binary scheduler parity claim or live Xbox acceptance.
The scene still renders birth poses. Next integration requirements are the
composed level collision/material provider, registry frame scheduling, active
pose checkpoint serialization/restore and native replay/visual verification.
