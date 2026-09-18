# Resting fragment mesh versus floor contacts

The connected side-beam cases agree between PC and Xbox, but replay equality alone does not establish correct resting contact. Added an opt-in read-only endpoint probe (`RF_REPLAY_FRAGMENT_SUPPORT_AUDIT`) and `tools/check_fragment_support_audit.py` to measure actual mesh and collision-sphere poses against the live composed world.

The probe transforms local mesh vertices and sphere centers by the body's current orientation/position. Downward sphere-center rays (8units, collision flags0x460) identify nearby upward-facing support planes. It reports both the closest sphere gap and the minimum gap across all spheres, avoiding the mistaken inference that the closest sphere describes the deepest overlap. Separately, every mesh vertex casts a1unit vertical segment starting0.5units above it; this checks the actual floor at that vertex's X/Z rather than an unbounded plane extrapolation. Negative gap means below the hit floor; positive means above it. Missing hits remain explicitly identifiable. This is not a general polygon intersection or continuous-contact solver.

The script loads the actual both-junctions-cut saves, advances301neutral frames, audits the endpoint and compares the full output checkpoint to the previously verified unaudited continuation. Both groups preserve the exact save bytes, and all five live pieces are sleeping. No body or geometry mutation is performed; only serialized collision-query scratch is borrowed.

| Beam group | Batch/piece | Spheres | Mesh gap at actual floor | Minimum sphere gap to selected plane | Actual floor face |
|---|---|---|---|---|---|
|92|0/0|14|-0.106614828|-0.008299232|1333|
|92|1/0|56|-0.061811209|-0.033142090|1334|
|108|0/0|64|0.001445651|0.001445651|1301|
|108|0/1|16|0.039094329|-0.038445115|1301|
|108|0/2|16|0.016676426|-0.037258387|1309|

The west pieces genuinely have vertices below the floor at those positions; this is not merely an image interpretation. The east pieces have small positive mesh clearances despite slightly penetrating sphere approximations on two pieces. Floor planes areY-2 for the west pieces andY-1.5 for the east pieces. Units are game units; no metric conversion is claimed.

Original-derived grid spheres use occupancy-dependent radii in `rf_physics_grid_spheres` (49ee3d path): radius is half-spacing multiplied by occupancy/4. This explains why sphere and mesh extents need not coincide, but it does not establish that these final resting discrepancies are faithful or acceptable. Separate the proxy-shape approximation from any integration/contact penetration before choosing a correction. Do not move only rendered vertices or blindly snap an entire body to an infinite floor plane.

Next implementation: qualify a geometry-aware terrain contact correction for fragments while preserving finite world surfaces, nearby obstacles, state publication and save continuation. Keep the existing sphere/mass provenance; do not treat diagnostic green output as proof that contact is correct. The observed penetration remains unfixed in this change.

Validation: both installed replay audits pass and preserve checkpoints (`artifacts/fragment-support-audit/report.json`). All123 PC tests pass (`artifacts/fragment-support-audit-tests.log`); stock NXDK compile/link/XBE/ISO succeeds (`artifacts/fragment-support-audit-xbox.log`). The endpoint audit currently runs from the PC replay callback; no new native runtime claim or screenshot is made. Prior native matching checkpoints supply the source poses, not a substitute for future native correction validation.
