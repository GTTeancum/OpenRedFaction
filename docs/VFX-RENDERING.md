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

rf_vfx_point_light reconstructs4dae50 using float-stored light minus position,
normalization and an unrounded distance/radius comparison. Coincident points
use the original4fabd0 fallback direction(1,0,0) and distance1. Rejected lights
return angular0 plus distance. Ordinary VFX calls use the normal/direction dot;
the optional zero-byte argument instead dots direction with (2*direction+normal)
scaled by float1/3, preserving intermediate stores. The returned angular factor
is not clamped; the light accumulator tests its sign before applying falloff.

2048 original/PC/NXDK cases execute complete4dae50 and actual math helpers
without hooks, covering both optional branches, coincident points and radius
rejections.3 invalid-input guards preserve output. This is geometry only;
cone/falloff dispatch, active-light list ownership and RGB accumulation remain.

rf_vfx_cone_light reconstructs complete4daf30. It shares the point-light
normalized direction and distance fallback, but its optional softened basis
is (direction+normal)*0.5, with float stores before the dot. Within radius,
it returns the angular factor and direction/axis dot; outside radius both
terms are zero while distance is still returned. No cone bounds or falloff
are imposed in this routine:4da8b0 applies those after the geometry call.

2048 original/PC/NXDK cases match all three outputs using actual vector
helpers, no hooks;3 invalid-input guards pass. Cone-axis inputs are finite
but not forcibly normalized, matching the original caller contract. This
remains a geometry component; light owner/order, attenuation and accumulation
must still be connected before claiming rendered scene lighting.

rf_vfx_light_add combines the four original falloff profiles with supplied
angular gain and weighted RGB, then adds to accumulated RGB at the original
float channel stores. It does not round falloff to float before accumulation.
Cosine uses the original float pi/2 constant and preserves the subtraction
order1-((radius-distance)/radius); no artificial clamp is inserted. Caller
acceptance handles radius/angular rejection before this stage. Output may
alias the accumulated input and errors preserve it.

4096 PC/NXDK comparisons match actual4da0b0/c0/e0/100 plus4dadb4 channel
addition, joined by an explicit angular-gain multiply; no hooks.4096 in-place
cases and3 invalid-profile/radius guards pass. This establishes point-style
float angular input composition. Cone-boundary attenuation still needs its
original higher-precision multiplication ordering when integrated, along with
segment lights, weighted color construction, active ownership and conversion.

rf_vfx_light_rgb reconstructs4daff0 after accumulation: overbright maximum
normalization, gain (negative bypasses gain), ambient floor for gain0..1,
upper clamp for gain>1, then truncated byte conversion. VFX passes gain2.
The original blue normalization product stays extended until multiplication
by255 on the negative-gain path. Initial double-only C differed by one byte
on some overbright cases. A bounded x87 helper now preserves that sequence
on the supported Win32/NXDK x86 builds, restoring the caller control word;
other architectures use long double and are not covered by these results.

4096 PC/NXDK cases match original4db065..4db1a7 with real min/max/ftol
helpers, no hooks.3 invalid-input guards preserve output. Inputs supply
post-accumulation RGB, so this does not prove4da8b0 active-list processing
or scene-light initialization. Native lighting integration remains open.

## Composed point-light shading

rf_vfx_point_lighting takes ordered, already-selected point sources in the
surface coordinate space, starts with ambient, applies4dae50 acceptance,
falloff/angle/color accumulation and4daff0 gain2 RGB conversion. It allocates
nothing and preserves output on error. Point-source descriptors carry position,
RGB, radius and falloff profile; they are not original mixed light records.
Callers must not silently omit non-point sources to use this function.

1024 PC/NXDK cases match complete original4daff0 calling4da8b0 and actual
geometry, all four falloff functions and final byte conversion, without hooks.
The harness supplies the original active pointer table and ambient, varying
0-16 point lights, ordered colors, radii, normals and positions.2 invalid
ambient/profile guards preserve output. This validates composed point shading
beyond the earlier isolated helper tests; mixed source accumulation, active
scene-light creation/filtering/transforms and VFX rendering remain open.

