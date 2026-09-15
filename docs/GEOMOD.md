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
