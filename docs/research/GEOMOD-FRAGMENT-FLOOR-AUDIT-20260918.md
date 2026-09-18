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


## Rejected corner-sphere experiment and strict contact gate

A local candidate augmented the original collision spheres with unique rendered mesh corners, in batches of64 spheres with radius0.002. It reused `campaign_physics_body_sweep`, including mover/material resolution, and chose the earliest contact without changing grid spheres or mass. Fresh92 replay and save-continuation comparisons passed, but actual floor measurements worsened: vertex gaps became-0.117291689 and-0.485071152. This is a failed contact fix despite matching replay state. The candidate was removed from runtime source and the original92 histories were regenerated. Failure evidence is retained locally in `artifacts/fragment-corner-candidate` and `artifacts/fragment-corner-audit.log`.

The body sweep uses the current orientation and translation; it does not sweep corners through the proposed angular pose. Meanwhile `rf_physics_solid_advance` commits the full incoming-step predicted orientation even at a fractional linear contact. That behavior is explicitly recovered and tested against the original binary (GEOMOD-SOLID-ADVANCE-20260917.md,128 original cases). Adding more translational point samples therefore does not supply full-pose collision handling. These observations identify a missing part of the attempted correction, not proof that every resting error has a single cause. Do not alter the verified shared advance routine blindly to make this fixture green.

`tools/check_fragment_support_audit.py --max-penetration .005` is now an explicit acceptance gate. It writes the complete diagnostic report first, then exits1 for missing actual vertex-floor evidence or any vertex below its finite floor surface beyond the supplied tolerance. This gate checks penetration only, not positive hovering gaps or arbitrary polygon contacts. Its default diagnostic-only behavior remains available. Nonfinite/negative tolerances reject.

After reverting the candidate, the complete92 connected replay again passes, the audit preserves both groups' prior save bytes, and the strict gate reproduces exactly the original two failures (-0.106614828 and-0.0618112087). Thus the new gate is intentionally red until a real correction passes. No gameplay fix ships in this turn, and the Xbox runtime was never rebuilt with the rejected candidate. Next work requires a fragment-specific contact policy that accounts for proposed angular pose and finite surfaces, with collision/state/restore verification; a render-only lift or unconditional floor snap is not an acceptable substitute.


## Contact-step rotation isolation

Added a PC-only, opt-in `RF_REPLAY_FRAGMENT_CONTACT_TRACE` diagnostic at the fragment query callback. It measures each mesh vertex against finite upward-facing world surfaces using the same bounded vertical rays as the endpoint audit. Three poses are sampled: incoming pose, real backed-off contact translation with incoming orientation, and the same translation with the full proposed orientation. Translation is obtained by calling `rf_physics_contact_advance` on a private body copy, preserving the original 0.05-unit backoff and float store boundaries. The trace does not modify the simulation body, contact material, fraction, or shared advance routine. It is excluded from the native Xbox compile.

`tools/check_fragment_contact_trace.py` runs the existing first/second-junction inputs for both groups, comparing each output checkpoint byte-for-byte with its untraced baseline. All four comparisons pass. The report and individual contact records are in `artifacts/fragment-contact-trace/report.json`.

| Replay | Contacts | Incoming minimum gap | After translation | After proposed rotation |
|---|---:|---:|---:|---:|
|92 first junction, largest rotation drop|17|0.008570433|0.008570433|-0.058114529|
|92 second junction, largest rotation drop|18|0.009123564|0.009123564|-0.053566933|
|108 first junction, largest rotation drop|34|0.157274127|0.157274127|0.024361014|
|108 second junction, only contact|1|0.001445651|0.001445651|0.001445651|

The contact fractions of the two west examples are0.513414979 and0.217607647. Contact backoff leaves their translations unchanged, while applying proposed rotation introduces actual finite-floor penetration. This directly demonstrates an angular contribution in these sampled contacts, rather than merely inferring it from endpoint images or sphere extents. The minima can belong to different vertices, and the bounded ray sample count can change as vertices rotate into range; these measurements are not a continuous angular sweep or full polygon intersection proof. They do not establish that rotation explains all final penetration or every shape.

Validation: four traced PC replays preserve exact baseline saves; all123 PC tests pass; stock NXDK build/link/XBE/ISO succeeds. No new XEMU session or visual correctness claim is made. No gameplay correction ships here. Next implementation remains a fragment-specific full-pose contact policy with finite-surface handling and saved-continuation validation, preserving the separately verified shared solid-advance contract.
