# GeoMod implementation status

GeoMod fidelity is the highest current priority. The developer room has live
rocket-driven faceted excavation and matching collision, but crater shape,
interior lighting, debris and eligibility remain prototypes. Later sections
record current changes; earlier milestones below are historical evidence.

## Current acceptance priority

GeoMod appearance and behavior remain blocking work before returning to other
gameplay systems. General deferred-polish guidance does not apply to crater
shape, exposed rock, interior lighting or debris. Match original-game evidence
and inspect visible results; rendering a hole or passing collision probes alone
does not establish fidelity. Stock64MiB Xbox limits continue to apply.

The rocket-impact path now spawns bounded debris after successful terrain cuts.
Chunk construction and launch have original-code evidence; the live motion and
lifecycle remain provisional. The dark folded-panel cavity is still an
independent acceptance issue. See the latest integration section below.

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

## Dirty-region crater atlas uploads (2026-09-15)

Completed face tiles now union their pixel rectangles into a pending dirty
region. The lightmap-update phase copies only that region into the native image,
then clears the pending flag. Generation initialization still marks the full
atlas dirty, so newly packed layouts cannot retain stale pixels. Multiple tiles
completed before upload are included in the union. Bounds are checked before
copying; Xbox retains the existing GPU-completion wait before native writes.
No new pixel allocation or change to lighting computation is introduced.

TERRAIN_UPLOAD counts uploads, copied pixels, largest rectangle and full copies.
The1200-frame replay performs731 updates but copies1629760pixels (3259520bytes),
including six full clears. Full-image copying on each of those updates would
move383254528bytes. The settling harness checks full-clear count and requires
at least90percent less copy work than that equivalent full-image path. Six
ordinary, three close-view and the settling PC replays pass; all captured
images are byte-identical. Evidence:artifacts/geomod-dirty-replays.log and
geomod-dirty-before-images.json.

Native verification:artifacts/xemu/render-20260915-120614 completes1200frames
and43 comparisons with8743pages free (34.152MiB). Upload counters match PC.
Renderer resource preparation averages0.216ms versus25.823ms in the preceding
1200-frame run, with maximum38ms versus57ms (full clears still cost more).
The scene phase peaks at165ms, camera/combat131ms and world rebuild77ms;
these remaining peaks are not smooth-frame acceptance. The decoded framebuffer
exactly matches the inspected120115 capture. All19disc entries restored;
owned PID33260 exited. No new GitHub image or altered-appearance claim.

## Original debris chunk geometry (2026-09-15)

Original467020 calls48fe30 with the effective crater radius and interior bitmap
when its debris gate permits it.48fe30 reaches490230 to construct each chunk.
The live reconstructed scene still does not call a debris spawner.

rf_geomod_debris_build reconstructs490230 without allocation: eight randomized
corners, twelve fixed triangles, signed dominant-axis rock UVs at32texels per
world unit, and a randomized lifetime in[1,4). It consumes exactly25 CRT draws.
Corner magnitudes interpolate between radius*.2 and radius*1.8; each corner's
axis signs follow its index bits. The caller owns the532-byte output record.
Invalid dimensions, nonfinite/nonpositive radius or degenerate numeric geometry
preserve both output and random state. Input storage must not overlap.

inspect_geomod_debris.py executes original490230 instructions from the hashed
installed executable. Only CRT draws and bitmap dimensions are supplied;
geometry, projection and lifetime calculations execute unchanged.60 cases span
five seeds, four chunk radii and square/non-square textures. Shared C output
matches all vertex, index, UV and lifetime values bit-for-bit, with identical
final RNG state. Generated evidence:artifacts/geomod-debris-original.json.
The interior and repeated-cut CTests pass, including new invalid-input rollback
checks. This establishes chunk construction, not original spawning counts,
launch velocity, collisions, rendering, sounds or visible destruction fidelity.
Those integrations remain next work; no changed scene or screenshot is claimed.
NXDK Xbox compilation also passes (artifacts/geomod-debris-xbox.log); no native
scene validation was run because this helper is not yet wired to live impacts.

## Original debris launch (2026-09-15)

rf_geomod_debris_launch reconstructs490150: normalize chunk position minus
blast origin (zero separation falls back to+X), apply the existing verified
oriented cone sampler with cosine minimum.5, then scale the result. Chunk
radius strictly below.13 produces12units/second; other chunks apply6,
(1-resistance), then1.4 as separate float-rounded multiplies. Resistance is
an explicit input corresponding to original field3c;48fe30 initializes it
from(radius-.05)*5. Exactly two CRT draws advance the caller's stream.
Errors preserve velocity and random state. No allocation or physics scheduling.

inspect_geomod_debris_launch.py executes490150 and its original vector/cone
helpers, replacing only the CRT integer random boundary.432 original/shared
cases match velocity bits and final random state exactly: four seeds, six radii
including adjacent float values around.13, nine positions and two origins.
Zero separation, positive/negative principal axes, near-vertical orientation
and translated positions are covered. Evidence is in
artifacts/geomod-debris-launch-original.json. The two GeoMod CTests pass,
including invalid-input rollback, small-chunk speed and full-resistance stop.
The60 construction comparisons still pass. NXDK compilation also passes:
artifacts/geomod-debris-launch-xbox.log.

Live impact integration remains unfinished; no new visual/native scene claim.
Next recover spawn count/placement and connect the bounded pool to collision,
motion and rendering. Newly exported490500/490890 suggest count depends on
fourteen world probes (six axial and eight diagonal), rather than a fixed
number per rocket. This is a decompiler lead, not yet executed policy evidence;
do not substitute an arbitrary fixed burst and call it original behavior.

## Original terrain-dependent debris count (2026-09-15)

rf_geomod_debris_probe_points reconstructs490500's six axial endpoints and
eight diagonal endpoints, using radius*.5773500204086304f for each diagonal
component. The caller performs the world queries in that order and supplies
resolved hit records to rf_geomod_debris_count. No collision ownership or
material lookup is implied by the API.

The accumulator starts at float(radius*14). Each490890 sample subtracts its
hit distance only when the query returns exactly1, a face exists and that
face's flag8 is clear; otherwise it subtracts the full radius. Each subtraction
rounds to float.490500 floors twice the remainder, then caps the signed result
at16. Preserve signed results: radius.1 with all misses produces-1 from numeric
residue; original48fe30 loops only for positive counts, so it creates no chunks.
Do not interpret this result as an unsigned allocation count.

inspect_geomod_debris_count.py executes original490500,490890 and their
rounding/flag helpers. Only48fc10 world queries are supplied synthetic results.
138 original/shared cases match count and all fourteen endpoint float bits:
six radii, misses, near/far hits, missing faces, excluded/other flags, return2,
each isolated contributing sample and mixed samples. Evidence is stored in
artifacts/geomod-debris-count-original.json. This verifies policy for supplied
contacts; actual live contact distances, flags and eligible surfaces still need
binding. The two GeoMod CTests pass, including signed residue and invalid-input
rollback; no changed scene or screenshot is claimed.
NXDK compilation passes (artifacts/geomod-debris-count-xbox.log). No native
scene run was needed for these helpers, which are not yet on the impact path.

## Live DEV debris integration (2026-09-15)

Rocket terrain impacts now query debris contribution before the successful cut,
then spawn recovered chunk meshes against the updated world. The origin is
impact+normal*(effective crater radius*.1), matching48fe30's5893c4 constant.
Chunks start at random offsets up to.5 units from that origin, with a ray clamp
to keep the placement out of intervening terrain. This first binding handles
the supported enemy-free room; original room-search retries and existing-debris
relaunch are not implemented. Surface queries use the current overlay and
source-face metadata; full original48fc10 eligibility remains unverified.

An80-slot pool uses50320 bytes including render/query scratch, with oldest-slot
replacement and no per-impact allocation. The DEV random stream is separate
from crater orientation. Chunk radius follows r^3*.2+.05, launch and mesh use
the verified helpers. Authored rock02 material remains shared. Chunks rotate
around sampled axes at initial rates in[pi,2pi); interpolation is practical
axis-angle rotation, not verified original orientation integration. Reset clears
the pool only after the existing safe terrain-reset guard succeeds.

Motion is provisional: point rays, gravity,35percent reflected velocity and
three-to-five floor bounces (normal.y>=.7) before rest. No swept chunk volume,
original restitution/random normal response, water/mover interaction, actor
damage or bounce sounds yet.48f900 confirms the floor threshold and bounce
budget but contains additional response logic that is not implemented here.

Timer terminology corrected by the48fd70 instruction oracle below: field74
is the age at which a one-second fade starts, not the fade duration.48f900
copies it into elapsed age field70 when the bounce budget ends, advancing
settled chunks to fading. The current DEV pass still uses an absolute removal
timer without fading; its expiry checks do not establish original parity.

Six ordinary destruction/traversal replays pass. dev_debris_check.py captures
560/580/620/900-frame close-range runs:48 total chunks across three cuts,
changing visible mesh hashes, floor contacts, zero pool replacement, and all
removed by900 under the provisional timer. The560/620/900 PC images and580
Xbox image were inspected: chips are visible against the cavity, shift lower
as they fall, then disappear; the dark folded-mound crater remains unresolved.

Native artifacts/xemu/render-20260915-123842 passes580 frames and44 comparisons.
DEBRIS matches PC exactly:[48,16,236,32,276,2973937245,50320,0]. These fields are
spawned,active,bounces,expired,rendered vertices/hash,owned bytes,replacements.
The machine reports64MiB and8757 available pages (34.2MiB). All19 disc entries
restored and the owned process exited. No GitHub image was added.

## Final-raster crater depth audit (2026-09-15)

The PC headless frontend can now export its final depth target through
RF_REPLAY_DEPTH_OUT. RFD1 contains width/height as little-endian uint32 values,
then width*height float32 depth entries in the existing forward24-bit encoded
domain. DEPTH_CAMERA prints the final sampled frame and exact position/basis
words. The export is diagnostic-only and does not modify scene rendering.

The new dev_crater_depth_check.py runs identical900-frame close-view inputs
with three rocket shots or with only the fire bits cleared. It requires exact
camera-word equality, no living debris, three/zero cuts and a living player.
Both target files contain640x480 finite depth values. The audit inspects all
pixels in the top350 rows, excluding the first-person weapon/HUD region.
The intact image was inspected; cut pixels exactly match the previously
inspected close-900 capture.

There are19487 substantially farther solid pixels within(186,136)-(346,317)
and zero substantially nearer pixels, using128 encoded depth units as the
threshold for meaningful displacement. Raw minimum difference is-71 units:
small differences from the two differently triangulated/projected paths remain
and are not claimed to be exact coplanar equality. Together with the earlier
world-position/normal checks, this supports a recessed cavity rather than an
outward mound. Do not reverse all normals to address the visual ambiguity.

Separately,17 pixels that had valid intact-world depth are now uncovered.
These are explicitly excluded from the recessed-solid count and retained as
an open seam defect; the cause is not yet proven to be the known mesh-closure
junction issue. Their full coordinates and raw comparison statistics are in
artifacts/destruction/depth-audit/report.json. The audit reports this defect
as OPEN; its passing comparison is not a no-holes or fidelity acceptance.

PC compilation and the paired900-frame audit pass. No shared renderer change,
Xbox rebuild, native run or new screenshot was needed for this diagnostic-only
frontend change. Next investigate the cavity's illumination/material treatment
against original behavior and separately repair the uncovered pixels.

## Projected seam evidence and rejected collision stitching (2026-09-15)

RF_REPLAY_MESH_OUT optionally captures the final headless replay mesh before
rasterization. RFM1 contains count,world_count,stride as little-endian uint32,
then preview vertex records (currently56 bytes). It runs only on the final
recorded frame and does not alter the draw stream. The paired depth harness
now saves both meshes alongside depth and pixels.

analyze_crater_seams.py tests every uncovered pixel against every world
triangle using exact integer edge equations on the existing1/16-pixel grid.
All17 uncovered pixels are outside every projected triangle. Thus changing
only the PC raster's floating-point coverage test cannot fill these gaps.
artifacts/destruction/depth-audit/seams.json records the two nearest edges
for each pixel. At(196,229), the adjoining projected edge segments use
(193.1875,224.5), (206.5625,244.6875) and(219.375,264.125); the intermediate
point does not lie exactly on the long snapped edge. Near the viewport top,
another pair starts at y=0 and y=-.0625. These are projected subdivision/clip
consistency leads, not proof that the known world closure defect is their cause.

A pending-bank experiment inserted existing near-collinear world vertices
into neighboring faces before collision publication, reusing clipping scratch.
It was rejected and fully removed. Some tiny inserted edges violated existing
convex-face validation; skipping those insertions avoided that initial failure
but the live replay subsequently returned RF_FORMAT at frame345. No validation
tolerance was weakened. Original core source was restored and rebuilt: both
GeoMod CTests and the paired900-frame audit pass again, with the original17
projected gaps still reported OPEN. Rejected source and logs remain ignored
under artifacts/geomod-rejected-stitch.c and geomod-stitch-*.log for diagnosis.

Next isolate render-only edge subdivision and canonical clipping, retaining
the verified collision mesh and existing light-grid source polygons. This
turn adds diagnosis only; no seam repair or changed Xbox result is claimed.

## Render-only edge subdivision and viewport repair (2026-09-15)

