# Remaining-solid clipping inside chronological reconstruction

The private chronological builder now accepts an optional current-solid clip
context. It binds the previous committed mesh to collision polygons, clips each
new cutter face against that actual remaining volume, and carries source/cut
edge-plane IDs into the existing compact-lineage append and cavity repair.
The usual builder wrapper passes NULL, retaining production behavior. The new
context is currently supplied only by isolated tests, with explicitly owned
static scratch; it has no production allocation/budget policy yet.

`geomod_current_solid_replay` exercises the existing16 solid/cavity cases using
this alternate branch. Each case checks closed coverage and1452 collision
queries against both the legacy result and current production replay. All52
retained target UV comparisons are unchanged. The new cap partitioning changes
tessellation, so the experimental target compares geometric coverage rather
than requiring the old vertex/face arrays to be byte-identical. The production
test retains its exact mesh-byte check.

`geomod_extracted_replay` composes a real first cut, grouping, extraction,
support-ID compaction and later cuts in a private replay owner:

1. A central slice separates a20-unit box. Remove its right component and
   retain the left: volume3600,6 faces.
2. A later cutter lies wholly in the removed region: volume remains3600,
   still6 faces/24 corners, and no vertex crosses the retained boundary.
3. A cutter crosses that boundary: volume3568,14 faces, closed coverage.

The test remaps support arrays using stable original-face mapping before
overwriting private active geometry. It leaves immutable source geometry intact,
so this directly exercises the resurrection concern from the earlier design
note rather than replacing the source with a smaller box.

Validation: all four relevant CTests pass (chronological_solid, disconnected,
current_solid_replay, extracted_replay); stock NXDK build succeeds. Production
still uses the previous path. This does not prove save/reload of extraction,
arbitrary removed shapes, stock-runtime memory/performance, atlas transfer,
body publication, rollback of the new context, or visual behavior. Those must
be completed before enabling this branch for live destruction.
