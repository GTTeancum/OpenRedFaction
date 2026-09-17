# Tilted subdivision slabs through chronological CSG

`rf_geomod_piece_cutter_mesh` adapts the recovered dimensions/basis/offset to
six outward quads with normalized per-face UVs. It is a practical mesh adapter,
not a claim that original4eed90 topology or material mapping is reconstructed.
Original4eed90 ends with4d1240 (center then subtract through4d1670), consistent
with centered slab construction. The original cutter-pose45 fixtures remain.

`rf_geomod_terrain_cut_convex` accepts a bounded convex polygon cutter directly
(up to20 faces/60 corners), copies it into the ordinary non-star history slot,
and uses the existing atomic chronological mesh/tree publication. This retains
quad topology instead of requiring a triangular star decomposition. Caller
materials are retained; generated source IDs areUINT32_MAX. Existing RGCH
non-star encoding supports these records without a format change.

## Actual mesh/collision checks

The geomod_disconnected target runs15 slab cuts on a20-unit cube: three axes
and five RNG seeds. Each cut has the recovered0.2 thickness and40-unit span.
All cases verify:

- Exactly two components using exact shared-corner identity.
- Closed reversed-edge adjacency, not only matching volume.
- Remaining volume within0.01 of8000-80/principal-normal-component.
- Both exposed surfaces hit from the slab center at fraction0.005 for a20-unit
  normal displacement, within0.00001.
- A ray travelling along the slab gap crosses without collision.
- Encode/decode produces identical mesh, collision planes/filter bindings and
  query results using the existing checkpoint comparison harness.

These are shared-core composition tests, not live visible subdivision.
Stock-profile NXDK build is logged in subdivision-mesh-xbox.log.

## Findings and remaining scope

The older rf_geomod_storage_prepare_convex_cut path does not retain supporting
plane identity: axisX seed24690 yielded3 groups (a detached cap) for a shape
that should have2. It is not used by the new adapter. Routing the slab through
triangular star cutting also rejected axisY seed0 withRF_FORMAT. The new convex
quad path passes both cases with exact adjacency; those older paths retain
known thin/tilted-input limitations and are not broadly qualified by this test.

Still required: subdivision on already-concave extracted chunks, repeated child
requeue with original batch limits and radius checks, material/atlas inheritance,
physics-body scheduling and dynamic rendering. Current source qualification is
a convex box; do not generalize these15 cases to the full worker lifecycle.
