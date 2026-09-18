# Broader authored solid neighborhoods and side beams

The read-only `tools/inspect_geomod_solid_neighborhoods.py` census transforms retained editor brushes with the loader's float stores, resolves compiled faces through authored face tokens and compares AABB candidates in serialized brush order. It uses the existing local `inspect_geomod_source_topology.py` research helper and exported `artifacts/future-vehicles-re/ctf06-editor-brushes.json`, plus installed geometry/inventory. Its artifact includes geometry/export hashes. AABB candidates are not exact intersections or destruction permission.

The955-brush export has41 flags0 structural owners with340 compiled room3 faces. All41 authored sources are closed, oriented and convex. Some cross rooms; many overlap detail/portal/air records. Convexity alone does not authorize publication. The census reproduces all six previously admitted post/beam neighborhoods exactly before reporting additional candidates.

Pillar12815 is flags4 (detail), not an ordinary flags0 structural source. It owns ten compiled faces (2704..2713) in detail room166, whose parent is room3. The earlier claim that it had no compiled ownership was incorrect: the structural room3 census excluded its separate detail room. Its authored life is-1 and initial protection is set; it cannot be discarded on the basis of that census. Existing binary evidence in TERRAIN-DETAIL-FACE-CLASSIFICATION-20260915.md distinguishes detail-owner byte from invincibility and does not establish a general delete/preserve rule. The cavity obstacle guard remains conservative for it.

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


## First live side beam: UID92

The explicit source92 selector uses an enemy-free body spawn(-2.75,-.4,0), facing west. It is single-source only; attempting a multi-source92 selection rejects rather than inventing unsupported post owners. The player settles at(-2.75,-.401361,0), and the ordinary rocket hits(-8.5,1.74999928,-2.49e-7) at frame258.

`tools/check_side_beam.py` measures the settled eye, generates its own input, saves after the first shot, reloads, aims farther along the beam towardZ2 and fires again. The first save is3176bytes. The second produces two settled detached pieces and4556bytes exactly matching uninterrupted play. Scene publication has12faces/62vertices at this endpoint; the smaller retained mesh reflects removed/extracted beam material, not an unchanged source. Player remains alive. PC continuation framebuffer inspected: the beam section is gone and a tilted chunk is visible below, with remaining supports, ceiling, hall and HUD retained. Audio not assessed.

The other seven side beams retain tested core/identity support but are not live selectors yet. Side-beam attached posts, mixed-neighbor edits and broad history remain open. Existing source66 and93..98 selectors are unchanged.

Native side-beam92 evidence: `artifacts/xemu/render-20260918-003428`,900frames,77checks passing,3877free pages at endpoint. Xbox's4556-byte checkpoint matches uninterrupted PC exactly. Native framebuffer inspected and confirms the missing beam section and fallen piece. Stock NXDK build succeeds; harness restores disc and closes emulator. Native reload of the side-beam-created checkpoint remains a separate follow-up.


## Opposite live side beam: UID108

Source108 now uses the mirrored open-room-side spawn(3.75,-.4,0), facing east. Like92, it is single-source only and retains actual roof81/air87 ownership. The replay accepts `--source 92|108`, derives its aim from the measured settled eye and uses the corresponding surfaceX rather than copying geometry or material state between beams.

PC108 settles at(3.75,-.401361,0), writes a3178-byte first-shot save and continues after reload to two cuts with three settled pieces. Its5070-byte final save equals uninterrupted play. Retained publication is17faces/94vertices; this differs from92 because the actual cutter/boundaries and resulting fragments differ. The framebuffer was inspected: missing beam material and a tilted extracted chunk appear under the opposite roof with neighboring supports/hall/launcher/HUD retained. No original-game screenshots or host input were used.

`python -B tools/check_side_beam.py --source 108` validates the new continuation; the default92 replay is rerun as a regression after parameterizing the tool. Remaining six side-beam profiles have core/identity support but no live selection. Attached-post interactions and native reload of the Xbox-created108 endpoint remain separate extensions.

Native108 reload/next-shot evidence: `artifacts/xemu/render-20260918-003832`,301frames,77checks passing,3685free pages at endpoint. The5070-byte Xbox output matches uninterrupted PC exactly. Native framebuffer inspected. Stock NXDK build succeeds; harness restored disc and closed emulator.


## Attached-post detail ownership audit

The census now reads the installed room records and parent links, reports compiled IDs and room metadata for every neighboring brush, and derives the relevant detail IDs from all four candidate posts. It checks the old six admitted neighborhoods and the four candidate post neighborhoods before writing the report. This is offline ownership evidence, not new runtime destruction support.

| Post | Structural neighbors | Detail UID | Compiled faces | Detail room |
|---|---|---|---|---|
|75|70,92|11179|1565,1566|65|
|79|70,92|11178|1563,1564|64|
|99|70,108|11181|1569,1570|67|
|103|70,108|11180|1567,1568|66|

All four also overlap earlier air66. Each trim has flags4, two compiled faces with flags0x1c8, and a zero-thickness authored Z extent at either-2.5 or2.5. Each owner room has detail byte1, life-1, initial protection set and parent3. These are thin detail surfaces touching the post at its lower outer edge, spanning Y-1.5..-.75. The previous hand-selected detail list missed11181; deriving it from the post relationships removes that assumption.

