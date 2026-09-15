# GeoMod implementation status

GeoMod fidelity is the highest current priority. The developer room has live
rocket-driven faceted excavation and matching collision, but crater shape,
interior lighting, debris and eligibility remain prototypes. Later sections
record current changes; earlier milestones below are historical evidence.

## Region input evidence

The format lead is [rf-reversed rfl.ksy](https://github.com/rafalh/rf-reversed/blob/master/rfl.ksy),
credited to Rafal Harabien/Open Faction and wardd64/Unity Faction and labeled
GPL-3.0-or-later. The new bounded C byte reader is independently written from
format observations, not translated schema code. No original runtime address
has yet been established for this reader.

Installed v180 section0x200 contains a record count followed by region UID,
flags, hardness and shape data. The reader retains position, serialized basis,
extents/radius and optional shallow depth. Unlike the cited schema's box-only
matrix condition, installed L9S1 region6078 (flags42) stores a matrix for a
shallow sphere; ordinary spheres such as L1S1 region6910 omit it. The condition
box OR shallow validates every installed region section. Flag bits and basis
are preserved; precedence and point-containment policy remain unrecovered.

Glass House has one box region: UID128, flags4, hardness25, center(0,0,0),
serialized dimensions(56,48,60), identity basis in file order forward/right/up.
This is a useful controlled destruction test location; hardness interpretation
and default/outside-region policy still need evidence.

## Validation and limits

rf_geo_region_tests validates903 records in82 sections across levels1/2/3/m,
plus an installed-box expectation, ordinary/shallow sphere cases, all truncated
prefixes of the room payload, invalid shape/hardness/nonfinite data, capacity,
trailing bytes and unchanged outputs on truncated input. The decoder performs
a validation pass before writing caller storage and allocates no memory.
Input/output storage must not overlap. PC tests and the NXDK build pass.
There is no native execution or destruction visual claim for this milestone.

## Next implementation

Load retained region owners for the developer room. Establish the original
hardness, overlapping-region and shallow-cut rules from executable evidence.
Add a bounded mutable solid representation and transactional subtraction:
new surfaces, renderer updates, collision rebuild and rollback on budget
failure must describe the same solid. Exercise a visible hole that the player
and weapon rays can traverse, repeated overlapping cuts, and room reset.
Existing geometry/room acceleration structures describe static snapshots and
must not be silently reused after topology changes. Keep full stock64MiB
memory accounting; room endpoint headroom is not a destruction memory budget.

## Bounded surface cutting foundation

`rf_geomod_polygon_split` splits a planar convex polygon with a normalized
plane, retaining winding and interpolating texture UVs. Coplanar polygons
belong to the front side; the classification tolerance is1e-5 world units.
Each output is limited to64 vertices. Finite input/plane and buffer capacities
are validated before outputs commit. This is a practical port implementation,
not a recovered original routine.

`rf_geomod_polygon_subtract` partitions a polygon against up to32 planes of a
convex cutter, keeping disjoint outside fragments. A validation/count pass
precedes output writes so capacity failure leaves outputs untouched. Neither
operation allocates memory. Scratch uses bounded local arrays; integration
must account for this stack use as well as caller-owned result storage.
Callers must provide planar convex input and nonoverlapping input/output
buffers. Region policy, solid topology and interior cap generation remain
separate requirements.

PC tests check an analytically known cut, UV interpolation, winding, tangent
and coplanar cases, required sizes, failure preservation,360 rotated cut
planes, and a centered square cutter whose surviving polygon area is3 from
an original area4. PC and NXDK builds pass. These are mathematical primitive
tests, not proof of watertight solids, native gameplay, visible GeoMod or
updated collision. Next: build the interior surfaces and mutable solid owner,
then integrate the same committed topology into rendering and collision.

## Interior surface primitive

`rf_geomod_interior_face` clips one outward cutter face against the negative
half-spaces of a convex source, then reverses its winding. This supplies the
correct facing direction for an exposed interior wall. Faces wholly on a
source boundary do not generate duplicate caps. The same64-vertex/32-plane
limits, no-allocation policy and capacity-failure preservation apply.

The PC test combines surviving cube surfaces with these interior faces for a
through-tunnel. A64-unit cube minus a16-unit tunnel gives signed volume48 and
four interior quads. Boundary exclusion, required counts and insufficient
output capacity are checked; the prior split/subtraction tests still pass.
This is not an edge-manifold/watertightness proof. It does not resolve arbitrary
concave sources, repeated cuts, coincident source/cutter boundary policy,
vertex welding, or T-junctions between fragments. In particular, the surface
subtractor's front-side coplanar convention is not a complete Boolean boundary
classification. A production solid owner must resolve these cases before
claiming general subtraction. No in-game geometry has been changed yet.

Next: retained mutable solid storage with transactional output and shared
render/collision consumption, including source/cutter material ownership.
Verify edge closure and repeated/coplanar cuts alongside live visual and ray/
player traversal evidence. Keep the developer room as the integration target.

## Coplanar boundary correction

A new identical-solid test initially returned the original cube volume64
instead of0. Surface subtraction now uses source polygon winding to distinguish
same-facing coincident cutter boundaries (remove the overlapping surface) from
opposite-facing contact (retain it). The lower-level splitter retains its
original front-side coplanar convention. Degenerate source polygons are rejected
before output commits. This supersedes the earlier surface-subtractor policy
limitation for these tested convex cases, not for arbitrary Boolean topology.

Tests now cover identical cubes (volume0), face contact (64), a partially
aligned cut (56), enclosing removal (0), disjoint cutting (64), and the prior
through-tunnel (48). A separate geometric edge-coverage check subdivides edges
at all result vertices and requires two opposite-directed incidences on every
segment. All cases pass; removing a face fails the check. This accounts for
T-junction coverage without claiming that the output already has welded/shared
mesh indices. PC tests and NXDK build pass; no in-game surface is modified.

Still required: a transactional mutable solid owner, repeated cuts into
non-convex results, general coplanar/near-degenerate cases, material ownership,
edge welding, renderer/collision replacement and reset. The convex cube tests
are evidence for these primitives, not general watertight GeoMod gameplay.

## Transactional mesh storage

The opaque rf_geomod_storage owner allocates original/reset data and two
bounded working banks together. Vertex and face capacities are explicit;
reported bytes include the owner and banks but exclude allocator overhead.
Each face retains material and original face IDs. Empty output represents full
removal. Begin clears only the pending bank; append validates capacity and
finite attributes; commit swaps banks and advances a nonwrapping generation.
Abort leaves the published mesh unchanged. Reset copies the immutable original
into the other bank. No append, commit, abort or reset allocation occurs.

Pending/current read-only views allow future rendering and collision builders
to inspect the replacement before publication. Borrowed views must not survive
bank reuse. This is data ownership, not complete cross-system atomicity: the
caller must validate solid topology and prepare renderer/collision resources
before committing all dependent state together. No game scene consumes this
owner yet, and existing developer-room controls do not reset it.

PC tests cover exact budget acceptance and one-byte-short rejection, overfull
and nonfinite edits, pending/current isolation, material/source IDs, empty
commits, original-data restoration,20 reset cycles and unchanged resident
allocation. Prior cutting/interior tests still pass. NXDK compilation passes;
there is no native runtime or live destruction claim for this owner yet.

Next integration: construct complete cut results in the pending bank, retain
cut history or equivalent topology for repeated cuts, and publish the same
validated mesh to renderer and collision in the developer room.

## Complete convex-cut preparation

rf_geomod_storage_prepare_convex_cut now combines the surface and interior
primitives into one pending replacement. It derives outward planes from the
source/cutter polygons, checks face bounds, finite attributes, planar faces
and convex half-space containment, then builds survivors and interior faces.
Caller-provided retained scratch avoids cut-time allocation and a large Xbox
stack array. Source material/face IDs survive; new interiors use cutter
materials and UINT32_MAX as their source-face sentinel. Commit remains
explicit so dependent rendering/collision can be prepared first.

The storage-backed tunnel test passes volume48 and geometric edge closure,
checks four interior materials, verifies live/pending isolation, commits and
resets. A deliberately undersized bank fails without modifying live vertices,
counts or generation. Attempting to pass the resulting non-convex tunnel back
as a convex source is rejected rather than misclassified. Prior geometry and
storage tests pass; NXDK compilation passes. This is a convex assembly path,
not repeated-cut support for arbitrary terrain or a live scene integration.

Closed input meshes remain a caller requirement; the plane checks alone are
not a universal manifold validator. Next: retain cut history or equivalent
non-convex solid representation, then use pending mesh views for shared
renderer/collision preparation and live developer-room cuts.


## Bounded repeated-cut preparation

rf_geomod_storage_prepare_cuts rebuilds the immutable original convex solid
minus the union of a complete caller-supplied history of up to8 convex cutters.
The working result may be non-convex: it is never reclassified as a convex
source. Original surfaces are fragmented by every cutter. Each interior face
is clipped to the original and then excluded by the other cutters. Touching
cutters lose their internal shared wall; same-facing coincident caps belong
to the earliest cutter, including its material. Source surface IDs survive.
Zero cutters prepares the original. History is caller-owned and must include
all retained cuts; this function does not append to or persist a history.

The caller retains rf_geomod_multi_work on the heap and accounts for its
sizeof in the stock64MiB budget. It contains two4096-vertex/512-fragment banks,
a single-face splitting workspace and nine32-plane arrays. There is no
cut-time allocation. Inputs are validated before beginning an edit; scratch
or mesh-capacity overflow aborts the pending edit and preserves live data.
Commit remains explicit, pending shared renderer/collision preparation.

PC tests pass crossed, identical, adjacent, separated, nested, partially
coplanar, enclosing and externally touching cutters in both orders; repeating
a history preserves the expected volume and geometric edge closure. Tests
also verify original-data restoration, cap material ownership, invalid later
history, the8-cutter bound and undersized storage rollback. Three intersecting
tunnels plus five duplicate cutters leave volume32 out of an original64,
both axis-aligned and after a common rotation. The closure checker uses a
consistent1e-6 world-distance tolerance for float endpoint subdivisions;
removing a face still fails its negative control. Prior split/storage tests
pass and NXDK builds the XBE/XISO. No native runtime cut has been exercised.

This supports repeated cuts of a convex original, not arbitrary imported
concave world geometry. The developer room still needs a bounded terrain
owner, history admission/region rules, shared render and collision publication,
weapon impact integration and reset before usable live holes can be claimed.


## Existing collision-query adapter

rf_geomod_collision_faces converts a prepared mesh into caller-owned position
and rf_collision_face arrays without allocating or retaining mesh-bank pointers.
Each caller-supplied face filter is preserved; source-order face indices allow
the collision tree's source_indices to resolve generated material/source IDs.
Planes are derived from winding, bounds use the same0.0001 expansion as the
existing geometry adapter (4e002b), and planar convex polygons are validated.
Capacity, numeric and filter validation precedes all output writes. This is a
practical generated-geometry binding, not recovered original GeoMod behavior.

The existing rf_collision_tree_open, rf_collision_thin_tree and
rf_collision_sweep_tree now run against both the original block and prepared
tunnel in PC tests. A ray through the original stops at fraction0.25; a
radius0.5 body stops at0.1875. Both clear the central cut tunnel. A radius1.1
body remains obstructed by its rim. A ray from inside hits the cut interior at
fraction1/3 with the inward-facing normal and generated-face material lookup.
Reset restores the original obstruction. Capacity and invalid-final-filter
errors leave the output arrays byte-identical. Rotated eight-cutter geometry
also binds successfully. These are real core collision queries on fixtures,
not player traversal or visual evidence from the live developer room.

The adapter requires retained output arrays plus the existing tree owner's
budgeted allocation. Live integration must account for the old and pending
resources together and publish collision and rendering from the same mesh
generation. No room owner or weapon impact currently calls this adapter.
NXDK compilation passes and produces the Xbox XBE/XISO; native execution of this adapter remains unverified.


## Shared world projection of generated faces

rf_preview_geomod binds generated mesh positions/UVs/materials and its matching
collision face planes to the existing world triangle fan, camera transform,
backface rejection, six-plane clipping and raster output format. The ordinary
world path calls the same generator with its existing serialized inputs.
Output uses caller-retained capacity with a sizing pass before writes; generated
faces currently have no lightmaps. This prepares draw data and does not submit
or publish live terrain. Collision bindings must match the same mesh snapshot.

The tunnel fixture now runs through the real PC rasterizer. Inspected original
and cut captures show the intact front surface, then a central passage with
four interior walls and surviving surrounding surface. Pixel depth checks
confirm the center becomes background while the rim and interior remain
visible; material slots, finite projection and capacity rollback also pass.
Captures are explicitly synthetic geometry, not a developer-room destruction
screenshot. RF_GEOMOD_CAPTURE_DIR enables optional original.ppm/cut.ppm output
from rf_geomod_interior_tests when an existing capture directory is supplied.

PC build and the100-frame developer-room ammo-refill replay pass after the
shared renderer change. Its final image was inspected: textured room, central
structure, handgun and HUD remain visible. Audio and full weapon-sequence
presentation were not reviewed. NXDK builds the XBE/XISO; native execution of
generated-terrain rendering remains unverified. Next is live terrain ownership
and joint rendering/collision publication, then weapon impact and reset wiring.


## Actual room cavity representation

Inspection of glass_house.rfl section0x100 found598 faces,782 vertices and91
serialized rooms. Faces0..5 form the inward-wound outer cavity with bounds
(-16,-12,-20)..(16,12,20), signed volume-30720. They cannot be passed to the
outward-solid cutter. The remaining geometry includes the central structure,
small closed members and paired surfaces; it is not one convex material solid.

rf_geomod_storage_prepare_cavity_cuts now expands a convex empty room by the
complete bounded cutter union. It flips plane interpretation for original
half-space validation, removes cut overlaps from inward room surfaces, and
keeps reversed cutter boundaries outside the original air volume. Opposing
contact faces are internal and removed; duplicate cutters retain one boundary.
It shares history exclusion, capacity rollback and explicit pending publication
with the solid path. It does not recover room merging, portals or cut eligibility.
Detached cutters can mathematically create separate cavities; gameplay admission
must prevent unwanted disconnected excavation. Other level geometry is external.

The retained workspace now includes a second single-face fragment buffer so
cavity seeds survive subsequent cutter exclusions: sizeof(rf_geomod_multi_work)
is259072 bytes with the current scalar layouts, outside mesh/tree allocations.
No cavity-cut allocation occurs. Live stock64MiB accounting must include it.

Tests expand the synthetic cavity by one/two crossed cuts and duplicate cuts,
checking signed volume and geometric edge closure. A separate installed-data
case imports positions, UVs and materials from actual Glass House faces0..5,
cuts x[-1,1],y[-11,-9],z[19,21], and verifies volume-30724 plus closed edges.
The existing collision tree then hits the excavated wall at z21 instead ofz20.
Run rf_geomod_interior_tests Installed_Game to include this case; CTest passes
the absolute installed-data path. Earlier solid/collision/projection tests pass.

This closes the original representation mismatch but does not yet change the
live developer room. Next: publish replaced outer faces and their collision
alongside the unchanged room contents, bind weapon impact and reset, then
verify the actual view and player movement on PC/Xbox. Arbitrary campaign
cavities and portal merging remain later work under the developer-room mandate.
NXDK compilation passes and produces the XBE/XISO; cavity cutting has not yet run inside XEMU.


## Bounded terrain runtime owner

rf_geomod_terrain owns original/reset mesh data, up to8 axis-aligned box
cutters, retained Boolean scratch, two position/face banks, copied source
filters and the current collision tree. It supports convex material solids or
convex room cavities explicitly. Source-face IDs must be unique and cannot use
the generated-face sentinel. Original filters follow their source IDs through
fragmentation; generated faces receive an explicit caller-supplied filter.
Box interiors currently use provisional world-scale planar UV coordinates.

A cut rebuilds from the complete retained history, binds pending collision and
builds its tree while old collision remains alive. Only after successful
preparation does it commit the mesh, tree, source-order face bindings and cut
count. Failure aborts the pending mesh and preserves the current generation.
Reset follows the same transaction with zero cutters. Borrowed snapshots expose
matching mesh/face/tree data and remain valid until the next successful edit.
This is a single-thread owner; projected/GPU resource publication stays with
the scene. Tree construction allocates, while clipping scratch is retained.

The total budget includes the original/working mesh, history, workspace,
position/filter/face buffers, old plus new tree storage and tree construction
scratch. Accounting conservatively counts embedded tree descriptors again and
excludes allocator metadata/external rendering resources. On the tested PC
layout, the actual Glass House outer-room owner at capacities2048 vertices and
128 faces requires an initial peak423490 bytes. This is not native Xbox free
RAM evidence or the eventual total scene cost.

Installed-data tests commit eight cuts, verify updated wall hits and original
versus generated filters, reject a ninth cut without a new generation, reset
and recover the original wall and resident byte count, reject invalid extents,
and exercise exact initial budget/one-byte-short rejection. An owner opened
with only its initial peak budget rejects a later cut because old+new collision
cannot coexist; its mesh pointers, generation, cut count and original wall hit
remain unchanged. Closing twice is safe. Existing geometry/render tests pass.

No gameplay scene instantiates this owner yet. Next is excluding replaced
source faces from existing world rendering/collision and including the owner
snapshot in both paths, then weapon-impact and reset controls. Authored cut
eligibility, arbitrary room unions and portal handling remain separate work.
NXDK compilation passes and produces the XBE/XISO; this owner has not yet been exercised inside XEMU.


## Full-world collision overlay

Glass House outer faces0..5 belong exclusively to room0. The other592 faces
belong to the other90 room records, so replacing room0 does not require
rebuilding the central structure. rf_geometry_collision_overlay_open now
borrows the complete original world while copying room/view descriptors and
owning one bounded replacement face-ID map. bind swaps that room's borrowed
tree descriptor and remaps tree source order to explicit geometry metadata IDs.
Other trees, vertex arrays and room lists remain borrowed. The dedicated close
releases only overlay allocations; world_close must not be used on its view.
Bounds expand conservatively and rebind allocates nothing. Invalid mappings
are rejected before changing the active map/tree.

Tests use all91 rooms: the original world still hits z20, while the overlay
hits the excavation at z21. A radius0.5 body stops at z20.5, and a point at
z20.25 locates/tracks in room0. A ray into the central structure retains the
same face and hit fraction. Rejected mapping updates preserve the previous
wall hit; reset/rebind restores the original boundary. Generated interior
metadata uses an explicitly selected original face with the same material;
this is a provisional metadata fallback, not a new serialized face identity.

Room-location testing exposed that the earlier generic filter-preservation
fixture used flag8, which the existing locator excludes via mask0x0c. The
actual-room fixture now supplies outer-wall flag256 for generated surfaces.
The owner still preserves whatever explicit filter its caller supplies. This
is why a ray-only check was insufficient for validating traversable space.

PC full-world checks and NXDK XBE/XISO compilation pass. Scene code does not
yet instantiate the overlay. It must rebind immediately after successful
terrain edits while no query can observe the retired tree, and project the
same snapshot while suppressing the original outer wall draw. Native player
movement, weapon-impact editing and reset controls remain unverified.


## Live developer scene integration

The shared developer scene now owns the terrain and collision overlay with
1MiB/64KiB caps. It copies source positions/UVs/filters from Glass House outer
faces0..5, resolves materials into the retained shared table and redirects
scene collision to the overlay. After a successful cut it rebinds immediately
before further queries. Source-order/generated material metadata maps back to
valid original face IDs using an explicit same-material fallback for interiors.

After the first cut, a persistent borrowed rendering view starts at original
face offset6, excluding only the old outer shell. Remaining geometry stays on
the existing static-world path, including Xbox retention. The matching terrain
snapshot is projected into the world's dynamic prefix before actor/HUD draws.
No serialized game bytes are modified. Before any cut, the original rendering
path and its lightmaps remain active. Close releases overlay and terrain after
other scene resources. The renderer's shared capacity remains3MiB in the tested
run, including reserved actor space; cuts exceeding owner capacity are rejected.

Use+AltFire is an explicit developer excavation tool, one edit per press.
It ray-tests the complete world and accepts room0 only, then creates a box with
half-extents(2,2.5,2). It suppresses that alternate weapon action. This does not
claim explosion-driven destruction, spherical cutters, authored GeoMod hardness
or arbitrary campaign-room topology. See DEV-ROOM.md for live PC/Xbox evidence
and remaining controls. The GEOMOD diagnostic reports enabled,cut count,mesh
generation,resident bytes,peak bytes,status,attempts and successful edits.

## Destruction focus: faceted rocket craters

Rocket impacts now use an inscribed20-face icosahedral cutter at authored
radius5 rather than a box. The shared terrain owner retains up to8 mixed
convex cuts, with60vertices/20faces reserved per history slot. The same
transactional clipping and collision publication apply. Room capacity is
4096vertices/512faces under its1MiB budget, with a512-entry overlay map.
Complex overlap may still reject within these fixed bounds.

The first crater render reused metal wall panels, producing a misleading
folded/protruding appearance. The DEV room now explicitly uses installed
rck_canyon_rock01.tga as its excavated substrate. Crater UVs use dominant
plane projection at one tile per4world units. This is testbed material
policy, not recovered per-surface GeoMod eligibility/material metadata.
Collision fallback still maps generated faces to the prior room surface
policy. Interior illumination and debris are unfinished; coarse facets
remain visible and are not represented as final destruction quality.

tools/dev_destruction_check.py reproduces intact/single/two-impact views,
an angled approach and traversal using ordinary game input. Two successful
cuts reach generation3; the player walks to x=-18.045902 beyond the original
x=-16wall, alive, at y=-13.067719. Peak terrain plus overlay accounting is
706675bytes. PC images show a deeper rock cavity after the second shot.
These checks cover this outer room, not arbitrary campaign geometry,
material eligibility, dynamic objects or a complete destruction system.

Priority clarification: GeoMod fidelity takes precedence over other systems.
The above approximation is an intermediate prototype, not the desired final
appearance. Native500frame two-impact/approach verification at
artifacts/xemu/render-20260915-083610 passes38comparisons and leaves
9092pages free. Image inspected; all19staging entries restored. Original
crater reference, material rules, debris, interior lighting and repeated
destruction behavior require continued work before this can be accepted.

## Authored interior setting and next original-code lead

The v180 section900 prefix stores a16-bit byte count, texture name and
32-bit hardness. The new bounded reader preserves output on errors and
does not interpret hardness. Glass House specifies rock02.tga and stored
hardness0. Its texture is in ui.vpp, not the five map archives. The DEV
scene now loads that setting with maps-first/ui-last lookup and transfers
only decoded pixels to the shared material table; ui.vpp is streamed and
staged by the Xbox build. The prior canyon-rock override is removed.

Format lead: reference/openfaction/common/include/formats/rfl_format.h,
commit e8a4a885ba866fc472702b3dc8a9e8208f9b91e4 (GPL format reference;
reader independently written). Original RF.exe SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836,
Ghidra export4618b0, loads the texture into646000 and hardness into646004,
replacing zero hardness with55. That runtime fallback is evidence only
here; region precedence and how hardness controls cuts remain unimplemented.

Existing export467020 provides the next cut-shape lead: it calls4375b0
with an input ID, derives scale from radius divided by definition+0x60
unless flag8 is set, calls45cff0, and invokes the effect owner437230. A bounded
128-entry journal and an ice-texture branch are visible in this routine.
These are raw decompiler observations, not yet verified callable signatures.
Trace4375b0/437230 and their data before selecting a replacement shape.

PC settings tests cover installed levels, truncated prefixes and overlong
name rollback. Five live destruction/traversal cases pass. The500frame
Xbox approach at artifacts/xemu/render-20260915-084302 passes38comparisons
with9091pages free; native image inspected with authored dark rock interior.
All19staging entries restored; owned emulator exited. This corrects material
selection, not crater fidelity, UV parity or interior lighting.

## Original crater factory verification (2026-09-15)

GeoMod remains the highest priority; the current visible icosahedron is a
prototype. No gameplay rendering change is claimed by this investigation.

For the RF.exe SHA above,4374c0 initializes four named templates through
437500;4375b0 retrieves them. Factory4e6d60 builds Holey01.v3d directly
from embedded constants:10vertices,16triangular faces and48corner UV pairs.
This is not an absent V3D archive entry. The other factory branches name
bit_driller_double, bit_driller_single and Holey_APC; they remain unverified.

Run python tools/inspect_geomod_template.py after exporting4e6d60 with
tools/ghidra/ExportSelected.java. It executes the original factory through
4cf9a0/4cf500 bounds computation in Unicorn, stopping at4e76f8 before the
auxiliary solid construction. Allocation and mesh container methods are
explicit capture boundaries; vector math and bounds execute original bytes.
It verifies bit-exact agreement against tools/extract_geomod_template.py
for all submitted vertices, face indices and UVs. Generated data remains
under ignored artifacts, with original executable/export hashes.

The closed inward-wound mesh has24paired edges and signed volume
-0.011949739812882259. It is NONCONVEX: maximum wrong-side vertex distance
is0.019375374254069707, well beyond rounding tolerance. Original bounds
calculation yields radius0.2196311503648758 at solid+0x60 and center
(0.005076570902019739,0.009751406498253345,-0.04012307897210121).
The harness confirms finite bounds and enclosure of every submitted vertex.

Our current convex-only terrain union cannot directly consume this shape.
Next: support concave cutters transactionally, validate original transform
and scale rules, preserve UV corners, and verify repeated-cut surface and
collision agreement on PC and stock64MiB Xbox. Do not replace this mesh
with its convex hull and call it original parity. Final factory auxiliary
processing, live visual reference, debris and lighting remain unverified.

Correction to earlier lead:437230 owns a bounded effect list and emitter
handles; it is not established as the geometry-subtraction worker.466b00
copies a descriptor into cut globals and selects texture mapping. Raw
45cff0 indicates maximum matching region hardness, default646004 when no
region matches, refusal at hardness100, and clamped1-hardness*0.01 scaling.
These region/transform observations still require execution verification
before integration. The level loader's zero-to55 fallback is documented
above; the live prototype does not yet apply those rules.

## Shared concave subtraction (2026-09-15)

rf_geomod_storage_prepare_star_cuts accepts up to8 ordered closed triangular
cutters, up to32faces each, with a strict interior kernel visible from every
face. It supports either convex solid subtraction or empty-room expansion.
Each cutter is partitioned into tetrahedra for exclusion; only input exterior
faces are emitted, with their UVs. No internal tetrahedron caps are rendered.
The caller must supply a non-self-intersecting surface. Validation checks
finite data, triangle bounds, opposite edge pairing, outward winding and
strict kernel containment. This is a practical CSG implementation, not a
claim to have reconstructed the original Boolean worker.

Mesh bounds and separating-plane rejection avoid needlessly partitioning
distant polygons. Both existing convex and new star paths share those checks.
The full-history/pending-bank contract remains: failures preserve live data.
Dependent collision construction and commit remain caller responsibilities.
Scratch grows from259072 to275488bytes; no cutting-time allocation is added.

Validation commands:
- python tools/inspect_geomod_template.py
- build/pc/Release/rf_geomod_interior_tests.exe Installed_Game artifacts/geomod-holey01-csg.bin

The original-machine-code harness exports a local, ignored triangle fixture
at10x scale with reversed outward corner order. The C tests cover solid and
cavity modes, one cut and two cuts overlapping by an x shift of0.4. They
check edge closure, single-cut solid volume and324 rays against independent
input-triangle intersections. Results after bounds rejection:
- Solid: one cut72vertices/22faces; two cuts642vertices/170faces.
- Cavity: one cut224vertices/60faces; two cuts593vertices/155faces.

An independently constructed dented cube distinguishes this from a convex
hull: removed volume7 rather than8, and center roof height0.25 rather than1.
It exercises solid/cavity and duplicate cuts,324 analytic collision rays,
interpolated affine UVs, geometric closure, invalid kernel/open edge/reversed
face rejection and output-capacity rollback. Existing convex fixtures pass.

The original template is not yet loaded into live rocket resources. Resource
loading, original basis/scale/region rules and new live visuals remain open.
The two-cut fragmentation counts also need improvement before larger live
histories can be accepted within the current512face limit. These tests prove
concave geometry handling, not original-game visual parity.

Final verification: PC GeoMod tests and five live destruction replays pass.
NXDK build succeeds;500frame XEMU approach at
artifacts/xemu/render-20260915-090812 passes38comparisons with9102pages
free (35.555MiB). Native framebuffer inspected: existing prototype crater
remains visually unaccepted. All19disc entries restored. Concave star
geometry is CPU-test verified; it is not yet the live rocket cutter.

## Owned concave history and original default orientation (2026-09-15)

rf_geomod_terrain_cut_star copies at most20triangles/60corners, the strict
world-space kernel and per-face materials into the retained terrain owner.
Mixed histories retain convex handling for boxes/prototype craters and use
star planes only for explicit star entries. Pending-slot type is overwritten
when a convex cut reuses it; reset does not preserve a stale star type.
Publication rebuilds the matching collision tree before switching live banks.
It adds100bytes per terrain owner; the star scratch budget is unchanged.

The original-template tests exercise solid and cavity owners with one/two
overlapping cuts. Published meshes match the previously verified pending
CSG output byte-for-byte, and324 extra rays through the owned collision
tree match the source triangles. Invalid kernels preserve generation/history;
a mixed box cut, reset and reused convex slot pass. Fixed-budget allocation rollback
fixtures from the previous owner also continue to pass.

Original4fccc0 calls4fad60 (sphere direction) followed by4fcfa0 (basis).
The already reconstructed rf_particle_cone_sample with cosine_min=-1 matches
the first stage, consuming two CRT draws. rf_geomod_random_basis reproduces
the basis stage, including the near-vertical branch. tools/inspect_geomod_basis.py
executes original4fccc0 with only57312d CRT draws supplied by the fixture.
All69 tested seeds match all nine shared float words and the final RNG state;
the text roundtrip error reported separately is not a matrix mismatch.
The near-vertical branch is exercised
by two specifically solved CRT seeds in addition to67 ordinary cases. This is the default branch in467020 when flag4 is clear.
The flag4 path can use4fce70 identity or copy an object basis. Selection for
each live weapon and the original RNG stream/seed still require verification.

Reproduce with rf_geomod_basis_probe plus python tools/inspect_geomod_basis.py,
and the original-template C test command above. Generated evidence stays in
artifacts/geomod-basis-original.json. PC GeoMod suites, original-template
owner tests and five live prototype destruction replays pass; NXDK builds.
The new shape and random orientation are not yet selected by live rockets.

Owner regression verification:500frame stock64MiB XEMU run
artifacts/xemu/render-20260915-091844 passes38comparisons with9102pages
free. Native framebuffer inspected; this remains the existing prototype
visual. All19disc entries restored and the owned emulator exited.

Next call-site evidence: raw x86 at4c5323 and4c6c12 pushes template ID0
and flags0 into467020, with radius read from class+0x14c. These are leads
for validating projectile dispatch before choosing the live orientation
policy. Full containing functions4c4ec0/4c69a0 remain to be reviewed.
References are reproducible with ExportReferences.java at467020.

## Live original-template asset (2026-09-15)

Prepare from the verified local factory evidence:
- python tools/inspect_geomod_template.py
- python tools/pack_geomod_template.py

The first command requires the4e6d60 ExportSelected.java output documented
above. The second rechecks the executable SHA and writes the988byte RFCT
version1 asset to ignored build/data/geomod-template.bin. This contains
the original radius, strict kernel and reversed outward position/UV corners.
No source geometry or original executable is added to tracked source.
The PC DEV scene reads that path from the repository working directory;
build-xbox.sh stages the same file on disc, where the scene reads D:\geomod-template.bin.
The standalone runtime does not load or execute RF.exe.

The bounded C loader requires an exact-size, finite, closed star template.
It preserves output on malformed input and rejects oversized files. Tests
cover every truncated prefix, bad magic, extra bytes, nonfinite radius and
word-equivalent decoded position/UV data. Template transforms reject invalid
bases/nonfinite centers without changing terrain generation. The positive
case exercises a90degree basis, translation and scaling through the owner.

DEV rockets now select this template, preserve its corner UVs, use the
verified default random-basis routine and radius/template-radius scaling.
The DEV random stream is explicitly seeded1; this is not the original
global RNG stream. The complete original transform operation order and
UV-remapping policy remain to verify. Region/default hardness is still
not applied, so this is not a claim of final crater size or retail parity.

All five PC destruction replays pass. Walk endpoint is
(-18.173584,-12.416787,6.002432), beyond the original x=-16 wall, with
two successful cuts and724697bytes peak terrain+overlay. PC images inspected
at artifacts/destruction/original-template-approach.png and
original-template-walk.png. Shape changes are visible and traversal remains
possible, but the exterior view still reads poorly as a cavity. Depth cues,
interior lighting, texture mapping and debris are the next visual work.

Native original-template verification: First attempt092603 restarted during
loading. Moving the1540byte template from the scene stack to a heap owner
and validating planes one face at a time reduced stack use. scene_miner
falls from27948 to26412bytes; template_decode from3628 to1584bytes.
The500frame rerun artifacts/xemu/render-20260915-092952 passes38comparisons
with9086pages free (35.492MiB). Native framebuffer inspected: the original
shape is visible, but cavity readability remains unfinished. PC/disc asset
bytes match; all19staging entries restored; owned emulator exited.

## Original mode4 texture projection (2026-09-15)

466b00 configures4f8720(4),4f8730(32.0) and all three bitmap slots from
the level substrate. These setters target5a3e9c/5a3eac and5a3ea0/a4/a8.
4f8740 mode4 overwrites face-corner UVs, so retaining the factory UVs in
world-space craters was incorrect. 4fa6d0 selects the dominant normal axis:
Y wins an X/Y tie, Z wins ties against that winner. Positive-axis U/V
pairs at5a3ee0 are (Z,Y),(X,Z),(Y,X); nonpositive normals swap the pair.
UV scales are32/bitmap-width and32/bitmap-height. Negative Y faces choose
the second bitmap slot, positive Y the third; the current GeoMod setup
assigns the same substrate to all three slots.

rf_geomod_planar_uv reproduces the mapping. tools/inspect_geomod_uv.py
executes original4f8740/4fa6d0 with only bitmap-dimension lookup510630
supplied, plus synthetic face/corner records. All126 UV pairs match shared
float words exactly across14 normals,3 dimensions and3 positions. Cases
include all axis signs, ties, negative world coordinates, non-power-of-two
dimensions and traversal of the original circular corner list.

The terrain owner applies this mapping only to generated interior faces
in the pending bank, after CSG and before collision publication. Original
level UVs survive. Scene configuration takes dimensions from the loaded
authored substrate. Tests verify unchanged geometry/face records, preserved
original-surface UVs, expected generated UVs, pre-cut-only configuration and
invalid-dimension rejection. The two stored dimensions add8owner bytes.
All PC GeoMod tests and five live destruction/traversal replays pass.

PC image artifacts/destruction/original-planar-uv.png was inspected: the
texture no longer follows the old stretched template UVs, but the cavity
remains visually ambiguous. The next concrete lighting lead is
src/core/preview.c generate_source: it uses0.25+0.6*abs(dot(normal,light))
for generated geometry, making opposite normals receive the same shading.
This is preview scaffolding, not original interior lighting. Replace it
with suitable scene lighting for terrain without changing unrelated weapon
projection. Original vertex transform arithmetic, hardness, surface
eligibility, debris and broader history bounds remain open.

Mode4 native verification:500frame run
artifacts/xemu/render-20260915-093901 passes38comparisons with9085pages
free (35.488MiB). Native framebuffer inspected with corrected texture
projection; cavity lighting remains unfinished. All19disc entries restored
and owned emulator exited. No new GitHub screenshot uploaded.

## Authored-light interior shading (2026-09-15)

The preview absolute-normal-dot hypothesis needed correction: both textured
backends ignored those vertex colors. PC overwrote them with texture times
neutral lightmap0.5; Xbox replaced textured-vertex RGB with white, then
halved it for the fixed2x lightmap combiner. Merely computing better colors
produced a byte-identical image. That failed visual experiment prompted the
explicit rendering fix rather than a claim based on updated draw data.

RF_PREVIEW_VERTEX_LIT uses a reserved lightmap tag for base texture times
vertex RGB. rf_preview_geomod_lit validates/copies explicit colors. PC
modulates the texture; Xbox preserves RGB and halves it to compensate for
the existing2x combiner with a white fallback lightmap. The legacy tagged
lightmap and unlit paths remain unchanged. A textured pixel fixture verifies
(200,160,80) times(0.2,0.4,0.6) becomes(40,64,48), checks invalid-color
rollback, and verifies the old path still produces(200,160,80).

The DEV terrain samples each generated face centroid against selected
enabled authored lights, using the shared rf_vfx_lighting evaluator with
level ambient and directional scale0.25. Existing textured room fragments
retain neutral modulation. A6144byte heap color buffer and existing light
selection scratch are reused; there is no per-frame allocation or I/O.
Lighting refreshes with each draw so changes in the light pool are visible.
The tested room has three authored lights. Five PC destruction replays and
the original-template/renderer tests pass. PC image
artifacts/destruction/authored-lighting.png was inspected; darker recesses
and differentiated surfaces now reach the framebuffer.

This is practical per-face scene-light sampling, not the original static
interior lightmap pipeline. Occlusion/shadows, finer sampling across large
faces, regenerated original-wall lightmaps, complete directional-global
policy and original interior lighting parity remain open. Per-face sampling
can also expose differences between coplanar fragments; improve this before
calling the final crater lighting complete.

Lit-interior native verification:500frame run
artifacts/xemu/render-20260915-094918 passes38comparisons with9085pages
free (35.488MiB). Actual Xbox framebuffer inspected: scene-light modulation
is visible on the crater, matching the PC appearance. All19disc entries
restored and owned emulator exited. No GitHub image changes.

## Authored hardness and ordinary regions (2026-09-15)

The shared hardness policy now follows original45cff0/45d520: sphere
membership excludes the radius boundary; oriented boxes include their
boundaries; maximum hardness among matching regions wins. With no match,
the stored level default applies, with zero converted to55 by4618b0.
Hardness100 refuses before changing scale. Other values multiply the
already normalized template scale by clamp(1-hardness*0.01,0,1).
Original52cac0 converts serialized forward/right/up to runtime right/up/forward.

tools/inspect_geomod_hardness.py executes the original ordinary-region
routine and its membership/vector helpers, supplying only the region-list
container. All45 cases agree on allowance, ice flag and exact float scale
bits: sphere/box boundaries, rotated box, overlapping regions in both
orders, no-match defaults, hardness0/25/55/99/100 and varied scales.
This does not establish shallow-region or ice geometry parity. The shared
API refuses matching shallow regions without modifying output; the live
DEV cutter also refuses ice until its special geometry behavior exists.

Glass House region128 is an ordinary hardness25 box of dimensions56x48x60,
covering the DEV room. Rocket cuts now use that region, reducing the
requested radius5 scale to75 percent. The region payload is loaded once
into bounded heap storage and released on scene cleanup. Random orientation
is generated only after hardness accepts the impact. Blast damage remains
independent of terrain refusal. Surface eligibility remains separate work.

PC region/rollback tests, original-template collision/interior tests, three
GeoMod CTest targets and all five ordinary destruction replays pass. The
walk endpoint is(-18.012171,-11.814328,6.090965): beyond the original x=-16
wall, with less floor removed. The replay retains its requirement to walk
over two units beyond that wall; its vertical range follows the shallower cut.
NXDK builds. Native500frame run artifacts/xemu/render-20260915-100341
passes38 existing gameplay comparisons, with9084pages free (35.484MiB).
These comparisons do not directly compare terrain vertex buffers.
All19disc entries restored and the owned emulator exited.

Both native and PC final frames were inspected. The smaller textured cut
is present, but its dark silhouette still reads like a lump rather than a
convincing recess. Visual fidelity is not accepted; crater depth/readability,
finer lighting, occlusion, debris and larger histories remain priorities.
No GitHub screenshots were added.

## Static-surface lighting policy correction (2026-09-15)

Crater lighting previously called the unsoftened VFX evaluator with full
global ambient and VFX RGB conversion. The retained ordinary static-lightmap
path uses half room/global ambient, softened point/cone accumulation, and
lightmap RGB conversion. DEV terrain now uses those same arithmetic stages
at each generated face centroid. Room0 ambient override is respected; the
result is doubled for the existing base-texture lightmap combiner convention.
It still has no shadow masks, texel grid, filtering or special-polygon path.
This is a correction toward surface policy, not a completed lightmap rebuild.

Temporary diagnostics (removed from runtime source) confirmed the selected
sources are two point lights at(0,-1.5,-12.5) and(0,-1.5,11.5), radius24.
Global ambient is40/255 and room0 has no override. This selected-source
measurement supersedes the earlier unqualified note saying three lights.
Generated back faces have centers behind the x=-16 wall, such as
(-18.1863,-10.5747,6.10964), with normal approximately(+.999,+.015,+.043).
They face into the opening; a blanket normal flip is not justified.
Diagnostic evidence remains in artifacts/geomod-face-probe.log and
artifacts/geomod-light-diagnosis.log.

All five PC destruction replays pass and NXDK builds. The original ordinary
lightmap oracle was rerun:512 PC/NXDK grids,10368 pixels, including softening
and mixed light types, agree with unhooked original4f3390 execution. PC
surface-light.png was inspected; the silhouette still reads ambiguously.
Do not treat this arithmetic correction as accepted visual fidelity.

The XEMU DEV harness now explicitly compares terrain presence, cut count,
generation, last status and publication counters, and enforces resident/peak
terrain memory within1MiB plus64KiB on both builds. Pointer-width-dependent
allocation totals are not required to match. This adds a publication check;
it does not claim to compare every generated vertex or prove appearance.

Native verification: artifacts/xemu/render-20260915-101047 passes39checks
including GEOMOD, with9084pages free. Both builds report two committed cuts,
generation3,699024resident/725843peak terrain bytes and no cut error. Native
framebuffer inspected: dark ambiguous silhouette remains. All19disc entries
restored; owned emulator exited. Original RGB conversion also passes8192
PC/NXDK cases. No new GitHub image.

## Corner lighting and close depth replays (2026-09-15)

tools/dev_destruction_depth.py now reproduces three595frame close/oblique
views after the same two rocket impacts. Each remains inside the original
room, alive, with two committed cuts and no publication error. All three
PC images were inspected sequentially. The side wall occludes more of the
cavity in the right-hand view; together with the previous geometric position
diagnostics this supports a recessed boundary, rather than a normal flip.
The rock remains visually too dark and ambiguous; this is not acceptance.

Terrain now evaluates retained static-surface lighting at each source corner,
using the owning face normal to preserve sharp rock creases. Preview clipping
interpolates RGB with the same rounded edge parameter as position/UV. Both
backends receive the resulting varying colors. Original-room fragments retain
neutral modulation. The heap color buffer grows from6144 to49152bytes, an
additional42KiB; it is freed with the scene and does not enlarge terrain CSG
storage. There is still no texel grid, shadow mask, or lightmap regeneration.

A clipped textured-quad test supplies an analytic linear color gradient:
the new vertices on the left frustum boundary retain red0.2, all projected
vertices follow the expected gradient, and the center pixel has the expected
texture modulation. Invalid corner RGB preserves prior draw data. Existing
face-color and legacy textured cases still pass. Original-template collision
tests, static-preview clipping, all five destruction replays and all three
depth replays pass. NXDK builds after fixing its stricter aggregate initializer
warning. The closer PC corner-lit image was inspected; overall readability
still requires work, especially interior contrast and occlusion.

Native close-view verification: artifacts/xemu/render-20260915-101726
completes595frames and39checks with9068pages free (35.422MiB). Xbox and
PC agree on two cuts, generation3, no publication error and bounded terrain
memory. Actual Xbox frame and final PC left/right views inspected; no visual
fidelity acceptance. All19disc entries restored and owned emulator exited.
No additional GitHub screenshots.

## Preserve lightmaps on surviving room fragments (2026-09-15)

The DEV terrain replacement previously discarded authored lightmap bindings
on all surviving fragments of the six original room faces. They sampled the
base texture at neutral brightness even before a cut. This exaggerated the
brightness difference around the dark crater and lost the room's lighting.

rf_preview_geomod_world_lit now resolves each surviving fragment's source_face
against the original geometry, retains its lightmap image, and projects each
new corner using the retained mapping and shared rf_lightmap_project routine.
The resulting coordinates pass through existing frustum and perspective
interpolation. Newly exposed faces remain vertex-lit. No lightmap asset copies
or new persistent allocation are needed, and original asset bytes stay intact.
This preserves surviving lighting; it does not regenerate shadows after cuts.

A synthetic clipped quad verifies image2 and analytically known lightmap UVs
at every output vertex. A colored lightmap then modulates the rendered center
pixel, proving the fragment takes the textured lightmap path rather than its
supplied corner colors. An invalid projection preserves the previous draw;
new rock still selects the vertex-light path. Existing original-template and
static-preview tests pass, as do all five destruction and three close/oblique
PC replays. NXDK builds. All three final PC depth images were inspected:
authored wall shading is restored, but crater depth/contrast remains unfinished.

Native verification: artifacts/xemu/render-20260915-102321 passes39checks
over595frames with9068pages free (35.422MiB). The actual Xbox framebuffer
was inspected and shows restored wall lightmaps, matching the PC view.
All19disc entries restored; owned emulator exited. No new GitHub images.

## Cached opaque-terrain shadows (2026-09-15)

The selected Glass House point lights have authored shadow mode1 (flags540,
bit4), so their crater contribution should not pass through enclosing rock.
rf_geomod_light_visible uses the current owned terrain tree for a bounded
light-to-corner segment. The final0.001 world unit is excluded to avoid
counting the receiving surface. This is a practical opaque-terrain query,
not the original projected shadow-mask raster. It intentionally treats the
DEV boundary as opaque and excludes other rooms, actors, movers and alpha.
Tests cover an unobstructed segment, a blocker before the sample, the surface
endpoint, and invalid-input output preservation.

For authored shadow-enabled point/cone sources, blocked corners receive
zero source weight; ambient remains. Source mode0 stays unmasked. Directional
and area-source shadows are not implemented by this pass. The existing
lightmap arithmetic evaluates the weighted light at each corner. Shadow
boundaries therefore interpolate across triangle fans and remain coarse;
soft shadows, texel-grid regeneration and final visual parity remain open.

A heap cache retains selected source values, shadow modes, ambient and terrain
generation. Exact byte comparisons detect changes, avoiding repeated ray work
while those inputs remain stable. A successful lighting update publishes the
cache after all corners finish; terrain publication invalidates via generation.
Dynamic-source change invalidation is implemented but not exercised by these
static-light replays. Geometry invalidation and cache reuse are exercised.

All five destruction and three depth PC replays pass, along with original
template/collision and rendered-lighting tests; NXDK builds. Close-view run
artifacts/xemu/render-20260915-103036 completes595frames and40checks with
9068pages free (35.422MiB). PC/Xbox both record2lighting rebuilds,1016shadow
rays,549blocked samples and426cached draws. Actual native and all three PC
views were inspected: occluded patches are darker, while overall appearance
remains unfinished. All19disc entries restored and owned emulator exited.
No GitHub screenshots added.

## Repeated excavation capacity (2026-09-15)

An800frame ordinary-input replay fires all six rockets at the opening. Before
this change, only four cuts published: the fifth/sixth hit the512face capacity.
The shared multi-cut emitter now attempts to join neighboring fragments only
when they share a reversed edge, a material/source-face identity, a common
plane and affine UV field, and form a convex union accepted by the collision
face validator. This preserves rock creases and UV seams. Merge scratch reuses
the existing heap workspace. Public raw storage append behavior is unchanged.

Compaction alone admits five live cuts. The DEV face allocation is now768
while vertices remain4096 and the total terrain budget stays1MiB. The dependent
collision overlay remains capped at64KiB. The six-shot PC replay now publishes
all six cuts with no rejection, resident809796/peak925604bytes including the
overlay. Existing destruction/movement and three close-view replays pass.
An independent ray-placed six-cut fixture also checks admission and increasing
signed cavity volume within1MiB. Existing smaller closed-edge and324 independent
ray fixtures continue to pass after compaction.

The expanded room-scale fixture DOES NOT pass strict closure. It reports four
balanced edge contributions where two are expected, first on a1.73568unit edge
at x=-16. The first cut also failed this check with compaction disabled, so this
is not evidence of a new merge-only defect. It remains unresolved whether thin
overlapping fragments or tolerance-level duplicate coverage cause the result.
The capacity fixture reports CLOSURE_DIAGNOSTIC rather than claiming closure
success; investigate and resolve it before accepting multi-cut geometry parity.

The first Xbox attempt reset after the initial impact. Its scene_miner compiler
frame was27484bytes. The enlarged face-ID array was moved to a3072byte heap
allocation, released during scene cleanup, to recover stack headroom. That
reset is not itself proof of stack overflow; subsequent native validation is
required before treating the larger cut history as working on Xbox.

Native follow-up: moving face IDs to heap reduces scene_miner from27484 to
24412compiler-frame bytes. artifacts/xemu/render-20260915-104714 completes
800frames and40checks with9024pages free (35.25MiB). All six rockets publish
cuts, generation7, no rejection; PC/Xbox report809796resident/925604peak
terrain bytes and matching shadow counters. Actual Xbox frame inspected:
the opening is visible, but the distant view does not prove edge fidelity.
The prior reset run104342 timed out, restored its disc and exited before
this run began. Follow-up also restored all19disc entries and exited its
owned emulator. No GitHub images added. Duplicate edge coverage remains the
next geometry issue; do not equate six accepted cuts with closed topology.

## Closure interval diagnosis (2026-09-15)

The checker now prints the failed subinterval, its midpoint and every matched
edge (up to eight), including distance from the sample and projected position.
This distinguishes a full overlapping edge from a tiny ambiguous junction.
The room-scale six-cut fixture's first failure is the same1.34534375358e-6
unit interval for every cut. Two exact opposing edges end at z2.93492723;
the nearby continuations end at z2.93492866 and2.93492818. At the sample,
their perpendicular errors are4.86224e-7 and3.24149e-7, inside the checker's
1e-6 positional tolerance. Thus four edges are counted in a tiny junction
interval, not along the full1.73568unit edge. This explains this particular
failure but does not establish that all other boundaries are valid.

A discarded experiment reconciled pending positions within the cutter's1e-5
plane tolerance and removed collapsed consecutive corners. It made the first
room-scale closure test pass, but later edits failed collision validation and
the ordinary six-shot replay regressed to four accepted cuts. It was removed
in full. A future repair must preserve each incident face's plane constraints;
blanket proximity welding is not sufficient. The closure threshold and the
working production geometry have not been relaxed or replaced.

Retained evidence: artifacts/geomod-closure-diagnostic.log contains detailed
intervals; the test command remains rf_geomod_interior_tests Installed_Game
artifacts/geomod-holey01-csg.bin. Existing enforced geometry/pixel tests pass;
room-scale closure remains explicitly diagnostic and unresolved. After removing
the experiment, the ordinary PC six-shot replay again accepts all six with
resident809796/peak925604bytes. No production code change, new Xbox run or new
visual fidelity claim is made by this diagnostic update.

## Consistent shared tetrahedron planes (2026-09-15)

Internal planes in the star-cutter decomposition were independently computed
from different anchor vertices on each incident tetrahedron. Even with a
shared geometric triangle, normal/offset rounding could differ, so the two
clipping half-spaces were not exact opposites. Internal triangles now sort
their three positions lexicographically before computing the plane, retaining
permutation parity for outward orientation. Both sides therefore use the same
arithmetic and anchor, with opposite signs. External template faces and their
input positions/UVs are unchanged; no proximity welding is performed.

The original-template solid/cavity fixtures now enumerate every shared edge
and require all four internal plane coefficients to be exact negatives of
their neighbor. Running the new check with the old arithmetic fails at that
assertion; restoring the consistent computation passes. The same tests retain
existing smaller closed-edge, volume, independent-ray and rollback checks.
All six ordinary destruction replays and three close-view replays pass.
The live six-shot PC result remains six accepted cuts with no rejection,
resident809304/peak924389bytes including the bounded overlay. NXDK builds.

This fixes a demonstrated shared-plane inconsistency but does not resolve the
reported room-scale1.345e-6 junction interval. Its strict closure diagnostic
still fails and remains open. Subsequent intersection rounding/order must be
investigated separately; do not claim closed topology or final visual parity.

Native verification: artifacts/xemu/render-20260915-105731 completes800frames
and40checks with9024pages free (35.25MiB). PC/Xbox both publish six cuts,
generation7, no rejection and identical reported terrain allocations. The
actual native framebuffer was inspected; its distant opening remains visible
and is not proof of microscopic closure. All19disc entries restored; owned
emulator exited. No additional GitHub screenshots.

## Direction-independent edge intersections (2026-09-15)

The polygon splitter interpolated intersections in each polygon's traversal
direction. For an edge from(-16,-1,0) to(32,2,0), clipped against y=0,
reversing that edge changes cancellation rounding and can yield different
position/UV words. A targeted regression fails with the prior implementation.
Crossing edges now select their lexicographically earlier endpoint before
computing the fraction and interpolating both position and corner UVs. The
polygon emission order/winding is unchanged. Texture seams remain independent:
different UVs on a neighboring polygon still interpolate that polygon's UVs.

The regression passes, as do360 rotated square cuts with exact matching
front/back vertex sets under reversed winding, area conservation, half-space
checks, invalid-input rollback and a distinct-UV seam case. Existing interior,
volume/ray and six-cut admission tests pass, plus six ordinary destruction and
three close-view replays. NXDK builds. No persistent memory is added.

This establishes direction-independent evaluation for the same input edge and
plane, not agreement after different sequences of clipping operations. The
room-scale near-coincident junction closure diagnostic still fails. That
remaining mismatch is not hidden by this change or by a relaxed test threshold.

Native verification: artifacts/xemu/render-20260915-110230 completes800frames
and40checks with9024pages free (35.25MiB); six cuts publish without rejection.
Decoded framebuffer pixels exactly match the previously inspected105731 frame,
so no new image was posted. All19disc entries restored and owned emulator
exited. The room-scale closure issue remains unresolved.

## Junction collision reproducer (2026-09-15)

The six-cut room-scale fixture now probes a65x65 grid around the previously
reported near-coincident junction at(-16,-7.36841655756,2.93492785774).
Segments start at x=-15 and travel15 units toward negative X. Grid spacing
is one float ULP at each local coordinate:2^-21 in Y and2^-22 in Z.
Every segment should encounter the room wall or the closed crater boundary.

Cut1 blocks all4225 rays. Cuts2 through6 each miss35 rays. Radius0.5 sphere
sweeps at those175 missed-ray samples all hit terrain. This demonstrates a
thin-ray collision failure in the local fixture, but does not establish
body escape, its exact geometric cause, or visible cracks. The correlation
with the closure diagnostic does not prove that the same junction causes
both failures. No geometry or collision tolerance was changed.

Run rf_geomod_interior_tests.exe Installed_Game artifacts/geomod-holey01-csg.bin
from the project root. Setting RF_GEOMOD_STRICT_JUNCTION=1 asserts zero ray
misses after collecting all six cuts and currently exits1. Normal runs retain
the clearly reported diagnostic and assert the tested sphere sweeps remain
blocked. Evidence:artifacts/geomod-junction-rays.log and
artifacts/geomod-junction-strict.log. The four targeted CTest checks pass;
the opt-in zero-miss regression intentionally fails until the defect is fixed.
This test-only change has no new Xbox build or visual acceptance claim.

## Junction rejection isolation (2026-09-15)

An independent double-precision triangle-fan reference now intersects the
actual emitted mesh for every missed local ray:35 of35 on each of cuts2-6.
It projects triangles into Y/Z, solves barycentric coordinates without an
edge tolerance, reconstructs X, and checks the finite ray segment. Direct
production face queries, bypassing the tree, still find zero hits for those
rays. The test asserts that every reported miss has a reference intersection.

The first missed ray starts at(-15,-7.36841726,2.93492746). Mesh face59 passes
the production bounding-box and plane tests, returning fraction0.0666667297;
its production polygon-containment test returns false. Its normal is
(0.216526687,-0.976276577,-0.000501434202). This narrows the investigation to
plane/intersection rounding and polygon evaluation, rather than tree culling.
It does not prove watertight topology or that all other ray directions work.

An experiment accumulating the generated plane offset in double precision
left the same35 misses and was discarded. No runtime code or tolerance change
is retained. Evidence:artifacts/geomod-junction-reference.log and
artifacts/geomod-plane-offset-probe.log. Fix the collision evaluation using
the emitted surface as the reference, preserving original-face compatibility;
do not enlarge holes or accept arbitrary nearby polygons to hide these misses.

## Generated-surface ray intersection fix (2026-09-15)

Generated terrain collision faces now opt into triangle-fan intersection,
matching their emitted/rendered vertices. The ray's dominant axis defines a
sheared two-dimensional projection; double-precision edge weights establish
triangle containment before the contact is rounded to floats. No proximity
threshold is added. Existing front-facing, finite segment, nearest-hit limit,
face filter and bounding-box policies remain. Original level faces initialize
the new mode to zero and retain their existing plane/polygon query path.
This is a practical port implementation, not a claim of original algorithm parity.

The former175 missed rays now hit:4225 local probes after each of six cuts,
25350 total. Zero misses is now an unconditional assertion in the extended
fixture; RF_GEOMOD_STRICT_JUNCTION is no longer needed. Three-axis tests cover
interior hits, one-ULP inside/outside edges, nearest-hit limits, reverse-side
rejection, coplanar rejection and oblique directions. The independent triangle
reference and original closure diagnostic remain. Strict geometric closure
still fails separately; fixing collision does not establish watertight topology.

All six ordinary destruction and three close-view PC replays pass. The live
six-shot PC terrain reports resident817644/peak936701 bytes, including the
bounded overlay. The new face-mode word is included in sizeof-based allocation
accounting; no mesh storage budget was enlarged. NXDK builds. Full PC CTest
is53/54: npc_motion_residency fails at1179/1305 on both unchanged69595922 and
this change (baseline rebuilt and run). Five focused collision/terrain/preview
checks and the extended six-cut fixture pass after restoring the final code.
Evidence:artifacts/geomod-triangle-probe.log, geomod-triangle-replays.log,
geomod-triangle-ctest.log and geomod-npc-baseline-test.log.

Native verification:artifacts/xemu/render-20260915-111742 completes800frames
and40 comparisons, with six accepted cuts and9023pages free (35.246MiB).
PC/Xbox terrain and shadow counters agree. Decoded framebuffer pixels exactly
match the previously inspected110230 capture; no new image was posted.
All19disc entries match their saved state and owned PID39472 exited.

## Room-wide repeated-cut collision coverage (2026-09-15)

The six-cut fixture now casts1734 rays per cut from(0,-10,12), using17x17
staggered targets toward each of six room sides. An independent double-precision
plane/barycentric triangle reference computes the nearest front-facing contact;
the production implementation uses ray-space edge projection instead. All10404
room-wide nearest-hit queries match within1e-5 of segment fraction. The same
10404 queries limited to half the expected hit fraction correctly report no hit.
The existing25350 junction probes also pass. This covers varied oblique rays
and all room sides, not arbitrary camera positions or all destructible levels.
The mesh-closure diagnostic remains unresolved; no visual acceptance is claimed.

CMake now registers geomod_repeated_cut_coverage with the project root as its
working directory. It runs the previously manual original-template and six-cut
fixtures, including unconditional zero junction misses. Its original inputs
must be generated with inspect_geomod_template.py and pack_geomod_template.py;
missing fixtures fail explicitly, as they do for the Xbox build. No copyrighted
fixture is added to Git. Evidence:artifacts/geomod-coverage.log. Runtime code is
unchanged in this follow-up, so no duplicate Xbox run or screenshot is needed.

## Interior lighting sampling diagnosis (2026-09-15)

The close PC capture was inspected again. The cavity is visible but remains
dark and faceted; it is not an accepted appearance milestone. A temporary
probe evaluated the arithmetic mean of each generated face's vertices with
the same authored lights, face normal, ambient, terrain occlusion and RGB
conversion used for corner samples. It compared that center lighting with
the mean of the corner colors, without changing rendered colors or geometry.

At generation3 (two cuts),92 generated faces were sampled. For the red channel,
13 face centers exceed their mean corner value by more than0.01;29 are lower
by more than0.01. Face84 has corner mean0.2490 and sampled center0.3843;
face80 has0.3712 versus0.4863. These values identify under-resolved surface
lighting, not a uniform brightness offset. The centroid is inside each convex
fragment. A finer surface-lighting representation must retain occlusion and
sharp normal changes, and must be checked against actual rendered results.

Evidence:artifacts/geomod-light-centers.log; the probe was temporary and removed.
No runtime change, visual improvement, or original-game appearance parity is
claimed. The captured probe output is pixel-identical to the prior close view.
Full lightmap generation remains the intended fidelity direction; a center
sample alone would still be an approximation, not proof of correct shadows.

## Bounded crater surface-lightmap grids (2026-09-15)

rf_geomod_light_grid_open/sample/uv provide the first part of generated
surface lightmaps. Each convex planar face receives a dominant-axis projection
and power-of-two grid from4x4 to64x64, including a border texel. Requested
spacing bounds the distance between interior grid samples. Oversized grids
fail explicitly; no hidden reduction in requested density. UV extrema map to
inner texel centers, retaining a border for filtered sampling. Grid samples
outside the polygon footprint clamp to its nearest projected edge, then return
to the receiving plane, avoiding lighting samples in neighboring solid terrain.
This is a practical port layout, not an original lightmap-packer reconstruction.

The implementation allocates nothing; grid/image/atlas ownership remains with
the caller. Layout failures preserve output. Tests cover all three principal
plane axes, UV/sample correspondence, interior sampling, out-of-footprint
clamping, insufficient extent limits, nonfinite spacing and invalid coordinates.
The six-cut original-template fixture samples every generated face at0.5-unit
spacing and checks each point against its plane and every convex edge. Texel
counts after cuts1-6 are2160,5824,10240,14976,18480,22784. The last represents
45568bytes of1555 pixel data before atlas padding, metadata or staging buffers.
Both GeoMod interior/repeated-cut CTests pass; NXDK builds.

Evidence:artifacts/geomod-light-grid.log and geomod-light-grid-xbox-build.log.
The grids are not yet used by the scene renderer: lighting evaluation, bounded
atlas ownership, upload and mapped drawing remain to be integrated. No new
visual result or XEMU runtime verification is claimed for this foundation.

## Crater lightmap pixel evaluation (2026-09-15)

rf_geomod_light_grid_bake evaluates every bounded grid sample with the existing
ordinary softened static-light accumulation, accumulated-RGB conversion and
1555 packing for MODULATE2X. Caller-owned linear staging buffers support an
even padded row pitch; no image allocation or GPU upload occurs here. Point
and cone lights can use the retained terrain collision tree for occlusion.
Unsupported enabled shadow types return RF_NOT_FOUND explicitly. The caller
provides selected lights, ambient and directional scale; source selection and
cache invalidation remain scene responsibilities. Stats commit on success as
texels/rays/blocked; numeric failures may leave completed staging pixels.

Tests verify exact constant1555 output (0x9df7 for ambient0.25/0.5/0.75),
row-padding preservation, insufficient-buffer rejection, and interior lighting
from a point source whose radius excludes the face corners. A separate opaque
occluder blocks the face center while other pixels retain their unshadowed
values; all256 shadow rays are counted and only a subset is blocked. Unsupported
shadow modes and missing terrain leave the staging output untouched. Both
GeoMod interior and repeated-cut CTests pass. NXDK builds the new function.

The shared pixel evaluator is ready for scene atlas ownership and mapped
rendering; it is not yet connected to live crater drawing. No visual improvement
or new XEMU runtime validation is claimed. Evidence:artifacts/geomod-light-bake-
build.log and geomod-light-bake-xbox-build.log. Existing lighting/closure/final
appearance limitations remain open.

## Generated lightmaps in the shared draw path (2026-09-15)

rf_preview_geomod_lightmapped accepts a per-face projection and final lightmap
image index for newly exposed terrain. It emits the existing perspective
lightmap coordinates and image tag used by PC/Xbox, including frustum clipping.
Original surviving faces retain authored mappings; an unbound generated face
can retain vertex lighting. Existing entry points keep their previous behavior.
Bindings validate axes, finite transforms and reserved image tags before either
output pass. Image ownership/index bounds remain the caller's responsibility.

The pixel regression connects the actual grid baker to this draw path: a white
surface samples the right half of a32x16 packed1555 atlas. The neighboring half
is red to expose incorrect atlas addressing. The visible center remains neutral
and darkens when the occluded lightmap replaces the unshadowed data. Projected
coordinates stay in the correct atlas half. Invalid bindings preserve output
vertices/counts. Existing static clipping and both GeoMod interior/repeated-cut
CTests pass. NXDK builds the shared path. Evidence:artifacts/geomod-mapped-
pixels.log and geomod-mapped-xbox-build.log.

This verifies generated lightmap drawing in the rendered-pixel fixture, not
live DEV-room integration. The scene still needs bounded persistent atlas
ownership, stable image registration before Xbox texture setup, generation/light
invalidation and pixel upload. No new in-game appearance or XEMU runtime claim.

## Live crater lightmap atlas (2026-09-15)

The DEV scene now owns a512x512 packed1555 atlas, a same-size linear staging
buffer, one64x64 tile scratch buffer and768 projection bindings. A separate
TERRAIN_ATLAS diagnostic accounts for1078292bytes (image record included),
limited to1280KiB; geometry retains its previous independent budget. The atlas
image is registered once in the shared lightmap table before Xbox texture
setup. Image ownership transfers to that table; scene cleanup frees staging,
tile and binding memory, and frees unregistered images on partial-open failure.

On terrain/light-cache invalidation, generated faces receive0.5-unit sample
grids, point/cone terrain shadows and packed pixels. Shelf packing refuses
atlas overflow instead of silently reducing density. Authored surviving faces
keep their saved lightmaps. Drawing uses the generated projection bindings.
CPU staging is copied into native image storage only during the lightmap-update
phase; Xbox waits for GPU completion there before modifying swizzled pixels.
Unchanged frames reuse the atlas and existing lighting cache. Reset terrain
has no generated draw; subsequent cuts invalidate the generation cache.

All six ordinary PC destruction replays and three close-view replays pass.
The live six-cut atlas contains22928 sampled texels across490 generated faces;
geometry remains resident817644/peak936701bytes. TERRAIN_SHADOWS now measures
per-texel sampling (147680 cumulative rays,117635 blocked over six rebuilds),
so the two-cut depth replay bound changes from10000 to100000 rays. Both replay
tools additionally enforce the separate atlas budget. The XEMU harness adds a
TERRAIN_ATLAS comparison for dimensions, ownership, generation, texels, face
count and image index; counters are not a claim of pixel identity.

The close image changes51814pixels within the crater bounds(91,65)-(369,388).
Close, left and right outputs were inspected. The cavity remains dark and
angular; this is live per-texel lighting, not accepted original-game appearance
parity. No additional GitHub screenshots. Evidence:artifacts/geomod-live-atlas-
close.log, geomod-live-atlas-replays.log and artifacts/destruction/depth outputs.

Native verification:artifacts/xemu/render-20260915-114309 completes800frames
and41 comparisons with8748pages free (34.171875MiB). Atlas allocation,
generation, texel/face counts and shadow counters match PC. The actual native
framebuffer was inspected and shows the crater; the distant capture does not
prove close-view fidelity. All19disc entries restored; owned PID39568 exited.

Performance limitation:the native world-geometry rebuild phase peaks at7839ms
(scene camera/visibility/world phase8624ms). The aggregate counter does not
isolate every operation, but synchronous full-atlas ray baking is now a major
new workload. The current implementation is not gameplay-ready; prioritize
bounded incremental baking or faster occlusion evaluation before acceptance.
Do not interpret the800-frame pass as smooth frame pacing.

## Rejected shadow-query validation optimization (2026-09-15)

A temporary owned-tree query skipped the full node-validation scan for each
shadow ray while retaining the same traversal and leaf tests. Nearest-hit,
short-segment and shadow-flag results matched the original query throughout
the six-cut room fixture. All PC replay captures were byte-identical. XEMU
render-20260915-114725 passed41 checks over800frames with8748pages free;
its decoded framebuffer matches the inspected114309 capture exactly. All19
disc entries restored; owned PID31044 exited.

It did not produce the required performance improvement. World rebuild maximum
was7886ms versus7839ms before; total world rebuild time was27290ms versus24677ms.
These runs have host/emulator timing variability, so this is not proof of a
regression, but it provides no evidence of a useful gain. The experiment and
its API/tests were removed; the validated original path is restored. Existing
pre-atlas111742 evidence had an844ms world-rebuild maximum (6748ms total),
confirming that the full synchronous pixel bake introduced the much larger
workload. Next step:bounded incremental baking/upload rather than smaller
validation changes. Final settling time and interruption by subsequent cuts
must be measured separately from per-frame budget.

Evidence:artifacts/geomod-owned-ray-native.log, geomod-owned-ray-replays.log,
geomod-owned-ray-before-images.json and the cited native performance.json files.

## Incremental crater lightmap baking (2026-09-15)

The shared baker now accepts a row-major sample interval while retaining full
tile addressing and pitch. The original full-grid entry point remains. Tests
split a tile across13-sample chunks, verify identical packed bytes/padding, and
reject an overflowing interval without touching staging. The scene keeps a
face/sample cursor and processes at most64 texels per draw update. Completed
face tiles publish to the atlas; unfinished tile scratch is never uploaded.
New terrain/light generations discard unfinished work, initialize the new atlas
to packed ambient and restart its layout. Completed frames reuse existing pixels.

TERRAIN_BAKE reports processed texels for the current generation, current/peak
update work, active state, last completed generation and canceled generations.
Per-face grid/placement storage raises separate atlas ownership to1124372bytes,
still under1280KiB. The limit bounds sample count rather than elapsed time;
source count and tree complexity can change the cost of each sample. Atlas
upload currently still copies the full image when tiles publish and remains
an optimization opportunity. Reset telemetry/rapid light changes need separate
coverage; no worker thread runs after the scene stops drawing terrain.

Six standard destruction replays and three close views pass; depth captures
are byte-identical to the synchronous bake. A new1200-frame settled replay
verifies six cuts, interrupted generations, all22928 final texels completed,
active0/completed generation7 and peak64 samples. The1100-frame trial remained
active with22464 texels, demonstrating why a separate settling check is needed.
The settled world/crater image region above y350 matches the synchronous
six-shot capture exactly; later-frame weapon animation differs below it.
Both interior/repeated-cut CTests pass. Evidence:artifacts/geomod-incremental-
replays.log, geomod-incremental-settled.log and artifacts/destruction/settled.log.

Native verification:artifacts/xemu/render-20260915-115520 completes1200frames
and42 comparisons with8748pages free (34.171875MiB). Both targets report
TERRAIN_BAKE [22928,0,64,0,7,3], proving final completion after three canceled
generations. World-geometry rebuild maximum falls from7839ms to84ms; the
scene camera/visibility/world phase falls from8624ms to816ms. Its separate
camera/combat subphase still reaches771ms and remains open. Different run
lengths make average-time comparisons less direct; neither peak comparison
proves every frame is smooth. The actual native capture was inspected and
shows the crater and weapon. All19disc entries restored; owned PID40300 exited.

## Spatial rejection before fragment joining (2026-09-15)

The compactor caches exact coordinate bounds for pending faces when the owner
capacity is at most768faces. Before looking for reversed shared edges, it
rejects pairs separated by more than the existing1e-6 edge-match tolerance.
A valid shared edge cannot be rejected by this test. Bounds move with removed
face records and refresh after a successful union; every edit starts from an
empty pending bank. Larger owners retain the uncached path. Edge comparison
also stops as soon as a coordinate disproves a match. Coplanarity, convexity,
UV-affinity checks and merge order remain unchanged.

The cache adds18432bytes to existing caller-owned work storage. Live terrain
resident/peak allocations become836076/955133bytes, still within the existing
geometry budget. A paired six-cut fixture runs768face cached and769face
uncached owners using the exact same cuts, requiring byte-identical vertices,
UVs and face records after each edit. Both GeoMod interior/repeated-cut tests
pass. Six ordinary, three close-view and the1200-frame settling PC replays pass;
all previously captured replay images remain byte-identical. Evidence:
artifacts/geomod-bounds-replays.log and geomod-bounds-before-images.json.

Native verification:artifacts/xemu/render-20260915-120115 completes1200frames
and42 comparisons with8743pages free (34.152MiB). Geometry counts/publication,
lightmap progress and counters agree with PC. The camera/combat phase peaks
at175ms versus771ms in the preceding1200-frame run; the enclosing scene phase
peaks at213ms versus816ms. World rebuild peaks at121ms versus84ms, with the
same64-texel bound; timings vary and this is not a universal frame-time claim.
The native framebuffer is pixel-identical to the inspected115520 capture.
All19disc entries restored; owned PID44584 exited. Edits remain visibly hitchy
at these peaks, so latency/settling improvements remain open.
