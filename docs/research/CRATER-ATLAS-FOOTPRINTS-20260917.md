# Live crater atlas footprint audit

The optional terrain base audit now records actual projected corner extrema in
atlas texel coordinates, using the same rf_lightmap_project helper as rendering.
The offset is UV * 512 - 0.5, matching bilinear texel-center addressing.
No gameplay or rendering behavior changes; collection runs only when exporting
the completed PC diagnostic atlas.

`python tools/check_crater_atlas_footprints.py artifacts/crater-effect-decay/900`
checks all generated physical-face and corner counts against their map owners,
rejects overlapping atlas rectangles, and bounds both bilinear taps using each
map's live projected extrema. For affine projection over convex faces, corner
extrema also bound the face interior. GPU interpolation precision and original
mapping allocation/grouping are not proven by this check.

Current ordinary two-blast Glass House result: 103 generated faces, 500 corners,
72 allocated maps, 71 referenced maps. All footprints remain inside owned tiles;
minimum margin is 0.499755859 texel. Moving one used map's minimum U to a quarter
texel outside its rectangle is rejected by the same checker. The corrupt CSV
exists only in ignored artifacts and was never loaded by the game.

The refreshed 550/900-frame comparison still has identical camera, material,
physical mesh and complete atlas, including these new extrema. This excludes
atlas-neighbor leakage at the shared projected-corner level in this view; it
does not resolve the dark appearance or justify brightening. Diagnostic PC
build succeeds; there is no new Xbox acceptance claim for this tooling-only
change. Previous actual endpoint inspection remains applicable: the persistent
surface bytes are unchanged. Evidence: atlas-footprints.json and atlas.csv in
artifacts/crater-effect-decay/900, plus the enclosing report.json.
