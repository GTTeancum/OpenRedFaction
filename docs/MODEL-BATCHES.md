# Model batch data

`rf_model_lod_metric` recovers complete `0x5182f0` through `0x5479b0` and
the unchanged distance helpers `0x4faed0 / 0x409fa0 / 0x40a000`. Render mode
0x66 subtracts camera coordinates with float stores, computes Euclidean length,
then multiplies by the value at original global 0x1818b50 and divides by the
value at 0x1818b48. Other modes return zero without reading the coordinates.
The API accepts these caller/global values explicitly. Singular arithmetic
is retained, including non-finite results, without clamping or fallback scale.

`tools/verify_model_lod_camera.py` executes the complete original metric and
detail-selection block in 2,000 cases with three-axis coordinates, signed
scales, zero denominators, mode gates and non-finite inputs. All selected
indices match exactly. Metric comparisons permit 1e-12 times max(1,abs(reference));
the maximum observed error on that scale is 2.57e-16. The portable double
calculation approximates x87 precision; this does not prove equivalence for
every threshold-boundary input. PC/NXDK builds and four CTest checks pass.
Live camera state and model draw submission remain to be integrated.

The installed-file probe now derives its metric through this helper before
selecting and loading geometry. All 2,375 loads across 95 models pass with
matching selected indices and vertex/triangle counts; these fixtures use
axis-aligned camera distances and unit metric scale.

Model directories now retain each LOD's serialized threshold bits, adding four
bytes per LOD (512 bytes at directory capacity), with no additional allocation.
`rf_model_file_select_lod` gathers the owning SUBM's thresholds in file order,
uses the recovered selector and returns the flattened LOD index accepted by
`rf_model_geometry_open`. It rejects missing submeshes and malformed level
counts without changing the caller's output.

Independent traversal matches all 170 serialized thresholds across 95 models.
Five modes and five supplied metrics per SUBM produce 2,375 successful selected
geometry loads with matching vertex/triangle counts; all 95 invalid-submesh
checks preserve output. Evidence: `artifacts/model-lod-files-verification.json`.

The shared miner diagnostic now selects through this API using the original
alternate-mode gate to keep its fixed highest-detail fixture. PC and NXDK
builds, four CTest checks and 64 MiB XEMU pass with unchanged animation results;
emulator evidence is `artifacts/xemu/20260908-202812-387666/report.json`.
This exercises selection through residency, but does not yet calculate a live
camera metric or switch detail while drawing. No screenshot was captured.

`rf_model_select_lod` recovers the selector in `0x52faae..0x52fb1d`, including
the threshold-match branch at `0x52fbe2`. Flags masked by 9 force the final
LOD. Otherwise alternate mode selects zero; ordinary mode clamps the supplied
minimum to the final LOD and scans thresholds backward, accepting equality.
The metric is multiplied by 2.5 only when both scaling and animation-instance
gates are set. Unordered comparisons do not select a threshold. Thresholds are
neither sorted nor sanitized. The port requires 1–3 levels and a nonnegative
minimum, leaving output unchanged on invalid arguments.

`tools/verify_model_lod.py` compares 2,000 unchanged original-block executions,
including original metric callees, equality, unordered values and flag gates.
The fixtures use axis-aligned positions and unit metric scale; the C API takes
the resulting metric as input. Three separate port bounds checks pass, as do
PC/NXDK builds and four CTest checks. File threshold retention, camera-derived
metric calculation and selection-driven residency remain to be connected.

The newly exported `0x52fa40` dispatcher calls prepared-skinning setup and then
`0x52e9e0`; it does not rewrite the second stream. The inspected loader chain
`0x53b408 -> 0x53ae5f -> 0x5696f0 -> 0x569880 -> 0x569920` likewise retains
the stream contents. This narrows the preparation investigation but does not
prove that no other lifecycle code writes the data. No visible change is claimed.

`rf_model_geometry_open` now assembles one LOD into owned batch ranges,
vertices, triangles and reuse records, with flattened material indices.
Triangle indices and reuse distances remain local to their batch. Its explicit
budget accounts for the geometry header and all four arrays; allocator metadata,
caller-owned archives, model directories and textures are excluded. This is
port-owned storage, not a reconstruction of the original allocator. Failed
loads release temporary allocations without publishing a partial result.

