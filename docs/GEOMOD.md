# GeoMod implementation status

Core gameplay in the developer room is the current priority. This milestone
adds authored region decoding; it does not implement terrain subtraction,
interior surfaces, broken glass, debris, collision changes or a usable GeoMod
weapon. Do not infer destruction support from the room's rendered glass.

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
