# Extracted-piece vertex placement

Added rf_geomod_piece_recenter for4d1330's vertex/bounds phase. It computes
vertex bounds, expands each side by stored float0.0001, adds the padded bounds
with a float store, and halves that result for the world origin. It subtracts
the origin from vertices and padded bounds, computes the maximum squared local
vertex length with the original float storage, then takes its square root for
radius. This is bounds-midpoint placement, not a mass or vertex centroid.

`python tools/probe_geomod_piece_placement.py` executes complete original4d1330
with eight supplied vertices and an empty face list, without service hooks.
Twelve cases vary size from milliscale to multiunit and center from zero to
65536. Original array access, bounds expansion, placement, vertex subtraction,
radius math and local-center zeroing all execute. It does NOT exercise face-plane
or mapping relocation because the face list is empty.

The generated raw-word fixtures compare all10 placement values and24 local
vertex coordinates to the C helper. Separate and exact in-place output both
pass, along with a last-vertex NaN rejection that preserves every output.
Validation precedes all writes; the helper allocates no memory. PC
geomod_notify CTest passes. Original RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Evidence: artifacts/geomod-postedit-re/piece-placement.json and placement-build.log.

No live extraction is connected yet. The rest of4d1330 rebuilds face planes and
adjusts mapping bounds/projection after translation; that must be integrated
with shared-vertex component extraction before replacing live geometry. Existing
base UV and lightmap ownership must survive the transfer. Empty input is an
explicit port error; the original empty-list no-op is outside this nonempty
helper contract. This helper alone is not a complete extracted-piece owner.

Stock-profile NXDK compilation/link succeeds (placement-xbox-build.log). No native execution or rendering acceptance is claimed before live integration.
