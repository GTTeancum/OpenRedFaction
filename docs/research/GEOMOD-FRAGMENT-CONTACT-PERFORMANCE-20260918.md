# Fragment contact performance — 2026-09-18

## Scope

Optimize the newly added reciprocal fragment-face contacts without removing thin static or committed-mover coverage. This is port implementation work, not a newly recovered original-game algorithm. The existing four angular intervals, strict earliest-hit order, response rules and save format remain unchanged. Edge/edge coverage and relative moving-platform response remain open.

## Measurement

The shared 24-word `rf_scene_fragment_profile` v2 ledger counts collision work and times `scene_detached_tick` with the existing Xbox guest millisecond clock. It excludes the first 16 warm-up frames; startup numerical fixtures are reset out of the ledger. PC has deterministic work counts but no guest timing. The harness compares the work counters across PC/Xbox and records timing separately. These are emulator subsystem timings, not hardware measurements or whole-game FPS.

Baseline native run: `artifacts/xemu/render-20260918-024344` (v1, 20 words). Stock64MiB, source108, three connected source brushes, 600-frame first-cut replay, native contact fixture enabled. Across79 active ticks and698 queries, fragment updates consumed9895ms: mean125.253ms, maximum243ms. It evaluated337472 triangles and73678 poses, with7668 corner casts and13930 reciprocal vertices.

## Implementation

Prepare five fragment poses and the local triangle planes once per world/mover reciprocal query, retaining original face/fan traversal order. Reject each local vertex-path interval against padded fragment bounds before triangle tests. A single serialized scene scratch buffer holds128 prepared triangles (14368 bytes on32-bit Xbox); geometry beyond capacity falls back to complete uncached traversal. The buffer is rebuilt for every query, so terrain publication cannot leave stale geometry. There is no heap allocation or persistent per-piece cache.

Tests include a129-triangle shape whose only contact lies in triangle129, followed by mutation of that triangle in the same storage to prove no retained hit. The standalone convenience wrapper lives in the test translation unit; production builds contain only the shared prepared-query implementation.

## Validation

- All123 PC tests pass.
- Both source92/source108 destruction histories preserve uninterrupted versus reloaded saves, protected-trim rejection and second-junction destruction.
- The first source108 save remains5300 bytes with SHA256 `d27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba`, equal to the native baseline.
- First- and second-cut floor audits pass both0.005 penetration and clearance limits; audit saves match ordinary continuation.
- PC work counts:698 queries,7668 corner casts,13930 reciprocal vertices,9048 triangle tests,7518 pose evaluations,15152 triangle preparations,15182 local interval rejects,698 shape preparations and zero fallbacks in this authored replay.

Optimized native run: `artifacts/xemu/render-20260918-025539` passed79 checks over600 frames, including all64 shared static/translated/rotated-mover fixture words and every compared work counter. Guest memory remained67108864 bytes, with3325 free pages (12.988MiB) at the endpoint. The harness restored its disc inputs and closed its emulator.

Active fragment time fell from9895ms to8195ms over the same79 active ticks: mean125.253ms to103.734ms (17.18% less time); peak243ms to230ms. Triangle tests fell97.32%, pose evaluations89.80%. The much smaller wall-time reduction shows that this triangle/pose work was only part of the cost. Whole-game FPS improvement is not established. These two runs are useful evidence, not a repeated benchmark or real-hardware performance claim.

The Xbox checkpoint matches PC and baseline exactly (SHA above). Native framebuffer SHA256 `ad18ceadb48c89b8c98548d169a7e9bc51b785197a4ac223c326bcc9bab41ca3` is byte-identical to the previously inspected `render-20260918-020011/framebuffer.png`; no changed visual content or new GitHub image. Remaining active update cost (~104ms average) is still unacceptable as a final target. Next measure the sphere/corner/world traversal portions before choosing another optimization.

The first native build attempt (`render-20260918-025428`) stopped before launch because the test-only standalone wrapper was unused in production under NXDK's warnings-as-errors. Moving that wrapper into the unit test fixed the build; the final PC executable and focused tests were rebuilt and passed afterward.


## Stage attribution

`artifacts/xemu/render-20260918-025903` measures sum/maximum-per-query guest milliseconds for the four contact stages in a separate8-word `rf_scene_fragment_stage_ms` ledger. It is reset with the other frame-zero counters and neither serialized nor used by simulation. Sphere time includes the visible-mesh qualification of a sphere hit; corner time includes pose preparation and supporting-side admission. Times are millisecond-resolution and may vary between emulator runs.

| Stage | Total ms | Maximum per query ms |
| --- | ---: | ---: |
| Sphere sweep and mesh qualification | 5245 | 31 |
| Mesh corner sweep | 2415 | 27 |
| Reciprocal committed mover vertices | 0 | 0 |
| Reciprocal world vertices | 129 | 2 |

Active fragment updates total7856ms across79 ticks (~99.443ms each); sphere and corner stages account for approximately67% and31% respectively. Zero mover time in this authored replay does not establish mover-query performance. Do not claim the difference from the prior103.734ms mean as another optimization: this pass only adds instrumentation, and run-to-run timing varies.

All79 checks pass, the deterministic work ledger is unchanged, and save and framebuffer are byte-identical to the prior optimized native run. Endpoint memory remains3325 pages; the disc was restored and the owned emulator closed. All123 PC tests also pass.

Inspection shows `collision_sweep_tree` validates every node before each traversal, while `sweep_rooms_prepared` revalidates the complete room/child lists for each sphere. `rf_collision_body_sweep` calls that route independently for each sphere. This is a candidate cause, not yet a measured attribution within the sphere stage. Next implement or instrument bounded query-local reuse of immutable world/tree validation across sphere sweeps; preserve public malformed-input checks, candidate ordering and exact contact results, and never retain validation across terrain publication or mover updates. Re-measure against this run before claiming an improvement.
