# VFX rendering reconstruction map

Evidence: installed RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
These are disassembly/decompiler findings, not a completed renderer or runtime
verification. Existing geometry verifiers cover the separate animation path.

## Entry and dispatch

`53ee90` is the mesh-instance render entry. It requires nonzero instance
byte90 and clear bit80000000 in instance word1c. Optional arguments update
instance vectors before dispatch; their meaning still needs verification.

Definition word8c selects the branch:

- 0: definition flags114 mask801 selects `53ef50 -> 516f70`; otherwise
  `53ef70 -> 516f50`. Both wrappers forward the instance and one argument.
  After either, positive definition word110 calls `53ef40 -> 516f90`.
- 1: `53ef90` uses `5404a0`, copies instance vertices from pointer80 into
  a stack record, and calls `516a00`. Its local vertex array is192 bytes;
  validate authored counts and original limits before porting this branch.
- Other values: no draw dispatch in this entry.

Do not infer that flag801 or word110 can be ignored in the ordinary path.
Export and trace516f50/516f70/516f90 next to identify renderer callbacks,
materials, clipping, submission and ownership requirements. The branch1
routine also calls517f00/517f20 conditionally; its state effects need tracing.

## Update and instance ownership

`54cce0` advances effect-instance time, handles loop/stop state, and calls
`53f050 -> 53f060` for each mesh. Mesh updates are distinct from render entry.
`54b0d0` allocates mesh instances at98-byte stride; definitions use124-byte
stride. `53cde0` starts active byte90 at zero, clears upper attachment flag
bits, allocates vertices*12 and faces*24 buffers, and retains the definition.
The C port must own persistent animated geometry/UV storage before live draw.

`53f060` clears byte90 outside the active interval. Verify activation and
persistence at the effect owner rather than assuming a successful sample alone
sets visible state. Geometry sampling currently returns NOT_FOUND for inactive
samples and does not own visibility state.

## Existing implementation boundary

Shared code covers owned mesh decoding, frame/key sampling, direct-parent and
cached skeletal-parent geometry, and UV sampling. It does not yet provide this
render entry, persistent effect draw ownership, virtual tag lookup, or native
XEMU VFX submission. Continue against these exact renderer entry points;
do not replace authored modes with a generic textured triangle demonstration.

## Renderer callback targets and face preparation

516f50/516f70/516f90 gate on renderer selector017c7bcc==66 and call
553ee0/554bf0/555080 respectively.555080 binds definition110 as a texture
and invokes555ac0 using definitiona0; word110 is a texture ID, not a count.

553ee0 copies sampled UVs into face records, resets projected-vertex and edge
caches, prepares faces through554a80, sorts via53c840, then submits material
passes through5159a0.554a80 projects missing vertices via518360, ANDs their
clip codes for trivial rejection, computes the face normal via559f50, and
checks facing via518460. Surviving faces enter a depth list with a material
bias. Lighting uses averaged adjacent-face edge normals and4daff0; preserve
this behavior when connecting materials rather than using flat default color.

rf_vfx_face_normal now matches2048 original uncached559f50 comparisons on
PC/NXDK through the actual4fb050 and4faaf0 helpers. Two guards reject
nonfinite/degenerate geometry with output preserved; the original degenerate
case generates NaN. Persistent normal caching, projection, facing, depth
sorting, material passes and native submission remain unimplemented here.

Facing518460 dispatches to5478f0 for renderer66. Perspective uses float-rounded
(origin-point) dotted with the normal and accepts strictly positive values.
Flat projection uses forward vector18186e0 and accepts nonpositive values.
rf_vfx_face_facing passes2048 original/PC/NXDK cases and2 guards.
554a80 depth bias constant589668 is61.0f; verify its arithmetic and sorting
before replacing the original depth-list mechanism.

rf_vfx_sort reproduces53c840 halving-gap swaps over16-byte records with
three float keys and an opaque item ID. Extended53c950 comparison checks
all three fields for a positive difference above0.003000000026077032f, then
falls back to key0 ordering. This is not ordinary lexicographic ordering.
2048 complete original/PC/NXDK lists match, including ties and epsilon cases;
two invalid-input guards preserve the list. Depth-key generation and live
list ownership still need integration; no rendering claim follows from sorting.

