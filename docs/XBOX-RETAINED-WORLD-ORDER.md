# Retained world: contiguous spans in existing draw order

Written2026-10-08 againstc36f6cd for the16:00 parent batch. The reported
same-scene L1S1 measurement is11.5 XEMU presented FPS; renderer draw/GPU waits
cost32.902ms with3003 visible world vertices across318 draw ranges. These are
parent-provided baseline observations, not results from this patch. Target
remains30 actual presented FPS with the same scene and gameplay. No helper
build, runtime, benchmark, new fixture or screenshot was run.

## Source bottleneck and change

`retained_world_prepare` generated GPU triangles in source-face order, then
sorted only face descriptors by material/lightmap and original vertex offset.
`retained_world_draw` merges consecutive visible faces only when vertex spans
are contiguous and material/lightmap match. Sorting descriptors without their
vertex spans prevented the existing merge from combining many neighboring
faces in draw order.

Preparation now builds those descriptors first. During this load-only phase,
the existing `start` word holds the original face index. The same comparator
sorts material/lightmap, then that index; for every nonempty face this final
key has exactly the same order as its old vertex offset. Each sorted face then
emits its original triangle fan, corner positions, UVs, lightmap UVs, colors,
material and lightmap directly into final write-combined GPU storage. `start`
is replaced by the new GPU offset before publication. Counts are checked
before each span and once at the end; the existing sfence publishes the data.

## What stays invariant

- Exact rendered face order, triangle order and all vertex attributes.
- Existing opaque-pass blending/depth state, texture/lightmap bindings and
  handling of transparent textures; this does not introduce new sorting policy.
- Per-face room/detail/backface visibility decisions. Invisible faces are never
  included just to make a larger draw. Visibility holes still split spans.
- The existing252-vertex draw limit and material/lightmap range merge.
- Grouping-disabled source order and existing flag behavior.
- Vertex count, descriptor size, allocated bytes and the4MiB retained-world cap.
- No extra vertex scratch, per-frame allocation, geometry work or GPU readback.

Only initial source traversal gains a face-metadata read; frame-time drawing
uses the same loop with spans now contiguous in its existing sorted order.
Existing `rf_xbox_retained_world[4:7]`, `rf_xbox_world_groups` and renderer timing
counters remain the evidence for the parent comparison. No FPS/range reduction
is asserted before that run. This change does not hide objects, reduce detail,
alter simulation, change pacing, or substitute a lighter benchmark scene.