The live terrain now owns a separate draw mesh rebuilt only when a cut/reset
binds a new generation. Existing source vertices lying within1e-6 of an edge
are inserted in edge order; endpoint duplicates within1e-6 are skipped. Each
inserted UV is interpolated on its owning face, preserving material seams.
A per-edge bounding box rejects separated candidates before distance work.
Physical vertices/faces, collision trees and light-grid inputs stay unchanged.
Preview count/plane descriptors borrow original plane values and have NULL
collision-vertex pointers; they must never be used as physical face records.

Subdivision alone reduced the paired frame's17 uncovered pixels to10. The
remaining cases involved shared clipped endpoints at0 versus-.0625 pixels.
The shared world projector now clamps its already-clipped,1/16-pixel-quantized
result to[0,640]x[0,480]. It does not enlarge internal triangles or relax the
raster's coverage rules. The paired900-frame audit now has zero uncovered
pixels,19487 substantially recessed solid pixels and no substantially nearer
pixels; it now asserts zero new uncovered pixels as a regression requirement.
The separate world-space closure-junction issue is not declared fixed.

Three-cut rendering uses1000 original vertices and1287 draw vertices. The
eight-cut stress pattern uses3120 and4089 respectively. Initial4096-vertex draw
storage was too close to that result, so the final draw allocation permits8192
vertices, while retaining768 faces and64 corners per face. Draw ownership plus
the additional neutral-color storage costs283668 bytes under a320KiB bound;
original physical terrain budgets are unchanged. This is one tested eight-cut
pattern, not proof of arbitrary destruction history or every cut arrangement.

The two GeoMod CTests and static-preview clipping test pass. All six ordinary
replays, three close/oblique views and the1200-frame six-cut settling check pass.
The three close images were inspected: the seam repair does not resolve the
dark, angular cavity appearance. dev_destruction_stress.py adds1500-frame
eight-cut/refill coverage with a completed29616-texel lightmap bake and a live
player. No new GitHub image is added.

The initial900-frame native run render-20260915-130456 passed45 comparisons
with the smaller draw allocation. All17 former gap coordinates have exactly
matching PC/Xbox RGB values (artifacts/geomod-seam-pixel-comparison.json), and
its framebuffer was inspected. All19 disc entries restored; owned PID9464
exited. The larger-buffer run130936 correctly failed the allocation comparison
because the reference PC binary was stale: all other TERRAIN_DRAW fields
matched, but PC reported152596 bytes versus Xbox283668. Its1500-frame guest
completed and all19disc entries restored. The PC reference was rebuilt; the
harness now also builds rf_pc_play before capturing future comparison baselines.

Final larger-buffer validation:artifacts/xemu/render-20260915-131241 passes
1500frames and45 comparisons with8612pages free (33.64MiB). TERRAIN_DRAW is
[3120,4089,969,283668,9] on both builds; all eight cuts committed and the
final lightmap bake completed. Its framebuffer was inspected. All19disc
entries restored and owned PID38716 exited.

Performance remains a limitation: the eight-cut run records a1108ms maximum
camera/world phase, including1053ms in combat/inspection work, and world
geometry rebuild averages32.202ms. These broad phases do not isolate CSG
versus subdivision cost. There is no matching pre-change eight-cut timing
baseline, so no performance improvement or smooth-edit claim is made.
Late-history edit stalls and broader cut-pattern coverage remain open.

Full eight-cut PC/native RGB is not bit-identical (24535 pixels differ; maximum
channel difference255). Both images were inspected and show the same test
scene/cavity, but that whole-frame difference is not explained by the focused
17-pixel fix and remains unverified. See geomod-stitch-eight-pixel-comparison.json;
do not treat the45 state comparisons as full image-parity acceptance.

### Eight-cut image difference audit (2026-09-15)

`tools/compare_geomod_capture.py artifacts/xemu/render-20260915-131241`
compares the actual 640x480 PC baseline and native Xbox framebuffer without
registration or resampling. Of 307200 pixels, 24535 differ: 24110 differ by only
one RGB level, 425 by more than one, and 290 by more than 16 (maximum 255).
The upper 350 rows contain 347 of the 425 larger differences; the lower weapon
and HUD region contains 78. An inspected crater bounding box x 280..365,
y 185..278 contains 303 differing pixels, 302 differing by one level and one
by 40. Thus the dark crater is shared by both outputs; backend mismatch does
not explain its overall appearance. The cause of larger differences remains
unverified, and numerical agreement does not establish original-game parity.
The native frame was visually inspected. No new GitHub screenshot was added.

### Original debris age and fade policy (2026-09-15)

`tools/inspect_geomod_debris_lifecycle.py` executes original48fd70 with only
pause-query436320, unlink48f3d0 and draw517080 hooks. Original x87 arithmetic,
integer conversion573528 and clamp40a520 execute unchanged. Across256 cases,
shared `rf_geomod_debris_age` matches elapsed-age float bits, removal decisions
and integer alpha. Cases include four lifetimes, adjacent representable values
around removal, zero/normal/large time steps and both pause states.

The routine first removes a chunk if age > lifetime+1, including while paused.
Otherwise it advances age by the frame interval unless paused. Alpha stays255
through lifetime, then truncates/clamps (1-(age-lifetime))*255. A time step can
therefore produce a zero-alpha draw before removal on the next invocation.
The original draw-driven clock also means visibility can affect advancement;
this harness does not establish scheduling or physics behavior.

The shared helper is ready but not yet connected to live rendering. The current
preview vertex format has RGB but no per-vertex alpha; implement and verify
bounded PC/Xbox debris blending before replacing the provisional expiry path.
Do not substitute darkened RGB for transparency. Two focused GeoMod CTests pass,
including new pause/removal boundaries and invalid-input rollback coverage.

### Live debris transparency (2026-09-15)

The shared lifecycle now runs during DEV debris draw submission. Chunks start
at age zero; exhaustion of the floor-bounce budget advances age to lifetime.
Alpha follows original48fd70 and deletion occurs on the next draw invocation
past lifetime+1. This fixes abrupt expiration, but does not establish original
room visibility scheduling, pause integration or bounce-response parity.

Reserved lightmap tags 0xfffffd00..0xfffffdff encode constant triangle opacity
without enlarging the 56-byte preview vertex. These tags select vertex RGB and
no lightmap. Xbox c[1].x supplies draw alpha; the fragment shader multiplies it
by texture alpha. Fading batches blend source-over, retain depth testing and
disable depth writes. PC applies the same blend policy and half-open triangle
edges to prevent double blending a shared boundary. Chunks are sorted by
camera-space center depth using bounded 80-entry scratch; intersecting chunks
and triangle-level transparency ordering remain unverified.

Pixel tests verify half-opacity over the clear color and an opaque surface,
zero opacity, unchanged background depth, foreground occlusion and shared-edge
coverage. Three focused CTests pass. Four live debris replays pass through
900 frames, with 48 chunks spawned/expired and a 50640-byte owned pool. The
700-frame native run at artifacts/xemu/render-20260915-133714 passes 45 checks,
with 8676 pages free (33.89MiB), PID 54848 exited and all 19 disc entries restored.
The first attempt 133516 stopped at the initial fading frame because the Xbox
validator lacked the new tag; the validator was fixed before the successful run.

The 700-frame PC mesh includes 93 fading vertices at alpha 4, 55, 76 and 153. PC and
Xbox captures were inspected: the room, crater, weapon and small floor fragments
render; the crater remains dark and angular. No new GitHub image was uploaded.
The existing provisional bounce, gravity and chunk-placement limitations remain.

Full-frame comparison finds 16719 differing pixels, 133 above one RGB level;
only one of those lies in the upper 350 rows. Remaining backend differences
are not claimed resolved by the state or focused pixel tests.

### Completed crater lighting audit (2026-09-15)

Set RF_REPLAY_TERRAIN_LIGHT_AUDIT to a CSV path for a PC DEV replay. At final
cleanup, the opt-in audit requires a completed lighting generation, then reads
every generated-face sample and recomputes its unshadowed and shadowed RGB.
It records positions, normals, blocked-light counts and actual stored atlas
texels. The audit is excluded from Xbox compilation and adds no normal-frame
work. Audit failure is reported as a replay error; no lighting is modified.
`tools/analyze_crater_lighting.py` summarizes the CSV and checks packed values.

The 900-frame three-cut depth replay produced generation 4, 171 generated faces,
9632 samples and two selected lights. All 9632 stored packed texels match the
recomputed shadowed RGB. 8246 texels are at 0x9084, the existing packer's minimum
R/G/B level 4. 5087 samples block both lights, 3830 block one, and 715 block neither.
Mean red lighting falls from .138178 without shadows to .100210 with shadows.
For normals with x>.9, 817 of 944 samples hit minimum packed brightness. These
counts include border samples and hidden faces; they are not visible-pixel
coverage or proof that the current shadow policy matches the original game.

The existing original lightmap seed code halves ambient before accumulating
lights, consistent with the live 0.0784314 seed for 40/255 global ambient.
The packer's minimum raises this seed rather than crushing it to black. There
is no evidence here to remove the half-ambient factor or brighten the texture.
Original shadow-mask generation and selected-light eligibility remain the next
fidelity checks. Evidence files are artifacts/destruction/depth-audit/lighting.csv
and lighting.json. Audit-enabled and disabled final frame bytes are identical;
PC and NXDK compilation pass. No new visual improvement is claimed.

### Crater blocker eligibility audit (2026-09-15)

The opt-in light audit now identifies the traversal-first accepted blocker for
each blocked light/sample pair, resolving reordered tree indices through
source_indices before inspecting source geometry or materials. It records
authored/generated classification and the recovered 0x2044 flag, signed portal,
alpha-format and coplanar exclusions. These counters are independent checks,
not the complete original shadow-volume cull. No live shadow policy changed.

The same three-cut 900-frame replay contains 14004 blocked sample/light pairs:
13083 authored-surface blockers and 921 generated-surface blockers. None of the
first blockers meet the tested flag/portal, alpha-format or coplanar exclusions.
Thus these exclusions do not explain this capture's darkness. This does not
prove all potential blockers qualify; the current query returns its first
accepted traversal hit, not necessarily the nearest hit. Mapping ownership,
volume bounds and projected polygon coverage remain separate verification work.

Re-ran original/PC/NXDK oracles: 4096 occluder cases and 2048 source-sampling cases
pass. Point sources use one origin and 255 mask subtraction; only kind 4 line
sources use two origins and 127 each. Therefore adding a soft area-light spread
to the two point sources would not follow this recovered source policy.
The next comparison is projected shadow-mask coverage versus binary point rays.

Evidence: artifacts/destruction/depth-audit/occluders.csv and occluders.json.
Extended-audit and disabled final frame bytes match exactly. Original oracle
reports are artifacts/lightmap-shadow-occluder.json and the source verifier's
reported artifact; this audit makes no new native visual or full-parity claim.

### Projected coverage diagnostic (2026-09-15)

The opt-in completed lighting audit now also runs recovered shadow preparation,
occluder culling, six-plane clipping, projection, receiver filtering, raster
subtraction and border replication against the same terrain and point sources.
It uses a bounded scratch owner allocated only for the offline PC audit, with
64-corner faces, 256 clipping vertices and mask storage below 512KiB. Runtime
terrain lighting and Xbox memory requirements are unchanged.

This comparison intentionally uses the port's existing per-face grids as
synthetic mappings, their face bounds and one receiver polygon per face. The
receiver-area threshold is zero to isolate coverage. This is not a recovery of
original generated-face mapping ownership, grouping, density or traversal order.
Border samples clamp to the port face while projected masks cover its planar
rectangle; that distinction is included in the aggregate comparison.

At 9632 samples, the ray-shadowed mean red is .100209856; projected mean red
is .097420701. Projection brightens 1011 samples and darkens 552 (epsilon 1e-6).
It does not explain broad darkness under these supplied mappings. The next
original-code target is generated-face lightmap construction and special
sampling, rather than adopting this diagnostic as a production replacement.

Re-ran 512 original/PC/NXDK complete shadow-pass cases, all passing, including
299 projections and 254 raster acceptances. The audit preserves final frame
bytes exactly. Evidence: artifacts/destruction/depth-audit/projected.csv and
projected.json; helper verification is artifacts/geomod-projection-pass-oracle.log.
No new native run or visual improvement is claimed by this diagnostic.

### Recovered new-face randomized lightmap fill (2026-09-15)

New evidence changes the lighting investigation. Original 4dbc50's staged
subtraction dispatcher calls 4dd8c0 in stage 5 and 4de4d0 in stage 7. In 4dd8c0,
faces passing 4de9d0's bit 23 predicate and lacking a signed lightmap index call
4e5b20 with mapping parameter 4.0; the adjacent 4f8740 call supplies the already
recovered crater UV path. The stage 7 fallback handles unmapped faces when
5a3a58==1, with mapping parameter 2.0. Exact mode/flag ownership and actual blast
execution remain to be verified; these are static control-flow observations.

4e5b20 constructs a mapping through 4e4180, then 4e5bb0..4e5c25 fills its RGB
rectangle using one CRT draw per texel: gray=(draw&63)+32, replicated to all
three channels. It assigns dirty byte 8 before calling 4f26a0. That routine's
lighting-regeneration gate requires dirty bits 6, so this immediate call uploads
without recomputing lighting. Subsequent dirty propagation may still relight
these mappings and needs tracing before claiming this is permanent lighting.

