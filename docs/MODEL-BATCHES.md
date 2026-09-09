# Model batch data

`rf_animation_stream_placed` connects the recovered entity-local view helper
to the running resident animation/triangle pipeline. Its placement description
supplies world view, entity position/orientation and coherent clipping projection
settings. These are read each frame, so a synchronous consumer can update the
next placement without restarting pose state or reallocating the preview mesh.
Model lighting remains the diagnostic ambient fixture. This is an adapter for
supplied transforms; it does not load a level entity or reconstruct spawning.

Fully culled frames now produce empty meshes successfully, including sequences
that never emit a triangle. The Xbox stream renderer accepts such meshes and
can clear/present a frame without drawing a model. Fresh screen-rejected records
are excluded from the diagnostic's emitted-attribute validation. Near-camera
intersection behavior and complete view-state recovery remain separate work.

The PC `rf_animation_check.exe --placed-stream Installed_Game/meshes.vpp
Installed_Game/motions.vpp` fixture verifies all 64 output hashes against the
unplaced inspection using an equivalent translated quarter-turn entity and
world camera. A second run changes placement after each callback and detects
all 63 subsequent same-frame hash differences. A third run yields 64 empty
meshes for an off-screen entity. These are integration properties, not a new
original-code comparison or proof of full scene rendering.

The Xbox streaming inspection now uses that translated quarter-turn fixture,
preserving its visible framing while exercising the placement code. PC/NXDK
builds and all four CTest checks pass; native run
`artifacts/xemu/20260908-215751-805997/report.json` passes on 64 MiB after
all 64 frames, retaining 464 final triangles. No screenshot was taken because
the equivalent framing introduces no new visible result. Loading actual level
entity placements and drawing models together with world geometry remain open.

`rf_model_local_view` reconstructs the primary entity-placement view update
at `0x547485..0x5474cc`, inside `0x5473f0` called by the model renderer.
It first stores camera minus entity position as floats, then rotates that delta
by rows of the entity orientation. The view rotation becomes the old view
rotation multiplied by the transposed entity orientation, following the
original matrix helper's per-element addition order. Other fields of the
portable projection description remain unchanged; in-place conversion works.
Double intermediates approximate x87 rather than establishing universal bit
equivalence. The position cache historically named `world` is model-local when
this view is used: placement changes the view, rather than requiring every
cached vertex and bone matrix to be translated into world space.

`tools/verify_model_local_view.py` compares 2,000 executions of the unchanged
original block and its complete subtraction, rotation, transpose, matrix product
and copy callees. The harness provides ECX as set by the immediately preceding
original instruction and resets x87 rounding for each fixture. Dyadic positions
and matrices include identity and quarter-turn entity orientations; all 112
portable view bytes match, including preserved projection settings. PC/NXDK
builds and all four CTest checks pass. The global push/pop view stack and the
secondary view state updated by the remainder of `0x5473f0` are not included.
This helper is not yet connected to a placed scene actor, so no new screenshot
was captured. Ghidra exports now include both `0x5473f0` and `0x547540`.

`rf_animation_stream` now drives a synchronous frame consumer across the 64
scripted diagnostic frames. Pose, controller and resident geometry state advance
once through the sequence. A single budgeted preview allocation is reset and
filled each frame; its borrowed lifetime ends when the consumer returns. The
consumer may remap material slots, because the next frame rewrites every used
vertex. Consumer errors stop processing and release producer allocations.

The Xbox model stream retains uploaded textures and a single 1 MiB contiguous
GPU vertex allocation for the process lifetime. It waits for outstanding GPU
work before overwriting that allocation, flushes writes and submits each frame.
Two vertical-blank waits pace each diagnostic step, but there is no measured
steady frame-rate guarantee. Shader state is still uploaded per frame. This is
a single 64-frame inspection stream ending on its final frame, not a resumable
game loop, world actor system or sustained performance test.

