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

## Point/cone/segment initialization

rf_vfx_light_create accepts explicit constructor arguments in an84-byte
rf_vfx_light_definition and initializes the80-byte candidate used by the
cache/selection/shading pipeline. Original4d8ed0,4d8f80 and4d9050 initialize
point2,cone3 andsegment4. RGB is multiplied by intensity with float stores;
enabled starts1 and class retains its low byte. Segment radius is first
reduced by float0.1, with float0.1 substituted if the result is nonpositive.
Cone4d9520 converts full angles in radians to negative half-angle cosines;
cone scale and low-byte squaring mode are retained. Unused fields are zero
for this fresh-source initializer, not inherited from a reused original slot.
Finite nonnegative radius/intensity/color, supported profiles and ordered
cone angles in0..2pi are required; invalid input preserves output.

2048 complete original constructor executions match all80 represented bytes
on PC/NXDK, with3 invalid-input guards. Original pool-slot insertion and cone
angle helper execute without hooks; an empty scene visibility owner prevents
external geometry updates. Both builds and24 CTests pass. This API does not
register a source, update generation, retain visibility metadata/reference
counts/radius-squared auxiliaries, or replace the native scene lighting.

Static owner evidence:4d8e10 scans1100 slots (268 original bytes each) and
appends a free slot to c4e6b8 when class is nonzero and879af8 is zero, otherwise
to c96768. It increments live countc96878 and clears active selection via
4d9fc0. Creation increments generationc96874 (cone through4d9520).4d9130
reduces references at+58; when the result is below1 it increments generation,
unlinks/clears type and decrements live count, then resets active selection.
4d8660 updates scene geometry only when879af8 andc96880 permit. Reconstruct
these lifetime/registration/visibility effects before binding authored lights.

## Source pool and cache ownership

rf_vfx_light_pool owns first-free slot IDs and two insertion-order lists using
caller storage. Its44-byte descriptor plus80-byte sources and16-byte links
uses105644 bytes at the original1100-slot capacity on PC32/NXDK. Initialization
zeros supplied storage; create/retain/release/move/enable/cache operations do
not allocate. Callers must keep the arrays alive and mutate managed state only
through the owner API. IDs retain original slot-reuse semantics; they are not
stale-handle-safe generation IDs. Capacity exhaustion returns an error while
preserving state, instead of the original constructor's unchecked-1 lookup.

Creation composes the verified initializer, routes class-nonzero sources to
list1 when world is0 and all others to list0, appends in original order,
starts references at0, increments live count/generation and clears active
selection. Retain increments references without invalidation. Release clears
active selection on every call and removes the source when decremented refs
fall below1, advancing generation/live count appropriately. Move and enable
advance generation without clearing active selection. Fresh reused slots
initialize unused shading fields to zero; original unused bytes may persist.
Visibility-geometry callbacks from4d8660 are still excluded.

rf_vfx_light_pool_cache walks list0 for world1 and list1 for world0, returning
pool-slot indices with geometric membership and generation caching. This
connects pool ownership to rf_vfx_lights_prepare: pass pool.sources and
pool.capacity as the source array/count. Class/color/enabled filtering happens
later. Changing world or bounds still requires caller cache invalidation;
source lifetime/move/enable mutations advance the pool generation themselves.

1024 mixed operation sequences compare actual original creation, retain,
release, move and enable functions without hooks. Live source fields, reference
counts, both linked-list orders and active/generation/count transitions agree;
PC/NXDK full represented state serialization also agrees.2048 original/NXDK
room-cache rebuild checks cover both lists. NXDK additionally fills1100 slots,
checks exhaustion preserves owner/output, and confirms first-free reuse after
removing slot17. Two invalid-operation guards preserve state. Both builds and
24 CTests pass. Full-capacity initialization needed a larger instruction limit
in the harness; no runtime code change was required. Scene visibility owner
was empty, so authored loading, visibility updates and native rendering remain.

## Authored level-light records