The new shared rf_geomod_light_noise implements the strided fill, preserving
row padding and RNG/output on invalid sizes. inspect_geomod_light_noise.py
executes the original loop with only CRT draws supplied. 64 original/shared
cases match every RGB byte and final RNG state, covering four seeds and
1..64 width/height choices plus original-image origin and padding guards.
Focused CTests cover shared padding and rollback; PC and NXDK builds pass.
No live rendering change is included yet: original mapping construction,
density and RNG stream scheduling remain separate requirements.

This is a concrete reason to reconsider the shadow-baked crater default,
rather than merely increasing its brightness. Earlier shadow audits remain
valid descriptions of that implementation, not evidence of original crater
lighting policy. Next: verify mapping density and later dirty propagation,
then connect the appropriate new-face lighting to the bounded shared renderer.
Evidence: artifacts/geomod-light-noise-original.json and Ghidra exports 436fc0,
4e3e90,4e5040,4e5b20,4dd8c0,4de4d0,4dbc50 and 4de9d0 for the existing RF.exe SHA.

### Original new-face mapping dimensions (2026-09-15)

4e4180 sets density from its caller parameter, adjusts it by the largest
face-flags bits 8..9 class, and measures projected surface extents. The recovered
numeric block 4e4452..4e453e rounds span*density+.5 down to an integer, clamps
above 64 with density rescaling, and clamps below 4 (ordinary) or 8 (special).
Zero rounded size uses denominator 1 for the minimum-size density correction.
There is no power-of-two rounding. The later mapping formula uses width-2 and
height-2 over surface spans with a one-texel offset; the existing port grid's
width-3/1.5-texel mapping is a different policy and must not be silently reused.

rf_geomod_lightmap_size implements the sizing block for already measured spans
and adjusted densities. inspect_geomod_light_size.py executes the original
block unhooked, including float-to-integer and max helpers. 120 cases match
both dimensions and adjusted-density float bits. Focused CTests cover a 9-texel
non-power-of-two result, zero-size minimum correction and invalid rollback.
PC and NXDK builds pass. Global density-class selection and axis/UV construction
are not claimed recovered by this helper; live rendering is unchanged.

The mapping's special byte comes from whether any grouped face has nonzero
field 3c. Its inhibit byte comes from 4e5f40's bit 5 query of supplied flags.
These static observations are not proof of the flags on actual cut faces.
Post-subtraction 4f0bd0 changes referenced owner state 1 to 2; 4e60c0 rebuilds a
face-reference list. Neither inspected routine calls lightmap regeneration.
This narrows immediate cleanup, but does not rule out later light dirtying.

Evidence: artifacts/geomod-light-size-original.json, exports 4e4180/4f0bd0/
4f0b90/4e60c0/466c50 and disassembly 4e5f40. Next integration must preserve
existing face mappings across later cuts, rather than repainting every old
surface whenever the terrain generation changes.

### Live persistent new-face lightmaps (2026-09-15)

DEV crater interiors now use the recovered grayscale fill and rounded mapping
sizes on both PC and Xbox. New mappings use caller density 4, ordinary minimum 4,
maximum 64 and the recovered width-2/one-texel projection convention. Authored
surviving surfaces retain their authored lightmaps. A 1024-entry owner preserves
new-face mappings across later cuts; compatible coplanar fragments inside an
existing mapping reuse its projection and texels. Otherwise current fragments
of that plane contribute to a new mapping. Plane/containment matching is a
practical port policy, not recovered original mapping identity.

The owner adds 94244 bytes; total tracked atlas ownership is 1218616 bytes,
within 1280KiB. Eight-cut playback uses 409 mappings and 22404 texels, with 2334
binding reuses and 1273 completed-map hash checks across later generations.
The initial 256-entry pool was too small and rejected a later edit; the enlarged
pool passes the same history. The 1024-entry/512-square atlas limits remain
bounded, and broader histories can still need admission/compaction work.

New maps fill completely before frame submission, using 64-texel scratch chunks
but no longer imposing the expensive shadow bake's 64-texel-per-frame delay.
Texels are written only inside newly allocated rectangles. Existing completed
rectangles are hash-checked on generation changes; reset clears mappings and
starts the explicit DEV RNG stream again. Original global RNG scheduling,
density-class selection, generated-face flag eligibility and subsequent
light-driven regeneration remain unverified. This improves the reconstruction
but is not original visual-parity acceptance.

The prior shadow bake is retained only as an opt-in PC reference through
RF_REPLAY_TERRAIN_SHADOW_REFERENCE; the old lighting audit also requires that
flag. Default live lighting no longer shadows the initial randomized maps.
Six-cut settling and eight-cut stress tests now check persistent mappings and
per-generation fills. dev_geomod_lightmap_reset.py verifies eight cuts, guarded
reset and a fresh cut: generation 11, 17 replacement maps, 1472 texels, no stale
completed-map checks. The paired depth audit still finds 19487 recessed solid
pixels, zero substantially nearer pixels and no new uncovered pixels.

The close PC capture shows more rock texture, but the cavity remains dark and
angular. That appearance is still open. No GitHub screenshot was uploaded.

Final native validation: artifacts/xemu/render-20260915-141427 completes 1500
frames/eight cuts and 46 checks. PC/Xbox agree on 409 mappings, 22404 texels,
2334 binding reuses and 1273 preservation checks; the largest single-frame
fill is 3589 texels. Available 8595 pages equals 33.57MiB; PID 48984 exited and
all 19 staged disc entries were restored. Eight updates copy 354687 texels,
including one full atlas upload. Native world-rebuild phase averages 12.935ms
(max 37ms), but the combat/inspection phase still reaches 1146ms on a late edit.
This does not establish smooth edits or full-frame pixel parity. The native
frame was inspected. The earlier 141020 run tested the same mappings with
incremental filling; 141427 verifies the final immediate-fill policy.

Late-edit profiling (2026-09-15): The native harness now exports the latest
eight DEV blast timings to terrain-edit-times.json, separating terrain cutting,
render-mesh/collision binding, debris preparation and debris spawning. These
wall-clock timings are diagnostics, not PC/Xbox equality checks.
Run artifacts/xemu/render-20260915-141949 completes 1500 frames, eight cuts
and 46 checks. Cut times grow 17,39,81,122,164,202,271,291ms; bind times grow
7,42,111,210,309,455,669,773ms. The final blast spends 3ms preparing debris
and 4ms spawning it. Binding includes render-only T-junction subdivision and
collision-overlay binding; this measurement does not separate those two.
The next optimization target is that combined binding path, while preserving
seam closure and physical geometry. Host scheduling affects these timings.
No performance improvement is claimed from instrumentation alone.
The native framebuffer was inspected: the dark mound-like appearance remains
unaccepted. PID 49500 exited and all 19 staged disc entries were restored.
No GitHub image was uploaded. Estimate remains ~49%; focus is GeoMod fidelity.

Bounded edge-query indexing (2026-09-15): Render-only subdivision now builds
three coordinate-sorted arrays of physical vertex indices, then binary-searches
each edge bounding box and scans the smallest candidate range. The original
double-precision predicates and UV interpolation are unchanged. Equal-fraction
candidates explicitly prefer the lowest physical index, preserving the prior
linear traversal tie rule. Physical CSG and collision meshes are unchanged.
The three uint16 index arrays add 24576 bytes; tracked draw ownership is
308244 bytes, below the 320KiB bound. Input remains capped at 4096 vertices;
no allocations occur per edge or per cut.
PC eight-cut final mesh (418336 bytes) and image (921615 bytes) match the
pre-index build byte-for-byte. The paired depth check retains 19487 recessed
solid pixels, zero substantially nearer pixels and zero new uncovered pixels.
Eight-cut stress and the three focused geometry/preview CTests pass.

Native validation: artifacts/xemu/render-20260915-143015 passes 46 checks
through 1500 frames/eight cuts. Bind times are 4,20,50,87,105,210,287,310ms;
cut times are 15,40,83,132,159,221,288,282ms. Compared with the preceding
141949 run, eighth-blast binding falls from 773 to 310ms (about 60 percent);
the four measured edit stages sum to 599ms versus 1071ms (about 44 percent
less). This is a native replay comparison subject to host scheduling, not
a claim of eliminated stalls or an overall FPS gain.
The inspected native framebuffer is pixel-identical to 141949. All 19 staged
disc entries were restored; PID 19572 exited. Available memory is 8593 pages
(33.57MiB) on the stock 64MiB target. Crater appearance and remaining edit
stalls remain open; this optimization does not constitute visual acceptance.

Rejected optimization experiment (2026-09-15): Compacting bit-identical
positions from the coordinate search index preserved the eight-cut PC mesh
and image byte-for-byte, and depth/stress checks passed. Native run
artifacts/xemu/render-20260915-143436 passed 46 checks with unchanged pixels,
but binding times were 4,16,45,95,132,196,284,342ms versus the retained
4,20,50,87,105,210,287,310ms. The final cut also increased from 282 to 326ms,
indicating host timing variation; this is not proof of a specific regression,
but gives no evidence of a meaningful benefit. The candidate-compaction
change was reverted. The coordinate-index optimization remains in place.
PID 44888 exited and all 19 staged disc entries were restored. The native
frame was inspected; crater appearance remains unresolved. Next profiling
should separate candidate search from repeated vertex-array moves before
another performance change. Estimate remains ~49%; GeoMod stays first.

Face-local subdivision assembly (2026-09-15): Each face is now expanded in
a 64-vertex scratch array and copied once to the output mesh. This removes
whole-mesh memmoves and updates to every later face offset for every inserted
vertex. Candidate searches, exact predicates, insertion order and UV arithmetic
remain unchanged. Scratch storage is 1280 bytes on the stack; the heap owner
remains 308244 bytes under 320KiB. Bounds reject malformed face ranges, more
than 64 corners, output overflow and inconsistent packed vertex counts.
The eight-cut PC mesh and image match the pre-index baseline byte-for-byte.
Depth/stress checks pass with 19487 recessed solid pixels and no new gaps.
Native artifacts/xemu/render-20260915-143854 completes 1500 frames/eight cuts
and 46 checks. Binding times are 3,8,11,21,26,32,42,48ms, versus the retained
indexed baseline 4,20,50,87,105,210,287,310ms. The eighth terrain cut takes
323ms; debris preparation/spawn take 3/4ms, totaling 378ms across the measured
stages versus 599ms indexed-only and 1071ms before these optimizations.
Timings are subject to host scheduling; this is not an overall FPS claim.
The inspected Xbox framebuffer is pixel-identical to the indexed baseline.
PID 54748 exited; all 19 staged disc entries were restored. Available memory
is 8593 pages (33.57MiB). Crater appearance still needs fidelity work; remaining
edit time is now dominated by terrain cutting. Estimate remains ~49 percent.

Lightmap detail scaling (2026-09-15): The constructor at 4e4180 takes the
maximum grouped face flags bits8..9, then 4e43eb..4e443a scales both supplied
densities by 0.5,1,2,4 for classes 0,1,2,3. The new shared helper
rf_geomod_lightmap_density reproduces that scaling. The unhooked executable
block and shared helper agree bit-for-bit in 40 cases through
tools/inspect_geomod_light_density.py; the original executable SHA is checked.
Evidence is stored in artifacts/geomod-light-density-original.json.
The oracle supplies the grouped class; it does not recover generated-face
flag inheritance or grouping. The live DEV path still supplies fixed adjusted
density 4 and does not yet call this helper. With caller density 4, original
classes would yield 2,4,8,16, so selecting one without provenance would be an
unsupported visual change. PC probe and NXDK builds pass; no new runtime or
visual acceptance is claimed. Continue tracing generated-face flag ownership
before replacing the live assumption. Estimate remains ~49 percent.

Holey01 property provenance (2026-09-15): inspect_geomod_template.py now
captures the six property words supplied at every original 4cfab0 factory
call. All 16 faces supply [0x100,0,0xffffffff,0xffff0000,0xffffffff,0].
The default initializer 4ce420 executes unhooked in that factory harness.
Consequently each template face has detail class1, which preserves caller
density4 under the independently verified scaling rule. The original
factory vertices, face order, UVs, bounds and radius continue to pass.
The report artifacts/geomod-holey01-original.json now includes properties
and template_detail_classes; allocation/container boundaries remain supplied.
Static inspection of 4dfbd0 shows the six property words copied to face+28;
4e0240 copies those same words when converting the cutter into the working
solid. Stage0 4dbdf0 calls that conversion. This supports template density4
at construction, but does not prove every later split/merge preserves class
or establish final mapping identity and relighting. No live density change
is justified by this evidence, and no visual improvement is claimed.
The lightmap finalization pass4dd8c0 only requests mapping creation for
eligible unmapped faces; the nearby4f98f0 resets mapping references and
updates bounds, rather than selecting the density class.
Next follow-up is post-split property inheritance and mapping/relighting
behavior. Dark crater appearance remains open. Estimate stays ~49 percent.

Relighting gate audit (2026-09-15): inspect_geomod_relight_gate.py executes
original4f26a0 until the light query4d9c00 or post-relight label4f2c79. All
256 dirty-byte values with inhibit0/1 (512 cases) match: lighting is queried
only when dirty bits1/2 are set and inhibit byte10 is zero. The initial
new-face dirty8 therefore bypasses the light query. The oracle stops before
pixel generation/upload; it does not replace branch instructions or prove
subsequent scheduling. Original SHA is checked and cases are recorded at
artifacts/geomod-relight-gate-original.json.
Ghidra direct references to4f26a0 identify creation4e5b20 and the bulk pass
4e5040. The bulk pass sets dirty7, recalculates maps, processes special-map
seams/borders, and uploads those with dirty8. Its reference export currently
contains no callers. Absence in this database is not proof of runtime
unreachability; indirect calls and other dynamic lighting paths remain open.
No automatic post-cut full relight is established by this evidence. The live
persistent grayscale policy remains unchanged, and the dark crater remains
unaccepted. Next investigation must find the runtime scheduling/texture
usage path rather than assume the bulk relight pass runs after destruction.
No new visual or Xbox runtime claim; estimate remains ~49 percent.