Keep both `model-preview.flag` and `model-stream.flag` in the staged Xbox disc
and rebuild the ISO to select streaming. Removing only the stream flag selects
the earlier fixed-pose inspection. Telemetry scene 2 reports 64 submitted frames
and the 1 MiB reserved GPU vertex allocation. The PC preview option
`--model-last` has the same arguments as `--model` and renders frame 63 for
comparison. `rf_animation_check.exe --stream Installed_Game/meshes.vpp
Installed_Game/motions.vpp` validates all 64 delivered meshes, ordered callbacks,
63 changing transitions and one reused allocation; frames 0, 4, 32 and 63 also
match separate fixed-frame results byte for byte. It deliberately remaps every
material after each callback to test that consumer mutations do not leak.

PC/NXDK builds and all four CTest checks pass. Native XEMU run
`artifacts/xemu/20260908-214922-522800/report.json` passes on 64 MiB after
64 submissions. Its final frame contains 464 triangles; comparison to the PC
frame passes with 4 of 307,200 pixels exceeding channel error 3, maximum 113,
mean per-pixel maximum error 0.00412. The final native framebuffer was visually
inspected. Intermediate images were not captured or visually audited; runtime
telemetry and the PC mesh checks establish sequence progress, not visual
correctness of every frame. Existing animation hash scope is unchanged.

The first textured miner inspection now draws on both PC and stock-memory
XEMU. `rf_animation_preview` evaluates the existing scripted pose sequence,
collects one requested frame through the recovered batch and triangle pipeline,
then converts its indexed 40-byte records into the existing preview backend's
expanded vertices. The inspection uses a fixed front camera at Z=2.2, one-megabyte
mesh allocation budget, full-bright texture display and the preview's screen-depth
conversion/1/16-pixel grid. These camera, lighting and backend choices are
diagnostic scaffolding, not recovered game presentation. Raw batch materials
resolve through the loaded model instances' primary texture slots.

The faceplate primary image is white with alpha ranging 0..89. Both preview
backends now support source-alpha blending for this inspection; the Xbox
fragment program preserves texture alpha separately from doubled RGB lighting.
The Xbox disables depth writes for textures containing nonopaque pixels. This
is basic inspection transparency in existing batch order: original sorting,
secondary reflection textures (including RefMap01), material animation and
world lighting remain open. There is no visible animation loop yet.

PC invocation (from the repository root):
```
build/pc/Release/rf_pc_preview.exe --model Installed_Game/meshes.vpp Installed_Game/motions.vpp artifacts/miner-pose-front.ppm Installed_Game/maps1.vpp Installed_Game/maps2.vpp Installed_Game/maps3.vpp Installed_Game/maps4.vpp Installed_Game/maps_en.vpp
```
The Xbox diagnostic selects the same pose when its staged disc contains
`model-preview.flag`; removing that file and rebuilding the ISO restores the
Live Mines preview. Rebuild the ISO after changing the flag (the existing ISO
can be removed explicitly before `tools/build-xbox.sh`). The normal archive,
animation and level validations still run. Telemetry word 31 identifies model
inspection as 1; level mode remains 0. Level texture/lightmap telemetry describes
the validated resident level resources, while GPU allocation checks use the
selected scene's textures. The harness derives model GPU texture bytes from
original material names and independent image dimensions.

`artifacts/xemu/20260908-214421-693301/report.json` records PASS on 64 MiB,
447 triangles, 75,096 requested GPU vertex bytes and 794,628 requested GPU image
bytes. Its native guest framebuffer was visually inspected: a complete miner
pose, textures and transparent faceplate are visible. The PC comparison passes:
10 of 307,200 pixels exceed channel error 3 (maximum 160 at differing pixels;
mean per-pixel maximum error 0.00779). This is backend agreement for one pose,
not original-game or PS2 visual parity. PC/NXDK builds and all four CTest checks
pass. Screenshot capture was appropriate here because this is new visible output.

