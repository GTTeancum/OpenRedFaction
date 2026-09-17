# Extracted piece shape and subdivision admission

`tools/probe_geomod_piece_shape.py` executes full original466550, including
its actual vector helpers, then the subdivision branch4667a0..4667f4/4669f7.
108 fixtures cover extent permutations, equal dimensions, aspect threshold3,
width threshold10, radius below/at1.5, and batch attempt counts0/9/10.
`rf_geomod_piece_shape_get` matches all five shape float words and the decision.
The shared helper rejects nonfinite/degenerate bounds without changing output.

Shape stores a unit principal axis, longest extent and longest/shortest aspect
ratio. Strict comparisons give ties to Y over X, and Z over either. Extent
subtractions and the aspect ratio are stored floats. The admission division
uses the stored longest extent and stored aspect, not the original short side.
Subdivision requires radius>=1.5, fewer than10 attempts in this batch, and
either aspect>3 or longest/aspect>10.

Inspection of4666a0 changes the interpretation of the pool from the dispatch
note: these are temporary subdivision work entries, not persistent physics
bodies. It optionally constructs a cutting solid and runs further CSG, extracts
subpieces and requeues smaller results. Terminal pieces pass to466440, and the
work entry returns to the free list.466440 builds a descriptor and calls4130b0.
The subdivision CSG and final object-construction services have not yet been
executed by this new probe; their detailed lifecycle remains open.

Validation: Release geomod_disconnected passes the108 binary fixtures, invalid
output-preservation cases and the existing actual-cut composition checks.
Stock-profile NXDK build is checked separately. No new live destruction or
visual acceptance is implied. Core selection/removal, mapping relocation,
subdivision and final body publication still need integration.
