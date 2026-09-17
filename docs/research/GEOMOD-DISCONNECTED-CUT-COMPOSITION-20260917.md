# Actual disconnected-cut composition

The PC `geomod_disconnected` test runs the production terrain cutter against
an outward box spanning -10 to 10. For each principal axis, a cutter with
half-extents 1 along that axis and 12 along the other axes removes a complete
central slice. A second slice at coordinate 5 separates a third component.

The actual resulting meshes, rather than hand-authored component fixtures,
pass through the shared connectivity, original-derived orientation classifier,
and vertex placement helper. Every component classifies outward. Recentered
vertices reconstruct their original world positions within 0.00001 unit.

Independent triangle signed-volume integration agrees with each rectangular
component's bounds. Total remaining volumes are 8000, 7200 and 6400 for zero,
one and two cuts. Collision rays starting in each opening hit the neighboring
solid one unit away in both axial directions; transverse rays pass completely
through the removed slice without a hit.

The test serializes the one-cut history, reloads into another terrain, and
performs the second cut on both instances. Mesh bytes, collision state,
component classification, volume and opening queries agree.

Validation: Release target `rf_geomod_disconnected_tests` builds and CTest
`geomod_disconnected` passes. This is a CPU-side composition test, with no new
Xbox execution or visual acceptance. It does not extract/remove components,
create dynamic bodies, relocate face/lightmap ownership, or publish object
notifications. Those remain production integration work. These axis-aligned
fixtures do not establish arbitrary concave/oblique component correctness.
