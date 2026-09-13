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
