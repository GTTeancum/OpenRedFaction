# Firing runtime integration

This is an implementation map derived from the original executable, not evidence
of playable combat. Source SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Verified building blocks

The port has retained NPC inventories, selected animation mappings, held weapon
models/materials, hand tags, muzzle placement and target aiming. The standalone
4257c0 ammunition helper matches original execution. These services do not yet
constitute a firing dispatcher, and muzzle queries must not consume ammunition.

## Dispatcher425830

The original dispatcher gates firing on actor state, current weapon, alternate
mode eligibility, timer state and inventory availability before creating shots.
The full function contains ordinary, continuous, special-weapon and network
branches; reconstructing only a normal pistol branch will not complete it.

The ordinary shot-loop trace establishes these integration requirements:

- Advance primary hand510 and wrap against the primary hand-list count before
  asking41b040 for the shot pose. Class flags can request multiple hands.
- Descriptor7c supplies the projectile count per hand; actor814 bit1000 doubles
  this count. Spread can change each projectile direction through RNG.
- Factory4c77a0 can be called again for a linked projectile. Preserve the actual
  dispatcher predicates rather than treating every factory call as one ammo unit.
- The ordinary post-creation path reaches4257c0 after shot processing. Flag814
  bit1000 can cause a second consumption call. Factory failures and special
  branches require separate execution tests before composing this ordering.

These statements are instruction/decompiler trace findings. Full dispatcher
execution and failure-prefix comparisons remain required.

## Factory4c77a0

`tools/audit_projectile_factory.py` decodes583 instructions over4c77a0..4c8030,
checks the original hash, and records direct calls and a reviewable instruction
listing in artifacts. Its AUDITED result proves static boundary presence and
address order only, not conditional runtime coverage.

| Call site | Callee | Required service |
|---|---|---|
|4c7a11|486da0|Generic object creation: kind2, owner handle and prepared descriptor|
|4c7ae4|4c8030|Projectile model setup when an object model exists|
|4c7b71|4d8ed0|Optional dynamic light|
|4c7bc4|497ca0|Optional attached effect|
|4c7bfd|5056a0|Optional positional sound|
|4c7c9b|48c9a0|Collision registration under the original gates|
|4c7eb3|4c2570|Special projectile setup|
|4c7fc8 /4c7fd6|49afe0 /49a420|Immediate model/ordinary collision branches|
|4c8000|4c4b50|Projectile hit processing|
|4c8006|48ab40|Mark object7c bit2 for later retirement|

Creation starts with a zeroed152-byte descriptor and baseline flags80000870;
weapon fields and predicates modify it before486da0. Allocation failure returns
zero after descriptor cleanup. Successful creation publishes weapon ID298,
descriptor pointer294, source-related flags and position, then conditionally
creates resources and links into the projectile list. Every optional owner needs
matching cleanup; a visible mesh alone is insufficient.

Descriptor264 bit20 reaches an immediate collision branch inside the factory.
After processing,48ab40 sets flag7c bit2. **It does not remove the registry entry
or free the object.** The factory subsequently returns its object pointer. Keep
creation, marking and eventual removal distinct; do not free this object before
the dispatcher's remaining accesses. The existing48ab40 export confirms the
single-bit mutation; actual cleanup scheduling must still be connected.

## Next implementation order

1. Reconstruct and verify the152-byte creation descriptor and generic kind2
   ownership, using retained weapon fields and the existing collision registry.
2. Reconstruct the full factory with explicit optional resource ownership and
   rollback/marking behavior; test immediate-hit and persistent projectile paths.
3. Connect the dispatcher to muzzle/aim, spread RNG, factory, ammunition,
   animation, timers and sound in the original order, including failure exits.
4. Verify a representative shot end to end in stock64MiB XEMU: registry state,
   target damage, ammo delta, timers and cleanup, then present a new visual.

Outstanding scope also includes live player/clutter targeting, replacement-class
hand lists, all campaign weapons and continuous projectile simulation.

## Creation descriptor verified (2026-09-13)

