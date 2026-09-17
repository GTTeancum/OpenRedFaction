# Owned extracted geometry and mass-center physics

`rf_geomod_piece_bank_append_physical` now stages the original-derived solid-grid
mass on the bank's copied, bounds-centered collision polygons. It subtracts the
sampled mass center from mesh corners and bounds, adds it to world placement,
and rebinds collision polygons before publishing the entry. The conservative
mesh radius follows the original radius-plus-center-length update. The original
birth radius is retained separately for the descriptor coefficient.

Mass/grid state resides in the bank's fixed allocation. Append does not allocate.
The previous geometry-only append remains available. Failed mass preparation or
collision binding does not publish a piece or disturb existing entries.

`rf_geomod_piece_body_open` converts the prepared grid to collision spheres and
opens an independently budgeted physics body with original birth flags8000003f,
identity orientation, zero velocities, prepared mass/inverse tensor, caller's
resolved material elasticity/friction and birth-radius-times-stored-float0.2
coefficient. Caller owns body close, scheduling, rendering and registration.
It deliberately does not substitute a fallback sphere for an empty solid grid.

## Checks

- The four-cut extracted replay and reload now use physical append at density2.5.
  Both rounds produce identical geometry, placement, mass/grid state, body state
  and ordered collision spheres.
- First extracted box:32 spheres, mass10000.3, body ownership1092 bytes on PC.
- Thin second box:0 spheres, mass1500.13, zero inverse tensor and324-byte body.
  This tests the original empty-grid behavior, not usable thin-piece motion.
- Unequal separated boxes shift mass center by approximately-0.666684 on X;
  all48 world corners and all12 rebuilt collision planes retain alignment.
- Negative density and insufficient body budget reject without publishing.
- Existing136 transformed ray/sphere contacts and5 misses still pass, including
  the post-failed-append check. Four-entry bank now uses8856 bytes on PC.
- Existing15 original mass fixtures and96 grid-sphere fixtures pass.
- Stock-profile NXDK build passes:piece-mass-owner-xbox.log.

## Remaining

Production chronological extraction is still disabled. Integrate subdivision
and thin/empty-grid handling, live scene body scheduling, collision response,
materials/atlas ownership and dynamic rendering. These checks are standalone
composition and build evidence, not live Xbox motion or visual acceptance.