Independent traversal verifies all 170 LODs across 95 models: 599 batches,
85,866 vertices and 99,214 triangles. Every LOD passes at its exact accounted
budget and rejects a budget one byte smaller. Array hashes, reuse records,
batch material mappings and allocation accounting match the independent reader.
The shared miner animation diagnostic uses this loader under a 1 MiB cap;
its original-code comparison still matches all 47,616 deformation evaluations.
PC build, four CTest checks, NXDK build and 64 MiB XEMU pass. Emulator evidence
is `artifacts/xemu/20260908-201548-908306/report.json`. No screenshot was taken:
the visible scene is unchanged. Renderer deformation, lighting and model draw
submission remain open.

`rf_model_file_batch_material` now resolves a batch's signed slot at offset
0x20 in its 56-byte blob header through the LOD texture table. The table's
byte ID selects a SUBM material; the accessor adds prior SUBM material counts
to match the flattened owned-material bundle. Negative slots return NOT_FOUND,
and invalid slots/material IDs or truncated records fail without publishing
output. LOD metadata retains the texture-table offset/count and owning section,
adding 1,536 bytes to the maximum-size directory without new heap allocation.

Original `0x569920` retains the header's +0x20 field while rewriting its data
pointers; `0x52e9e0` reads that field for texture selection, and `0x504000`
maps it through the LOD byte IDs to 200-byte runtime materials. Independent
installed-file traversal matches all 599 material mappings across 170 LODs
and 95 models. PC build, CTest and NXDK build pass. Negative-slot and malformed
table handling are port guards; those branches are not represented in the
installed-data comparison. Drawing integration remains open.

The shared miner diagnostic now loads all 744 vertices from LOD 0 and runs
their actual position/weight/bone records through collision deformation after
each prepared pose. It retains 29,760 bytes of vertex records for the 64-frame
sequence under an explicit 1 MiB vertex-data cap; allocation is released on
every exit path. Triangle collision tests and renderer deformation are not
substituted by this numeric check.

The independent verifier traverses the original LOD payload and executes the
unchanged collision block for every vertex/frame, totaling 47,616 evaluations.
The eight-word result is now
`[2,25,64,3699116198,3589827904,4200471932,1621267238,1404]`.
PC build, four CTest checks, NXDK and 64 MiB XEMU all pass; evidence is
`artifacts/xemu/20260908-200823-522166/report.json`. No screenshot was captured
because the displayed static scene is unchanged. Next connect renderer-specific
vertex reuse, material selection and lighting to visible model drawing.

The shared 64-frame animation diagnostic now connects actual miner BONE data
and evaluated animation poses to prepared skinning matrices. It builds the
25 stored transforms once, allocates 2,400 bytes for stored/prepared matrices,
and maintains 512 bytes of bounded generation stamps on the stack. Temporary
matrix storage is released on success and failure. Each frame includes the
prepared matrices and stamps in the cache hash.

The independent original-code verifier constructs stored transforms through
`0x519720`/`0x4fe900` and runs `0x51ba00` after the existing original pose
evaluation. The complete eight-word result is now
`[2,25,64,3699116198,3589827904,13735429,1621267238,1404]`.
PC, CTest, NXDK and numeric 64 MiB XEMU pass; runtime evidence is
`artifacts/xemu/20260908-200552-959380/report.json`. There is no new framebuffer
capture: this connects animation to prepared matrices, but does not yet draw
the miner or feed actual vertices through the deformation/lighting pipeline.

`rf_model_prepare_skinning` connects the existing matrix composition helper
to `0x51ba00`'s prepared-matrix cache. Once pose evaluation is current, original
code composes each stored bone transform (definition +0x64, stride 76) with
its animated pose (instance +0, stride 48) into instance +0x960, stride 48.
It refreshes only when the entry's 16-bit stamp (+0x1396, stride 48) differs
from instance generation +0x1cf8. The port accepts bounded caller-owned arrays
and requires the pose to be evaluated first. It updates a stamp only after
successful composition; malformed-input behavior is a port guard, not an
original error-path reconstruction. Earlier successful entries remain updated.

`tools/verify_prepare_skinning.py` executes complete unchanged `0x51ba00` and
callees with pose stamps pre-current. Across 2,000 cases / 8,000 bones, 4,042
prepared entries refresh and the remainder retain their existing contents;
all matrices and stamps match bit-for-bit. Pose recomputation is deliberately
outside this test's scope. PC build, CTest and NXDK build pass. Integrating the
actual animated pose, stored transforms and deformation consumer remains open.

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
