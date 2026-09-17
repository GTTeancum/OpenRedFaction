# Clipping cap polygons to remaining volume

`rf_geomod_polygon_clip_solid` is a bounded shared primitive for the replay
integration problem described in GEOMOD-EXTRACTION-REPLAY-INTEGRATION-20260917.
It partitions a convex candidate polygon by the actual solid's face planes,
then uses oriented triangle solid angles at each cell centroid to classify
volume membership. This supports nonconvex boundaries, cavities and separate
closed components without replacing the remaining geometry by a convex hull.
The centroid is used only after plane partitioning, not to discard portions
of an unsplit polygon crossing a removed region.

The caller can retain inside or outside cells. Boundary cells are omitted in
both modes. The existing polygon splitter preserves UV interpolation and its
1e-5 plane tolerance. The caller guarantees convex faces, closed consistently
oriented topology and a convex candidate polygon. Nonintegral winding outside
the acceptance tolerance returns a format error; this is not a repair routine.
The implementation is a practical geometric reconstruction, not a claim of
matching original binary arithmetic.

Memory: two caller-owned banks of vertices and fragment ranges; no allocation,
recursion or growth. Intermediate partition overflow returns RF_RANGE before
changing the result descriptor. Scratch may change. Global face-plane
partitioning may create many cells; production budget/performance measurement
and supporting-plane ID propagation remain required before live hookup.

PC geomod_disconnected verifies inside/outside area complements, UV interpolation
and capacity failure for actual cut meshes: retained rectangular components,
queries outside them, two/three disconnected components (areas360/320), an
L-shaped remainder (391), a notch section (340), a hollow solid (384), and a
rotated box (400). Retained queries run on all three principal axes. Expected
areas are independently derived from the fixture dimensions. These are cross-
section tests, not proof of a complete watertight CSG transaction.

Release test passes and stock NXDK compile produces default.xbe and the ISO.
The chronological builder does not call this primitive yet. Integration still
needs retained-volume ownership, support provenance, component selection per
prefix, save/reload reproduction, atlas transfer, rollback and body creation.
No runtime resurrection fix or new visual behavior is claimed yet.
