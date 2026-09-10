# Projection profiling and optimization

The optional scene profiler measures seven sequential parts of each tick using
an injected millisecond clock. Xbox pacing sessions enable it; deterministic
fixtures leave it disabled. Tick 0 through 15 are excluded to avoid initialization
costs. `rf_scene_profile[8][4]` records calls, total milliseconds low/high, and
maximum milliseconds; row zero is unused. `rf_scene_profile_stage` records the
current tick and last completed boundary for failure diagnosis. QMP reads remain
non-atomic. Profiling does not change the simulation delta or allocate memory.

Live stock-64-MiB XEMU observations, each over approximately 20 host seconds:

| Work per tick | Baseline | Reuse fan corners | Add clip classification |
|---|---:|---:|---:|
| Animation/stance | 4.19 ms | 3.91 ms | 4.03 ms |
| Camera/world projection | 94.31 ms | 72.26 ms | 60.32 ms |
| World hash | 0.90 ms | 0.77 ms | 0.82 ms |
| Model rendering | 6.94 ms | 6.51 ms | 6.45 ms |
| Scene support/checks | 0.42 ms | 0.39 ms | 0.36 ms |
| Presentation/integrity | 27.59 ms | 29.75 ms | 28.04 ms |
| Physics commit | 0.10 ms | 0.15 ms | 0.14 ms |
| Observed ticks/guest second | 7.37 | 8.59 | 9.81 |

Run directories are `pacing-20260909-205058`, `pacing-20260909-205254`, and
`pacing-20260909-205455` under `artifacts/xemu`. These are measurements on this
emulator/host, not original-hardware benchmarks or a claim of playable speed.
Per-stage call counts can differ by one because the reads occur during a tick.
Projection remains the largest measured cost. The two-pass projection and full
level traversal remain; moving work out of catch-up ticks and visibility work
still need attention. The profiler's presentation category includes platform
integrity checks and GPU waits, so it is not a pure GPU execution measurement.

The shared C projection loop now transforms a polygon fan's anchor and previous
corner once, retaining their rounded camera-space values and face-corner UVs.
It classifies triangles against the same six frustum planes: wholly outside
triangles emit nothing, wholly inside triangles bypass polygon clipping copies,
and crossing triangles retain the prior plane order and intersection arithmetic.
No new persistent geometry cache or GPU allocation is introduced. This is an
optimization of the port-owned preview path, not recovered original renderer code.

Verification:

- All 94 untextured level spawn images match the retained pre-classification
  executable byte-for-byte; all render successfully. `tools/verify_projection_corpus.py`
  takes an explicit `--reference` executable and records both executable hashes.
  The local report is `artifacts/projection-corpus.json`.
- The pre-optimization 32-camera retained-world check matches exactly, including
  its cumulative hash. The complete 664-frame PC input trace also matches the
  pre-change reference, including world/camera hashes and body state.
- The textured/lightmapped 1920x1440 staged README scene remains byte-identical.
- Full PC build and all five CTest checks pass; NXDK builds successfully.
- XEMU run `20260909-205633-876144` passes the complete 664-tick turn fixture
  in stock 64 MiB: world hash 1553922570, camera hash 2407323874, body hash
  2897823093, all ten actor rings and all 308 final body bytes match PC.
  The CPU mesh cap remains 2 MiB. No framebuffer was captured; reference
  projection hashes cover all ticks. The unbounded controller ISO was restored
  after this finite verification run.

These fixtures cover many views but do not establish original-game/PS2 visual
parity or full campaign correctness. No screenshot was posted because the visual
output is unchanged. The earlier intermittent interactive RF_RANGE error remains
open and is not claimed fixed by this optimization.