rf_vfx_face_append matches554b0f..554be7 for2048 original/PC/NXDK cases.
The signed material index is multiplied by61 without an early float cast;
biased depth0/depth1 spill to float while depth2 stays extended during max
selection. Only key0 and item are written; key1/key2 retain their prior values.
The caller must initialize list storage deliberately. Three guards verify
full/invalid counts and nonfinite depths do not mutate the list or count.
Preceding culling, edge-cache resets and persistent list ownership remain
separate; this test is not a renderer/native playback test.

rf_vfx_face_prepare composes clip-code AND rejection, uncached normal,
facing and biased depth.2048 complete554a80 original/PC/NXDK comparisons
use preprojected cached vertices with no hooks:758 clip rejects,651 back
faces,639 visible. The original real normal/facing/depth code runs.
Two malformed input guards preserve output. Projection itself, cache lifetime,
edge-cache mutation, persistent face lists and draw calls remain external.

rf_vfx_world_face composes518bf0 view transforms/clip codes with face
preparation.2048 complete original554a80 runs start with an empty projection
cache and execute real518360/518bf0, normal, facing and depth code with no
hooks:1232 rejected,816 visible. Camera origins/matrices, perspective/flat,
clipping and far-plane modes vary. Flat-facing direction comes from the
scaled projection matrix third row, matching global18186e0. The helper does
not yet retain projected vertices for sharing across faces or submit draws.

## Persistent local geometry owner

rf_vfx_instance now borrows a decoded nonempty mesh and owns one allocation
for its40-byte owner, vertices*12 positions and faces*24 UVs. Open checks the
budget before allocating; close is repeatable. Update fills local geometry
and UVs and publishes active only after success. Inactive time returns success
with active0; errors also clear active and may leave partial buffers, which
must not be drawn. Parent poses and render/material state are not yet bound.

28 PC/NXDK ownership cases cover14 installed meshes at normal/exact mesh
budgets, with short instance budgets and injected allocation failures.112
updates compare complete metadata/vertex/UV buffers across builds; successful
vertex updates also match the verified mesh sampler. Invalid time clears
active. Repeated close frees only instance storage, leaving its mesh alive.
Instance storage ranges136-1156 bytes for these assets, excluding the borrowed
mesh, textures, future projection/list storage and allocator overhead.

Updates now select frames and resolve key tracks once per mesh. Remaining
per-vertex matrix/center recomputation should be hoisted before native playback. Empty-vertex definitions remain unsupported by this
owner and require a separate center-only update path. These are explicit open
items, not claims of complete effect-instance behavior or native rendering.

The key-evaluation optimization retains all112 PC/NXDK instance-update
outputs and2048 original geometry-dispatch comparisons. Read-only instruction
observers assert at most3 key-query calls per update, independently of vertex
count. No native frame-time improvement is claimed from this harness result.

## Instance face-list composition

rf_vfx_instance_faces reads authored face indices/material IDs from the owned
mesh and gathers the current persistent positions. It composes world-face
preparation and original sorting. Caller-owned face outputs and order records
need36 bytes per face. Secondary sort keys are retained; initialize them before
first use. On failure count is0, though output buffers may be partial. Inactive
instances return an empty list. This stage performs no allocations or draws.

112 PC/NXDK list comparisons across14 installed meshes match complete face
outputs and sorted records, including372 visible records. Short capacities
reject active meshes without publishing a count. The existing2048 original
world-face comparisons still pass. This verifies composition of the recovered
primitives, not complete553ee0 behavior. Per-face decoding/projection is still
repeated; retain decoded face metadata and shared projected vertices when
binding the renderer. Parent poses, lighting, clipping to screen polygons,
material passes and native XEMU submission remain open.

## Material scalar tracks

rf_vfx_material_track reconstructs shared arithmetic in54a930 type1,
54a9e0 and54aa80. It floors the unrounded double time position, subtracts
that index from a separately float-rounded position, interpolates adjacent
samples or copies the terminal sample, then clamps0..1. Zero rate retains
sample0; absent tracks/default selection are caller responsibilities. Invalid
empty/nonfinite/negative-time inputs preserve output.

