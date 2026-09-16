# Original child-face degeneracy rejection — 2026-09-15

`tools/verify_geomod_csg_degenerate_face.py` executes complete4dfe20 with no hooks for30 cases:26 geometries/plane modes and4 supplied-plane direction/magnitude cases. `tools/verify_geomod_csg_split_degenerate_child.py` executes three complete4e2650 splits with actual corner copying, ring moves, interpolation and4dfe20 validation. Allocation/incidence containers are supplied as in the prior whole-split probe; face disposal4cfb20 is recorded rather than freeing original allocator state. JSON retains all results. Same RF.exe SHA-256 as earlier reports.

## Actual rejection policy

The split caller passes the parent face as the plane source to4dfe20 (4e2953/4e299b), so child validation normally copies that plane rather than recomputing a new normal.

For a supplied plane, acceptance requires more than two corners and a **nonzero dot product of the plane normal with the accumulated triangle-fan cross products**. The final comparison is against exact zero, not a positive area epsilon. Negative projection is also accepted; this routine is not a winding-consistency guard. A valid XY triangle with supplied X-axis or zero normal is rejected; positive, negative and doubled Z normals are accepted.

Executed unit-base triangles with heights1,1e-4,1e-6,1e-12,1e-20,1e-30 and1e-40 are all accepted with supplied Z plane. Exactly collinear/zero-height and two-corner faces are rejected. A self-crossing bowtie whose signed fan area cancels is rejected. A duplicate consecutive corner is retained and the face accepted when total projected area remains nonzero. Validation leaves all tested ring links and corner attributes byte-for-byte unchanged; it does not remove duplicates or collapse short edges.

With a null plane source,4dfe20 first computes a Newell-style normal and plane. Extremely small values can behave differently: the1e-40-height case rejects in that mode, while the supplied-parent-plane case accepts. Do not interpret the supplied-plane result as an unconditional guarantee for arbitrary normal recomputation or every floating-point underflow case.

## Complete split behavior

A four-corner polygon(0,0),(1,-h),(2,0),(0,1) is split from vertex0 to2. At h=0, its first child is exactly collinear:4dfe20 rejects it,4e2650 calls4cfb20 and explicitly sets that output pointer to0 at4e296a. The second child survives as a triangle. The overall split still returns1 and the source ring is empty; this is partial child survival, not rollback of the split.

At h=1e-12 and1e-30, both children survive as triangles with original corner attributes intact. These cases execute actual split construction and classification, not only a stand-alone area calculation. Disposal is a recorded boundary, so the fixture proves the caller's removal decision/output nulling but not allocator reclamation internals.

Recommendation: do not add a broad positive-area/sliver cutoff and label it original parity. Original child validation tolerates very thin nonzero fragments and may retain duplicate consecutive corners. Keep topological identity and existing closure checks distinct from this rejection test. If the port chooses stronger robustness guards, document those as policy and verify they do not erase valid shallow/repeated-cut surfaces.

## Live reconstruction fix

The second overlapping shallow blast generated a three-corner fragment A,B,A (A=-16.3084259,-8.89137554,7.17526579; B=-16.3084297,-8.89137936,7.17528343). Its zero area later caused polygon subtraction to reject the entire edit. The shared splitter now suppresses exact-zero-area children immediately after intersection generation, preserving valid siblings without introducing an area tolerance or positional welding.

The live regression now accepts both cuts; the physical terrain snapshot closes with952 vertices/183 faces. Duplicate and reset/refire fixtures still pass, as does the eight-cut original-template closure/volume/ray suite. Native400-frame run render-20260915-195913 passes47 comparisons with8581 free pages. Actual framebuffer inspected; crater darkness remains unresolved. Owned PID57360 exited and21 disc entries were restored. PC-only physical snapshot export was added afterward and its mesh was checked independently.
