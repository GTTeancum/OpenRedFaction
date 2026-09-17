# Nonconvex current-solid cuts and recursive pieces

The subdivision worker now calls `rf_geomod_storage_prepare_solid_cut` against
its current closed solid. It no longer requires a convex terrain owner. Source
faces must individually be convex/planar, have exact shared-edge closure, and
fit the bounded32-face/2048-corner source workspace. Cutter remains convex.
Output storage capacities are caller-owned; temporary allocation is separately
budgeted and reported. A private storage proxy publishes only pending metadata
on success; failures cannot change the committed mesh or byte-count output.

The existing current-solid winding clip classifies cap regions against actual
volume. Shared-edge provenance drives canonical intersections and topology
repair. For this path only, adjacent coplanar faces use one representative
plane; their internal seams retain actual endpoint-backed diagonals. Source
edge intersections are preferred over intersections of independently rounded
planes, including an internal seam lying on another partition plane. This
avoids a spurious corner atX11.636 in a solid bounded byX10 and subsequently
avoids almost-duplicate cap corners. No position-proximity vertex welding is
introduced. Coplanarity checks retain the existing1e-5 plane tolerance.

The worker uses storage plus collision bindings, without constructing a full
terrain tree for every temporary child. Existing per-face temporary IDs still
carry filters and external surface identity through repeated cuts.

## Center-shift polygon repair

One terminal five-corner face passed before mass-center translation but failed
the convexity gate afterwards: float translation changed a nearly-collinear
edge's inward distance to-1.3165e-5. `rebind_translated_piece` partitions only
rejected non-triangular faces into centroid-fan triangles. Boundary positions,
UV endpoints, material/source IDs, old-face mapping and filters are retained;
centroid position/UV are arithmetic means. Capacity is checked before expansion,
and append still allocates nothing. This is a practical topology adaptation,
not a recovered original triangulation policy. Invalid triangles still reject.

## Evidence

Three explicitly closed L-section solids (area391, height20, volume7820) are
cut by recovered tilted slabs. Every result has exact closed-edge adjacency,
two components and volume within0.01 of7820-78.2/normalZ. Scratch is586772 bytes
on PC. Full recursive runs then produce:

|Seed|Attempts|Terminal pieces|Tracked PC peak bytes|
|---|---:|---:|---:|
|0|10|11|933836|
|12345|9|10|933684|
|24690|10|11|933876|

Every terminal piece passes exact adjacency and physics-body creation. The
three convex batches still pass deterministic replay, distinct filter/source
identity checks and peak-minus-one budget rejection with untouched outputs/RNG.
Release geomod_disconnected, geomod_current_solid_replay and
geomod_extracted_replay pass after the changes. Stock-profile NXDK build passes:
artifacts/geomod-postedit-re/nonconvex-subdivision-xbox.log.

## Still open

An existing notch produced by the older terrain path contains T-junctions and
fails exact seed adjacency; it cannot be fed directly into this new helper.
The L fixture is authored with explicit matching edges and tests actual concave
geometry, not that older path's input normalization. Integrating live extraction
must ensure compatible closed topology first. Broader irregular/sliver and
resource-exhaustion coverage, native runtime memory/performance, scene body
scheduling, response, atlas ownership and rendering remain unfinished. There
are no new native visuals or hardware acceptance claims from this step.
