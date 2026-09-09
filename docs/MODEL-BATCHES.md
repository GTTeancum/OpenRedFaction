# Model batch data

`rf_model_lighting_setup` now assembles the complete `0x52dad0` operation from
caller-owned model/global inputs and a candidate-light list. It scales ambient
RGB by 255, builds both fixed directional contributions, handles the alternate
first-light mode, honors local-light disabling, and composes the verified local
selection/direction/attenuation helpers. Original constant bits and matrix
orientation are preserved. The second fixed light intentionally gives its red
and green channels the same green-derived intensity, as in the executable.
No heap allocation occurs; output is published after successful setup.

`tools/verify_model_lighting_setup.py` executes complete original `0x52dad0`
with unchanged callees for 2,000 finite fixtures spanning alternate, disable
and flag-0x400 states. All 21 float outputs match bit-for-bit in this run,
although the verifier permits 2e-6 relative / 1e-5 absolute error. PC build,
four CTest checks and NXDK build pass. World candidate discovery, source
global values, model-instance preparation and drawing are not yet connected.

`rf_model_local_light_direction` recovers `0x52dd51..0x52dd9d`: normalize the
selected delta using `0x4fab70`, optionally replace Y with 0.5 and normalize
again when model flag 0x400 is set, then apply the supplied 3x3 rotation via
`0x4fac60`. Zero and unordered lengths become (1,0,0); infinity follows the
original reciprocal/multiplication path. No post-rotation normalization occurs.

`tools/verify_local_light_direction.py` compares the unchanged block and complete
vector callees for 2,000 cases covering both flag states, zero, NaN, infinity
and tiny deltas. All outputs match bit-for-bit in this run, with a small
floating tolerance permitted by the verifier. PC build, CTest and NXDK build
pass. The helper takes already-selected deltas and caller-supplied rotation;
fixed-light setup, global gates and scene integration remain open.

`rf_model_local_light_color` recovers the selected-light color tail at
`0x52dd9d..0x52ddda`: scale is `(1-sqrt(distance_squared/radius_squared))*255`,
then multiplied by each light color component. No clamp or singular-input
replacement is added. The caller supplies the selected light; selection,
direction and global setup remain separate. Double intermediates approximate
the original x87 operations before final float stores.

`tools/verify_local_light_color.py` compares 2,000 unchanged original executions
covering random finite distances/colors, radius boundaries, zero radius,
negative distance, NaN and infinity. All 2,000 outputs match bit-for-bit in
this run (the verifier permits a small floating tolerance and checks non-finite
classes). PC build, four CTest checks and NXDK build pass. Universal x87
rounding equivalence is not asserted.

`rf_model_choose_local_light` recovers the selection block
`0x52dcaf..0x52dd48` within light setup `0x52dad0`. It scans enabled candidates,
forms light-position minus model-position and selects the nearest candidate
inside its squared radius, keeping the first on equal distances. The original
compares an extended squared distance with radius, then its float-stored value
with the current best. NaN radius passes the first x87 comparison, while NaN
distance cannot win the nearest comparison. No candidate returns index -1,
zero delta and FLT_MAX distance. The port adds pointer/count bounds checks.

`tools/verify_model_light_choice.py` matches 2,000 unchanged original selection
executions and complete vector callees, including ties, exact-radius cases,
disabled lights and NaN radii; 1,655 fixtures select a light. PC build, CTest
and NXDK build pass. Global disable gates, direction normalization/rotation,
attenuation and the two fixed directional lights are not yet integrated.
The original setup takes ambient RGB from globals, constructs two fixed
directions and uses the selected local light as its third contribution.

`rf_model_vertex_lighting` reconstructs complete helper `0x52fcf0`. It takes
the supplied vector's dot product with three light directions, clamps negative
and unordered dot products to zero, and combines their RGB contributions with
ambient RGB. It does not normalize the vector. Each channel is capped at 255
(unordered channel sums also select 255), stored as float, then converted by
original `0x52fcb0`'s 12,582,912 float-bias/low-byte operation. There is no
additional lower clamp. Default round-to-nearest is assumed.

`tools/verify_vertex_lighting.py` executes the complete original helper with
unchanged callees for 2,000 cases, including 20 NaN/infinity-vector fixtures;
all output bytes match. Negative/overrange channel fixtures exercise conversion
and upper clamping. PC build, CTest and NXDK build pass. Double intermediates
are used in the port; arbitrary x87 rounding equivalence is not claimed.

NaN vectors yield zero directional factors and therefore ambient-only color
for ordinary finite lights. This explains the helper's response, but does not
yet prove that a source NaN reaches it unchanged: second-stream preparation,
light setup and the complete rendering path remain to be connected and checked.

`rf_model_file_vertex_reuse` now streams the signed 16-bit distance from a
batch's extra-data region. It preserves nonpositive values as the original
fresh-deformation decision and rejects positive distances beyond the current
vertex index. Unsupported formats, truncated regions and invalid indices
leave output unchanged. No allocation or deformation occurs in this accessor.

The installed-data verifier now compares all 85,866 distances as well as
vertex/triangle bytes. It finds 24,861 reused vertices, no negative distances
and a maximum backward distance of 455. Every positive reference stays inside
the batch. Probe checks cover out-of-range indices and truncated regions.
PC build, four CTest checks and NXDK build pass. Negative signed values are
preserved by the implementation but are not represented by installed files.
The renderer's downstream reuse of clip flags, positions, depth and lighting
still requires integration; copying both deformed streams blindly would not
reproduce that branch.

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
