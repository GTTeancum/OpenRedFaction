# Debris gravity and remaining actor interaction

`tools/probe_debris_gravity.py` executes original48f62f through48f64a,
without replacing any service. The checked original executable SHA256 is
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
140 cases cover seven vertical velocities, five accelerations (including
zero/reversed gravity), and four frame durations. The FPU control word is027f.

Original gravity is applied after48f900 collision processing, only if the
remaining bounce count is positive. It multiplies acceleration by the stored
float frame duration, subtracts from vertical velocity in double precision,
and stores float once. The old scene divided acceleration by60 as a float
before subtraction. The shared rf_geomod_debris_gravity now retains the
original operation order; it rejects nonfinite/negative-duration inputs and
nonfinite results without changing output. Reversed acceleration is allowed.

The existing geomod_debris_motion test now compares all140 captured gravity
results bit-for-bit, in addition to72 displacement fixtures and error guards.
The live two-shot replay retains35 accepted crossing cases matching original
math and35 successful splash requests; five wet births also match.

## Remaining gameplay work found in the enclosing loop

Static inspection only:48f678 tests chunk flag2 before the actor loop.
48f69f resolves actor entries,48f6c2 compares their room with the retained
chunk room, then48f6db calls506fd0 for sphere overlap. On overlap,48f6ee sets
flag2 and48f6f9 computes speed; the damage scalar is speed * chunk radius *
0.5 (constant5893c0), passed to4892c0. Further effects/physics calls follow.
The loop backedge48f79f goes to48f69f, not to the flag test: setting flag2
must not be interpreted as stopping the remaining actors during that pass.

The current scene debris owner does not retain this suppression flag or
perform actor interaction. Core birth already returns the initial flag for
small fragments. Required before integration: execute boundary/multiple-actor
cases, recover exact damage routing and physics-side calls, preserve the flag
through floor contacts/relaunch, and test gameplay in a controlled encounter.
Spin still uses the scene's angle adapter instead of the original accumulated
rotation matrix. Neither actor behavior nor exact rotation is completed here.

## Native verification

Stock64MiB run artifacts/xemu/render-20260917-004419 passes63 state
comparisons over550frames, with4158free pages (16.242MiB). All246 ripple
vertices and capture inputs match PC bit-for-bit. The inspected native
endpoint retains room, damaged post, weapon and HUD; exact animated rotation,
actor interactions and audible output are not established by this endpoint.
The harness restored the disc and exited. Other projects' emulators were
left untouched.