rf_projectile_descriptor_prepare now builds the full152-byte descriptor and
boost decision at486da0. It preserves name-length>4 selection, model token,
raw descriptorbc/ac fields, position/basis, scaled forward velocity, special
spin and flags. Important boundaries: factory accepts weapon==weapon_count,
but4c90f0 spin predicate excludes it; special-weapon spin still applies.
Speed<50 tests the original unscaled speed, even when boost changes velocity.
The scale product is stored as float before multiplying forward components.

verify_projectile_descriptor.py executes original4c77a0 to the486da0 call
with real string/vector/spin/4c90f0 code. Only owner lookup and player/powerup
predicates are supplied, with arguments/order checked.2048 cases match all
152 bytes plus boost on PC and compiled NXDK at027f:250 boosted,1264 spinning.
Five PC/NXDK invalid/nonfinite guards and two NXDK null guards pass. Both
builds and22 CTests pass. Evidence artifacts/projectile-descriptor.json.
This does not allocate a projectile; generic kind2 ownership and all factory
post-allocation effects remain open. No new native scene replay or visual.

## Fixed projectile pool verified (2026-09-13)

Original487100 kind2 checks the50-object cap and calls48b590. The underlying
48b4d0 pool contains50 records of314h (788) bytes. Initialization links them
in increasing slot order;48b610 releases to the head, so reuse is LIFO.
Allocation does not clear payload. The optional original heap fallback is
disabled in the stock-Xbox port pool.

rf_projectile_pool owns39424 bytes:39400 record bytes plus24 bookkeeping
bytes, including live-slot guards. Free links are slot+1 tokens rather than
raw pointers. Acquire/release preserve all payload except the free-link word,
which release overwrites. Raw record storage is reserved for the pending
reconstructed object initialization; it is not yet a complete projectile type
or an allocation in the running scene. Callers must clean resources and
remove registrations before returning a slot. Marking flag2 alone does not
authorize immediate release.

verify_projectile_pool.py runs48b4d0/48b590/48b610 unhooked with heap fallback
off.4096 PC/NXDK operations match original normalized free links, selected
slots, free/live/peak counts and all untouched payload bytes:2034 allocations,
2006 releases,56 exhaustion returns. Four NXDK invalid-release/null guards
preserve pool bytes. Both builds and22 CTests pass. Evidence:
artifacts/projectile-pool.json. Generic486da0 initialization/registration,
model/physics ownership and complete factory effects remain open.

## Projectile registration owner adapter (2026-09-13)

rf_projectile_store combines the fixed pool with50 stable type2 owner
wrappers (40824 bytes on32-bit PC/Xbox). It appends the object list, inserts
the generation-checked registry handle, consumes the UID, and invokes an
explicit initialization service. Successful initialization publishes the
generic400000 flag. It does not zero the reusable original-sized payload.

Failure invokes cleanup after list detachment while registry identity is
still available, then releases handle and pool slot. UID/generation changes
are retained across failed initialization. Flag2 marking does not invoke
close. Close takes a handle, so stale generations cannot close reused slots.
The initialize/cleanup callbacks must preserve identity and list links; no
reentry or registry/list mutation is supported. Cleanup must release partial
resources and cannot fail. The eventual factory must supply those services.

verify_projectile_store.py and the PC projectile_owned_lifetime CTest cover
52 initialization/cleanup cycles, full pool/registry exhaustion, stale handles,
mark-without-removal, and failed initialization after acquiring a resource.
Compiled NXDK callbacks verify visibility and cleanup order. Both builds and
all23 CTests pass. Evidence artifacts/projectile-store.json. The first
Unicorn run exceeded its100000-instruction observation cap in the ordinary
registry memset; raising that test cap to1000000 completed successfully.

This is a port ownership adapter using independently verified pool/list/
registry components, not a claim that full486da0 is reconstructed. Room,
model, sphere and physics initialization and complete4c77a0 resource/hit
services remain required before live projectiles. No new native scene run.

## Moving creation physics verified (2026-09-13)

