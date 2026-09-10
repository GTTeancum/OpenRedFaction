# Projection profiling and optimization

## Static world plane rejection

The original cached static-solid renderer `55f5e0` calls `4163a0` at `55f82f`
and compares its unrounded result with zero. `55f841` rejects nonpositive or
unordered distance; `55f846` accepts strictly positive distance. The alternate
cached path repeats the rule at `55fa40`. The viewer was copied from `1818690`
at `55f637`; the cached face plane is at record+8. `4163a0/40a0b0` evaluate
z product + y product + x product + plane constant in that order. This evidence
is for RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
The local Dash Faction legacy renderer hook at 5605f6 supplied the function
address lead; no renderer source was copied from that project.

`rf_preview_plane_visible` reconstructs this predicate. The shared preview
applies it to static world faces before projection/clipping, after validating
their material/lightmap references. Movers retain the previous behavior until
their local-camera path is verified. This avoids allocating or uploading
static backfaces; it adds no mesh allocation and preserves the 2 MiB CPU/GPU cap.
The original cached-face construction and complete room/portal visibility are
still outside this implementation.

`tools/verify_world_facing.py --nxdk` checks 2,500 cases against the original
branch and both callees, including exact plane boundaries and nonfinite values.
PC and compiled NXDK match every decision. NXDK restores incoming FPCW027f after
using extended precision. Both builds, five CTests, and the 32-view retained
projection/allocation check pass.

The campaign-start yaw overflow at tick 342 is resolved: all 480 ticks complete
with a 1,161,048-byte CPU world peak. Stock-64-MiB XEMU replay-20260909-221358
passes PC world/camera hashes, input ring, initial spawn words and final body;
sampled completed GPU peak is 1,157,856 bytes. Normal diagnostic 664-tick replay
replay-20260909-221735 also passes. Both harness runs restore the regular ISO.
`tools/verify_player_replay.py` now exercises all four 480-tick commands at both
the old diagnostic and campaign start. All eight pass, and the campaign yaw
case still verifies the greater-than-1-MiB fallback.

Image audit against the retained pre-culling PC renderer: neutral campaign and
diagonal replay final images are exact. Across 94 untextured starts, 30 are
exact and 64 differ by 1..221 pixels. The largest change was inspected and shows
small edge differences. The 1920x1440 README reproduction differs by two pixels;
the stored screenshot remains unchanged. These are measurements against the
old double-sided port preview, not original-game or PS2 image validation.
Strict `verify_projection_corpus.py` correctly fails on differing images;
its explicit `--report-differences` mode records them as DIFFERENCES, not PASS,
with hashes and retained differing images. See artifacts/projection-corpus-differences.json.

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
Projection remained the largest measured cost in these three runs. Full level
traversal, visibility and moving work out of catch-up ticks still need attention;
the subsequent single-pass change is described below. The profiler's presentation category includes platform
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


## Single-pass retained projection

The actor-follow path now generates the world into the idle actor half of its
existing 2 MiB CPU vertex allocation. Only a successful projection is copied to
the world half; the actor is generated and appended afterward. This removes the
sizing traversal without adding any allocation or changing the effective world
vertex capacity. The split is rounded down to a whole 56-byte vertex, leaving
1,048,544 world bytes and 1,048,608 scratch bytes. The previous 1 MiB world budget
also held only 18,724 whole vertices, so the representable output is unchanged.

`rf_preview_update_world_staged` exposes this path for callers that own scratch
storage. Scratch can change on failure, but destination bytes and mesh metadata
are preserved. Overlapping ranges and undersized scratch are rejected before
generation. The existing two-pass API remains available to callers without
scratch space. Inputs must remain stable and disjoint from both output ranges.
This is port-owned rendering work, not recovered original game code.

Live run `pacing-20260909-210227` passes the pacing check in stock 64 MiB:
world projection averaged 31.50 ms/tick, presentation/integrity 24.82 ms,
animation/stance 3.88 ms, and model rendering 6.06 ms. Throughput measured
14.62 ticks per guest second, versus 9.81 in the previous run. GPU vertex
storage remains 2,097,152 bytes. These host/emulator observations still fall
well short of the simulation target and do not establish hardware performance.