Solid draw and dynamic-light distinction (2026-09-15): Static inspection
of4f0c00 shows ordinary faces use material+30, mapping+36 and the mapping
image texture at mapping+0c/image+10. In the normal multitexture route the
mode is global1808328, already verified as the ordinary lightmapped mode.
The inspected selection block supplies no separate crater brightness gain.
Debug/alternate texture modes and full runtime call coverage remain outside
this observation; no blend-policy change follows from it.
Correction to the preceding gate audit: 4f2c79 begins a separate dirty-bit1
dynamic-light branch. It may query4d9c00 with arguments1,0 and add dynamic
RGB to stored map RGB before saturating to1555. The existing oracle stops
at this label and therefore proves only the static bits2/4 gate. Its output
field is now queries_static_lights and its scope explicitly excludes the
dynamic branch. Initial dirty8 still has neither static nor dynamic query
bits, but later dirty1 scheduling cannot be ruled out by that oracle.
Helpers4e6020/4e6080 update scrolling face UVs; they do not consume dirty
lightmaps. Continue through dynamic-light scheduling and original crater
material selection. No new visual or native-runtime claim; ~49 percent.

Gameplay material setup (2026-09-15): The DEV path already obtains its
substrate name from level section900; rock02 is the selected GlassHouse
level material, not an arbitrary renderer fallback. Original4f8740 mode4
supports base/down/up texture handles, but the gameplay request setup
466b00 assigns all three identically through4f86f0/4f8700/4f8710.
inspect_geomod_material_setup.py executes466bd9..466c1d and those actual
setters for all256 low-byte blast flag combinations. Without flag0x10 all
three handles come from global646000; with it they come from loading
ice_ice01.tga with arguments-1,1. Only that texture-load boundary is supplied;
all256 selections and load requests match. Original SHA is checked and
results are artifacts/geomod-material-setup-original.json. Loader handle
provenance, final CSG and rendering are not covered by this oracle.
No alternate floor/ceiling material is justified for ordinary gameplay by
this setup. Existing live ice-cut rejection remains; supporting ice requires
its material path rather than reusing rock. No source render change or new
visual acceptance follows. Current priority remains crater readability,
runtime dynamic lighting and mapping identity; estimate ~49 percent.

Complete closure sweep (2026-09-15): The checker previously returned after
the first failed interval. RF_GEOMOD_CLOSURE_ALL=1 now reports every failing
interval in each room-scale stress cut without changing tolerances or the
reported closed result. Coordinate differences now cast to double before
subtraction rather than rounding to float first. This correction does not
remove the known first1.34534375358e-6 junction failure.
Running rf_geomod_interior_tests Installed_Game artifacts/geomod-holey01-csg.bin
with that environment variable produces artifacts/geomod-closure-all.log;
tools/analyze_geomod_closure.py summarizes it. Failing interval counts are
5,30,98,133,191,230 after cuts1..6. Cut1 failures are1.0596e-6..1.3453e-6
units long. Later cuts include intervals as long as2.74915108302 units.
At cut6,190 intervals have one matched edge,29 have three and11 have four.
These are interval lengths under existing positional/angular tolerances,
NOT measured gap widths; nearly parallel displaced edges can fail coverage
along a long interval. Do not infer large visible holes from these numbers.
The three focused CTests pass, including enforced ray coverage. Room-scale
closure remains diagnostic and fails; passing tests are not closed-topology
acceptance. No production geometry or framebuffer changed. The next repair
needs full-boundary analysis, not only the first tiny junction. ~49 percent.

Opposing-edge distance audit (2026-09-15): Full closure mode now searches
for the nearest oppositely directed edge satisfying the existing angular
and projected-interior tests, independently of positional tolerance. It
prints CLOSURE_NEAREST with an explicit found flag; a found0 distance is a
sentinel and must not be interpreted as geometry. The summary tool reports
missing candidates separately. No tolerance or production vertex changed.
The longest failing interval2.74915108302 has an opposed edge at distance
1.46611917376e-6. Across cuts1..6 the greatest distances among eligible
opposed candidates are1.62075e-7,1.91186e-6,2.56371e-6,2.56371e-6,
2.62674e-6,2.69094e-6. Missing eligible candidates number2,9,23,31,41,47;
these may fail endpoint/angular eligibility and are not proven missing faces.
Evidence: artifacts/geomod-closure-nearest.log and its .summary.json, produced
by the full-sweep test and tools/analyze_geomod_closure.py. The three focused
CTests pass while the strict room-scale closure diagnostic still fails.
This localizes the long-interval failures to small boundary disagreement,
but does not excuse them or solve unmatched intervals. Repair must preserve
incident face-plane constraints; increasing tolerances or blanket welding
would conceal the issue. No rendered output change; estimate ~49 percent.

Rejected split-band experiments (2026-09-15): The splitter currently
classifies plane distances within1e-5 as on-plane and preserves those
vertices. Testing1e-6 and1e-7 independently, without changing any closure
checker tolerance, breaks existing installed-data interior fixtures. The
1e-6 variant fails rf_geomod_storage_prepare_star_cuts at the repeated
original-star fixture (current test line886). The1e-7 variant fails the rotated eight-cut volume/
closure fixture (printed volume32.0000025,64faces). Neither variant was
retained; source bytes were restored and the original interior executable
rebuilt and rerun successfully. No Xbox or rendered improvement is claimed.
Artifacts: geomod-split-band-1e-6.log, geomod-split-band-1e-7.log,
geomod-split-band-results.json and geomod-split-band-restored-tests.log.
This rejects a tolerance-only repair; it does not prove the current band
is mathematically ideal. The next implementation must preserve common
boundary/incident-plane constraints through repeated splitting rather than
independently classifying already rounded vertices more aggressively.
Strict room-scale closure remains open; estimate ~49 percent.

Boundary construction experiments (2026-09-15): Two bounded alternatives
were tested independently and reverted. First, internal subtract/interior
clipping kept position/UV intermediates in double until polygon publication,
leaving the public vertex format and tolerances unchanged. Existing interior
and eight-blast gameplay checks passed, but closure failures became
5,26,99,148,209,252 rather than5,30,98,133,191,230. The gameplay draw mesh
grew from4089 to4125 vertices. This is not a closure improvement.
Second, polygon compaction required numerically identical reversed edge
endpoints rather than1e-6 proximity. Interior tests passed; failures were
5,29,98,133,191,230. Cut6 grew from2320vertices/536faces to2356/554 without
fixing closure. Therefore proximity-based merging is not sufficient to
explain the repeated boundary defect.
Artifacts: geomod-wide-clip-tests.log/.summary.json,
geomod-wide-clip-stress.log, geomod-exact-join-tests.log/.summary.json.
Original source bytes were restored, PC targets rebuilt, and three focused
CTests plus eight-blast stress rerun successfully. No Xbox build used these
experiments; no live source change or visual improvement is retained.
The evidence favors investigating shared intersection identity/plane
provenance across successive operations; this remains a hypothesis, not a
proven repair. Incident face constraints and bounded Xbox storage must be
preserved. Strict closure remains open; estimate ~49 percent.

Intersection provenance trace (2026-09-15): A synchronous optional observer
now records each successful split intersection: plane4, canonical edge
endpoints3+3, and rounded result3. No clipping values are changed. Observer
registration must serialize with clipping and context lifetime is caller-owned.
The interior fixture enables it only for the first cached room-scale cut
when RF_GEOMOD_INTERSECTION_TRACE names an output file, then unregisters it.
Binary format is RFI1 followed by13 little-endian float32 values per event.
artifacts/geomod-intersections.bin contains338 events. Running
tools/analyze_geomod_intersections.py with --point -16 -7.36841679 2.93492723
finds three unique constructions (two/two/four occurrences). Their Z values
are2.9349286556243896,2.934927225112915,2.9349281787872314; X is-16 and Y
is-7.368416786193848 throughout. Two arise on different cutter planes
from already clipped wall edges; the third clips a cutter edge against X=-16.
The three observed planes intersect at approximately
(-16,-7.368416394851663,2.9349285303154686), rounding to
(-16,-7.3684163093566895,2.9349284172058105). This offline fit is a candidate
shared corner, not proof that all incident constraints permit replacement.
The trace identifies divergent construction paths; production repair must
retain complete plane/edge provenance rather than infer identity by proximity.
Three focused CTests and NXDK build pass. No geometric or visual change is
claimed, and no new native replay was needed for this dormant observer.
Estimate ~49 percent; current focus remains shared GeoMod boundaries.

Canonical plane-corner primitive (2026-09-15): rf_geomod_plane_corner
constructs one float corner from three unit-normal supporting planes. It
canonicalizes normal/distance signs and sorts planes to fix arithmetic order,
solves in double precision, then checks the rounded result against all three
planes. It allocates no heap storage. Singular determinant below1e-10, invalid
unit normals/nonfinite inputs and float residuals beyond1e-5 return errors
without changing output. These are primitive admission bounds, not changes
to the closure checker or existing splitter tolerances.
The independently fitted first-junction result is reproduced bit-for-bit
for all six plane orders and eight sign combinations. Duplicate-plane, NaN
and null-input rollback cases pass. Three focused CTests and NXDK build pass.
This is a practical reconstruction primitive, not original-binary parity.
It is not yet called by live clipping: correct incident-plane identity and
constraint propagation must be carried across splits first. Spatial proximity
is not sufficient evidence for assigning a plane triple to a vertex. No
closure fix or visual improvement is claimed until that integration passes
the complete boundary/collision/native checks. Estimate remains ~49 percent.

Seed adjacency recovery (2026-09-15): rf_geomod_seed_adjacency recovers the
opposite incident face for each directed edge of a packed closed seed mesh.
It uses exact endpoint equality, rejects zero-length edges, same-direction
duplicates and missing/multiple opposites, and performs validation before
publishing caller-owned uint16 face IDs. It allocates no memory and accepts
4..32 faces with3..64 corners. Output must not alias input storage.
Tests cover the stock room box, both installed Holey01 cutter instances,
invalid packed layout/open input, short output and unchanged-output failure.
The installed-data interior test passes; three focused CTests and NXDK build
pass after correcting an initial header declaration-order compilation error.
This provides seed topology for supporting-plane identities. It is not yet
propagated through fragment splits or used to change live crater vertices.
Existing closure failures therefore remain. Next integration must preserve
edge identities in work/seed/split buffers and assign the cut-plane identity
to newly created edges; matching positions after the fact is insufficient.
No visual or new native execution claim; estimate ~49 percent.

Tracked edge splitting (2026-09-15): rf_geomod_polygon_split_tracked now
propagates each input vertex outgoing-edge support ID. Existing edge portions
retain their ID; newly created cut-boundary portions receive the supplied
cut-plane ID. On-plane corners choose the cut ID when the outgoing edge
leaves that output half. Geometry and UV arithmetic remain shared with the
ordinary splitter; optional metadata does not change the intersection point.
Two64-entry uint16 scratch arrays add256 bytes of bounded stack, no heap.
Metadata and geometry buffers must be disjoint; invalid/short output preserves
output and counts. Existing splitter callers pass no metadata.
Tests compare ordinary/tracked geometry across crossing, touching and diagonal
planes; verify edge endpoints against their assigned supports; preserve both
cut IDs after consecutive perpendicular splits; and verify short-output rollback.
Three focused CTests, eight-cut PC stress and NXDK build pass. The PC endpoint
image is byte-identical to the pre-index baseline. No new Xbox replay claim.
History bank/seed/split sidecars are not wired yet, so the canonical corner
solver remains outside live clipping. The next change must carry these IDs
through subtraction output, reversals and history-bank copies before solving
shared corners. Closure and dark crater appearance remain open; ~49 percent.

Tracked polygon subtraction (2026-09-15): rf_geomod_polygon_subtract_tracked
carries outgoing-edge support IDs through every clipping plane and publishes
them alongside packed surviving fragments. The separated fast path and coplanar
policy preserve the matching IDs; size queries and short-capacity failures keep
output unchanged. Three64-entry uint16 arrays add384 bytes of bounded stack
in subtraction, in addition to the splitter scratch; there is no heap allocation.
Existing callers use the same geometry path without supplying metadata.
Tests independently check each emitted edge against its supporting plane,
including both plane orders, separated and coplanar cases; ordinary geometry
and fragment records remain identical. Three focused CTests and the1500-frame
eight-cut PC stress pass. Its endpoint image is byte-identical to the pre-index
baseline. NXDK builds the XBE and XISO; no new native replay was performed.
History-bank copies, seed clipping and reversal still need metadata integration
before the corner solver can change live intersections. Closure and crater
appearance remain unresolved; estimate ~49 percent, current area GeoMod.