rf_physics_creation_body_open_moving carries descriptor6c linear velocity and
descriptor78 angular vector through the existing49ec90/49f010 preparation.
The previous static constructor supplies zero vectors through the same path;
the creation seed layout stays unchanged. Generated/inherited mass, fallback
spheres, material coefficients and ownership budgets share the existing code.

verify_creation_body.py --moving compares640 complete original constructor
cases with PC and compiled NXDK, including all308 represented state bytes
and owned sphere records. Only original heap calls are supplied; the fallback
opaque word remains normalized because its original value is unspecified.
Checks cover560 allocation failures,640 short budgets, repeated open/close,
and14 NXDK null/nonfinite motion failures without allocation or owner changes.
The unchanged static mode also passes640 cases. Both builds and23 CTests pass.
Evidence: artifacts/creation-body-moving-verification.json and
artifacts/creation-body-verification.json. No new native XEMU run or visual.

This completes the shared moving physics entry point, not projectile factory
integration. Resolve model radius/spheres and map the creation descriptor in
the registered projectile initializer, then implement remaining factory
resources, collision/hit effects and deferred retirement before live firing.

## Registered projectile model/physics resources (2026-09-13)

rf_projectile_initialized_open composes the fixed pool/list/handle adapter
with generic model attachment, model-derived collision spheres and moving
creation physics. Descriptor name is resolved by the caller; wrapper kind
comes from descriptor word1. Negative radius uses attached model bounds;
without a model the object radius defaults to1 while descriptor radius0
remains0 for physics. A single model sphere is centered after querying it.
Original unspecified temporary sphere words are port-initialized to zero.
The caller descriptor stays unchanged; resolved fields enter the physics seed.
Only prepared projectile descriptors are accepted: no mass grid, initial
tensor or caller-installed sphere list.

Model service errors, missing models, sphere conversion errors, insufficient
budget and physics failures release partial resources before registration
and pool rollback. Close releases physics then model. Flag2 alone preserves
the resources/registration for later factory accesses. The raw788-byte record
is still reserved; these resource owners live beside the50 pool slots.
Each owner is348 bytes:17400 for the array,58224 including the40824-byte
store, before sphere/model storage. Per-slot peak includes scratch and owned
spheres. Model backend allocations require their own caller-enforced budget;
this is not yet the complete scene memory accounting or a loaded model backend.

verify_projectile_resources.py compares12 PC/compiled NXDK integration cases
with supplied model/heap services, checking all represented attachment/body
state and owned spheres. Six successes cover model-less negative/zero/positive
radii and zero/one/two model spheres. Failures cover absent model, load, bounds,
sphere service, budget and nonfinite motion; two additional NXDK allocation
failures exercise temporary and retained sphere allocation rollback.
The harness supplies calloc separately because NXDK calloc depends on real
allocator metadata that a malloc-only stub cannot reproduce.
Both builds,24 CTests,640 moving constructor comparisons and52 registry
lifecycle checks pass. Evidence: artifacts/projectile-resources.json.

No native XEMU scene run or new visual. Remaining: bounded retained model
services, generic common/parent/room fields, full4c77a0 post-creation effects,
collision/hit dispatch, deferred retirement and425830 firing scheduling.
This verifies a port composition of reconstructed components, not the entire
original generic factory or functional combat.

## Authored projectile model catalog (2026-09-13)

The installed44-weapon table declares5 static projectile models,11 effect
models and28 model-less entries. Rockets and several enemy attacks use VFX;
a static-only backend would omit required projectile visuals. Distinct
filenames are retained by weapon ID, including empty strings and duplicates.

rf_projectile_model_catalog_read/load implements the required $V3D Filename
field from4c2c80, separately from optional third-person held-weapon models.
Original filename length below5 sets kind0;5143f0 finds the final dot, then
5001d0 compares its suffix case-insensitively to .vfx: kind3 if equal, kind1
otherwise. Thus .vcm selects1 here, unlike the actor classifier. Invalid,
missing, duplicate or oversized fields preserve output. Resident catalog is
4356 bytes; archive text scratch is108890 bytes for the installed table.

