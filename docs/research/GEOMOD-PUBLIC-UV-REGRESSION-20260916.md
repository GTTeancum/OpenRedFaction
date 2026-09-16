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