`rf_model_geometry_emit_batch` connects resident triangles to the recovered
routing, clip-input preparation, polygon walker and generated-vertex/fan
emission. Direct triangles append their original indices plus a 16-bit base;
compute-clip mode keeps the original strict capacity gate. Source batch ranges,
triangle indices and reuse references are checked before output writes. The
caller supplies processed vertices and an initialized reusable clip pool.
Clipping resets pool order per triangle; preserved pool bytes remain initialized
caller data. The adapter initializes its local original records to zero.
Capacity or runtime failure can retain earlier triangles, so the caller must
discard that batch's output on failure. There is no allocation, archive access
or GPU submission in this function.

The `--batch-triangles` model probe exercises one mixed batch: a direct
triangle, a side-clipped triangle producing two generated vertices and a
two-triangle fan, and a common-plane rejection. Exact expected indices,
generated screen coordinates, BGR/alpha and untouched fields pass. Additional
checks cover invalid source-index preflight and strict-versus-inclusive direct
capacity behavior. This fixture is an independently specified geometric check,
not a complete original-renderer comparison.

The shared animation diagnostic now runs triangle emission after each of its
256 real miner render batches over 64 frames, checking index divisibility/range
and nonempty total output. Its reusable output allocation holds 4,096 40-byte
vertices and 24,576 indices, plus a caller-owned clip pool; cache/clip/second
storage remains 56 bytes per maximum batch vertex (346 here). The fixed distant
camera does not establish coverage of clipping during this real-model run.
The existing eight-word animation hashes retain their previous scope and do
not hash newly emitted indices. Independent full-batch original-code comparison
and GPU material/draw integration remain open.

PC/NXDK builds, all four CTest checks, the mixed-triangle probe and the PC
animation diagnostic pass. `artifacts/xemu/20260908-213608-287389/report.json`
records a passing 64 MiB XEMU run with all 64 animation frames completed.
No framebuffer was captured because model output is not yet submitted for drawing.

`rf_model_project_clip_vertex` reconstructs complete `0x5477a0`: flags 1/2
skip processed records, clamp mode rejects nonpositive/unordered Z, and new
projections preserve generated flag 4. Zero/unordered Z outside clamp mode
uses FLT_MAX for the initial reciprocal. The depth-bias gate is bias*20 < Z;
adjusted reciprocal depth does not affect screen XY. X has an intermediate
float store while Y remains extended until viewport scaling. The portable
implementation uses double intermediates; universal x87 equivalence is not
claimed. `tools/verify_model_clip_projection.py` compares 2,400 full original
executions, all record bytes exact, including flags, clamping, bias boundaries
and nonfinite inputs.

`rf_model_emit_clip_polygon` reconstructs `0x52f6c1..0x52f84e`: retained
records select the triangle corner by byte 26; generated records append
40-byte vertices. After projection the original explicitly replaces reciprocal
depth with 1/Z, emits scaled depth/reciprocal, BGR and current alpha, UV and the
depth byte, preserving unused fields. Fan indices use the first polygon vertex
and wrap the supplied base to 16 bits. The strict original capacity gates
reserve the full polygon count even when some vertices are retained. Port
guards reject invalid pointers/corner ordinals and index-width overflow before
mutation; common masks or fewer than three vertices emit nothing.

`tools/verify_model_clip_emission.py` runs the unchanged original emission
branch and all projection/depth callees for 600 supplied polygons containing
2,200 generated vertices. Counts, mutated clip records, full vertex buffers
and index buffers match byte for byte, including preserved bytes and base wrap.
Five port guard cases cover capacity equality, missing records, invalid corner
selection and common-mask rejection. Both PC/NXDK builds and four CTest checks
pass. These stages still need integration with actual clipped resident model
triangles and renderer submission; they do not yet draw an animated character.

`rf_model_clip_polygon` assembles `0x549e00 / 0x549bd0` with the recovered
intersection, attribute, classification and pool helpers. Planes run in
ascending bit order; each walker starts at the second input vertex and wraps
the first last. Rejected vertices produce previous-edge then next-edge
intersections before generated records are released. Pointer identity,
preserved record fields and free-slot reuse follow that traversal.