Room protection is decoded from the same fields as `rf_geometry_initial_collision_filter`: room byte34 is owner kind, and float+36 greater than zero clears initial protection. Binary evidence at4e36a0 and the CSG candidate gates is recorded in GEOMOD-ELIGIBILITY-DETAIL-COLLISION-20260915.md and TERRAIN-DETAIL-FACE-CLASSIFICATION-20260915.md. Protection and detail classification are distinct; neither authorizes blindly deleting these faces or treating their flat bounds as solid terrain.

Implementation still required: admit the two actual structural neighbors through explicit post profiles; retain and identify the separate detail owners; reject cuts that intersect protected detail until a supported interaction policy exists; include that retained geometry in identity/save validation; then verify post cuts and shared beam/post blasts. The current runtime still rejects these unsupported post neighborhoods, and the cavity pillar guard remains intact. No new playable behavior or native validation is claimed by this audit.

Validation: `python -B tools/inspect_geomod_solid_neighborhoods.py` passes all installed-data assertions and writes `artifacts/geomod-solid-neighborhoods.json` with input hashes. Results remain955 brushes,41 ordinary room3 owners,340 compiled structural faces and41 closed convex authored sources. No build or emulator run is needed for this read-only research change.


## Guarded side-post core decoding

The decoder now admits explicit post profiles75/79/99/103 with ground70, beam92 or108, earlier air66 and the exact expected detail owner. Unknown overlapping brushes still reject. Each profile imports two solid neighbors instead of the old posts' three. The trim is retained as a separate owned guard containing its UID, room, two compiled face IDs and authored bounds; it is never imported as a solid or included in replaced IDs. Decoder qualification requires two faces with flags0x1c8, the expected detail room, owner kind1 and initial protection1, and verifies all compiled corners lie inside the retained bounds.

`rf_geomod_authored_post_admit` validates finite ordered cutter bounds and rejects overlap or contact with the trim (1e-5 tolerance). Callers must use the bounds from the exact prepared cutter before a cut. This is a conservative guard, not an implementation of breakable detail or permission to alter adjacent sources. Existing scene selectors and identity capture still reject these four sources until guard provenance is incorporated into their policy.

Installed-data tests open each post, confirm four compiled windows/two solid neighbors/one guard, prove a transverse ray blocked before cutting and clear after the original-template cut, build publication, reset, and prove collision restored. Trim overlap, exact boundary contact and NaN bounds reject; changing the compiled owner's detail byte also rejects decoding without publishing an owner. Tests verify neither trim face appears among replaced IDs. The normalized original-template radius is1.05000007; bounds and CSG use the same derived scale. An initial unnormalized-scale test did not clear the whole post, and the test was corrected to use the actual template radius conversion.

| Post | Published faces | Published vertices | Decoded resident bytes | Decode peak bytes |
|---|---|---|---|---|
|75|36|178|3480|1037727|
|79|37|180|3480|1037727|
|99|39|184|3480|1037727|
|103|37|180|3480|1037727|

All123 PC tests pass (`artifacts/side-post-tests.log`), including existing identity and checkpoint tests. Stock NXDK compile/link/XBE/ISO succeeds (`artifacts/side-post-xbox.log`). No native gameplay run or visual acceptance is claimed for these decoder-only profiles. Next: retain guard identity across scene/save admission, apply the guard to prepared gameplay cutters, enable the post selector, and verify repeated cuts/reload before shared beam/post transactions.


## Protected-detail source identities

The decoder now retains each guard's two authored face tokens as well as its compiled lookup IDs. A shared profile accessor supplies the expected detail UID and room to identity capture. Capture admits the four two-neighbor profiles only with their guard present; it checks both compiled faces' tokens, room, flags0x1c8, owner kind1/protection1, non-replacement status, and aggregate bounds against the guard. Guard faces remain static and are not added to the dynamic atlas manifest.

Guarded sources use RFAS v3, loader policy5 and publication policy14. The DGRD extension hashes the detail UID, room, authored face tokens and bounds after the ordinary source content. Numeric compiled face lookup IDs are excluded from this canonical extension; the full compiled/editor sections remain hashed as before. Missing guards or incompatible policy reject. Existing v1/v2 source identities retain their previous bytes, including the pinned UID94 test.

Installed manifest coverage now includes19 sources. All four new profiles have14 chart references and1613087-byte peak capture using the test's fixed capacities, below2097152. Decoded residency becomes3488bytes/1037735-byte peak after retaining authored guard tokens. Repeated capture APIs agree; all19 identities and window digests are pairwise distinct. Seven invalid guard variants per post reject without modifying digest or peak output: missing guard, wrong UID, wrong room, changed bounds, changed authored face token, substituted source-face reference and duplicate reference. Low-level tests separately prove bounds/tokens affect identity while numeric guard lookup IDs do not, and malformed data preserves output.

Validation: all123 PC tests pass (`artifacts/side-post-identity-tests.log`); stock NXDK compile/link/XBE/ISO succeeds (`artifacts/side-post-identity-xbox.log`). This completes immutable identity support only. Live selectors, exact prepared-cutter guard enforcement in publication/restore, and native post save/continue validation remain open. No live Xbox behavior is claimed by these tests.