2048 original/PC/NXDK cases per routine match with actual floor/ftol/clamp
helpers and no hooks; three failure guards pass. Retained material ownership,
track selection and texture/light/draw integration remain open. The sampler
alone does not prove complete material appearance or native playback.

rf_vfx_material_evaluate connects borrowed MATL array offsets and rate to
the scalar sampler without an allocation or aligned float-pointer cast.
Blend samples clamp before interpolation as54ab20 does while loading;
brightness/opacity retain raw finite values until the final clamp. Missing
tracks return NOT_FOUND; invalid spans, embedded sentinel offsets and invalid
numeric inputs preserve output. No caller defaults are invented.

2048 original/PC/NXDK comparisons cover all three tracks, unaligned starts,
zero/high rates and terminal sampling;7 guards cover empty/truncated/sentinel
spans, negative rate/time, invalid track and NaN. Original runtime executes
without hooks; loader blend clamping is supplied as normalized original input
(the separate material parser verifier checks that loader behavior). Existing
6144 scalar comparisons and24 tests still pass. Embedded mesh frame opacity,
retained renderer material ownership and native drawing remain open.

rf_vfx_mesh_material_evaluate resolves pre40000 local material records from
the owned mesh. Blend uses the serialized array; scalar brightness uses the
embedded color_word (default0 before30011, as53d0c0 loads it); opacity uses
the retained frame opacity fields, which53d0c0 distributes to every material.
The shared byte sampler accepts a stride so frame samples need no temporary
array. All frames must carry opacity presence. New global material references
return NOT_FOUND for the future effect owner to resolve. Outputs survive errors.
The accessor reparses preceding variable records; cache resolved views when
binding retained renderer ownership. It does not allocate or submit draws.

verify_vfx_mesh_material.py covers14 authored meshes with80 original/PC/NXDK
scalar evaluations and126 missing/global/range guards. Original scalar routines
receive arrays assembled from decoded records; parsing remains covered by its
separate original-oracle harnesses. Both builds,24 tests and prior scalar/view
regressions pass. Native VFX appearance is still unverified.

## Retained global material bank

rf_vfx_material_bank_open copies standalone MATL chunks in directory order
into a single budgeted allocation containing the owner, contiguous decoded
views and serialized arrays. Nonempty track offsets are rebased into the
shared byte arena. Complete record consumption is required. Failed reads or
parsing release the allocation and preserve a null output. Directory/archive
owners may close immediately after success; repeated bank close is safe.
The contiguous views can feed rf_vfx_material_textures_open. Texture ownership
and header-count reconciliation remain separate and are not implied here.

rf_vfx_mesh_material_sample resolves newer mesh-local material IDs through
the retained global bank; older meshes use their embedded tracks. It rejects
out-of-range payload spans/global IDs and preserves output on failure.

Seven installed asset banks match PC/NXDK, including every decoded field and
raw array byte;120 reversed-ID scalar bindings match the scalar sampler.
29 guards cover short budgets, allocation failure, each material read failure
and malformed records with rollback. The PC harness closes its real archive
and directory before sampling; NXDK receives supplied archive/heap services.
Bank sizes are20,564,294,564,914,572,20 bytes, excluding textures, archive,
allocator overhead and meshes. Xbox and PC core/probe builds pass. The active
interactive PC executable was left running, so no full PC relink was attempted.
No native VFX draw or complete effect-instance ownership is claimed.

## Bitmap frame clocks

rf_vfx_texture_frame reconstructs54a630 effect-time and54a6e0 normalized
frame selection. The first computes count/duration * speed, multiplies by
time minus signed start and1/15, floors, then clamps negative indices to0.
The second floors count*time, subtracts start with original32-bit wrap, then
adds count once for negative indices and clamps remaining negatives to0.
Modes0/2 wrap at the upper bound; other modes clamp to the last image.
One-image resources return index0 before reading clock values. Output is an
owned image-array index rather than the original base+1+frame bitmap handle.
Nonfinite/out-of-range conversion and invalid counts fail without mutation.

4096 original/PC/NXDK comparisons execute both complete routines with supplied
50f380 bitmap metadata and actual floor/ftol/max helpers, covering negative
time, offset, speed, zero speed, looping/clamping and one-image resources.
Four invalid guards pass. Duration provenance and renderer texture-slot
binding remain open; this is not proof of native VFX appearance.

