# Retained Xbox skeletal rendering

Status: implemented, compile checks only. The user has paused gameplay, parity and emulator tests. There is no measured FPS gain or native visual-correctness result for this backend.

## Work removed from each frame

The Xbox campaign can now keep NPC bind geometry in GPU-accessible memory and submit bone matrices instead of CPU-skinned, projected and clipped triangles. The callback in `scene_npc_render_family` accepts individual material batches. Accepted batches skip scratch initialization, vertex skinning, normal processing, CPU projection, triangle clipping/expansion, mesh hashing and dynamic vertex upload. CPU pose evaluation, collision, room visibility, animation controllers and game logic remain unchanged.

The historical `artifacts/step-profile/performance.json` run attributed about5.462ms to NPC presentation, within about52.4ms of scene work. Those figures describe the old path for one scene; they are not a promised saving. Retained static-world rendering separately targets the historical13.029ms world rebuild. Runtime measurements must establish the combined result and whether GPU processing becomes the next bottleneck.

The shared PC renderer keeps its existing implementation. Missing textures keep the CPU ambient-color fallback. Unsupported projection, a full queue/cache, memory limits or allocation failure use the existing CPU batch path. Malformed input still returns an error. Skipped CPU vertex counts/hashes intentionally differ on Xbox when a batch is retained.

## Cached data and GPU work

`retained_models.h` is private to the Xbox renderer. It caches batches by source geometry, batch number and bone count. At first use, a two-pass builder follows reused-position references, preserves each vertex's own UVs, and copies expanded triangles in source order. Positions and up to four weights/bone indices are retained once; weights use the shared renderer's denominator256 and stop at the first zero influence. These are triangle arrays, not an indexed GPU mesh.

Each draw part has at most28 bones:84 float4 rows fit c8..c91 of the Cg vp20 profile. A palette overflow or change in authored two-sided flag0x20 starts another part without reordering triangles. An unused influence fetches palette slot0 with zero weight. The shader applies the four weighted transforms, the model-local camera transform and screen projection. The generated program has64 instructions; c0..c5 hold view/projection/depth/tint and the compiler's c6 literal is uploaded explicitly. Active palette rows are uploaded at most eight float4 rows per command: the NV097 constant method window accepts32 dwords. See XEMU's [method range](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/methods.h.inc).

Single-sided parts use GPU backface rejection; front winding accounts for the local-view determinant and screen-axis signs. Flag0x20 disables culling for that part. Shader arithmetic follows the shared mathematical transforms, without reproducing x87 intermediate rounding. The existing textured-NPC color modulation is retained. First-person weapons, rigid clutter, world weapons and pickups are not part of this change.

The shader follows the installed NXDK `samples/mesh/vs.vs.cg` screen-space convention: divide XYZ explicitly and keep camera depth in W. XEMU's [vertex-program implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c) converts that output back into homogeneous clip coordinates for its host renderer. Its [draw-state implementation](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/gl/draw.c) reverses host front winding to account for screen Y. This source inspection supports the implementation choices; it does not verify this game's near-plane crossings, exact-zero camera depths, depth agreement or winding in a live frame.

## Ordering and stock64MiB ownership

An accepted batch queues a copy of its stack-owned matrices/view and its current position in the CPU triangle stream. The presenter interleaves these records at that position, preserving relative order with CPU fallback batches, later clutter and the first-person weapon. It restores the projected-vertex program, literal constant, vertex arrays and culling afterward, and invalidates texture/blend bindings before resuming CPU draws. An all-retained NPC group needs one shader installation.

The skeletal cache has a separate4MiB hard limit covering its128-entry frame queue, fixed cache metadata, per-part metadata and GPU vertex allocations rounded to4KiB pages. Heap allocator overhead and other game allocations are additional. Source geometry remains owned by the campaign; GPU vertices and part metadata are copied, while queued pose data lasts for one frame. CPU code only writes the write-combined vertex buffer at cache creation and does not read it back. Stream shutdown waits for GPU completion and frees the cache before a new level is loaded.

This does not establish the total64MiB peak. The static-world cache has its own4MiB limit, and both caches coexist with original geometry, images, CPU scratch, the dynamic GPU stream and framebuffers. Allocation failures and over-budget batches fall back; representative level loads and handoffs still need actual memory evidence.

`rf_xbox_retained_models[8]` publishes queued batches, rendered batches, successful cached batches, accounted resident bytes, submitted vertices, submitted palette parts, frame fallbacks and lifetime cache misses. These counters describe rendering work, not gameplay correctness or visual parity.

## Checks still required when testing resumes

- Start with a short representative hall run containing the miner and robot; compare frame costs, retained counters and available pages against the earlier path.
- Inspect walking and turning poses, reused-position UV seams, culling, two-sided/transparent surfaces, occlusion and the camera crossing triangles.
- Verify drawing beside CPU-rendered doors/props and first-person weapons, including a mixture of retained and fallback batches.
- Exercise cache/queue exhaustion and level handoff, then check that allocations retire and the new scene cannot reuse stale geometry.
- Update native render-stream checks to distinguish intentional GPU/CPU mesh differences while preserving gameplay-state checks.

No runtime, benchmark, replay or correctness-test process was launched for this implementation while tests were paused.

Compile evidence: `tools/build-xbox.sh` produces the XBE/XISO, and `cmake --build build/pc --config Release --target rf_pc_play` builds the maintained PC executable. Logs are under `artifacts/opening-handoff/retained-models-build.log` and `retained-models-pc-build.log`. Existing PC warnings and the existing NXDK linker section-merge warning remain.