Cavity history edge ownership (2026-09-15): Live cavity reconstruction now
recovers exact source/cutter edge adjacency and retains supporting-plane IDs
through seed subtraction, every history work-bank copy and final reversal.
IDs0..31 reference source planes; each cutter reserves128 IDs, with four per
star tetrahedron (outer face first). Convex cutters use their face indices.
Reversing winding remaps outgoing-edge IDs with the required one-edge offset.
The work owner adds28672 bytes (28 KiB), counted by existing terrain allocation
budgets; eight-cut PC stress and the six-cut1MiB fixture still pass. No rendered
vertex layout changes. Finite-solid reconstruction remains untracked for now.
Installed Holey01 tests check the retained first bank fragment's edge endpoints
against their identified planes after one and two cavity cuts. This is targeted
metadata evidence, not an exhaustive audit of all emitted edges. Three focused
CTests, eight-cut PC replay and NXDK XBE/XISO builds pass. The PC endpoint image
remains byte-identical to the prior baseline. No new native replay was performed.
The canonical three-plane solver is still not invoked during clipping; actual
corner positions and closure failures have not changed. Next: supply the face
support plane and use the retained edge plane at intersections, with explicit
handling for singular/coplanar supports. Estimate ~49 percent; GeoMod remains
first priority, including the unresolved dark crater appearance.

Live shared-plane corners (2026-09-15): Cavity clipping now supplies the source
face support, retained edge support and cutting plane to rf_geomod_plane_corner.
Successful three-plane solves replace the independently interpolated position;
UV interpolation remains unchanged. Singular/coplanar support triples retain
the old edge interpolation. Finite-solid and public untracked clipping remain
unchanged. This is a practical port repair, not claimed original implementation.
Six room-scale closure failure counts change from5,30,98,133,191,230 to
8,21,50,71,104,120. Later cuts improve substantially, but the first cut worsens
and no cut is fully closed. No positional/closure tolerances were widened.
The initial eight-blast replay rejected its last cut: collision tree creation
needed more temporary memory than the remaining1MiB terrain allowance, with
743 faces/3162 vertices and872176 bytes already committed/reserved.
rf_collision_tree_open_scratch now accepts caller-owned temporary workspace.
Terrain reuses the completed clipping vertex banks, already included in its
base budget. Tree retained bytes are unchanged; peak excludes borrowed scratch
only when its owner already accounts for it. The old allocating API remains.
A fixture compares actual nodes, reordered faces and source indices against
the allocating path, checks short-scratch rollback, then releases the scratch
before exercising ray/body collision on the resulting tree.
All eight PC blasts now succeed:3162 physical vertices,3928 render vertices,
766 inserted render-edge points, terrain peak1011072 bytes under1048576.
Three focused CTests pass. The identical-camera depth audit again reports19487
recessed solid pixels, zero substantially nearer pixels, zero new uncovered
pixels and minimum raw depth delta-71. NXDK XBE/XISO builds pass.
Crater visual fidelity and strict closure remain unresolved. Estimate overall
~49 percent, GeoMod ~60 percent; these are judgment estimates, not test coverage.

Native verification: artifacts/xemu/render-20260915-154159 completes1500 frames
and eight cuts, passing46 comparisons with8601 physical pages free (33.60MiB).
All19 disc entries restore byte-exactly and the owned PID43756 exits. The native
framebuffer was inspected: the dark mound-like appearance remains, so this is
not visual acceptance and no GitHub image was added.

Four-plane junction audit (2026-09-15): Temporary process-local instrumentation
recorded the actual support triples at the first unresolved corner. IDs are
(0,32,35), (0,32,44), (32,44,0), (44,32,0), where0 is the room wall,
32/44 are outer cutter faces and35 is the first tetrahedron's third internal
plane. All corner solves return success. The internal plane and neighboring
outer face should share the same authored seed edge, but their rounded plane
coefficients define different intersections with the wall.
The four distinct planes yield a maximum double-solved corner separation of
1.5257496324347654e-6. This explains disagreement at this particular junction;
it does not prove the cause of every later closure failure. Using successful
three-plane solves alone cannot force four inconsistent planes to meet.
The exact float coefficients are reproducible with:
python tools/analyze_geomod_corner_supports.py tests/fixtures/geomod-first-corner-supports.txt --output artifacts/geomod-first-corner-supports.json
The analyzer restores float32 from nine-significant-digit diagnostics before
solving; it reports all plane combinations, separation and cross-plane residuals.
Instrumentation is removed, shared source is restored to the retained native-
verified implementation, and focused CTests pass. No gameplay change is claimed.
Next repair must use the actual shared seed edge or higher-precision planes
constructed from that geometry; choosing nearby vertices or widening the
closure epsilon would conceal the disagreement. Overall ~49%, GeoMod ~60%.

Exact seed-edge intersections (2026-09-15): Cavity corner construction now
first checks whether two supports belong to the same star cutter and share
exact authored edge endpoints. Outer faces contribute three seed vertices;
internal tetrahedron faces contribute their two outer edge endpoints. Exactly
two common vertices identify the seed edge; coincident/opposite supporting
planes are excluded. The third support must come from the room or a different
cutter. Canonically ordered edge endpoints are intersected directly with that
third plane, requiring a finite parameter in[0,1]. Other configurations retain
the three-plane solver and its singular fallback. No proximity weld, widened
closure tolerance or new heap allocation is used. This also avoids using a
rounded internal plane as an independent constraint on its own seed edge.
The room-scale closure counts become0,9,16,19,27,39, versus8,21,50,71,104,120
before this change. The first crater now closes and the fixture enforces that
result; later-cut closure remains an open defect. Its geometry is262 vertices/
64 faces; the sixth cut is2312 vertices/532 faces. Collision/volume tests pass.
The1500-frame eight-blast PC replay succeeds with3116 physical vertices,
3729 render vertices and613 inserted boundary points. Terrain peak1003680
bytes stays below1MiB. Three focused CTests and the NXDK build pass. The depth
audit retains19487 recessed pixels, no substantially nearer or newly uncovered
pixels, minimum raw depth delta-71. Appearance is not accepted by these checks.

Native seed-edge verification: artifacts/xemu/render-20260915-155050 passes46
comparisons across1500 frames/eight cuts, with8600 pages free (33.59MiB).
All19 disc entries are restored exactly; owned PID20376 exits. The native
framebuffer was inspected and still has the dark mound-like crater appearance.
Eighth-edit timings are352ms CSG,39ms bind,4ms debris preparation and3ms spawn
(total398ms), versus336ms in the immediately preceding shared-plane run.
This is a correctness improvement with a measured edit-time regression, not
an FPS improvement. Remaining closure, performance and visual work stay open.
Estimate GeoMod ~61 percent, overall ~49 percent; current area is destruction.

Rejected precise-plane experiments (2026-09-15): Recomputing normalized plane
coefficients in double directly from canonical seed triangles reduces closure
failures to0,1,9,18,21,30. The first crater stays closed. An initial on-demand
implementation and a32KiB plane cache give the same counts. Eight PC cuts pass
with3096 physical/3722 render vertices and terrain peak1033092 bytes under1MiB.
However, the identical-camera depth audit exposes one new uncovered pixel at
(196,229), with19486 recessed pixels rather than19487. This version is rejected.
Keeping the old float-plane corner solver and using precise planes only when
intersecting an exact seed edge with another cutter removes that pixel, but
closure becomes0,6,17,21,27,46, worsening the later baseline39. Also rejected.
Evidence: artifacts/geomod-precise-cache-tests.log and its summary, precise-cache-
depth.log, precise-edge-tests.log and summary, and precise-edge-depth.log.
Both experiments are removed, including the plane cache and added test assertion.
The retained exact-seed-edge version is restored; focused CTests, the depth
audit (19487 recessed, no new uncovered pixels) and eight-cut PC replay pass
again with3116 physical/3729 render vertices. This turn makes no retained
geometry change. The next hypothesis is that classification using rounded
float planes and construction using higher-precision planes must be reconciled
together. These experiments do not prove that hypothesis or justify loosening
closure/depth tolerances. Overall ~49%, GeoMod ~61%; overlap accuracy remains open.
Restored NXDK XBE/XISO builds also pass; no new XEMU replay was run for the
rejected changes, and the last retained native verification remains155050.

Seed supporting-line correction (2026-09-15): The second blast's first failing
junction uses support IDs(0,160,161) and(0,160,188): room wall, second cutter
outer face, and either its internal boundary or neighboring outer face.
The exact seed-edge path declined the intersection because its parameter lay
outside[0,1], falling back to inconsistent rounded-plane solves. Supporting
planes define an infinite line; the remaining clipping planes bound the final
fragment. The line path now accepts any finite parameter and finite float
result, retaining the existing topological/parallel guards and all tolerances.
Both recorded constructions now produce exactly(-16,-9.89635181427002,
-0.18177662789821625). Before, the second produced y=-9.896350860595703 while
the first produced z=-0.18177726864814758. This is one verified junction repair,
not proof that every nearby point represents the same topological corner.
The test harness accepts RF_GEOMOD_INTERSECTION_CUT=1..6 (default1), allowing
any room-scale blast's rebuild to be traced. The analyzer's optional
--require-identical requires matches with exactly identical recorded results;
it rejects the prior trace and passes the repaired trace at this junction.
Evidence: artifacts/geomod-second-cut.bin, geomod-second-line.bin and their
intersection.json reports;1192 events in each selected second-cut rebuild.
Room-scale failure counts are0,8,12,16,24,35 (previously0,9,16,19,27,39).
The first crater remains enforced closed. Focused CTests and all eight PC blasts
pass (3112 physical/3720 render vertices,608 inserted points). Depth audit:
19983 recessed pixels, zero substantially nearer or new uncovered pixels,
minimum raw delta-71. The actual PC close-up was inspected: dark mound-like
appearance remains. NXDK builds pass. No visual parity or full closure claim.

Native supporting-line run: artifacts/xemu/render-20260915-160151 completes
1500 frames/eight blasts and passes46 comparisons with8600 pages free.
The19 saved disc entries restore byte-exactly and owned PID48856 exits.
The actual framebuffer was inspected; dark crater appearance remains unresolved.
Latest eighth-edit timing is219ms CSG+27ms binding+2ms debris preparation+2ms
spawn=250ms. Host scheduling varies, so this single run is not an FPS claim or
an isolated performance attribution. Overall ~49%, GeoMod ~61%.

Settled lighting comparison (2026-09-15): tools/dev_geomod_lighting_compare.py
replays the same2500-frame three-blast close-up with current persistent noise
maps and the opt-in PC shadow reference. Both bakes finish, the player remains
alive and debris settles. Camera words and all640x480 encoded depth pixels are
identical. Current maps process7419 texels; the shadow reference processes9728.
Both actual PNGs were inspected: the reference is visibly darker, and neither
resolves the mound-like appearance. Restoring that old path is not supported
by this comparison. No production lighting change or Xbox replay was made.
Outputs are local in artifacts/geomod-lighting-comparison: inputs.bin, both
logs, depth buffers, PPM/PNG captures and report.json. No GitHub images added.
This harness isolates the two current lighting policies; it is not an original
visual reference and does not prove the correct relighting schedule. Authored
light admission, receiver mapping and static/dynamic relighting remain open.
Overall ~49%, GeoMod ~61%; current area is destruction lighting.

Crater light-source admission audit (2026-09-15): The opt-in completed PC light
audit now records each admitted light's type/profile, shadow mode, radius,
position and RGB. analyze_crater_lighting.py accepts both the original CSV and
new comment records, and reports geometric point-light distance coverage.
Settled three-cut GlassHouse reference: two type2/profile0 lights, radius24,
RGB(.909803987,1,1), at(0,-1.5,-12.5) and(0,-1.5,11.5), both shadow mode1.
The first is24.3616..32.0606 units from every one of9728 grid samples: none is
inside its radius. The second is17.6383..25.7819 units away, with8621 samples
inside its radius. Admission is a whole-terrain bounding-box query, so admitting
a light that cannot reach these crater samples is not by itself a loader bug.
The reference ambient is(.0784313753,.0784313753,.0784313753). Mean red is
.13993248 without visibility masks and.101047247 after ray shadows;8228/9728
samples pack to the minimum1555 value. Recovered projected masks give mean
red.09863345. All packed atlas values match recomputed shadow shading.
Blocker totals include out-of-radius lights and should not be interpreted as
actual lost light energy. These are all grid samples, including borders, not
screen-pixel coverage. Original live relighting and mapping ownership remain
unproven. The audit capture is byte-identical to the prior shadow reference.
PC build and new/legacy audit analysis pass; no production rendering change,
new Xbox replay, or GitHub image. Evidence is in artifacts/geomod-lighting-
comparison/light-sources.csv and .json. Overall ~49%, GeoMod ~61%.

Original dynamic query gate (2026-09-15): inspect_geomod_dynamic_relight_gate.py
executes4f2c79 with all256 dirty values, two mapping-inhibit values and two
states of the actual4d9fb0 global byte c96890 (1024 cases). Dirty bit1 enters
the dynamic branch regardless of mapping byte10. With c96890 zero it calls
4d9c00 using owner0, bounds at mapping+34/+40, and final arguments1,0. With
c96890 nonzero it bypasses that query and reaches4d9fd0 setup. Clear bit1
jumps to4f2f0a. The getter executes unhooked; execution stops at query/setup/
upload-path entry. Mapping owner index is fixed to-1 in this bounded oracle.
The original static-gate oracle also passes512 cases. Initial dirty8 therefore
bypasses both static and dynamic light queries; it is not evidence that later
updates are automatic, or that all crater maps should immediately be relit.
A disassembly search finds4f1fd8 writing mapping dirty1. Corrected Ghidra entry
4f1f30 first checks signed face mapping index+36, requires current dirty byte0,
transforms bounds and calls5079f0 before marking.4f1ff0 traverses a hierarchy
and calls4f1f30 for candidate faces. These are candidate scheduling semantics,
not yet full executable/reconstructed behavior verification. Trace callers and
complete this marking path before binding live updates.
An initial export mistakenly created an interior entry at4f1f40; the Ghidra
project now removes that entry and creates4f1f30, and the stale interior export
was removed. Use4f1f30.c.txt/4f1ff0.c.txt with disassembly, not the discarded export.
Evidence: artifacts/geomod-dynamic-relight-gate-original.json and corrected
analysis exports. No game source or rendered output changes this turn.
Overall ~49%, GeoMod ~61%; current area is destruction lighting updates.

