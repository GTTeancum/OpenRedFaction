# Public terrain UV regression

Built and ran rf_geomod_uv_public_lineage_probe against the normal rf_core library.
The synthetic closed-solid/cavity test executes both committed cuts through public
terrain APIs; it does not include geomod.c or invoke a private mapper.

All16 cases accepted both edits;39 surviving exact-position/plane/material corner
comparisons found10 UV changes. Example: (1.25,9.25,-1) changes from
(-0.125,0.15625) to(1.15625,-0.125) after the second cut. The log is
artifacts/authored-post-live/uv-public-lineage.log. This is numeric reproduction,
not evidence of that exact corner appearing in an installed level.

The executable intentionally returns1 for a counterexample and is not a passing
CTest target. The production fix remains open. Current terrain_prepare rebuilds
all cutter faces and terrain_map_pending reprojects every generated face from its
new rounded normal. History import invokes the same full rebuild. Correcting only
live edits would therefore disagree with reloads. Preserve birth-generation UV
through chronological clipping, with transactional reconstruction and existing
support-plane identities; do not merely change the axis tie epsilon.

## Face lineage infrastructure

Added optional internal geomod_face_lineage scratch (2048bytes) and tagged
compaction/repair entry points. Different birth tags cannot merge; same-birth
compaction moves tags with removed face indices, and repair propagates each tag
to all concave partition outputs. Existing full-union callers pass NULL and
allocate no lineage scratch, preserving current production behavior.

geomod_face_lineage tests same-birth merge, differing-birth exclusion, index
movement, ordinary repair and one-to-many concave partition propagation. It and
five rebuilt geometry tests pass (polygon split, interior faces, repeated-cut
coverage, transaction storage, history check). The public UV probe still reports
16 accepted cases/39 comparisons/10 changed corners. This is a prerequisite,
not a texture fix or Xbox acceptance. Chronological old-face clipping, retained
support-plane provenance, new-face projection and transactional history replay
remain to connect. Existing resident-memory accounting is unchanged until a
caller opts into the separately owned scratch.
