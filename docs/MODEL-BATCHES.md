# Model batch data

`rf_model_render_vertex_pair` reconstructs the fresh-vertex deformation block
at `0x52ee9d..0x52f154` inside rendering routine `0x52e9e0`. It transforms both
input streams with the prepared bone matrices, including translation for both,
accumulates byte-weighted components, then divides the six sums by 256. It
stops at the first zero weight and does not renormalize. This differs from
collision's per-contribution division and retains the observed operation/store
order, including position Y remaining extended until its weighted accumulation.
Double intermediates approximate x87; universal floating-point identity is
not claimed. The second stream comes from batch +8, but its preparation must
be traced before treating it as an ordinary normal direction.

`tools/verify_render_vertex_pair.py` executes the unmodified block for 2,000
dyadic fixtures and compares all six output components bit-for-bit. Two port
checks verify atomic invalid-bone rejection and ignoring slots after zero.
PC build, CTest and NXDK build pass. The block is not integrated into scene
drawing, and duplicate reuse, camera, projection and lighting are excluded.

The renderer's extra-data stream begins with signed 16-bit backward reuse
distances per vertex. Positive values bypass fresh deformation and reuse
earlier output. The asset audit now records these values for anomaly cases:
only elite_security_guard LOD 1 vertex 74 has positive distance (4) among the
twelve non-finite normals. The other eleven have zero, so reuse alone does
not explain their treatment. Bounds and full reuse semantics remain to port.

`rf_model_collision_vertex` reconstructs the position-deformation loop at
`0x54e344..0x54e3c0`, reached from collision routine `0x54e200`. It starts at
zero, consumes at most four link slots and stops at the first zero weight.
Each bone index selects a prepared 48-byte transform from instance offset
`0x960`; `0x4ff020` transforms the input position. The transformed point is
multiplied by weight/256 using constant `0x589e48` and added to the result.
There is no normalization by sum and no normal input in this loop.

The port accepts caller-supplied prepared transforms and adds bounds checks
before publishing output. Invalid active indices leave the result unchanged;
indices at and after the first zero weight are ignored. Original vector
addition order and float-store boundaries are retained, with double
intermediates for the matrix multiplication; arbitrary x87 extended-precision
rounding equivalence is not asserted.

`tools/verify_collision_vertex.py` executes the unchanged original loop and
its complete vector callees for 2,000 deterministic dyadic fixtures, comparing
all output bits. Two further cases check port bounds and early termination.
PC build, four CTest checks and NXDK build pass. This establishes collision
position deformation only: pose preparation, rendering deformation and
non-finite normal treatment remain separate requirements.

The installed `0x518c41` layout now has bounded vertex and triangle readers.
They stream three position floats, three normal floats, two UV floats and
eight raw bone-link bytes per vertex; triangles retain three 16-bit indices
and a 16-bit flag field. Only this observed descriptor is accepted. Position
and UV values must be finite, and triangle indices must fit the batch.
Reads allocate no heap memory and preserve output on failure.

The layout lead is the pinned [Open Faction format document](https://github.com/rafalh/openfaction/blob/e8a4a885ba866fc472702b3dc8a9e8208f9b91e4/common/include/formats/v3d_format.h),
checked against installed data; no implementation source was copied.
Ghidra and disassembly show `0x569d20` merely stores the descriptor word,
so that function supplies no evidence of numeric decoding semantics.

`tools/verify_model_vertices.py` compares per-batch hashes over all 85,866
vertices and 99,214 triangles in 599 batches / 95 models. All bytes match
independent traversal. Bounds and unsupported-descriptor probes preserve
output. Triangle flags observed are 0 and 32. PC build, four CTest checks and
NXDK build pass; the new readers are not yet used by the Xbox scene.

Twelve source vertices contain non-finite normals: mutant1 (1),
elite_security_guard (2), fp_hmac_armA (1), fp_shotgun_armA (4), and
fp_shotgun_armB (4), all V3C files. Their normal bits are preserved exactly;
the reader does not invent replacements. Bone-weight byte sums also vary
from 127 to 510 rather than always totaling 255. The interpretation and
original treatment of these cases require further reconstruction before
skinning/lighting can be considered faithful. Neither the raw link slots
nor the triangle flag bits have been given unverified runtime behavior.

The follow-up link audit now cross-checks every non-255 bone slot against
the model's BONE-section count and checks vertex use through all triangle
indices. All active indices fit their skeletons. All twelve non-finite normals
belong to referenced vertices, so unused-vertex filtering does not remove them.
19,060 vertices have weight sums other than 255: 18,598 sum to 254, 327 to 253,
41 to 252, and the remaining 94 have other sums. All are triangle-referenced.
The audit records each model/LOD/batch/vertex, raw weights/bones, active sum
and reference status in ignored `artifacts/model-link-audit.json`.

Original-code investigation found float 1/255 constants at `0x5895dc` and
`0x589d68`; `ExportBaseline.java` now exports their reference sites and enclosing
functions. These are candidate consumers, not proof of bone normalization:
for example, `0x53a370` uses 1/255 while decoding animation data, and `0x5468c0`
uses it in graphics state. No weight renormalization has been introduced.
The next required evidence is the actual mesh deformation consumer and its
treatment of these raw slots and non-finite normals.

`rf_model_file_batch` exposes file-relative ranges without allocating the LOD
blob. The original `0x569920` loader reserves 56 bytes per batch, aligns to
16 bytes and assigns eight region pointers in sequence: positions, normals,
UVs, indices, optional triangle planes, extra data, optional bone links and
optional auxiliary data. Each region is followed by 16-byte alignment.
The seven 16-bit lengths/counts and 32-bit format descriptor live in an
18-byte table after the blob and an intervening 32-bit field.

The port retains batch-table offset/count, flags and auxiliary count in each
LOD directory entry. On-demand access walks preceding descriptors to recover
the requested ranges, validates them against the attachment boundary and
publishes output only on success. Empty regions use offset zero. This adds
2,048 bytes to the fixed maximum-size model directory, with no geometry heap
allocation. Repeated batch queries are quadratic in batch count; a future
resident geometry load should traverse the directory once.

Evidence is the original executable's Ghidra export for `0x569920` (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`)
and independent traversal of every installed V3C file. The verifier
`tools/verify_model_batches.py` checks 95 models, 170 LODs and 599 batches,
including all eight offsets/sizes, counts and format bits. Probe checks reject
out-of-range batch indices, truncated descriptor ranges and truncated payload
ranges without changing output. All 599 descriptors have value 5344321.

PC build, four CTest checks and NXDK build pass. The existing numeric 64 MiB
XEMU scene/animation regression also passes at
`artifacts/xemu/20260908-192720-951644/report.json`, without a framebuffer capture;
it does not exercise batch decoding or model drawing. This establishes region
boundaries, not decoded vertex/index semantics or rendered model fidelity.
The later numeric-reader findings above supersede the initial descriptor
mapping lead; next recover actual vertex consumers and bone-link semantics,
then connect geometry to owned materials and the renderer.