## Mixed-light accumulation

rf_vfx_lighting composes original types1..4 in caller-supplied order: directional
dot/scaling, point geometry/falloff, cone geometry/boundary attenuation and
segment distance/falloff. Cone attenuation preserves a double factor through
falloff and angular multiplication, rather than passing a rounded factor to
the earlier point-style light_add API. Segment endpoints and projected points
retain float stores before distance. Final gain2 conversion uses the verified
light_rgb path. The76-byte port descriptor contains selected/transformed
source values, not serialized original records. Ambient remains enabled and
per-light visibility weights are absent, matching the VFX call.

2048 complete original4daff0/4da8b0 comparisons match PC/NXDK with0-16
mixed lights, all four falloff profiles, cone squaring and varied geometry.
Original active tables/ambient/transformed vectors are supplied; no math or
lighting hooks replace original routines. Invalid ambient/type guards preserve
output. This does not include creating/filtering/updating the live light list,
the disabled-light white fallback, edge caches, specular/glare or native draws.

## Source transforms

rf_vfx_light_transform reconstructs4d8480: type1 rotates its direction only;
types2/3/4 subtract origin before transforming position; type3 rotates its
axis without translation, and type4 transforms its second endpoint as a point.
Original4faa30 uses row-wise Z/Y/X product addition order. Unused vectors and
non-vector descriptor fields are preserved; in-place use is supported.
Invalid used inputs/results preserve output.4d9fd0 applies this operation to
the active list when lighting gates and transformed-space flag1818b84 permit.

2048 PC/NXDK cases compare the full original4d9fd0/4d8480 transform walk
then4daff0/4da8b0 shading. Every transformed descriptor field and final RGB
matches, across all types/profiles and0-16 sources. Origins and bases vary.
The harness supplies active lists and ambient; no original math hooks. The
point-only regression also passes. Live list selection remains separate.

Static references locate list reset/append paths around4d9a06/4d9bb6,
4d9c49/4d9d8b and4d9e17/4d9f5c, plus reset4d9fc7. Resolve their containing
functions and caller selection rules before binding the scene-owned list.

## Spherical candidate filtering

rf_vfx_lights_sphere reconstructs4d99c0 over an explicitly supplied candidate
array, returning stable indices without allocation. It retains enabled byte4c
and class byte4d, rejects black sources, applies both class switches, accepts
type1 and skips unknown types. Types2/3 compare squared center distance with
(radius+source radius)^2 using a strict inequality. Type4 first uses509100's
segment projection, including zero-length handling, stored float length and
reciprocal, float projection/clamping, and stored offset/closest coordinates.
This projection differs from the segment shading path and is kept distinct.
Invalid inputs/capacity preserve caller output. Extreme coordinates/segments
outside safe float intermediates are rejected rather than propagated.

2048 complete original4d99c0 cases match PC/NXDK selected counts and ordered
indices (0-32 candidates), including exact tangencies and adjacent floats,
degenerate segments, unknown types and class/color/enabled filtering. Three
invalid-input guards preserve outputs. The fixture uses4d96c0's real cached
room fast path with matching generation at room+1c8 and candidate array at
room+1bc; no original functions are hooked. Both builds and24 CTests pass.
This is instruction emulation evidence, not a native XEMU rendering check.

Static evidence:4d96c0 refreshes room candidate lists when generation c96874
changes, filtering linked sources against room bounds+8/+14. Null room uses
a separate global cache only when879af8 is enabled.4d9870 instead caches at
object+360/+36c using bounds+48/+54.4d9c00 and4d9dd0 perform box filtering.
These cache rebuilds/box filters, ownership and global gates remain open.
4d99c0's disabled global gate preserves the previous active list; an enabled
query resets it even when candidate lookup fails. Callers must preserve that
state distinction when integrating the stateless spherical helper.