The 32-camera retained-world test now compares staged output with independently
allocated two-pass output on every view. It also checks undersized scratch,
overlapping ranges, output-capacity exhaustion after partial staging, and an
invalid late mover pose: all failures preserve the entire destination allocation
and descriptor. Both the original 32-view transcript and complete 664-tick PC
input transcript remain byte-identical. All five CTest checks and both builds pass.


Full Xbox turn verification `20260909-210345-153679` also passes in stock 64 MiB:
664 world/camera ticks, all ten actor rings, and the final 308-byte body state
match PC. World hash remains 1553922570 and body hash 2897823093. This exercises
the staged path on the compiled Xbox target with its adjacent scratch region.
The ongoing controller ISO was restored afterward; no new screenshot was taken.


## Presentation waits

Xbox renderer profiling now exports `rf_renderer_profile[8][4]` using the same
calls/elapsed-low/elapsed-high/maximum layout as the scene profiler. It excludes
the first 16 stream submissions. The rows separate validation, the pre-upload
GPU wait, vertex upload/color adjustment, shader/state setup, the initial VBlank
and clear, draw submission/completion, swap queuing, and the final wait.

Run `pacing-20260909-210637` measured 9.26 ms in initial VBlank/clear and 15.14 ms
in the trailing VBlank wait. Actual draw submission/completion was 0.46 ms; upload
and color adjustment took 1.13 ms. The presentation/integrity category totaled
27.23 ms. This identifies a synchronization delay, not heavy GPU drawing, in this
particular static scene.

The renderer now omits the trailing wait. It retains the next frame's initial
VBlank wait, GPU completion before publishing framebuffer metadata, and
`pb_finished()` queue back-pressure. The installed NXDK implementation queues the
swap and advances its triple-buffer index in `pb_finished`; its triangle sample
uses one initial VBlank wait and no trailing wait. Source evidence:
`C:/nxdk/lib/pbkit/pbkit.c` (`pb_wait_for_vbl`, `pb_finished`) and
`C:/nxdk/samples/triangle/main.c`. Installed NXDK HEAD:
`fb5a9a7a58a431e8d70a9e7da87898059df376c0`; pbkit.c SHA256:
`2f57944124b9250d6ffdc071b2b60c2ec2f105edd3771b52282f67bbfd4bac91`.
This is port-owned scheduling, not a recovered Red Faction rendering policy.

Run `pacing-20260909-210919` passes the live pacing check: trailing phase 0.01 ms,
initial VBlank/clear 10.35 ms, and presentation/integrity 13.62 ms. Observed overall
throughput changed from 14.47 to 15.25 ticks/guest second. CPU projection varied
from 30.70 to 36.82 ms between these runs, so they are not a controlled whole-game
speed benchmark. The 60 Hz simulation target remains unmet.

The pacing harness now owns `local/xemu-harness/pacing-base.qcow2`, copied once
from the installed emulator image and subsequently used with temporary snapshots.
This avoids the shared HDD lock encountered in failed launch
`pacing-20260909-210753`; no other emulator was closed. The general smoke harness
also accepts `--hdd <path>` for a separate base image. Source HDD writes remain
isolated by `-snapshot`. No image or desktop capture was used.


Full turn run `20260909-211014-307540` passes after the presentation change:
664 ticks, ten PC-matching actor rings, the 308-byte final body, and unchanged
2 MiB GPU vertex storage on stock 64 MiB XEMU. The ongoing controller ISO was
restored. No framebuffer comparison was performed in this run; GPU completion,
submission metadata, scene/state parity and memory telemetry were checked.


The later capacity fix described in `INPUT.md` allows first-person views that
exceed the staging half to use the entire existing 2 MiB destination through the
two-pass path. The single-pass figures above apply to views fitting the staging
half; they are not performance claims for the larger fallback views.