Shared dynamic-map marker (2026-09-15): rf_lightmap_mark_dynamic reconstructs
4f1f30 after caller mapping lookup. A negative signed mapping index skips all
access; an already-dirty map is unchanged. Otherwise radius expands the face
minimum/maximum, and an inclusive test of the light center marks dirty1.
This is expanded-AABB admission, not a Euclidean sphere/box-distance test.
436db0/436d70 subtract/add the scalar radius; the prior candidate description
of transformed bounds was imprecise. Invalid numeric inputs preserve dirty;
these guards are explicit port policy outside the original valid-input scope.
verify_lightmap_dynamic_mark.py executes complete original4f1f30 with only
40a480 mapping-array lookup supplied. Vector initialization, radius expansion,
5079f0 bounds test, dirty gating and stores execute unhooked. All6144 cases
match the shared C probe: indices-1/0/32767, all256 dirty values, eight center
positions including inclusive limits and just-outside points. Additional
negative-radius and NaN guards preserve dirty. No allocation is introduced.
Disassembly places direct marking calls at4f1ec3 and4f2092. Caller4f1e80
chooses a flat face collection or hierarchy4f1ff0;4f1ff0 recurses through
children after bounds rejection. That full traversal and live map ownership
remain unimplemented here. The marker alone does not update a rendered map.
PC probe build and original/shared verification pass. Evidence is
artifacts/lightmap-dynamic-mark-original.json. Overall ~49%, GeoMod ~61%;
current area is destruction lighting updates, with no new visual claim.
NXDK XBE/XISO builds also pass. The new marker has not yet been exercised
in a native replay; live rendering remains unchanged.

Retained crater base-color seeds (2026-09-15): Each persistent noise mapping
now saves the original RNG state before its first texel is generated. This
allows exact8-bit grayscale base RGB to be regenerated for additive lighting,
without decoding lossy1555 or allocating another RGB atlas.1024 uint32 seeds
add4096 bytes. Seed ownership follows the existing mapping lifetime and reset.
The opt-in PC RF_REPLAY_TERRAIN_BASE_AUDIT exports map dimensions, saved seeds
and actual completed packed rectangles. verify_geomod_base_seeds.py independently
replays the CRT LCG and packing in Python, checks every texel and verifies that
each map's final state is the next map's starting seed. Eight-cut playback
passes420 mappings/22335 texels; guarded reset plus a fresh shot passes17 maps/
1472 texels with first seed1. The eight-cut image is byte-identical to the
previous retained build. No light has yet been added through these seeds.
PC build, both replay audits and NXDK XBE/XISO builds pass. This turn does not
claim a new native replay. Live light traversal, source lifecycle, marking old
and new affected bounds, map update scheduling and additive atlas application
remain open. Overall ~49%, GeoMod ~61%; destruction lighting updates.

Seeded additive rectangle (2026-09-15): rf_lightmap_noise_live_rectangle joins
the retained base seed to ordinary mapping sample reconstruction and the
verified rf_lightmap_live_pixel operation. It regenerates8-bit base grayscale
using the original CRT stream, adds selected class-light RGB, and writes a
linear1555 rectangle. It accepts1..64 texels per dimension, leaves row padding
untouched, and uses no allocation or retained-seed mutation. Scene selection,
dirty scheduling, image transfer and synchronization are still caller work.
The focused fixture checks no-light output against the original noise-fill/
packing path, a red point light increasing only red, exact base restoration
after removing the light, row padding, and short-capacity unchanged output.
An initial fixture passed pitch where the existing packer expected total RGB
bytes; corrected before the passing run. Three focused CTests pass, including
the enforced first-crater closure and repeated-cut collision coverage.
This is a shared adapter, not a recovered original function boundary. Core
pixel math remains the existing recovered implementation. There is no new
scene lighting or visual-parity claim. Overall ~49%, GeoMod ~61%.
NXDK XBE/XISO builds pass; native execution of this new adapter remains
unverified until scene integration.

Scene additive lighting integration (2026-09-15): The default persistent crater
atlas now queries current class lights using flags1,0, retains the selected
source records in its existing63-source cache, and marks maps against both old
and new light bounds. Source or terrain-generation changes recompute affected
rectangles from retained base seeds, update their hashes and schedule atlas
rectangle uploads. Removed sources therefore restore exact base colors rather
than leaving stale illumination. Segment sources conservatively mark every map;
source shading still determines their actual contribution. No new heap owner
or RGB atlas is introduced. This is practical scene scheduling over recovered
query, marker and pixel operations, not a claim of original traversal parity.

A PC-only RF_REPLAY_TERRAIN_TEST_LIGHT inserts a diagnostic warm point source
at(-14,-8,4), radius8, during frames1000..1999. It is not an authored gameplay
light. tools/dev_geomod_dynamic_cycle.py runs paired1500/2500-frame recordings,
verifies three settled cuts, no active debris, a living player, identical paired
camera/depth data, a visible light-on change, and byte-exact light-off restoration.
Actual base, lit and removed captures were inspected: exposed rock brightens,
but the unacceptable mound-like crater appearance remains. Evidence is local in
artifacts/geomod-dynamic-cycle; no README images were changed.

Three focused geometry/preview CTests pass. The default eight-cut seed audit
passes420 maps/22335 texels. NXDK XBE/XISO builds pass after fixing an unused
PC-only frame parameter warning. No new native replay has been run, and the
colored-light diagnostic is PC-only; native additive execution and real gameplay
light lifecycles remain explicit follow-up work. Overall ~49%, GeoMod ~61%.

Native additive diagnostic (2026-09-15): xemu_render_check.py now accepts
--terrain-test-light only with --dev-room. It enables the same timed diagnostic
point source on PC and Xbox, saves/restores terrain-test-light.flag alongside
other disc settings, and leaves ordinary boots with no synthetic light.
The1500-frame native lit run at artifacts/xemu/render-20260915-164312 passes
46 comparisons with8648 pages free (33.78125MiB). The actual framebuffer shows
the warm contribution on exposed rock. All8019 pixels changing by more than
8/255 in the paired PC lit/base images are closer to the lit reference in the
native capture. Native versus PC lit images have86 pixels exceeding8/255 in
any channel; this is not exact image parity. The20 saved disc entries were
restored byte-exactly and the owned emulator exited. Light-removal native
verification follows separately; the crater's mound-like appearance remains
unaccepted. No README image changes. Overall ~49%, GeoMod ~61%.

The2500-frame native removal run at artifacts/xemu/render-20260915-164500 also
passes46 comparisons with8648 pages free. Its framebuffer was inspected and
shows the unlit crater again, without the warm contribution. Its PC reference
is byte-identical to the earlier no-light control; native versus this PC frame
has86 pixels exceeding8/255 in any channel. This supports visible restoration
but does not prove byte-exact native restoration without a paired native
no-light control. All20 disc entries restored and PID45848 exited. Both native
runs use the synthetic source, not real gameplay light spawning/expiry. The
source-selection/update path now has native visual evidence; gameplay light
lifecycles and final crater readability remain open. Estimates unchanged.

Side-view investigation (2026-09-15): Existing world-normal and final-depth
evidence already establishes recession; no normal reversal was made. Added
replay/out arguments to dev_crater_depth_check.py and a folder argument to
analyze_crater_seams.py. dev_geomod_side_views.py generates two ordinary-input
1620-frame three-cut recordings, moving sideways and turning after the settled
front capture. Its generated bytes match both executed recordings. It preserves
strict depth failure gates rather than declaring these views passing.

Actual left/right cut captures were inspected. Both retain the mound-like
appearance; this is not a fidelity improvement. Identical cut/intact camera
words, completed bakes and living player/debris checks pass. The left view has
24782 recessed pixels,112 nearer-threshold pixels (minimum delta-139), and two
new uncovered pixels. The right has9727 recessed pixels,8 nearer-threshold
pixels (minimum-133), and no new uncovered pixels. The original128-unit depth
threshold and10000-pixel frontal area requirement both remain unchanged; these
failures are not proof of protruding geometry. Quantization versus real local
depth error needs separate attribution.

Exact projected triangle tests put both uncovered left pixels outside every
world triangle. Pixel(281,228) lies between triangle edges1119/1269 near a crater
junction; (499,328) lies between edges180/183 whose clipped endpoints differ at
x639.9375 versus640. These are separate concrete repros; the latter suggests a
screen-clipping mismatch but its cause is not yet proven. Evidence is in
artifacts/geomod-side-views/{left,right}. No shared engine change, native run or
GitHub image addition. Overall ~49%, GeoMod ~61%; destruction boundary repair
and appearance remain active.

Exact frustum construction (2026-09-15): Shared world clipping now sets the
constrained coordinate of each new intersection exactly to its active plane
(near/far Z, side X=+/-Z, vertical Y=+/-0.75Z). Previously interpolation could
leave an intersection just inside the right plane, project to639.999878, then
floor to639.9375 while the adjacent edge reached640. This is exact constrained
construction, not an epsilon expansion or a screen-coordinate proximity weld.
Other interpolated coordinates and attributes retain their existing arithmetic.

The new crossing-triangle regression in preview_static_tests fails on the old
source at its exact640 boundary assertion and passes on the retained source.
Three focused geometry/preview CTests pass. Front depth playback passes19985
recessed pixels, no substantially nearer pixels, no new uncovered pixels, and
minimum raw delta-71. Both side pairs were rerun; the left uncovered count falls
from2 to1 and the right remains0. Left pixel(499,328) is fixed; crater-junction
pixel(281,228) remains outside every projected world triangle. Existing side
near-depth threshold failures and right projected-area failure remain explicit;
no acceptance threshold was relaxed. The actual new left capture was inspected:
no broad appearance improvement; the dark mound-like reading persists.

NXDK XBE/XISO compilation passes. No native replay of this clipping change has
yet run. This closes the specific screen-edge construction defect, not all
frustum seams or crater topology. Overall ~49%, GeoMod ~61%; next work remains
the crater junction and destruction appearance. No GitHub screenshot changes.

Remaining render junction attribution (2026-09-15): The opt-in PC final-state
RF_REPLAY_TERRAIN_MESH_AUDIT exports terrain draw faces/corners, source IDs,
planes, world positions and UVs as CSV before owner cleanup. It adds no
per-frame allocation or Xbox code. Its captured image is byte-identical to the
ordinary left-view cut capture. The CSV is the render-subdivided mesh, not the
physical collision mesh; do not confuse those ownership domains.

At uncovered pixel(281,228), projected edges trace to face114 corner5 and
face138 corners1..2. analyze_geomod_render_junction.py restores CSV coordinates
to float32, computes double segment projection, and reports fraction
0.96893721538919, distance1.0694725709238113e-6. The current render insertion
predicate rejects that candidate because squared perpendicular error exceeds
1e-12. Residual is approximately(-1.05999516e-6,-1.04232337e-7,-9.6526994e-8).
The long face138 edge lacks the short neighboring edge's corner, leaving two
different projected segments after1/16-pixel flooring. Evidence:
artifacts/geomod-side-views/left/source.csv and junction.json. Reproduce with
analyze_geomod_render_junction.py source.csv --edge 138 1 2 --point 114 5.

This identifies the failed subdivision predicate but does not justify a global
weld tolerance increase. Shared support identity or a bounded floating-point
construction rule must distinguish this junction from genuinely separate edges.
No production geometry was changed this turn. PC build and export reproduction
pass; no new native run. Overall ~49%, GeoMod ~61%; destruction boundary repair.

Float-rounding render subdivision (2026-09-15): The render-only T-junction
predicate retains its existing1e-6 geometric test and additionally admits a
candidate whose three coordinate residuals fit the float32 rounding intervals
of the candidate and interpolated edge endpoints. For fraction t, each bound
is halfULP(candidate)+(1-t)*halfULP(A)+t*halfULP(B). Half-step values are computed
in double from float exponent bits, including subnormals. Search ranges,
endpoint guards and existing64-corner/8192-vertex limits remain in force.
Source vertices and physical collision faces are untouched; the accepted
candidate is inserted into the draw polygon with interpolated owning-face UVs.
This is a practical rendering repair, not proof of original topology or a fix
for the35 remaining six-cut physical closure failures.

The recorded missed candidate now passes coordinate bounds approximately
(1.90735e-6,9.53674e-7,2.38419e-7). The independent junction analyzer reports these
bounds alongside the rejected legacy distance gate. Both side views were rerun
and inspected; the left uncovered count drops1->0 and right remains0. Existing
near-depth threshold failures (left112/right8 pixels) and the right view's
9727-pixel area below the frontal10000 gate remain explicit. The side-view
script still exits nonzero; these are not all-green side geometry claims.
Front paired depth passes19985 recessed pixels, no nearer or uncovered pixels,
minimum delta-71. Eight-cut playback passes with3112 physical vertices and3762
render vertices (650 inserted, previously608), using the same308244-byte draw
owner. Lightmap bake and memory gates pass. NXDK compilation passes. The dark
mound-like crater appearance remains unchanged at normal viewing scale.

