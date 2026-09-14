# Retained Xbox model rendering

Status: focused stock64MiB XEMU runtime checks have resumed. The first180-frame check renders and matches selected PC gameplay state; full visual/parity checks remain open. See [sustained performance work](XBOX-PACING-POSE-SHARING.md).

## Work removed from each frame

The Xbox campaign can now keep NPC bind geometry and rigid prop/weapon geometry in GPU-accessible memory. It submits camera/bone matrices instead of CPU-skinned, projected and clipped triangles. A shared callback accepts individual material batches from NPCs, clutter, held world weapons and pickups; bone count0 with NULL matrices denotes a rigid mesh. Accepted batches skip scratch initialization, vertex skinning, normal processing, CPU projection, triangle clipping/expansion, mesh hashing and dynamic vertex upload. CPU pose evaluation, collision, room visibility, animation controllers and game logic remain unchanged.

The historical `artifacts/step-profile/performance.json` run attributed about5.462ms to NPC presentation, within about52.4ms of scene work. The same run attributed1.606ms to clutter,0.692ms to world weapons and0.731ms to pickups. Those figures describe the old path for one scene; they are not a promised saving. Retained static-world rendering separately targets the historical13.029ms world rebuild. Runtime measurements must establish the combined result and whether GPU processing becomes the next bottleneck.

The shared PC renderer keeps its existing implementation. Missing textures keep the CPU ambient-color fallback. Unsupported projection, a full queue/cache, memory limits or allocation failure use the existing CPU batch path. Malformed input still returns an error. Skipped CPU vertex counts/hashes intentionally differ on Xbox when a batch is retained.

## Cached data and GPU work

`retained_models.h` is private to the Xbox renderer. It caches batches by source geometry, batch number and bone count. At first use, a two-pass builder follows reused-position references, preserves each vertex's own UVs, and copies expanded triangles in source order. Positions and up to four weights/bone indices are retained once; weights use the shared renderer's denominator256 and stop at the first zero influence. Skeletal vertices occupy52 bytes; rigid vertices contain only position/UV and occupy20 bytes, with no bone attributes. These are triangle arrays, not an indexed GPU mesh.

Each skeletal draw part has at most28 bones:84 float4 rows fit c8..c91 of the Cg vp20 profile. A palette overflow or change in authored two-sided flag0x20 starts another part without reordering triangles. An unused influence fetches palette slot0 with zero weight. The shader applies the four weighted transforms, the model-local camera transform and screen projection. The generated skeletal program has64 instructions; the rigid shader has18 and uploads no bone palette. Both use the same camera/projection constant layout; c0..c5 hold view/projection/depth/tint and the compiler's c6 literal is uploaded explicitly. Active palette rows are uploaded at most eight float4 rows per command: the NV097 constant method window accepts32 dwords. See XEMU's [method range](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/methods.h.inc).

Single-sided parts use GPU backface rejection; front winding accounts for the local-view determinant and screen-axis signs. Flag0x20 disables culling for that part. Shader arithmetic follows the shared mathematical transforms, without reproducing x87 intermediate rounding. The existing textured-NPC color modulation is retained. Rigid models keep current per-frame placement and authored two-sided flags. GPU winding comes from vertex positions; correspondence with the stored static face-plane tests still needs native validation. First-person weapons and moving world geometry are not part of this backend.

The shader follows the installed NXDK `samples/mesh/vs.vs.cg` screen-space convention: divide XYZ explicitly and keep camera depth in W. XEMU's [vertex-program implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c) converts that output back into homogeneous clip coordinates for its host renderer. Its [draw-state implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/gl/draw.c) reverses host front winding to account for screen Y. This source inspection supports the implementation choices; it does not verify this game's near-plane crossings, exact-zero camera depths, depth agreement or winding in a live frame.

## Ordering and stock64MiB ownership

An accepted batch queues a copy of its stack-owned matrices/view and its current position in the CPU triangle stream. The presenter interleaves these records at that position, preserving relative order with CPU fallback batches, later clutter and the first-person weapon. It restores the projected-vertex program, literal constant, vertex arrays and culling afterward, and invalidates texture/blend bindings before resuming CPU draws. The presenter switches shaders when the ordered queue changes between skeletal and rigid meshes. Consecutive batches of the same kind share the installed shader; CPU fallbacks still restore the projected-vertex state.

All retained model kinds share a separate4MiB hard limit covering its128-entry frame queue, fixed cache metadata, per-part metadata and GPU vertex allocations rounded to4KiB pages. Heap allocator overhead and other game allocations are additional. Source geometry remains owned by the campaign; GPU vertices and part metadata are copied, while queued pose data lasts for one frame. CPU code only writes the write-combined vertex buffer at cache creation and does not read it back. Stream shutdown waits for GPU completion and frees the cache before a new level is loaded. Any future in-place model-topology change must invalidate its cached geometry.

This does not establish the total64MiB peak. The static-world cache has its own4MiB limit, and both caches coexist with original geometry, images, CPU scratch, the dynamic GPU stream and framebuffers. Allocation failures and over-budget batches fall back; representative level loads and handoffs still need actual memory evidence.

`rf_xbox_retained_models[8]` publishes queued batches, rendered batches, successful cached batches, accounted resident bytes, submitted vertices, submitted draw parts, frame fallbacks and lifetime cache misses. `rf_xbox_retained_model_kinds[6]` separates skeletal/rigid queued batches, submitted batches and submitted vertices. Scene mesh counts/hashes describe only remaining CPU triangles; retained placement/batch counts describe submissions, not confirmed visible pixels. These counters describe rendering work, not gameplay correctness or visual parity.

## Remaining runtime checks

- Start with a short representative hall run containing the miner and robot; compare frame costs, retained counters and available pages against the earlier path.
- Inspect walking and turning poses, reused-position UV seams, culling, two-sided/transparent surfaces, occlusion and the camera crossing triangles.
- Verify props, pickups and held world weapons with rotating/moving placement, beside CPU-rendered doors and first-person weapons, including a mixture of retained and fallback batches.
- Exercise cache/queue exhaustion and level handoff, then check that allocations retire and the new scene cannot reuse stale geometry.
- Update native render-stream checks to distinguish intentional GPU/CPU mesh differences while preserving gameplay-state checks.

Tests were paused during implementation; the subsequent focused render run is recorded in [the performance notes](XBOX-PACING-POSE-SHARING.md).

Compile evidence: `tools/build-xbox.sh` produces the XBE/XISO, and `cmake --build build/pc --config Release --target rf_pc_play` builds the maintained PC executable. Logs are under `artifacts/opening-handoff/retained-rigid-build.log` and `retained-rigid-pc-build.log`. Existing PC warnings and the existing NXDK linker section-merge warning remain.

First-person GPU rendering remains open: its CPU path clips at0.01 units and writes `16384/(1+reciprocal_z)` for a separate depth band. Reusing the world shader would change clipping/self-occlusion; retain that path until its full projection policy is adapted.

The subsequent [draw submission pass](XBOX-DRAW-SUBMISSION.md) groups GPU
commands into blocks of at most128 dwords. Conservative posed-batch bounds and
matrix-key caching remain an opt-in experiment: their measured CPU cost exceeded
the GPU savings in the initial scene. Normal play does not allocate those records.