The caller supplies initialized/reset pool storage, original record pointers
and union/common masks. The port bounds pointer lists to 48 entries, rejects
generated input records and accepts attribute flags 0 through 7. Interpolation
currently requires finite factors in [0,1]; failed calls may leave partial pool
changes and require a reset. Returned pointers remain valid until the owner
changes original records or resets/reuses the pool. This helper allocates no heap
memory and does not submit triangles.

`tools/verify_model_clip_polygon.py` executes complete unchanged `0x549e00`
and its original callees for 600 positive-Z triangles across side/far planes,
with UV interpolation and constant RGB. Output counts, masks, pointer identities,
pool usage/order and all 2,304 pool-record bytes match exactly in every fixture.
The verifier permits 2e-6 scaled float error, with zero observed. Integrated
near/custom-plane and varying-color cases remain open; isolated intersection
and attribute checks are documented below. Clipped-vertex projection, fan index
emission and draw submission remain separate work.
PC and NXDK builds plus all four CTest checks pass. This clipper has not yet
been connected to the emulator diagnostic's draw path.

`rf_model_clip_pool` provides caller-owned storage for 48 original 48-byte
records and a slot-order array. Reset mirrors `0x549270`, rebuilding slot order
and clearing usage while preserving record contents. Allocation follows
`0x5496e0`: read a slot, increment usage, fail when the new count reaches 48;
successful allocation only writes generated flag byte 25 to 4. Thus there are
47 successful allocations before first exhaustion. Release follows `0x5492d0`,
decrementing usage and placing the released slot at the free-list head.

A port-owned live bitset rejects invalid or double releases without corrupting
the pool. Calls after exhaustion require reset; unlike the original unchecked
follow-up access, they fail without further mutation. Reset, allocation and
release perform no heap allocation. Record contents must be initialized by the
owner before their first use; preserved fields are intentional.

`tools/verify_model_clip_pool.py` compares 2,043 operations with complete
unchanged reset/allocate/release functions, including ten first-exhaustion
events. Counters, normalized slot order and all record bytes match. Three
port-only guard cases pass. This is pool behavior, not polygon traversal;
the recovered plane walker still needs to use these slots and free rejected
generated vertices in original order.
PC/NXDK builds and four CTest checks pass.

`rf_model_classify_clip_vertex` recovers complete `0x518320` through
`0x518bd0 / 0x5475d0`. Render mode 0x66 writes only byte 24 of the 48-byte
record; other modes leave it intact. The shared mask calculation follows side,
perspective and far-plane gates, treating nonpositive/unordered Z as bit 0x80.
It does not generate near bit 1 or custom-plane bit 0x40. Camera-space position
is supplied; this helper does not transform or interpolate the vertex.

`tools/verify_model_clip_classification.py` runs 3,000 complete original wrapper
executions, covering mode gates, plane gates, equality and non-finite values.
All 48 record bytes match. Fresh projection now shares the mask calculation;
its 2,000 original-code comparisons remain byte-exact. PC/NXDK builds and four
CTest checks pass. Pool allocation/release and polygon traversal still need
assembly with the recovered intersection, attribute and classification stages.

`rf_model_clip_intersection` reconstructs all seven plane-bit branches in
`0x549324..0x54954c`, returning position and interpolation factor without
allocating a temporary vertex. Near/far intersections retain the original
factor-of-one fallback for zero/unordered depth differences. Side planes
derive their ratio from signed X/Y minus Z and force the resulting Z onto
the selected plane. The custom plane preserves float stores for difference
vectors, denominator, factor and scaled displacement. Its point and normal
are supplied explicitly in a 32-byte plane-state struct.