rf_level_lights_begin/rf_level_light_next read version180 section300 using
45f260's field sequence: UID/name, position/disk orientation, script/header
byte, packed flags/RGBA, radius, inner angle/outer delta, cone scale, profile,
segment length and six intensity/timing values. The632-byte output preserves
raw degrees, disk matrix order and all flags. The minimum113-byte disk record
bounds the declared count; strings are bounded, floats finite, and the final
record must exhaust its section. Errors preserve both reader and output.
No allocations, constructor calls or older-version compatibility are implied.

Independent inventory finds22293 records in93 installed levels:12936 point,
2411 cone and6946 segment shapes. Maximum934 lights (ctf06.rfl) fits the1100
slot pool. All22293 records match byte-for-byte in PC and compiled NXDK,
and truncating the final byte of every record rejects without mutation on
both builds. NXDK supplies only archive reads; the shared section/bounds/
field decoder executes. This checks the static original field sequence,
not execution of the original loader. Both builds and24 CTests pass.

45fbc0 decodes shape=(flags>>4)&15, cycle=(flags>>8)&15; flags1/2/4/8/2000
map to original bytes86/87/84/91/85.45f740 chooses initial intensity by cycle,
constructs segment endpoints around the center along matrix axis18, converts
cone degrees with float0.01745329238474369, creates the source and applies
enabled byte91. Cycles3/4 consume original RNG for timers.45f260's param3==0
path forces class86 to0 and marks byte92. Runtime conversion, original reader
execution, retained authored owners/timing and scene activation remain open.

## Authored activation conversion