verify_projectile_model_catalog.py passes16 PC/compiled NXDK reader cases,
three PC archive budget boundaries and51 filename classifications using the
original extension/comparison helpers unhooked where the length gate permits.
An independent comment-aware table scan matches every filename/type, and all
referenced compiled static/effect assets exist in the installed archives.
Original full table parser is not executed. Both builds and24 CTests pass.
Evidence: artifacts/projectile-model-catalog.json.

Next: consume this catalog in bounded retained static/VFX model services,
then bind those services to the registered projectile initializer. No new
native scene run, resource rendering or functional firing is claimed here.

## VFX header and loader trace (2026-09-13)

The existing particle runtime does not contain a VSFX file loader. The
projectile path is489fe0 ->502a60 ->501110 ->53c9e0 ->54b5b0. Shared effect
definitions are cached by filename;501110 optionally creates a36-byte
instance through54b0d0, which owns separate animated object arrays.503390
reaches501af0 to restart/unpause the effect. These resource/instance paths
are traced, not yet reconstructed.

rf_vfx_header_read reconstructs54b789..54bacb before allocation. It returns
version, consumed prefix and30 compact fields mapped to original owner
offsets in effect.h. Original52c910 version gates supply older defaults;
derived counts preserve unsigned32 wrap. Bad magic, incompatible versions
and truncated headers leave output unchanged. The reader allocates nothing.
Consumers must validate counts and widened byte totals before allocating;
this header decoder does not establish complete effect residency budgets.

The seven distinct authored projectile VFX use3000d,3000e and40006. Older
prefixes consume104 bytes, newer128. DrillMissile01 has7 mesh objects;
ShellTest/laser01 each2; spikeprojectile1; NanoAttackMissile2 plus1 particle
object; SonarAttack has1 particle object and SpitAttack2. No substitution of
static geometry for particle effects is appropriate.

verify_vfx_header.py executes original header instructions and real52c910/
523990, supplying only file read/seek/error primitives. All30 fields and
consumed bytes match1031 original/PC/compiled NXDK cases (7 assets plus1024
version/random-field cases).857 invalid/truncation cases preserve PC/NXDK
outputs, including every truncated prefix of all seven installed assets.
Both builds and24 CTests pass. Evidence: artifacts/vfx-header.json.

Remaining: object chunks, animation tracks, textures, shared definition and
instance allocation budgets, bounds, playback and backend integration. No
full original file load, native XEMU scene run or new visual is claimed.

## Bounded streamed VFX directory (2026-09-13)

rf_vfx_directory_open streams header and record headers from meshes.vpp in
two bounded passes, retaining only header/entry metadata and12-byte record
ranges. Physical stored length includes its size word but excludes the type;
payload is length-4 bytes. All seven projectile assets consume their exact
file lengths with that interpretation. A naive type+size+length advance
overshoots the next record by4 bytes.

The directory preserves all types for later decoders and does not emulate
the original unknown-record skip branch. It checks structural boundaries,
not object semantics or header-count agreement. The archive remains borrowed
and must stay open/unchanged. rf_vfx_chunk_read restricts reads to one payload.
Output commits only after both passes; allocation/read failures roll back.
Budgets include the216-byte owner and retained records, excluding archive,
stack and allocator overhead. No whole-file copy or per-read allocation.

All seven files contain29 records and use228..300 bytes per directory.
NanoAttackMissile includes WARP in addition to mesh/particle/material data;
that record must be decoded for fidelity. Original54b5b0 dispatch leads to
53d0c0 SFXO,5420f0 PART,54ab20 MATL and56a9c0 WARP. The first three are now
exported for reconstruction; no object/track decoder has been implemented yet.

verify_vfx_directory.py compares PC actual-archive output and compiled NXDK
with supplied archive/heap services against an independent record walk.
42 NXDK failures cover short budgets, allocation, second-pass I/O, truncation
and invalid lengths. Reopening, repeated close and every payload boundary
also pass. Header1031-case comparison/857 guards and24 CTests still pass;
both builds succeed. Evidence: artifacts/vfx-directory.json.

