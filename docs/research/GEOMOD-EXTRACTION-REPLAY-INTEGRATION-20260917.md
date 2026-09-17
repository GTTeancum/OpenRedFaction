# Extraction integration: replay constraint and local geometry

Source inspection identifies two requirements before enabling extraction:

1. `terrain_prepare_chronological_mesh` starts every edit/import from immutable
   source bank2 and replays all committed cutters. Extraction decisions must
   therefore be reproduced at the same chronological point, with stable labels
   or an equivalent validated persistent representation.
2. `prepare_chronological_step` retains prior active surfaces, but generates
   the new cutter's cap polygons against the immutable source planes and prior
   cutter volumes. Deleting extracted faces from a prefix alone is insufficient:
   a later cutter inside discarded material can generate cap surfaces there.
   The cap construction must account for discarded volume/current solid as well.

These are source-derived constraints, not an executed resurrection test. A
centroid-only face filter, a final largest-component pass, or silently replacing
the immutable source with a convex hull would not preserve the required solid.
The eventual validation must cut inside a previously removed piece, cross its
old boundary, repeat after reload, and test rollback without recreating faces.
Multi-room ownership, concave pieces and support-plane remapping also remain.

The local-geometry staging step is now implemented by `rf_geomod_mesh_recenter`.
It shares the original-derived placement arithmetic with `piece_recenter`,
operates directly on20-byte mesh corners, and preserves UV bytes. Exact in-place
operation uses constant scratch rather than a separate12-byte-per-corner
position array. Collision must be rebound after translation; atlas projection
ownership is still separate and is not claimed to be transformed.

Validation: all12 original placement fixtures match for both float3 and mesh
corner layouts, separate and in-place outputs. Late-invalid input preserves
outputs. Real extracted cut components pass local/world reconstruction and
collision-plane translation checks with unchanged UVs. PC geomod_notify and
geomod_disconnected pass; stock NXDK builds default.xbe and diagnostic ISO.
This step is not yet wired into scene destruction, and no visual acceptance
or native runtime behavior change is claimed.