Native1620-frame side-view verification at artifacts/xemu/render-20260915-165844
passes46 comparisons with8647 pages free (33.77734MiB). The actual framebuffer
was inspected. Previously uncovered pixel(281,228), which held clear RGB16,16,24,
now holds rock RGB24,22,21 in both native and PC captures. The already-repaired
screen-edge pixel(499,328) also matches at53,49,43. These exact pixel checks
complement the full PC depth coverage audit; they are not complete native image
parity. All20 saved disc entries restored byte-exactly, and owned PID10056 exited.
Overall ~49%, GeoMod ~61%; destruction shape/readability and remaining topology
remain priorities. No GitHub image was uploaded.

Community lighting cross-check (2026-09-15): Inspected Alpine Faction commit
8348aebf40d597211f3a43be5eb025bd6a023310, game_patch/misc/g_solid.cpp. Its injection
at original4e5bbe handles fullbright faces by skipping the stock lightmap fill.
The accompanying author comment identifies GeoMod craters among surfaces without
calculated lighting and describes using level ambient as a future option, not
an existing stock operation. This is independent community evidence consistent
with the already executed original random-fill/dirty8 path. It is not a complete
runtime proof and supplies no stock-game visual capture.

The same source's466c00 patch adjusts crater pixel density for nonstandard image
sizes, with32 as the configured default and256-square stock bitmaps. Our current
GlassHouse rock02 is256-square; this does not explain its appearance or justify
a texture-scale change. Existing recovered blend-state evidence still supports
MODULATE2X. No automatic ambient fill, arbitrary brightness multiplier, texture
replacement or renderer change follows from this investigation. A stock-game
visual reference remains needed before calling the dark crater appearance a
specific lighting regression; physical closure defects remain independently open.

Primary source: https://github.com/GooberRF/alpinefaction/blob/8348aebf40d597211f3a43be5eb025bd6a023310/game_patch/misc/g_solid.cpp
Local read-only reference clone: local/alpine-reference (ignored by Git).
The repository is MPL-2.0; no source code was copied into the port. This check
narrows the lighting hypotheses; it is not a visual fix or a percent milestone.
Overall ~49%, GeoMod ~61%; destruction fidelity remains the current priority.

Physical edge attribution (2026-09-15): Re-ran the complete installed-template
stress fixture, retaining closure counts0,8,12,16,24,35. Invoking the fixture
without its installed-game/template arguments only runs synthetic tests; that
initial invocation was not accepted as a complete stress audit. The full run is
artifacts/closure-current.log and its summary. Existing pass criteria are intact.

The test fixture can now export the physical mesh at the selected cut using
RF_GEOMOD_MESH_TRACE and the existing1..6 RF_GEOMOD_INTERSECTION_CUT selector.
The CSV matches the render-audit column layout but contains physical corners
and collision planes, before render subdivision. Second-cut evidence is
artifacts/closure-second-source.csv. This adds no production runtime code.

analyze_geomod_edge_pairs.py finds opposed midpoint projections without first
rejecting angular mismatches. Face18 edge1 (length0.0102979306) has an opposed
candidate at face110 edge3, with midpoint separation8.15789458e-7 and angular
residual2.51024581e-8. It passes the existing distance criterion but fails the
1e-8 angular criterion. The old CLOSURE_NEAREST reported no candidate because
it applies that angular filter before recording nearest distance. This is not
evidence that an entire neighboring face is missing. It also does not establish
that all35 failures have the same cause or justify relaxing the closure test.

The exact candidate shares one endpoint with the short edge; evaluating physical
T-junction subdivision and consistent shared corner construction is the next
repair direction. Render-only insertion has already repaired the sampled pixels
but leaves physical faces unchanged. Both focused geometry CTests pass, including
all existing collision probes; later closure remains explicitly diagnostic.
No engine change or native rerun this turn. Overall ~49%, GeoMod ~61%.

Rejected physical subdivision experiment (2026-09-15): Tried inserting existing
near-edge source corners into pending physical polygons using already-owned
work scratch, before collision binding. No heap or budget increase was used.
The unrestricted prototype caused the finite-solid fixture's second cut to fail;
restricting it to cavity publication preserved finite-solid behavior but the
second cavity cut returned RF_FORMAT. The failing collision-face check was
convexity: face78 edge6 against vertex8, inward-1.49590181772e-8 versus permitted
-2.4011370129e-9, with10 corners. This is not a memory-capacity failure.

A second experimental variant validated each expanded polygon and reverted
invalid individual faces. It admitted the two-cut fixture but broke the enforced
first room-scale crater closure, introducing tiny unmatched edge intervals.
Thus applying the render-side proximity approach directly to physical geometry
is not acceptable. No collision or closure threshold was widened.

Both prototypes and temporary fixture bypasses were discarded. Source and test
files were restored byte-exactly to their pre-experiment versions; the PC player
and geometry fixture were rebuilt. All three focused geometry/preview CTests
pass again, including enforced first-cut closure. The existing later35 failures
remain. The rejected guarded prototype is local artifacts/rejected-physical-splits.c;
evidence is physical-split-rejection.log and physical-split-guarded.log.
No experimental Xbox build or native run was made. The next repair must address
shared corner construction before final polygons, not insert approximate
collinear points into physical faces after the fact. Overall ~49%, GeoMod ~61%.

Short-edge construction trace (2026-09-15): Captured second-blast intersections
through the existing observer. At(-16,-8.78587341,2.00874901), all four recorded
constructions have identical input planes/endpoints and identical float output.
The clip plane is(0.836305678,0.539372623,0.0983359963,17.9222183). This case is
not two differently rounded versions of the same generated corner. The opposed
long edge passes through another point sequence on that plane, leaving the
short edge slightly noncollinear after float storage. Evidence:
artifacts/short-edge-second.bin and its intersection.json;1192 events overall.

Tested balancing each tetrahedron plane offset around the minimum/maximum dot
products of its three source vertices, keeping rounded normals unchanged. This
minimizes the unquantized maximum residual for that chosen normal. Shared
internal-plane checks and the full fixture passed, but closure counts remained
exactly0,8,12,16,24,35. Later mesh counts changed slightly (sixth cut2306 vertices/
529 faces versus2310/531), without resolving the targeted defects. The numerical
adjustment was therefore discarded. No broader fidelity benefit is claimed.

Restored core source byte-exactly, rebuilt PC player and fixture, and reran all
three focused geometry/preview CTests successfully. No native experiment was
run. Rejected source and results are artifacts/rejected-balanced-planes.c and
balanced-planes-closure.log/.summary.json. Preserve shared supporting-edge
identity through final face assembly as the next structural repair direction;
neither another global epsilon change nor offset centering is established as
an effective repair. Overall ~49%, GeoMod ~61%; overlapping destruction geometry.

Final compacted-edge provenance (2026-09-15): Cavity supporting-plane IDs now
survive append_compact and join_polygons. Each output vertex retains its outgoing
edge ID through polygon ordering, removal of the joined seam, pending-array
compaction and final append. At the last corner copied from polygon A, the
outgoing ID comes from polygon B's continuation, not A's removed seam. Face
support IDs are retained when contributors agree; UINT16_MAX explicitly marks
mixed-support merges rather than inventing a common identity. Merge eligibility
and all geometry/UV calculations are unchanged.

The bounded multi-work owner adds4096 uint16 edge IDs and768 uint16 face IDs,
9728 bytes total. Provenance is valid for tracked cavity owners whose capacities
are at most4096 vertices/768 faces; larger generic owners keep their prior
geometry behavior and leave these metadata arrays unspecified. Finite-solid
paths remain untracked. This is supporting infrastructure, not a physical seam
repair or permission to merge nearby edges using these IDs blindly.

The original-template fixture validates every retained outgoing edge's two
endpoints against its supporting plane after one/two cuts (182/465 edges), using
the existing1e-4 provenance tolerance, and verifies authored face support IDs.
Its existing geometry equality, rollback and collision checks pass. An initial
placement of the new assertions exercised the simpler box fixture; moved them
to the intended original-template loop and reran before accepting coverage.
Eight-blast PC playback passes with unchanged3112 physical/3762 render vertices,
650 render insertions, complete lighting and existing memory ceilings.

Native1500-frame eight-blast verification at artifacts/xemu/render-20260915-171948
passes46 comparisons with8597 pages free (33.58203MiB). The actual framebuffer
was inspected; the existing dark crater shape remains, with no visual-fidelity
improvement claimed. All20 saved disc entries restored byte-exactly and owned
PID55652 exited. NXDK compilation, PC original-template edge-ID assertions and
runtime budget checks pass. Later physical closure remains0/8/12/16/24/35 failures
across the six stress cuts; edge metadata has not yet changed their geometry.
Overall ~49%, GeoMod ~61%; shared-edge construction is the next repair step.

Support-pair audit (2026-09-15): Added an optional synchronous compaction observer
at the end of bounded tracked cavity preparation. It exposes borrowed read-only
pending geometry and face/edge support arrays only during the callback, before
publication. It is disabled by default, adds no allocation, and is not a stable
view of published provenance after another edit. The fixture's
RF_GEOMOD_COMPACTION_TRACE uses the existing selected-cut option to export CSV.

The known second-cut pair is exactly(face18 edge1: face-support0,edge-support176)
and(face110 edge3: face-support176,edge-support0). Across all six captured stress
cuts, no merged face has an unknown/mixed support ID. The final2310 vertices and
531 faces group into485 unordered support pairs. Every recorded closure failure
at each cut belongs to a group containing at least three edge records, including
all35 at cut6. This establishes available matching identity; it does not prove
interval overlap, opposite winding, or a valid geometric repair.

analyze_geomod_support_groups.py provides reusable grouping with explicit scope.
Evidence is artifacts/compaction-supports/cut1.csv through cut6.csv, report.json,
failure-pairs.json and cut6-groups.json. The original-template fixture was run
for every selected cut; geometry counts remain unchanged. All three focused
CTests and rebuilt PC player pass; NXDK XBE/XISO compilation passes. No new native
replay or appearance change is claimed for this diagnostic-only addition.
Next step is support-matched interval assembly, retaining collision convexity
and first-cut closure rather than inserting arbitrary nearby vertices.
Overall ~49%, GeoMod ~61%; overlapping destruction geometry.

Support-matched split prototype (2026-09-15): Added an offline geometry workflow
that groups exact float32 endpoint tuples by unordered supporting-plane IDs,
orders candidates along each original edge's dominant coordinate, and inserts
interior candidates from that support group only. Ambiguous equal-coordinate
candidates are reported rather than silently chosen. No gameplay code is changed.
The prototype uses zero UV placeholders and material0; it is geometry evidence,
not a usable replacement mesh or a visual-fidelity result.

rf_geomod_mesh_probe reads bounded RGM1 vertex/face snapshots and invokes the
actual rf_geomod_collision_faces validator. Matching supports alone still produce
invalid polygons:7 in the second cut,29 in the sixth. The prototype replaces only
those invalid polygons with center-fan triangles, preserving every newly split
boundary segment. The added --mesh mode in rf_geomod_interior_tests applies its
unchanged geometric closure test directly to each snapshot. No distance, angular,
convexity or closure threshold was relaxed.

All six resulting snapshots pass collision-face validation and strict closure:
cut1:354 vertices/80 faces; cut2:918/197; cut3:1556/339; cut4:2242/500;
cut5:2690/595; cut6:3274/722. The original live results remain unchanged with
0/8/12/16/24/35 closure failures. The corrected offline meshes have zero failing
intervals. These are separate representations; do not describe the live game
as fixed. In particular,722 faces at cut6 approaches the768-face budget, and
no eight-cut capacity, tree-memory, ray/body, rendering or Xbox runtime acceptance
has been established for this prototype.

Tools: probe_geomod_support_splits.py, geomod_mesh_probe.c (rf_geomod_mesh_probe),
and rf_geomod_interior_tests --mesh. Evidence: artifacts/support-split-second,
support-split-sixth, support-split-cut1/cut3/cut4/cut5 and
support-split-additional-cuts.json. Original snapshots also pass the same collision
validator. Normal three focused CTests pass after the diagnostic additions.
Next: reduce subdivision overhead where possible, preserve real UV/material
attributes, implement bounded shared C assembly and validate runtime collision
and stock64MiB Xbox operation. Overall ~49%, GeoMod ~61%; overlap geometry.

Selective partitioning and eight-cut capacity (2026-09-15):
The host mesh probe now tries a diagonal that produces two collision-valid,
consistently wound polygons before falling back to a center fan. It rejects
candidate diagonals within1e-6 of another boundary corner or whose midpoint
is within1e-6 of a boundary segment. Without both guards, thin duplicate
regions passed collision binding but failed the unchanged closure checker.
No closure or collision tolerance was relaxed.

The original-template fixture accepts RF_GEOMOD_STRESS_COUNT=6..8 (default6)
and RF_GEOMOD_INTERSECTION_CUT up to the selected count. Eight live baseline
admissions, increasing signed volume, collision probes and the existing1MiB
terrain ceiling pass; baseline closure after the first cut still fails.