rf_level_light_activate converts the retained v180 record into constructor
arguments plus enabled/phase/delay/visibility state.45fbc0 flag decoding selects
shape/cycle, class and visibility.45f740 selects high or low intensity; segment
endpoints use center +/- stored half-length times disk matrix row1, while cone
axis uses disk row0 (matching52cac0's matrix reordering). Cone inner and outer
angles convert degrees to radians with the original float constant and outer
sum precision. Colors multiply bytes by the original float1/255. A caller flag
models45f260's default load path forcing class0.

Cycles3/4 use a supplied15-bit original RNG draw times1/32768 for timer
variance, preserving arithmetic order and the0.1 minimum. Cycles1/2 do not
consume a draw. The caller still owns shared RNG order, live timer state and
source replacement; this conversion does not execute those side effects.

The installed records exposed zero-width cone boundaries (outer delta0).
The source initializer and mixed shader now accept equal inner/outer values.
The original outer rejection ensures accepted points are strictly below the
shared boundary, so this does not enter a zero-denominator interpolation.
The old strict-order guard incorrectly rejected these authored sources.

All22293 authored records plus512 synthetic cycle3/4 cases match actual
45fbc0/45f740 and constructors/enable on original/NXDK for every80-byte source
field and phase/delay/visibility value. PC matches all100 activation bytes and
80 candidate bytes. The fixture supplies original owner fields and only hooks
15-bit RNG for synthetic cycles; the original reader, replacement and live
visibility effects remain separate.2048 mixed-shading cases now include equal
cone boundaries and still match original/PC/NXDK. Both builds and24 CTests
pass. Retained light owners/timing, pool activation and native drawing remain.

## Retained authored-light owner

rf_level_owned_lights_open composes the bounded reader, activation conversion,
pool creation and enable operation into one budgeted allocation. It reserves
the original1100 source/link slots and retains136 bytes per authored light:
UID, pool ID, raw flags/cycle parameters and activation state. The56-byte owner
plus pool arrays and retained records reaches232680 bytes for the largest
installed level. Names/scripts/disk-only fields are not retained by this
runtime owner; required constructor/timing data is copied. No archive pointers
are retained. Repeated close frees at most one allocation and clears the owner.

Capacity/count/budget checks precede allocation. Partial failures free the
private owner and preserve the caller output and RNG state. Cycles3/4 consume
shared-stream draws through a local RNG copy, committing on successful load;
other cycles do not advance it. The caller controls world routing and the
original default-loader class override. This owner does not register lights
with the live scene, step timers or perform4d8660 visibility updates.

All93 installed levels (22293 sources) match the complete retained activation,
pool source/link and state payloads on PC/NXDK. Every level passes exact-budget
loading and rejects a one-byte-short budget. Archives/source contexts close
before retained-state checks, and repeated close is verified. NXDK heap-failure
and late-conversion-failure tests preserve output/RNG and release private
storage. The harness supplies archive reads and heap services only; original
component verification is recorded above. An initially ambiguous free-symbol
lookup in the harness was corrected to require an exact linker symbol match.
Both builds and24 CTests pass. Native scene installation/rendering remains.

## Native campaign scene ownership

The shared campaign scene now opens rf_level_owned_lights after particle/RNG
state is available, using a256KiB owner budget, world routing1 and the default
loader class override. It passes the scene's existing RNG stream and closes
the light owner on every scene cleanup path. Snapshot telemetry records count,
bytes, live count, generation, source/runtime hashes, per-field hashes and RNG
before/after. This installs real retained level lights in the running scene;
model/VFX drawing still needs to consume them. Original startup RNG ordering
for randomized light cycles remains to be bound with the retained timer work.

Native replay initially exposed a mismatch hidden by the instruction-only
owner checks: the first L1S1 source's retained base RGB was one float step
below PC in all three channels, although intensity-scaled source RGB matched.
Per-field hashes and native raw-value readback localized it. Forcing the
original x87-style byte*float-reciprocal conversion through a double
intermediate removed the mismatch; the broad startup SSE environment was not
independently diagnosed. Temporary full color readback storage was removed;
compact field hashes remain in the harness.

artifacts/xemu/replay-20260913-163906 passes180 frames in stock64MiB XEMU.
All231 L1S1 lights load in137072 bytes, with generation462, source hash
2326735975, runtime hash98195217 and every retained field hash matching PC.
RNG3357800067 remains unchanged for these authored records. Normal disc flags
are restored by the replay harness. All22293 activation comparisons plus512
randomized cases still pass, as do both builds and24 CTests. No screenshot was
captured because rendering is unchanged. Timers, visibility callbacks and
native light-driven drawing remain open.

## Authored light timer step

rf_level_light_clock_step reconstructs45fa30 using a24-byte clock value.
Cycles1/2 and disabled lights hold state; active cycles add seconds with a
float store. At elapsed>=delay the phase toggles once, elapsed resets to0
(discarding excess time), and one original15-bit draw computes the next delay.
The timer update doubles the random term before adding center-minus-variance,
then stores/clamps to float0.1. This differs from the activation expression's
addition order. Flag2 enables between-transition interpolation; its arithmetic
stays extended until the final intensity float store. The result reports
whether intensity changed and whether a draw was consumed. The caller owns
RNG draw ordering and applying the intensity through pool/visibility updates.

4096 cases match complete original45fa30 and actual4d93d0 on PC/NXDK for
phase, elapsed, delay, intensity and update/draw flags. Only the RNG integer
is supplied; the color update's intensity argument is observed without
replacing that routine.4096 NXDK in-place cases and2 PC/NXDK invalid-input
guards pass. Both builds and24 CTests pass. This helper does not yet extend
the scene's retained clock storage or schedule timers in the native frame.

## Timer-to-pool intensity updates

rf_vfx_light_pool_color reconstructs4d93d0's source RGB multiplication and
conditional invalidation: class0 increments generation, while class-nonzero
requests4d8660 visibility work without advancing generation. Neither path
clears active selection. Finite input/result validation precedes all writes.
The returned request does not execute scene visibility geometry callbacks.

rf_level_light_tick composes clock evaluation, one shared RNG draw only on a
phase transition, and the color update against the current pool-enabled state.
It retains authored base color separately from changing intensity. It first
evaluates whether a draw is needed, then reevaluates with that draw; only the
final successful result commits clock/RNG. A failed color update preserves
clock, RNG, pool and visibility output. No heap or live frame scheduling is
introduced by the bridge; callers still own persistent elapsed clock storage.

4096 original45fa30/4d93d0 comparisons match PC/NXDK clock values, source RGB,
conditional generation, visibility requests, unchanged active-selection state
and RNG advancement. The original RNG integer comes from the CRT sequence;
actual timer/color callees execute. A NXDK late-color-failure test confirms
transactional state preservation. Both builds and24 CTests pass. Remaining
work is persistent clock/frame ordering and visibility callback dispatch,
followed by light-driven native rendering.

### Retained light clocks and frame-order evidence (2026-09-13)

The light owner now retains one 24-byte clock per authored record, initialized
from the verified activation phase, delay and intensity, with zero elapsed
time and update flags. The 32-bit owner header is 60 bytes; total storage is
105660 + 160 * count, peaking at 255100 bytes for 934 lights. Ownership
verification compares all 22293 PC/NXDK records in 93 installed levels and
independently checks every initial clock field against activation. Exact and
short budgets, allocation failure, rollback and repeated close pass.

Original 433260 calls 487a40, then iterates the pointer registry at 646098
in ascending index order. At 43334f it calls 45fa30 with seconds from 5a4014
and ECX loaded from the registry slot. Accessors 40a480 and 40a490 return
the slot address and count; count is re-read each iteration. Next comes
4e6150 at 43336f. This is static executable evidence, not an end-to-end
frame replay. Registry insertion/removal order, frame gates, shared RNG
ordering with surrounding systems and visibility dispatch remain unbound.
Retained clocks do not yet advance in scene frames or change rendering.

Validation: both builds and all24 CTests pass. Native stock64MiB XEMU
180-frame replay-20260913-165531 passes PC owner/source/runtime/field
telemetry comparisons with the larger allocation (142620 bytes for231
L1S1 lights). Clock contents are covered by the PC/NXDK ownership harness;
native replay does not yet expose clock hashes. Normal disc flags restored.

### Original light registry dispatch (2026-09-13)

verify_level_light_order.py executes original45ec40 append with reserved
capacity and original43332b..433363 dispatch, including actual40a480 and
40a490 accessors. All384 cases /131106 visits pass. The supplied timer
body observes owner/seconds and changes registry count after the first
visit in growth/shrink cases. This confirms ascending order, count re-read
on every iteration, exact seconds forwarding and balanced stack. It does
not replace the separate original/PC/NXDK timer arithmetic comparisons.

Loader461ef0 reads records in file order;461f90 checks owner byte92, then
461f99 pushes the owner,461f9a selects registry646098 and461f9f appends
through45ec40. This loader ordering is static instruction evidence.
The append verifier reserves1100 slots; allocation growth is excluded.

Frame scheduling is after487a40, a substantial actor/physics update with
multiple callees, and before4e6150. Therefore inserting timer stepping at
the beginning of the diagnostic frame would not establish shared RNG
fidelity. Gate436320 returns byte637086;433260 skips simulation when it
equals1, with a further conditional gate involving64ecb9/64ecba,434200
and player state. These gates and surrounding-system RNG ordering are
still static evidence, not ported/verified full-frame behavior.

Discard requested-address exports45d583 and45e373: Ghidra created entries
inside instructions, producing invalid decompilation. They are not evidence
for light removal or lookup semantics; recover proper function boundaries
before using those sites.

### Spotlight visibility box test (2026-09-13)

rf_visibility_light_cone_box reconstructs4d81d0 with six prepared planes
and539780 corner selectors. Dot products use original Z/Y/X order and
add the plane distance without a float store before comparing to the
double constant -0.001 at589d50. Any selected corner above that threshold
rejects the box. This differs from portal classification at zero and from
the radius-expanded box selection used by VFX light lists. Callers must
supply the correct spotlight planes and selectors; they are not view planes.

verify_light_cone_box.py runs actual4d81d0 and its corner/signed-distance
callees without hooks. All4096 original/PC/NXDK cases match, including all
eight selectors and neighboring floats around -0.001;1074 pass the test.
Three invalid-input guards preserve output on PC/NXDK. Both builds and
all24 CTests pass. Full4d86d0 also constructs spotlight planes, walks room
geometry trees and marks dirty state. Those operations and4d8660 alternate
view dispatch remain unbound; this change alone does not alter rendering.

### Spotlight plane construction (2026-09-13)

rf_visibility_light_cone_planes reconstructs4d86d0 preparation through
4d89f8, before geometry traversal. Inputs are resolved position/axis,
radius and stored half-width (original source+8c); the width calculation
in4d9520 is not included. The original4fcfa0 basis retains the axis
magnitude except its strict near-vertical branch. Far center uses the
original axis times radius, with a float product store before addition.
Four far corners use separately stored up/right offsets and ordered adds.
Near/far planes precede four side planes, using existing verified normal
and three-point constructors. Errors preserve all120 output bytes.

verify_light_cone_planes.py executes the original preparation entry and all
math callees unchanged, stopping at4d89f8. All2048 original/PC/NXDK plane
and selector arrays match, including nonunit and near-vertical directions.
Another2048 composed generated-plane/box decisions match all three builds;
three invalid-input guards preserve output. Both builds and24 CTests pass.
Width generation, alternate-view transformation and room/tree dirty-state
traversal still need integration; rendering is unchanged.

### Authored spotlight visibility preparation (2026-09-13)

rf_visibility_light_cone_prepare composes4d9520 stored half-width
(float)(tan((double)outer_angle*0.5)*radius) with the verified six-plane
construction. Inputs use the authored outer angle in radians. Corrected
plane construction to accept finite negative half-widths: installed lights
include180-degree angles, whose binary32 radian conversion exceeds pi and
yields negative tangent/width. The original retains that sign. Zero width
and degenerate plane geometry remain guarded.

verify_light_cone_prepare.py compares actual4d9520 and4d86d0 preparation
without math hooks against PC/NXDK for all2411 installed spotlights plus
1024 synthetic angle/radius/direction cases. All six planes and selectors
match;577 combined cases have negative width. Three input-error guards
preserve output. Existing2048 plane and2048 composed box comparisons,
both builds and24 CTests pass. Native scene/room traversal is not yet bound;
this arithmetic has instruction-level NXDK evidence, not new XEMU evidence.

### Face lighting dirty-state pass (2026-09-13)

rf_visibility_light_faces reconstructs the accepted-node/solid face loop in
4d86d0. Original49cc80 receives face+28, so its signed-word predicate is
face+34 (not face+c). Positive values skip; zero/negative values proceed.
With low-byte mode zero, flags+28 gain40000 without a per-face box test.
Otherwise negative signed lighting indices at+36 skip, and shared lighting
records are indexed through solid+c0. Update low byte selects dirty bit1
or2 for early skip; a geometry hit assigns1 or3 to the entire dirty byte.
Supplied faces retain original linked-list order and shared indices.

verify_light_dirty_faces.py executes full original4d86d0 flat-solid path
with only the bounds predicate supplied; parent bounds pass, per-face
predicate results/call order are checked. All2048 original/PC/NXDK cases
match face flags and shared bytes. A PC/NXDK callback-failure case confirms
earlier updates remain committed. Both builds and24 CTests pass. Tree root
selection, right-before-left traversal and native state binding are next;
this face pass does not claim whole-room visibility or rendered lighting.

### Geometry-tree lighting updates (2026-09-13)

rf_visibility_light_tree composes accepted-node bounds tests with the
verified face dirty-state pass. Uses rf_collision_node layout and borrowed
dirty-face ranges, with caller-supplied stack. Roots pop in reverse order;
accepted nodes push left then right, process their faces, then visit right
before left. A bounds miss prunes children and faces. Invalid indices,
capacity overruns and excess visits are bounded; earlier updates remain
committed on failure. Requires a disjoint acyclic forest. No allocation.

verify_light_dirty_tree.py executes full original4d86d0 with two accepted
room roots,15 geometry nodes and actual linked face lists. Only bounds
predicates are supplied. All1024 original/PC/NXDK cases match node/face
call order, pruned subtrees, face flags and shared dirty bytes. Two NXDK
guards cover insufficient initial scratch and invalid accepted-node child.
Existing2048 face-pass comparisons, both builds and24 CTests pass. Root
collection/detail-room selection, native lighting-state ownership and
alternate-view dispatch remain before scene integration.