`tools/verify_model_clip_intersection.py` runs 2,800 original executions through
the attribute boundary, including original allocation/vector callees and pool
reset. All seven bits, equal depths, dyadic coordinates and non-finite inputs
are covered. All position/factor records match byte for byte in these fixtures;
the verifier allows 2e-6 scaled error and fixes x87 mode to 0x37f. Portable
double intermediates do not guarantee universal extended-precision parity.
No clamp is applied to the factor here; integration must resolve the existing
attribute helper's restricted factor domain explicitly. Attribute dispatch,
new-vertex clip classification, pool lifetime and polygon traversal remain open.
PC/NXDK builds and four CTest checks pass.

`rf_model_clip_attributes` recovers `0x54954c..0x549626`: interpolate UV0 for
flag 1, UV1 for flag 2 and RGB for flag 4. The model renderer supplies flags 5.
UVs use `(outside-inside)*factor+inside` with a final float store; byte colors
use truncation through original `__ftol`, not nearest rounding. Disabled fields,
position, flags and alpha remain untouched. A local result supports aliased
inputs. The port accepts finite factors in [0,1] and flags 0–7; intersection
generation must establish that domain or handle a reported range error.

`tools/verify_model_clip_attributes.py` compares 3,000 unchanged original-block
cases with original integer-conversion callees and dyadic factors, covering all
UV/RGB flag combinations. All output bytes match; four invalid-argument cases
preserve output. The harness explicitly sets x87 control word 0x37f per case;
an initial run without that setup produced a one-ULP mismatch. These fixtures
do not establish universal x87 equivalence. Intersection factors/positions,
alpha interpolation and pool allocation are not part of this helper.
PC/NXDK builds and four CTest checks pass.

`rf_model_prepare_clip_triangle` recovers the three 48-byte clipping records
assembled at `0x52f5b2..0x52f699`. Position and lit RGB use the signed-reuse
resolved cache entry; each corner retains its own clip mask and UVs. It sets
generated flags to zero and the corner ordinal to 0/1/2. Unrelated bytes remain
untouched. Index and resolved-source bounds are validated before any writes;
bounded negative reuse follows the original subtraction rather than being
silently treated as zero. No allocation or pool mutation is performed here.

`tools/verify_model_clip_inputs.py` executes the unchanged original assembly
block, constructors, pool reset and copy callees in 2,000 cases / 6,000 records.
All 144 output bytes per triangle match, including preserved fields, lighting
modes and reused position/RGB with independent UVs. Three bounds fixtures pass.
PC/NXDK builds and four CTest checks pass.

Ghidra exports now include `0x549e00`, `0x549bd0`, `0x549310` and pool helpers.
The dispatcher visits selected plane bits 1 through 0x40 in ascending order,
swaps input/output pointer lists after each plane and exits on common rejection.
The pool reset establishes 48 pointers to 48-byte temporary vertices. The
per-plane traversal starts at the second input vertex with a wrapped pair at
the list tail. Edge interpolation and pool lifetime still need reconstruction
and executable verification; the clip input helper alone does not clip polygons.

`rf_model_route_triangle` reconstructs renderer routing beginning at `0x52f473`.
Screen-rejection mode drops a triangle if any vertex clip byte is nonzero.
Otherwise, enabled frustum clipping first rejects a nonzero intersection of
all three clip masks. Surviving triangles run the recovered facing test, then
route directly when clipping is disabled or the union of clip masks is zero;
a surviving nonzero union requires polygon clipping. It validates cache indices
against both supplied capacity and original signed-short addressing before
publishing a result. No memory is allocated.

`tools/verify_model_triangle_route.py` executes the unchanged original routing
branches and facing callees for 3,000 cases. All decisions match: 1,734 rejected,
647 direct and 619 requiring clipping. Three invalid-index guards preserve
output. The comparison stops before index capacity checks or polygon generation;
those stages remain open, as does submission of model triangles to the GPU.
PC/NXDK builds and four CTest checks pass.