Reproduce repaired snapshots with tools/probe_geomod_support_splits.py
<compaction.csv> --out <directory> --partition. The new option records both
collision validation and closure, and explicitly reports runtime capacity
fit separately. Host inspection arrays permit oversized candidates solely
for measurement; runtime owner capacities were not increased.

All eight repaired snapshots pass collision validation and unchanged edge
closure: vertices/faces are318/64,828/159,1396/271,1966/383,2376/462,
2892/560,3492/684,3954/772. The sixth cut previously needed3274/722 with
center fans. The eighth baseline is3165/733; expansion creates39 invalid
polygons, and the partitioner resolves each with one extra face. Thus this
representation exceeds768 faces by4 even without any center-fan overhead.
Evidence: artifacts/support-partition-eight/summary.json and per-cut logs;
artifacts/partition-eight-stress.log contains the live baseline admission run.

This remains an offline geometry result with placeholder UVs and materials.
Shared runtime integration, transactional failure handling, real attributes,
1MiB peak ownership, Xbox execution and actual appearance remain unverified.
The focused interior, repeated-cut and static-preview tests pass. No native
run or new screenshot was warranted by these host-only changes.

Shared partitioner extraction (2026-09-15):
rf_geomod_partition_mesh now lives in src/core/geomod.c and invokes the same
private collision-face validator without host-sized position/filter arrays.
It uses caller-owned destination arrays, no heap allocation or mutable global
state, and a bounded64-vertex local polygon. Material/source IDs and existing
vertex attributes are copied exactly; the fallback center averages positions
and UVs in double precision. The returned view preserves input generation.
Output arrays are disposable scratch on errors; the result view changes only
on success, so pending-edit callers must not pass live buffers as destinations.

The mesh probe delegates to this shared implementation. All eight resulting
RGM1 snapshots are byte-identical to the previous closed offline outputs,
including3954 vertices/772 faces at cut8. A retained real ten-corner crater
fixture verifies two-piece binding, nonzero UV/material/source preservation,
all boundary vertices retained, unchanged input, RF_RANGE at face/vertex limits,
and unchanged result view on failure. An eight-corner star exercises center-fan
UV interpolation and winding through real collision binding. The focused
interior/repeated-cut/clipping tests pass; PC player and NXDK XBE/XISO build.

This is a shared-code integration step, not a live geometry fix. Support-pair
edge assembly is still in the host experiment, and no new game/XEMU execution
was performed. Next: consume compact provenance while preparing pending CSG,
reuse dead CSG scratch for the replacement, and validate admission/abort plus
peak memory before publication. The four-face overflow at cut8 remains open.

Live support-matched repair and Xbox verification (2026-09-15):
Pending cavity assembly now inserts exact endpoints from edges carrying the
same unordered supporting-plane pair, ordered by the dominant edge coordinate.
Duplicate positions are removed; conflicting equal fractions reject the edit.
Each face interpolates inserted UVs from its own original boundary edge, then
uses the shared collision-valid partitioner. All output is assembled in dead
clipping workspace before replacing the pending bank. Live/source banks remain
untouched on failure. The workspace union adds no geometry allocation; expanded
provenance and cached bounds add1280 bytes. Provenance observers run before
repair and borrow pre-repair metadata; tests now validate it at that point.

Repair applies to tracked cavity owners up to4096 vertices/1024 faces; larger
owners retain their prior generic behavior. Cached joining extends to800 faces.
The actual scene uses800 faces consistently across terrain, draw, atlas and
lighting allocations, retaining the1MiB terrain and320KiB draw ceilings.

Eight ray-placed live admissions now REQUIRE unchanged strict closure after
every cut, instead of merely printing later failures. Cut8 is3956 vertices/
773 faces, peak1020708 bytes. Counts after cut3 differ slightly from offline
snapshots because subsequent rays now query the repaired live tree. The768-face
control owner rejects cut8 with RF_RANGE and preserves its seven-cut vertices,
faces, generation and collision coverage byte-for-byte. Evidence:
artifacts/live-support-eight.log. All six rebuilt GeoMod/clipping tests pass.

The1500-frame actual PC developer-room replay admits eight cuts, uses3900
physical/render vertices with zero extra render-only seam insertions, terrain
peak1034912 bytes and draw owner311188 bytes. Its469 generated noise maps occupy
23394 atlas texels. The paired three-cut depth comparison has19487 substantially
recessed pixels, zero substantially nearer pixels and zero new uncovered pixels;
minimum raw coplanar delta remains-71. This is one view, not universal coverage.

Stock64MiB XEMU render-20260915-175409 completes1500 frames and all46 PC/Xbox
comparisons, with8578 free pages (33.5078125MiB). Native framebuffer inspected:
the wall, floor, weapon and dark crater render, but the mound-like appearance
is still unresolved. No original-game appearance parity claim. PID49712 exited;
all20 disc entries match their saved bytes/absence. No GitHub images added.

Next is crater readability and broader cut/view coverage. Side-view depth
threshold failures from earlier work still require revalidation. Arbitrary
campaign destruction, material/lighting fidelity and edit stalls remain open.
Estimate: overall~49%, GeoMod~63%; current area is destruction geometry and
appearance. This increase reflects live closure/rollback and Xbox validation.

Side-view depth attribution after live repair (2026-09-15):
Both process-local side replays now render zero newly uncovered pixels. Strict
128-unit depth gates still fail: left has254 nearer pixels (minimum-140) and
right8 (minimum-133); the right view also has9727 recessed pixels, below the
existing10000 frontal-view count gate. No thresholds were relaxed.

Every flagged pixel was inspected by tools/analyze_geomod_nearer_pixels.py
against all emitted world triangles, reproducing the raster winner and checking
its depth within4 float-operation rounding units. All254 left and8 right pixels
come from material0/lightmap0 floor triangles in both cut and intact views.
They do not come from the dark generated crater material. Actual captures were
inspected: the crater still looks mound-like, especially from the left.

The replay now exports the actual terrain source planes/positions beside the
projected meshes. Its authored upward floor is exactlyY=-12. The emitted
reciprocal depth and that plane reconstruct each affected triangle vertex's
pre-rounding screenY: all residuals lie within the expected0..1/16 pixel floor
operation. Analytical depth for this camera/floor gives a maximum196.5974-unit
screen-rounding displacement. Both intact and cut depths fall within that bound;
left cut errors relative to the exact floor range-191.98..-168.71 versus
-62.98..-39.71 intact. Right values are similarly bounded. This supports floor
triangulation/projection rounding as the cause of these nearer-depth flags,
not cavity geometry protruding toward the camera. It does not prove arbitrary
world geometry or all projection noise harmless.

Evidence: artifacts/live-support-side-provenance/{left,right}/nearer-surfaces.json,
report.json, *.terrain.csv and actual cut/intact PNGs. The side-view harness now
accepts --out to preserve earlier evidence and runs this attribution alongside
seam checks, while retaining its nonzero strict-gate exit. No engine behavior
changed and no new native run was needed. The front depth check and prior native
eight-blast validation remain as recorded above. Crater material/lighting cues
and a stock-game appearance reference are next; no percentage increase for this
diagnostic clarification (overall~49%, GeoMod~63%).

Authored projectile-light inputs (2026-09-15):
The installed Rocket Launcher weapons.tbl entry declares $Glow true, inner
radius1, outer radius3 and RGB{100,50,100}. Its following muzzle-flash light
is a different white source with radii4/7; it must not overwrite projectile
fields. The shared explosive decoder now retains the optional glow block,
validates ordered fields and byte colors, and normalizes RGB to0..1. Absent or
false glow leaves zeroed fields; malformed/duplicate blocks preserve output.
No heap allocation added; the retained definition grows24 bytes.

Installed rocket/grenade tests and seven malformed glow cases pass, including
a truncated block that must not borrow the following muzzle light's radius.
A valid fixture also includes the full following muzzle block and retains the
purple projectile values. PC player and NXDK XBE/XISO builds succeed; no
new native run was needed for unconsumed metadata. Light registration,
movement, removal, original inner/outer attenuation and thruster placement are
not yet connected; this metadata change has no visible lighting effect. Base
crater noise and brightness remain unchanged. Original impact-flash/particle
recipes and projectile glow are separate lifecycles, not interchangeable.

Reference search found the historical Glass House page and screenshot link:
https://www.redfactionwiki.com/w/index.php?title=Glass_House&oldid=4343
https://www.redfactionwiki.com/w/images/b/bb/0412_screen000.jpg
The page identifies the map, but image retrieval was unavailable in the current
web tool. No visual comparison, patch-version identification, or appearance
acceptance is claimed from search metadata. Do not treat this as the missing
verified stock-game capture. No remote screenshot was added to the repository.

Current area: authored gameplay lighting for GeoMod. Overall~49%, GeoMod~63%.

Live rocket glow on generated crater surfaces (2026-09-15):
Original4c7b23..4c7b81 creates a point light at4d8ed0 using outer radius,
enabled/intensity, RGB, dynamic1, shadow-condition1 and linear profile0.
The stored inner radius is not passed. The bounded Unicorn harness
inspect_weapon_glow_constructor.py executes the actual argument setup under
the installed RF.exe SHA, with five enabled/inner-radius cases. It stops before
allocation, so it does not prove original movement, destruction or rendering.
Community rf/weapon.h and rf/gr/gr_light.h names guided address selection;
implementation comes from the executable observation, with no copied patch code.

rf_weapon_projectile_light converts an active flight plus decoded weapon into
the existing shared point-light descriptor, with no allocation. Disabled/inactive
flights return NOT_FOUND without changing output. The scene adds these sources
to the generated-crater dynamic-light cache each frame. Position changes rebuild
affected rectangles; impact/expiry removes the source, and the existing retained
base-seed path restores the previous lighting. No permanent crater brightness
gain or extra light pool allocation. Authored static world surfaces, actors,
thruster-specific offsets/flicker and impact/muzzle effects remain separate work.

The dedicated paired replay harness accepts a pre-integration PC executable
(--baseline), records both executable and input hashes, and compares550/560-frame
runs. At550 an active third rocket changes3798 pixels within[223,143,347,318];
depth and selected body/rocket/terrain state match the control. At560 the rocket
has impacted and RGB/depth are byte-identical to the unlit control. Actual images
were inspected. Evidence: artifacts/projectile-glow/pair/report.json plus captures.
This establishes the selected flight/removal states, not every animation frame.

Stock64MiB XEMU render-20260915-181613 completes the illuminated550-frame run,
passes46 PC/Xbox checks, and ends with8642 free pages. All1086 PC pixels changed
by more than4 color levels are closer in the native capture to the lit control
than the unlit control. Native pixels elsewhere are not identical to PC, so no
whole-frame pixel parity claim. Framebuffer inspected; PID43488 exited and all20
disc entries were restored byte-for-byte/absence. No GitHub images uploaded.

The crater is still dark and mound-like after the rocket disappears. This is
restoration of an authored transient light, not acceptance of destruction
appearance. Current area: gameplay lighting around destruction. Overall~49%,
GeoMod~64%, pending broader lighting and visual-parity work.

Post-impact native follow-up: render-20260915-181812 completes560 frames with
all46 checks and8642 free pages. Framebuffer inspected. In the selected
crater region x180..354/y130..324, one pixel differs from the unlit PC control
by more than8 color levels (maximum40); this is not exact native pixel parity.
PC light removal is byte-exact as above. PID59540 exited and all20 disc entries
were restored byte-for-byte/absence. Constructor/movement/inactive/error unit
checks pass together with installed rocket/grenade definitions. Both native
runs use the new NXDK build. No stock-game visual-parity claim.

Rocket impact resource preparation (2026-09-15):
Installed vclip rocket_impact carries code_explode and selects explosion recipe
"rocket hit", with six central emitters and2.0 seconds play time. The optional
20-spark reference "explosion random bits 2" is absent from emitters.tbl; only
"explosion random bits" exists. The existing resolver therefore returns mask63,
with six central slots and no optional sparks. No replacement emitter invented.

New rf_explosion_materials retains all resolved slots through the shared particle
animation loader, deduplicating case-insensitive filenames. Six central slots
share four images: JustFire.tga64x64, JustSmoke.tga64x64, Fire01.tga32x32 and
explosionflare01.tga128x128. The complete owner uses102704 bytes, inside an explicit
128KiB test budget. Bindings preserve absent slots as UINT32_MAX. Frames remain
valid after archives close and source definitions are overwritten; no frame-time
allocation is needed. Resource errors release partial images and preserve output.

Installed-resource tests verify actual vclip/recipe linkage, the missing optional
reference, all resolved bindings, duplicate reuse, one-byte-short budget failure,
a missing last-slot image after earlier successful loads, and repeatable cleanup.
The test can export first-frame RGBA for native image inspection. All four outputs
were inspected: fire and smoke carry variable alpha, while Fire01 and the flare
have alpha255 and black backgrounds, requiring their authored glow blend modes.
Evidence: artifacts/explosion-materials/test.log and texture-*.png. This is source
asset/resource validation, not an in-game blast screenshot or effect acceptance.

Runtime work remaining: convert scaled definitions into emitter templates, create
all eligible central emitters at actual impact, run process/release actions against
the existing particle pool, bind these texture indices during rendering, and verify
expiry/overlap plus stock64MiB output. Audio, optional missing-spark policy and other
impact effects remain open. No permanent crater appearance change this turn.
Overall~49%, GeoMod~64%; current area is destruction impact effects.

PC resource tests and PC player/NXDK XBE/XISO builds pass. No new XEMU run:
this owner is not connected to live impact rendering yet.
