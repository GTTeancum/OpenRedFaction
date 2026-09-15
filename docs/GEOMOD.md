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