`rf_model_triangle_facing` recovers the culling branch at `0x52f4e8` through
`0x5478f0` and its subtract/cross/dot helpers. Triangle flag 0x20 bypasses
culling. Otherwise the normal is `(b-a) cross (c-b)` with original float stores.
Perspective mode accepts a strictly positive dot of `(camera-a)` with that
normal. Nonperspective mode accepts when the forward-vector dot is not strictly
positive, including equality and unordered results. No normal normalization or
epsilon is introduced. World vertices come from the renderer cache, including
the copies populated by duplicate processing.

`tools/verify_model_triangle_facing.py` executes the unchanged renderer branch
and complete callees for 2,000 cases, covering both modes, flag bypass,
degenerate triangles, NaN and infinity. All decisions match, with 1,196 accepted.
The helper accepts caller-supplied vertices/view state and does not yet perform
frustum rejection, polygon clipping or index submission.
PC/NXDK builds and four CTest checks pass.

The shared animation diagnostic now runs all four miner LOD0 batches through
resident rendering after every actual evaluated pose: 256 batch evaluations
and 47,616 vertex visits across 64 frames. It uses the prepared 25-bone matrices,
with a fixed view and ambient-only lighting. Its largest batch has 346 vertices;
one 33,216-byte working allocation is reused, reset before each batch and freed
on all exit paths. A separate 1 MiB working-buffer cap bounds allocation.

Runtime checks verify per-vertex UVs, ambient color/alpha, preserved fields and
duplicate world/clip copies. These pass on PC and 64 MiB XEMU; evidence is
`artifacts/xemu/20260908-205654-012799/report.json`. PC/NXDK builds and four
CTest checks pass. The original-code animation/collision comparison still
matches its eight-word output, unchanged. The new render buffers are checked
by runtime properties, not added to that original-code hash: full original
comparison of these real animated render outputs remains open. Triangle
submission and world-derived view/lighting are also open. No screenshot was
taken because the displayed static scene is unchanged.

`tools/verify_model_render_files.py` now exercises all 95 installed models,
599 batches and 85,866 vertices through the resident batch processor, including
all 24,861 duplicate references. Independent archive traversal verifies the
batch vertex/reuse counts. The C probe checks duplicate world/clip copies,
per-vertex UVs, ambient RGB/alpha and untouched output bytes. All pass.
It allocates exactly 96 working bytes per batch vertex and frees the buffers
after each batch, retaining one LOD under the existing 4 MiB test budget.

This is a controlled integration fixture with identity transforms for all 256
bone slots, a fixed camera and ambient-only lighting. It does not claim real
skeletal poses, original-code equivalence for every source vertex, triangle
submission or visual parity. It does include the installed non-finite second
streams without sanitizing their bits. PC build passes. Real evaluated poses
and emulator-side batch processing remain the next integration step.

`rf_model_geometry_render_batch` now assembles resident vertex processing:
fresh vertices deform both streams, retain world position, project, normalize
the visible second stream, optionally light it and emit attributes; duplicates
use the recovered cache path and their own UVs. `rf_model_finish_render_vertex`
implements the visible fresh tail, including normalization even with lighting
disabled. Fresh and duplicate paths share attribute emission without changing
their different visibility/cache rules.

Processing performs no allocation or archive reads. Caller-owned buffers use
96 bytes per vertex (32 cache, 12 clipping position, 12 second stream, 40 output)
plus the 20-byte Win32/Xbox buffer descriptor. Batch ranges, capacity, backward
references and active bone indices are checked before writes. Caller buffers
retain fields the original leaves untouched and must be initialized before
first use. Triangle clipping/submission and material binding remain external.

`tools/verify_model_render_batch.py` compares 250 batches / 2,000 vertices,
including 592 duplicates, with consecutive unchanged original vertex paths
`0x52edac..0x52f3cc`. All cache, clipping, second-stream and output bytes match.
The verifier supplies streams, matrices and view globals and sets per-vertex
registers externally; it does not emulate buffer locking or triangle submission.
Invalid initial reuse and active bone-index fixtures reject without writes.
PC/NXDK builds and four CTest checks pass. Next exercise real animated batches
through these buffers and connect triangle processing to the Xbox renderer.

