# Xbox draw submission optimization

The normal Xbox path groups opaque static-world faces by material/lightmap and
submits larger pbkit command blocks. Shared PC gameplay/rendering remains built.
An experimental model-bounds path stays disabled for normal play: measurements
showed its CPU cost exceeded its GPU savings in the inspection scene.

## World draws

The retained static-world pass already disables blending and alpha testing and
writes depth with LESS. At cache creation, face descriptors are sorted by
material/lightmap, retaining original vertex order within each group. The GPU
vertex allocation is never read back or reordered. Per-frame room visibility
and facing still select faces; visible ranges sharing a material/lightmap now
share one BEGIN/END, including noncontiguous ranges. Geometry, shading, texture
coordinates and draw-range limits remain unchanged.

This grouping applies only to the opaque retained-world pass. CPU geometry,
models, particles and their relative presentation order are unchanged. Material
sorting can affect exact-depth ties between overlapping opaque surfaces; the
native pixel comparison covers the tested camera, not every campaign surface.
Future transparent-world support must preserve its required order separately.

The existing4MiB world-cache budget is unchanged: sorting uses the existing
descriptor array, with no second face/vertex cache. `rf_xbox_world_groups[2]`
reports BEGIN/END groups and visible contiguous vertex ranges. Setting
`renderer-world-off.flag` before loading retains the prior source-order ranges
with their separate BEGIN/END pairs (`xemu_render_check.py --unsorted`).

## GPU submission blocks

The installed `C:/nxdk/lib/pbkit/pbkit.c` calls `pb_cache_flush` from every
`pb_end`: an sfence, an MMIO cache-flush request/poll and DMA write-pointer
publication. The new builder groups method words into blocks of at most128
dwords, following pbkit's explicit debug limit. Framebuffer/resource lifetime
waits remain. Shader loading groups up to24 four-word instructions (120 words
including method headers). Bone constants retain the eight-float4/32-dword
method window; multiple complete methods may share a submission block.

`rf_xbox_command_blocks[6]` reports actual/unbatched block counts for retained
world, shader uploads and model parts. World material grouping separately
reduces the method stream itself. Setup/particles/HUD outside these sections are
excluded. `renderer-batch-off.flag` restores tiny blocks for comparison.

## Model-bounds experiment: disabled in normal play

The retained GPU backend can reject entirely off-screen batches without CPU
fallback. `rf_model_bounds_build` follows batch-local triangle indices and
backward position-reuse chains. It bounds positions per influencing bone using
the GPU's first-zero-weight termination rule. Nonnegative weights totaling at
most256 put each vertex inside the convex hull of bone-transformed boxes and
the origin (when below256). Larger totals bypass rejection.

Posing uses double intervals with float-error margins, then caches expanded
float center/extents. Exact prepared-matrix byte equality reuses the posed box;
rigid boxes require only one evaluation. Camera tests reject only a complete
box beyond one of four homogeneous screen planes or behind the camera, keeping
crossings and uncertain overflow. Near/far policy stays with the renderer.
Animation, events, physics and collision continue independently of rejection.

When explicitly enabled by `renderer-cull-on.flag` / `--culled`, a record costs
1,216 bytes plus48 bytes per bone for its matrix key. It and the fixed box/cache
metadata count inside the retained-model4MiB cap, and retire with that cache.
Normal play allocates no bounds records or keys. `rf_xbox_model_visibility[8]`
reports checked/rejected skeletal and rigid batches, rejected vertices, bound
bytes, bypasses and the disable flag; `rf_xbox_bounds_poses[2]` reports computed
and reused boxes.

Before world grouping, the same-XBE culling/batching comparison
`render-20260914-152457` / `render-20260914-152721` passed selected PC gameplay
state and had zero differing final RGB pixels. Culling removed43/106 batches
and13,446/36,507 vertices, with24 bounds evaluations/82 reuses and40,384 bound/key
bytes. However NPC preparation rose from1.982ms to3.921ms, while the GPU draw/
wait phase only fell from19.957ms to19.299ms. These concurrent-emulator phase
measurements justified keeping culling off; reduced vertex counts alone did not
establish a performance win. Revisit only with evidence from a GPU-bound scene.

The shared helper passes12,000 comparisons against independent brute-force
float skinning of every triangle vertex:9,338 rejected/2,662 retained. Tests
cover supported bone counts, reuse, weight termination, reflection, shear,
nonuniform scale, mirrored/off-center projection, camera crossings and invalid
inputs. This verifies conservative rejection, not completeness of rejection.

## Native validation

Final default-path pair: `render-20260914-153547` (source order/tiny blocks) and
`render-20260914-153350` (grouped world/larger blocks), both with model culling
off. The comparator passes with zero differing pixels and identical selected
PC gameplay checks. Both submit106 model batches/36,507 model vertices with
zero fallback and770,476 accounted retained-model bytes. World BEGIN/END groups
fall from510 to41; visible contiguous ranges change from510 to509. The frame
still draws the same837 faces/5,274 vertices.

Measured retained-world/shader/model-part pbkit blocks fall from1,530/127/644
to15/12/139. Native draw/wait phase mean falls from22.183ms to15.354ms; platform
sink mean falls from24.787ms to17.567ms. Concurrent emulator activity affects
these phase timings: they are supporting evidence, not a sustained FPS claim.
Machine-readable comparison: `artifacts/draw-submission-comparison.json`.

Unlimited controller run `play-20260914-154020` measured26.102 presented FPS,
53.625 simulation ticks/sec and22.086MiB available across45.016 seconds. Its
sampled player body matches all77 words from the preceding live baseline, with
the same575 visible world faces/3,003 vertices. Its world groups fall from the
baseline's322 to24; contiguous ranges change322 to318. The prior45-second live
baseline measured21.067 FPS/37.556 simulation ticks/sec. Another project's XEMU
was present at that baseline and absent when checked after the new window; its
exit time is unknown. Therefore the23.9% observed FPS difference is NOT a
controlled optimization speedup. The older27.612 FPS run also had different host
conditions. Evidence: `artifacts/model-optimization-live-baseline.json`,
`artifacts/xemu/play-20260914-154020/sustained-performance.json` and
`artifacts/draw-submission-live-comparison.json`. The optimized emulator was
left running without a frame limit; no host input or desktop capture was used.

`compare_native_renderer.py` requires identical XBE bytes except the two cxbe
packaging timestamps, matching PC reference/input/camera, selected gameplay
state, zero fallback and every RGB pixel of the final native framebuffer. The
timestamps are at XBE header0x114 and certificate+4, from installed
`C:/nxdk/tools/cxbe/Xbe.h`; every other byte participates in the comparison.
These180-frame checks cover one final image, not a full animation/campaign run.
Sustained FPS is measured separately with the unlimited controller launcher.

```powershell
ctest --test-dir build/pc -C Release --output-on-failure -R conservative_model_bounds
python tools/xemu_render_check.py --frames 180 --seconds 240 --unsorted --unbatched
python tools/xemu_render_check.py --frames 180 --seconds 240
python tools/compare_native_renderer.py artifacts/xemu/render-REFERENCE artifacts/xemu/render-OPTIMIZED --out artifacts/draw-submission-comparison.json
python tools/xemu_play.py
python tools/xemu_performance_watch.py artifacts/xemu/play-RUN/live.json --seconds 45
```

First-person GPU rendering, busy-room and moving-camera coverage, coplanar
surfaces, cache retirement/handoffs and stock hardware verification remain open.