Next: bounded mesh/particle/material/warp object decoders, track storage,
instance playback and actual projectile backend. No native XEMU scene run
or new visual; this is structural archive access, not complete VFX loading.

## Complete SFXO face records (2026-09-13)

rf_vfx_face_read reconstructs53d277..53d474. A record consumes96 bytes,
or120 before3000d where six interleaved UV floats become u[3]/v[3].
The100-byte port view retains triangle indices, nine color bytes, two
vectors, a scalar, material and four trailing opaque words; padding and
absent legacy UVs are zero. Pre40000 material indices decrement with unsigned
wrap. Colors use double-precision255 multiplication, truncation and low-byte
retention. Disassembly exposes the multiplier that raw Ghidra output omitted.
Finite geometry and signed32-range scaled colors are required by the port;
errors preserve output. Vertex/material references are resolved later.

verify_vfx_face.py executes the original complete face loop with actual
integer/float/vector/version readers and original ftol, supplying only file
read/error primitives. The missing-material diagnostic is suppressed;
represented fields still include negative/wrapped material IDs.2190 cases
match original/PC/compiled NXDK:2048 generated faces across six versions and
all142 mesh faces from the projectile assets (128 DrillMissile,4 ShellTest,
4 NanoAttackMissile,4 laser01,2 spikeprojectile).238 invalid/truncated cases
preserve PC/NXDK output. Both builds and24 CTests pass.
Evidence: artifacts/vfx-face.json.

Next: mesh object framing, animated vertices, timing and materials, then
particle/warp records and effect playback. The test extracts authored face
spans independently; it is not a complete production mesh loader. No native
XEMU scene run or new visual is claimed.

## Mesh prefix and per-mesh timing (2026-09-13)

rf_vfx_mesh_timing_read reconstructs53d47a..53d553: packed14-bit rate,
retained low two flags, old integer endpoints converted to times, inclusive
sample count from3000c, and explicit times/count in newer versions. Counts
preserve original unsigned wrap; allocation consumers must bound them.
Zero-rate old division and nonfinite explicit times are port errors.

rf_vfx_mesh_prefix_read composes name/parent, enabled byte, vertex count,
legacy vertex span, faces and timing. Empty parent resolves to Scene Root;
the first dash removes the parent prefix. Names/parents are bounded; face
spans and all vertex indices validate before output commits. The168-byte
view returns the remaining payload offset, before material count/list.
It allocates nothing and does not yet decode tracks or materials.

verify_vfx_timing_prefix.py passes2048 original/PC/compiled NXDK timing
cases,98 timing guards,38 prefix cases and155 prefix guards. Prefix cases
include all14 authored projectile meshes plus24 synthetic parent/version
combinations; exact face/timing outputs compose with independently extracted
framing. The entire original mesh-prefix routine is not executed.
Both builds and24 CTests pass. Evidence: artifacts/vfx-timing-prefix.json.

Track allocation must use mesh sample counts: ShellTest meshes have31
samples while the file header says30 frames; laser01 has3 versus2, and
NanoAttackMissile10 versus9. DrillMissile and spikeprojectile each retain
16 and31 respectively.55f560 reads three16-bit components per vertex;
sampling/transform semantics still require reconstruction.

Next: material sections, edge records, vertex frames, UV/transform tracks
and instance playback. No native XEMU scene run or new visual is claimed.

## Standalone MATL data decoder (2026-09-13)

rf_vfx_material_read reconstructs standalone54ab20 data in a208-byte view:
200 represented original field bytes, consumed byte count and deferred
bitmap-request mask. Bitmap IDs remain-1; the mask preserves which original
lookups occur, including the primary empty-name lookup. Exact-case
$original_map/$original_map_rgb markers preserve original-map behavior and
flags. Nonzero boolean input normalizes to1. Types0/1 handle texture fields;
type2 retains color bytes; other types consume only type/rate as original.