`rf_model_project_vertex` reconstructs `0x52f154` through the visible/rejected
branch at `0x52f31e / 0x52f3cc`. It subtracts camera position with float stores,
applies the original matrix order, optionally replaces depth, computes frustum
clip bits and retains camera-space position when requested. It then computes
the clamped depth byte via `0x52fc70`, projects with reciprocal depth and applies
strict screen bounds. Screen rejection overwrites the clip byte with 1; otherwise
frustum flags remain. The returned visible flag describes the screen branch,
not whether the frustum clip mask is zero. Depth output is written even on
rejection. Other cache/output fields are preserved for adjacent render stages.

The port accepts original view globals through a 112-byte caller-owned struct;
it allocates no memory. Singular and non-finite arithmetic follows the recovered
comparisons, including unordered depth clamping to zero. Projection uses double
intermediates to approximate x87; universal threshold/rounding parity is unproven.
`tools/verify_model_projection.py` executes the unchanged block and full clip,
depth and conversion callees in 2,000 cases covering all five gates, signed/zero
depth and non-finite inputs. All 2,000 output records match byte for byte, with
893 screen rejections. The verifier allows 2e-6 scaled float error while requiring
exact clip/depth/visibility and preserved bytes. Connect this stage to fresh
deformation, lighting, cache lifetime and triangle submission for model drawing.
PC/NXDK builds and four CTest checks pass for this change.

`rf_model_render_reuse_vertex` reconstructs the positive-reuse branch entered
at `0x52edac`, including its UV tail through `0x52f3cc`. It copies the prior
vertex's world position and clip byte. A nonzero clip byte leaves all 40 output
vertex bytes untouched. Otherwise it copies projected X/Y, scales reciprocal
depth through original `0x52fce0` and the separate reciprocal scale, writes
cached or flat BGR plus current alpha and the depth byte, then writes the
duplicate's own UVs. Bytes not written by the original remain intact.

The port's 32-byte cache view represents separate original global arrays.
Reuse does not populate the destination's projected coordinates, depth byte
or RGB cache. The API performs no allocation and rejects nonpositive/out-of-
range backward distances without modifying outputs. This is supplied-cache
processing; fresh projection and cache lifecycle remain separate work.

`tools/verify_render_reuse.py` runs 2,000 unchanged original branch executions,
including full copy and depth callees. All cache-array and output-vertex bytes
match, with 1,042 clipped and 958 visible cases, both lighting modes, distinct
UVs and preserved fields. Five port bounds cases pass. PC/NXDK builds and four
CTest checks pass. The helper is not yet connected to visible model drawing.

The installed-file audit finds no chained reuse: all 24,861 positive references
point directly to fresh vertices across 599 batches and 95 models. This is
consistent with the original leaving duplicate projection/RGB caches untouched;
it is an installed-data observation, not a guarantee for arbitrary mod assets.

`rf_model_render_vertex_lighting` connects the previously recovered lighting
helper to the normalization at its actual renderer call site. At `0x52f31e`,
visible fresh vertices call `0x4faaf0` on the deformed second stream, then call
`0x52fcf0` when lighting is enabled. The normalization computes inverse length
and multiplies all three components with float stores. It has no zero/NaN
fallback, unlike the local-light direction helper. This closes the missing
normalization between rendering deformation and lighting; the lighting helper
itself still does not normalize. Clipping precedes this block.

`tools/verify_render_vertex_lighting.py` executes the unchanged call-site block
`0x52f31e..0x52f34d` and complete callees for 2,000 inputs, including zero,
NaN, infinities, subnormal and large finite vectors. All normalized-vector/RGB
records match byte for byte in these fixtures. The verifier permits 1e-6 vector
error and matches NaN classification, while requiring exact RGB. No universal
x87 precision claim is made. PC/NXDK builds and four CTest checks pass.
Duplicate-vertex cache integration and visible model drawing remain open.

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
