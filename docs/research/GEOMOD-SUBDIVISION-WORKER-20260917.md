# Bounded recursive extracted-piece worker

`rf_geomod_piece_subdivide` composes shape admission, recovered slab setup,
chronological convex CSG, component extraction, recenter/radius checking and
mass-prepared piece ownership. It returns a private output bank; it does not
publish terrain edits or register scene objects.

The worker uses16 fixed queue slots, each bounded to128 corners/32 faces,
1024-corner/256-face cut scratch, and an output bank of2048 corners/512 faces/16
pieces. One temporary terrain owner exists at a time. A caller byte budget covers
worker allocation, output bank and temporary terrain peaks; allocator overhead
is excluded. A failed stage destroys all private allocations, preserving output
pointer, RNG and stats. The worker never silently converts a numerical failure
into a successful terminal chunk.

Original-derived rules: original shape gate and three-draw slab pose; at most10
split attempts per batch; extracted labels1..N-1 queue before retained label0;
children larger than their parent's pre-cut radius are discarded; terminal
pieces receive the recovered mass/grid setup.466640's reachable insertion appends
to the queue tail (the apparent sorting loop is bypassed by testing a nonzero
stack address). This is a practical shared worker, not a full binary-equivalent
implementation of4666a0. In particular, it uses port CSG and fixed port capacities,
constructs cutter poses in the source coordinate frame and returns bank ownership.

Temporary terrain IDs require unique non-UINT32_MAX source IDs, whereas child
faces can share an original surface or be newly exposed. The worker remaps each
input face to a unique temporary ID, then restores its external source ID while
copying the corresponding collision filter. This keeps filters and material/UV
metadata associated with the intended surfaces over subsequent cuts.

## Verified

Release geomod_disconnected runs three seeded20-unit cube batches:

| Seed | Attempts | Terminal pieces | Discarded | Tracked PC peak bytes |
|---|---:|---:|---:|---:|
|0|10|11|0|787552|
|12345|9|10|0|787552|
|24690|10|11|0|787552|

Every terminal piece has positive prepared mass, exact closed-edge adjacency,
and can open/close a bounded physics body. A repeated run checks RNG, stats,
geometry, placement and mass state byte for byte. Distinct authored and generated
filter markers survive recursive remapping. Budget one byte below measured peak
returns RF_RANGE with unchanged caller RNG/output/stats. The existing15 slab,
45 original pose and108 original shape fixtures remain passing.

Stock-profile NXDK build passes:subdivision-worker-xbox.log. Native runtime
memory/performance and visual behavior have not yet been tested for this worker.

## Remaining

Input must currently be convex, including children accepted by the underlying
terrain owner. Arbitrary nonconvex fragments require the current-solid CSG
integration; this limitation remains a blocker for general live extraction.
Queue exhaustion/radius-discard branches need broader cases. Atlas ownership,
live physics scheduling/response, notifications, dynamic rendering and saving
moving pieces are separate unfinished integration work. No campaign or visual
completion claim follows from these tests.