The next metadata boundary is50f380: animated bitmap duration is derived
from a byte frame count at43 and float field4c, multiplied by float0.001
(constant5894c8). Static bitmaps return count1/duration1. Resolve field4c
loading and units before deriving this value from the retained VBM rate;
count/rate alone must not be assumed equivalent. Full PC/Xbox builds and
all24 CTests pass for the clock implementation.

rf_vfx_texture_duration now preserves the50f9cd float(rate*0.001) store
and50f380 float((count/stored)*0.001) query.50ebd0 reads the VBM rate word
through its parameter7, returned by50fcb0 to50f6e0 local330. Animated counts
are bounded to the original byte range; static count1 returns duration1.

rf_vfx_material_texture_sample selects retained primary/secondary images
through the texture binding table and authored words14..16/27..29. It uses
the reconstructed metadata and requested clock domain, returning a borrowed
image pointer with no allocation. Absent/original-map bindings return
NOT_FOUND; caller override ownership, lighting and drawing remain open.

2048 duration comparisons execute actual50f9cd and complete50f380 without
hooks, matching PC/NXDK.2048 NXDK image-pointer selections match complete
original54a630/54a6e0 with real metadata/handle helpers and both slots. The
image descriptors are supplied fixtures, not decoded pixels or native draws.
Four numeric guards and three binding guards preserve output. Both builds
and24 CTests pass; no native appearance claim.

## Edge material color

rf_vfx_material_color reconstructs553ee0 after its light query. It applies
a per-channel minimum from brightness*255 rounded by504e90 (float product,
then add0.5 and truncate). Mesh flag10 supplies white instead. Material type2
then multiplies each byte by authored RGB at material9..b and float1/255,
truncating without an intermediate float spill. The function accepts supplied
light RGB and permits in-place output; lit brightness must be finite0..1.

2048 PC/NXDK cases match original554303..5543a5 or unlit5542ad..5543a5
with the actual brightness sampler, byte converter and ftol helpers, no hooks.
2048 in-place checks and3 invalid-brightness guards pass. This does not
reconstruct4daff0 scene lighting, edge-normal/cache ownership,4db1b0 specular
or4db760 glare coordinates, nor full material passes or native drawing.

## Scene-light integration boundary

Static analysis of4daff0/4da8b0 establishes that rf_model_lighting_setup is
not a replacement for this VFX path: the model setup selects a local light,
whereas4da8b0 iterates the active pointer list c4d588/count c9687c. The VFX
call supplies ambient enabled, position, averaged edge normal, no visibility
weight array and gain2.4daff0 starts with ambient5a38d4..dc, accumulates,
normalizes RGB if its maximum exceeds1, applies gain and converts to bytes.
Its exact intermediate stores still require an execution oracle.

4da8b0 gates on5a38c4==0 and5a38cc!=0; the opposite branch writes white,
not ambient-only. With the VFX null weight array, each active light gets255
weight. Light record fields observed: type8; vectors c/18/24 with transformed
copies5c/68/74 selected by1818b84; radius3c; RGB40/44/48; falloff index54.
Type1 uses a directional dot and5a38e0. Type2 calls4dae50 for normal/distance
terms; type3 calls4daf30 and uses cone bounds84/88, factor38 and flag4e.
Type4 evaluates distance from a segment. These type meanings are inferred
from math and must be verified against authored light creation/updates.

The executable falloff table5a38e4 contains4da0b0,4da0c0,4da0e0,4da100:
linear remaining-radius fraction, its square, cosine profile and square root.
Raw Ghidra exports are under artifacts/analysis/rf_b8fb9ab4c9bf.4db1b0 has a
separate specular accumulation with light flags4c/4d;4db760 computes reflection
coordinates. None of these raw exports alone prove reconstructed semantics.

Next integration requirements: retain the original active light ordering and
transformed vectors, reconstruct/verify point and cone geometry4dae50/4daf30
and falloff dispatch, then reproduce4daff0 conversion before the already
verified material minimum/tint. Keep specular/glare as authored extra passes.
Do not replace this with the existing three-light model approximation or an
unlit texture preview and label it faithful VFX lighting.