## Box candidate filtering

rf_vfx_lights_box reconstructs the shared geometry/filtering behavior of
4d9c00 (explicit bounds with room candidates) and4d9dd0 (object bounds and
object candidates). It preserves candidate order and enabled/color/class
filters. Type1 is accepted; unknown types are skipped. Point/cone lights use
507ba0's inclusive bounds expanded by radius on each axis, including expanded
corners. This is deliberately not Euclidean sphere/box intersection. Segment
lights call the existing reconstructed508b70 segment/box test after expanding
the bounds with float stores. The caller provides output capacity for all
candidates; invalid input preserves count and indices, without allocation.

2048 cases per original path match exact PC/NXDK selected counts and indices,
including0-32 candidates, both class switches, black/disabled/unknown sources,
expanded corners and adjacent floats, crossing/missing/degenerate segments.
Original cached room/object records are supplied with matching generations;
all original geometry and collection accessors execute without hooks. Three
invalid-input guards pass, as do spherical-filter regression, both builds and
24 CTests. No native XEMU visual claim: cache generation/rebuilds, live source
ownership, global active-list state and VFX edge-light integration remain.

## Room/object cache refresh

rf_vfx_light_cache_refresh reconstructs the non-null room4d96c0 and object
4d9870 cache generation/membership behavior. A caller-owned20-byte descriptor
holds generation, count, capacity, index pointer and explicit validity. On an
unchanged valid generation it skips source/bounds reads and preserves the
cached indices. On invalidation it rebuilds geometric membership in supplied
source traversal order, without the enabled/color/class filters used during
active selection. The shared box implementation now separates these filters
from membership geometry. Type0 and unknown types do not enter bounded caches.

Unlike original dynamic arrays, the caller reserves space for the complete
source list before refreshing. No per-query allocation occurs; invalid input
or insufficient capacity preserves the old cache and indices. Source reorder,
removal and bounds changes require caller invalidation. The original generation
is stored before rebuilding; the port commits it after a successful bounded
refresh so a failed refresh remains retryable. Global/null-room behavior and
source lifetime/invalidation dispatch are not implemented by this API.

2048 rebuild cases per original path match PC/NXDK indices/counts/generations.
The original linked lists, collection clear/append, box/segment geometry and
both source-list gates execute; preallocated collection capacity avoids heap
growth, with no hooked functions. Another2048 sequences compare original and
NXDK cache hits after source mutation, followed by generation changes that
remove all sources. Two capacity/bounds guards preserve state. Both builds,
box/sphere regressions and24 CTests pass. PC rebuilding is checked directly;
the mutation sequences use original/NXDK instruction emulation. No live XEMU
lighting or scene ownership claim follows from these harness results.

## Cached selection through final shading

rf_vfx_lights_prepare bridges cached source indices to the contiguous active
source array consumed by rf_vfx_lighting. A92-byte query selects spherical or
box filtering, class switches and optional source transform. It preserves
cached traversal order, reads current source flags/colors, applies the shared
filter and copies/transforms accepted sources into caller scratch. No heap is
used; scratch capacity must cover the cache count. An invalid query, stale
index or transform failure leaves selected count zero, although scratch may
be partially written. Callers must not shade failed output or alias inputs.

1024 complete original pipelines now run4d96c0 cache rebuild,4d99c0/4d9c00
active selection,4d9fd0 source transforms and4daff0/4da8b0 final mixed shading.
PC/NXDK agree on selected count and RGB, and NXDK additionally compares every
prepared source field with the original selected/transformed source. No hooks
replace original functions; the fixture supplies ordered linked sources,
room bounds, ambient and preallocated original cache capacity. Three NXDK
bad-query/index/capacity guards leave selected count zero. Both builds and24
CTests pass. This is composed instruction-emulation coverage, not native VFX
rendering. Existing diagnostic scene model lighting still uses fixed ambient;
source construction/lifetime/invalidation, global disabled-state behavior,
edge caches and native VFX draw submission remain required integration work.