Three animation arrays remain checked offsets into borrowed input, replacing
original pointers. rf_vfx_material_sample returns clamped blend samples or
raw color/alpha words. Blend clamping normalizes negative zero to positive
zero, matching original math helpers; nonfinite blend sampling is rejected.
No allocation occurs, so the caller must retain the input or copy its data
into a bounded owner before freeing source storage. This is the standalone
MATL format; the older embedded mesh material format remains required.

verify_vfx_material.py executes complete original54ab20 with actual string,
integer, boolean, float, version and math helpers. File read/error and bitmap
lookup services are supplied.1034 original/PC/compiled NXDK cases match all
represented fields, lookup requests, consumed bytes and samples (pointer
representations normalized):10 authored records plus1024 synthetic cases.
730 invalid/truncated inputs preserve output. Original comparisons caught
and corrected boolean normalization and negative-zero blend behavior.
Both builds and24 CTests pass. Evidence: artifacts/vfx-material.json.

Remaining: embedded materials, bounded input/bitmap ownership, mesh edges,
vertex/transform tracks, particle/warp records and effect playback. No bitmap
resources, native XEMU scene run or new visual are claimed.

## Embedded VFX mesh materials (2026-09-13)

rf_vfx_embedded_material_read reconstructs original53d5d1..53d98e for
pre40000 SFXO materials. It inherits packed mesh rate and sample count,
retains version-gated texture parameters and clamped blend spans, and
returns the single color word separately. Opacity count is retained but
its samples must come from subsequent mesh frame decoding. Color/opacity
pointer words are UINT32_MAX sentinels, never input offsets. Original-map
strings are ordinary bitmap requests in this older format. No allocation
or bitmap binding occurs; errors preserve the caller output.

verify_vfx_embedded_material.py executes the original material loop with
actual file/version/string/math helpers and supplied file/bitmap services.
All represented fields, bitmap request order, blend samples, color word,
consumed bytes and reserved counts match PC and compiled NXDK across1034
cases:1024 synthetic and10 authored DrillMissile01/spikeprojectile records.
Original pointer values are normalized; unfilled alpha values are not
compared.1886 invalid/truncated cases preserve output. Both builds and24
CTests pass. Evidence: artifacts/vfx-embedded-material.json.

Remaining: bounded input/bitmap ownership, mesh edges, vertex/UV/transform
tracks, frame opacity, particle/warp decoding and effect playback. This
is decoder verification, not a native XEMU run or working combat.

## VFX mesh bounds and edge records (2026-09-13)

rf_vfx_mesh_edges_read covers original53d98e..53dc17, from bounds through
keyframed flag selection, before frame allocation. It preserves initial
owner114 flags, pre30002 compatibility bits, the two3000a optional floats,
and packed owner88 flags. All14 authored projectile meshes reach the next
frame section. rf_vfx_edge_read retains original44-byte edge fields with
zero-initialized untouched storage and a checked input span of face indices
instead of pointers. Both APIs borrow input and allocate nothing. Finite
geometry, signed nonnegative counts, checked spans and adjacency bounds
are required; failed reads leave output unchanged.

verify_vfx_mesh_edges.py compares1038 original/PC/compiled NXDK cases:
14 authored meshes plus1024 synthetic records,3330 edges total. The harness
runs actual integer/vector/float/version/boolean helpers, supplies only file
services, normalizes edge pointers and checks adjacency plus consumed bytes.
Original reverse face-link conversion is exercised with zero edge indices;
the port intentionally retains those references in rf_vfx_face.words_80
until owned mesh construction validates and resolves them. This is not a
claim of completed pointer binding.8113 invalid/truncated cases preserve
output. Both builds and24 CTests pass; artifacts/vfx-mesh-edges.json records
the result. No native XEMU run or new rendered effect is claimed.

Next: quantized vertex frames, UV/transform tracks and frame opacity, then
bounded resource ownership, particles/warps and playback.

## Serialized VFX animation frames (2026-09-13)

