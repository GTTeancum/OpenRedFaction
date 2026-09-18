# Broader authored solid neighborhoods and side beams

The read-only `tools/inspect_geomod_solid_neighborhoods.py` census transforms retained editor brushes with the loader's float stores, resolves compiled faces through authored face tokens and compares AABB candidates in serialized brush order. It uses the existing local `inspect_geomod_source_topology.py` research helper and exported `artifacts/future-vehicles-re/ctf06-editor-brushes.json`, plus installed geometry/inventory. Its artifact includes geometry/export hashes. AABB candidates are not exact intersections or destruction permission.

The955-brush export has41 flags0 structural owners with340 compiled room3 faces. All41 authored sources are closed, oriented and convex. Some cross rooms; many overlap detail/portal/air records. Convexity alone does not authorize publication. The census reproduces all six previously admitted post/beam neighborhoods exactly before reporting additional candidates.

Pillar12815 is flags4 (detail), not an ordinary flags0 structural source. It has no face ownership in the parsed static compiled geometry. This does not prove it is absent from other detail representations or can be discarded. Existing binary evidence in TERRAIN-DETAIL-FACE-CLASSIFICATION-20260915.md distinguishes detail-owner byte from invincibility and does not establish a general delete/preserve rule. The cavity obstacle guard remains conservative for it.

## Added source profiles

The decoder now uses explicit beam neighborhood records rather than deriving roof/air/post identities from beam UID arithmetic. Existing95/98 profiles remain identical. Eight additional real beam sources are admitted to the core decoder:

| Beam | Roof | Earlier air | Posts |
|---|---|---|---|
|89|69|88|73,77|
|90|69|88|72,76|
|91|69|88|74,78|
|92|69|88|75,79|
|107|81|87|100,104|
|108|81|87|99,103|
|109|81|87|101,105|
|110|81|87|102,106|

Each still requires earlier air66, exactly the three named solid neighbors, the five-face/eighteen-corner roof air at the beam's upper plane, ordinary compiled eligibility and existing ownership/geometry validation. No unknown neighbor is ignored. Each beam has six source faces and eight compiled windows; earlier air clipping and neighbor void provenance use the profile's actual roof UID.

Tests load all eight from installed data, open the solid core, prove an initial transverse ray blocked, cut the middle with the original template, prove it clear, build neighbor-aware publication, reset and prove the ray blocked again. Results are39..42 published faces/200..206vertices per source. Each decoded owner retains22368bytes with1251955-byte accounted peak. These are decoder measurements, not whole-game memory figures. Existing source identity tests remain part of the full suite.

Live source selection, identity/manifest policy and checkpoint integration for these eight are not yet enabled. Attached posts likewise still need their own neighborhood qualification. Mixed cavity/detail/structural CSG is not solved by this expansion. This change broadens a proven structural path without conflating it with detail-brush behavior.

Validation: all123 PC tests pass (`artifacts/side-beams-tests.log`); stock NXDK compile/link/XBE/ISO succeeds (`artifacts/side-beams-xbox.log`). No live or native visual acceptance is claimed for the new beam profiles.


## Identity and scene policy integration

`rf_geomod_authored_beam_roof` exposes immutable profile metadata from the decoder table (zero for unsupported/non-beam IDs). Identity capture, scene material digest policy and connected-publication workspace selection now use this same table. The helper is not a substitute for geometry validation; decoder checks still qualify every asset.

The installed manifest test now covers15 sources: the previous six solids and cavity plus eight side beams. Each identity is stable across capture APIs, distinct from every other selected source, has complete references for compiled windows and produces a distinct window-publication digest. The existing pinned UID94 identity remains unchanged. Every beam also rejects a substituted roof-owner UID while leaving digest bytes and peak output untouched.

All eight added beams have21 material/chart references. With the test's fixed manifest capacities, peak capture is1616063bytes for89..92 and1550527bytes for107..110, below the unchanged2097152-byte limit. They retain the existing beam loader3/publication12 policy; actual geometry/material/chart bytes and owner IDs distinguish them. Scene material policy2 now applies to the same beam set. Live selectors/spawn locations and complete save/reload for the added beams remain separate work.

Identity/policy validation: all123 PC tests pass (`artifacts/side-beams-identity-tests.log`), and stock NXDK compile/link/XBE/ISO succeeds (`artifacts/side-beams-identity-xbox.log`). No new native runtime claim is made for these policy changes.
