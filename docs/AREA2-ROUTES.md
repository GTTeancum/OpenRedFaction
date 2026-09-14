# Area2 section boundaries and bounded texture loading

L2S3 now survives a30-second stationary PC simulation:1799 updates create2569
particles and recycle2446, leaving123 alive without an unsupported-effect error.
That is a duration check at spawn, not full gameplay traversal.

Three separate PC walking fixtures place the player once outside an authored
exit, wait30 frames, then move into the real contact volume. They inject no
event or forced transition. Each finishes240 frames with continued movement
after arrival:

| Source | Exit | Destination | Exit frame | Arrival translation |
| --- | --- | --- | --- | --- |
| L2S2a |5150| L2S3 |56|63,4,15.5|
| L2S3 |6604| L3S1 |56|-167,-70.96875,-59.25|
| L3S1 |66| L2S3 |58|167,70.96875,59.25|

The translations match the corresponding authored Load_Level positions.
The L2S3/L3S1 names contain legacy `.d4l` suffixes in the original event data;
the existing campaign resolver maps them to installed `.rfl` sections.

`python tools/replay_area2_routes.py` reproduces the fixtures and asserts real
transition count, UID/frame, translated arrival, player survival and movement
after the boundary. The earlier L2S1 exit3218 walking-only fixture did not
trigger because the door requires Use and the live adapter skipped its chamber
reference. The first-pass interlock and Use fixtures are recorded in AIRLOCKS.md. These separate fixtures
do not demonstrate an uninterrupted walk through Area2 or its full encounters.

## Texture fallback

L2S2a failed full-resolution world/mover texture loading within12MiB. The shared
loader now first attempts the original textures, then retries with dimension
caps256,128,64 only on RF_RANGE. Each failed attempt frees its partial images
and mappings before retrying. Other errors still propagate. The cap is a
first-pass quality policy; improved selective residency and PS2 parity remain
completion work. Original game files are untouched.

The new rf_geometry_materials_open_limit keeps first-use name deduplication and
geometry-local material mappings. It uses the existing area-filtering image
reducer and accounts for simultaneous source/reduced pixels plus name/pointer
scratch in peak usage. It does not increase the12MiB material loading budget.
The unlimited API remains a zero-cap wrapper.

L2S2a selects128 after three attempts. PC records5,689,412 resident bytes and
5,834,329 peak bytes, excluding allocator overhead and other subsystem owners.
WORLD_TEXTURE_BUDGET / rf_scene_world_texture_budget contains chosen dimension
cap (zero=original), attempts, resident bytes and peak bytes. Pointer-size
differences mean PC/Xbox byte totals need not be bit-identical. Subsequent
sections that fit at full resolution select zero independently.

The world_texture_budget CTest verifies the actual L2S2a full-resolution failure,
failure-preserved output, invalid-cap rejection,128-pixel bounds, peak budget,
complete local slot mappings and cleanup. It does not certify final visual quality.

## Xbox loading and evidence

The Xbox gameplay path previously loaded diagnostic world textures and built a
diagnostic mesh before scene_preview immediately freed both and loaded its own
combined world/mover resources. That early load failed before the gameplay
fallback could run. Gameplay now goes directly to its scene loader after the
common geometry/collision/lightmap setup. Standalone diagnostic previews retain
their former loading path. Legacy diagnostic fields38..41 are zero for gameplay
instead of describing a discarded image owner; scene texture diagnostics and
actual in-level free pages provide the relevant measurements.

The native harness now accepts --exit-start-uid with --spawn, validates that a
real exit occurred, and compares transition count, last UID/frame and successfully
opened destination against PC. It also samples world texture budgets while each
section is live. Fixture files are restored and only its own bounded emulator
is closed; the user's manual session is untouched.

Stock64MiB run render-20260914-175734 passes the L2S3-to-L3S1 walking transition
at frame56, finishing240 frames with6542 free pages (25.555MiB) in L3S1. The
comparison includes final body, inventory, mission goals, particle simulation
and mover state. It does not prove pixel parity or every intermediate frame.

The first L2S2a native attempt, render-20260914-180140, failed in the redundant
diagnostic load described above. After removing that redundant gameplay load,
render-20260914-180422 passes all selected gameplay comparisons across240 frames.
Both PC and Xbox cross exit5150 at frame56 into L2S3. Xbox samples the128 cap
and5,834,329-byte peak in L2S2a, then original-resolution textures in L2S3.
The final L2S3 scene retains4699 free pages (18.355MiB) on stock64MiB.
This verifies the isolated boundary and continued simulation, not the full encounter.

```text
python tools/xemu_render_check.py --input artifacts/area2-routes/walk.bin --spawn --level L2S3.rfl --exit-start-uid 6604 --seconds 360
python tools/xemu_render_check.py --input artifacts/area2-routes/walk.bin --spawn --level L2S2a.rfl --exit-start-uid 5150 --seconds 360
```