rf_vfx_frame_read reconstructs53ddb8..53e05d for one frame, using explicit
version, mesh flags, vertex/face counts, frame index and legacy defaults.
It locates input-backed uint16 vertex triples and interleaved UV spans,
retains quantization vectors/extra floats and per-frame transforms, and
clamps legacy material opacity. Presence bits distinguish supplied blocks
from shared data inherited from earlier frames. Unassigned fields zero.
The first directional vector remains serialized, before4fab70 normalization;
owned resource construction must normalize it. No allocation occurs.

verify_vfx_frame.py runs the original frame branch and real helpers, supplying
file read/error/seek services. It observes direction before normalization
while allowing the real normalizer to run.1255 original/PC/compiled NXDK
cases match represented fields, vertex bytes, deinterleaved UVs, transforms,
opacity and consumed bytes:231 authored frames across all14 meshes plus1024
synthetic cases.4447 invalid/truncated inputs preserve output. Both builds
and24 CTests pass. Evidence: artifacts/vfx-frame.json.

These views do not allocate or attach resources, interpolate animation,
parse the subsequent key tracks, or dispatch effects in the scene. Direction
normalization, shared-frame ownership, key tracks and playback remain open.
No native XEMU run, working firing or new screenshot is claimed.

## VFX transform key tracks (2026-09-13)

rf_vfx_key_tracks_read reconstructs53e080..53e424 for keyframed meshes,
retaining base position/quaternion/scale and three borrowed key spans.
Serialized records are40 bytes each. Counts retain the original low16
truncation, followed by checked byte spans. Before3000a, position/quaternion
defaults are an explicit caller input because they originate in runtime
globals; scale defaults1. Installed keyed assets serialize their base data.

rf_vfx_key_read preserves translation/scale records and converts rotation
records to the original20-byte layout: quaternion components multiply by
16383 then truncate to low16, and five float parameters truncate to low8.
The remaining20 output bytes are zero. Finite and signed32 conversion-range
guards reject unsafe values rather than overflowing C casts. Errors preserve
output. No interpolation, allocation or resource attachment occurs.

verify_vfx_keys.py executes the original instructions with real helpers and
supplied file read/error services. Legacy globals are explicitly seeded to
the caller defaults, not asserted to be reconstructed startup state.1031
original/PC/compiled NXDK cases cover7 authored keyed meshes and1024 synthetic
cases,6160 keys total. Base values, counts, converted records and consumed
bytes match.4111 invalid/truncated cases preserve output. Both builds and24
CTests pass. Evidence: artifacts/vfx-keys.json. Combined verified decoders
consume each of the14 authored mesh payloads exactly, including the optional
key section; they still require a composed bounded owner and resource binding.
No native XEMU run or new scene visual is claimed.

## Bounded owned VFX meshes (2026-09-13)

rf_vfx_mesh_open composes the verified prefix, material, edge, frame and
key readers into one owned SFXO resource. One allocation contains the owner,
frame views and a private payload copy. The source can be released after
success. Frame/key spans and edge-directory offsets refer to owned data;
serialized subordinate material/edge records retain their local span rules.
All local face materials, both directions of face/edge references, and
external material indices are range checked. Trailing data is rejected.
External material validation still depends on the caller supplying the real
definition-wide material count. The owner starts mesh runtime flags at zero.

The budget includes the308-byte32-bit owner,112 bytes per sample and retained
payload, excluding caller input, stack, allocator overhead and future bitmap
resources. The sample count is checked against the budget before allocation
or frame iteration. Failed parsing frees the allocation without publishing
an owner; repeated close is safe. Frames without serialized blocks remain
explicitly absent for later shared-frame resolution.

verify_vfx_mesh_owned.py compares PC and compiled NXDK header/frame/payload
bytes for all14 authored meshes after overwriting/releasing source input.
Resident sizes range1290..10650 bytes.14 exact budgets succeed;132 failures
cover short budgets, truncation, trailing data, local materials, edge links
and external materials, with no leaked/published owners.14 injected malloc
failures also preserve empty output. Repeat close passes. This composes
previously original-verified decoders; it is not an original allocator match.
Both builds and24 CTests pass. Evidence: artifacts/vfx-mesh-owned.json.

Next: material/bitmap binding, normalized directional state, shared vertex/UV
selection, interpolation and rendering; particles and warp objects remain
required for complete VFX effects. No native XEMU run or new visual claimed.

## Owned VFX texture bindings (2026-09-13)

rf_vfx_material_textures_open consumes decoded material bitmap_requests,
producing primary/secondary/glare slot mappings and case-insensitively shared
all-frame texture owners through the existing particle animation/image loader.
Only requested slots resolve; original-map markers remain for runtime binding.
Empty/missing requests map to UINT32_MAX, preserving the original bitmap-1
result. Missing named slots remain recorded with empty animation owners.
Found corrupt/unsupported resources fail the load and release partial owners.
First matching archive wins. Views and archives can be released after success.
The budget covers owner, worst-case binding/name slots, image descriptors and
pixels, excluding caller views, decoder stack and allocator overhead.

verify_vfx_textures.py reconstructs20 authored material views from the seven
projectile VFX files. Their16 distinct textures all exist in maps1..maps4.vpp,
and all16 are single-frame assets. Bindings and every pixel hash match
independent archive image loads after source/archive retirement. Total
residency356344 bytes includes tables and image descriptors. Exact/short
budgets, missing archives, case-insensitive sharing and malformed views pass.
This texture storage is a port ownership policy, not original bitmap IDs.

verify_vfx_textures_nxdk.py executes compiled material/animation ownership
with zero archives and supplied calloc/free:5 PC/NXDK comparisons cover
missing slots, empty input, budgets and invalid views; one injected allocation
failure and repeated close also pass. Actual image decoding was checked on
PC here; this is not a native Xbox texture or GPU verification. Both builds
and24 CTests pass. Reports: artifacts/vfx-textures.json and
artifacts/vfx-textures-nxdk.json.

Next: compose definition/material/mesh lifetimes, normalize direction, bind
frame/key interpolation and renderer submission, then native XEMU evidence.
Particles/warp objects remain necessary for complete projectile effects.

## VFX frame selection and vertex expansion (2026-09-13)

Playback trace: wrapper501ab0 routes kind3 to54cce0. It advances instance
seconds, converts them to15Hz effect-frame units, handles pause/loop/stop,
and calls53f050 ->53f060 for meshes.54cf70 starts particle retirement rather
than immediately removing all effects. Full effect lifecycle remains open.

rf_vfx_frame_select reconstructs53f060..53f12f. The original float constant
58a268 is0.06666667014360428 (1/15), with an explicit float store before
subtracting mesh start time and multiplying its packed rate. It preserves
the inclusive endpoint gate and flag2's zero interpolation fraction. Safe
returned indices clamp the final interval to the last sample; original raw
floor/next values are normalized only for that representation. Zero samples,
nonfinite inputs and unsafe integer ranges fail without changing output.

rf_vfx_vertex_decode reconstructs complete53cca0. Serialized uint16 storage
is interpreted as signed16 coordinates, multiplied by per-axis scales with
float rounding before origin addition. It produces object-local geometry;
it does not apply animation or parent transforms. rf_vfx_mesh_vertex binds
this to retained data and selects shared frame0 unless flag4 enables separate
vertex frames, with index/span guards and unchanged outputs on failure.

verify_vfx_sampling.py matches2048 original/PC/compiled NXDK frame cases and
2048 vertex cases with real original math helpers. Two invalid frame cases
preserve outputs. Extended verify_vfx_mesh_owned.py checks3128 authored
vertex expansions against original53cca0 (1564 each at normal/exact budgets),
plus invalid frame access, alongside existing ownership/rollback tests.
Both builds and24 CTests pass. Reports: artifacts/vfx-sampling.json and
artifacts/vfx-mesh-owned.json. No interpolation or native XEMU visual claimed.

Remaining mesh update53f060 branches: transform matrices, key evaluation
569f70/56a250/56a3f0, vertex/UV interpolation, active visibility and parent
attachment transforms. These must compose before normal effect rendering.
