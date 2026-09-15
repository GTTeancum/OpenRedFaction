# Shallow GeoMod region selection and deformation (2026-09-15)

**Actionable path:** shallow regions deform one side of the cutter by scaling its normal displacement. They are not blanket no-destruction regions, and they do not simply clip the cutter against a fixed-depth plane. Current rf_geomod_hardness refuses all matching shallow regions, so this authored behavior is missing.

## Selection:24 executed cases

`tools/verify_geomod_shallow_selection.py` executes complete45cff0, actual45d520 sphere membership and arithmetic callees. Only the region-list container is supplied; prior crater count is zero. Synthetic unit normals/radius100 contain the tested origin, with depth2 for first region and3 for second. The stored normal comes from runtime region+28, the Up row (serialized basis's Up vector).

- First matching shallow region selects its normal/depth.
- With exactly one selected limit, another shallow region with dot(first,new)>float32(.95) does not add a second limit. Its hardness still participates in maximum-hardness selection.
- A second selected normal with dot(first,second)<float32(-.1) refuses the request; equality is accepted. Exact and adjacent float boundaries were tested.
- A third matching shallow region after two limits have been selected refuses the request, even if it is parallel to the first; the near-parallel skipping condition only runs while selected count equals1.
- Selected outputs at GeomodParams+4c/+58 are `-normal*depth`. A single selected limit leaves the other zero. Hardness25 yields scale3.75 from input5; hardness100 still refuses.
- Early failure can leave original scratch parameters partially modified. A shared public API can retain its unchanged-output failure guarantee while reproducing acceptance behavior.

## Shape:30 unhooked executed cases

`tools/verify_geomod_shallow_deformation.py` executes original4dc103..4dc190 and actual vector/projection helpers without hooks, using established stack locals from4dbdf0. Six signed cardinal limiting directions cross five offsets with nonzero center, radius5 and depth2.

For unit limiting direction n and offset v from cutter origin, the tested branch leaves v unchanged when dot(v,n)<=0. On its positive side, it keeps the tangential component and changes normal displacement to dot(v,n)*depth/radius. Radius5/depth2 maps normal distances5->2 and2->.8. The latter example distinguishes compression from a fixed-depth cap.

The caller supplies normalized limiting vectors and their lengths;45cff0's negative depth vector means ordinary positive authored depth restricts the side opposite region Up. Raw4dbdf0 contains a second analogous branch, but this harness only proves the first. Both-limit sequential composition and the older-crater interaction loop in45cff0 remain to execute before claiming full shallow parity.

## Implementation recommendation

Extend a bounded preparation result with up to two limiting vectors, retaining ordinary hardness/ice behavior. Apply the recovered one-sided deformation to transformed cutter vertices before CSG, using effective template radius as the denominator. Verify both-limit handling, continuity across the center plane, generated UV/normal recomputation, and repeated cuts before enabling all campaign shallow regions. Do not simply remove RF_NOT_FOUND or replace it with an unrestricted spherical cut.

Evidence JSONs: shallow-selection.json and shallow-deformation.json beside the scripts. SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No shared-source edits/builds/emulator runs. This is independent CPU evidence, not visual acceptance.

Primary review: retained the executable probes under tools/ and reran them successfully. These prove the scoped original CPU behavior, not live port or visual parity.

## Shared deformation primitive

`rf_geomod_shallow_point` now implements up to two ordered limits, with original-offset activation gates and current-point unsigned projection distance. `tools/verify_geomod_shallow_both.py` executes both original branches and compares100 cases with compiled shared C, including the sign-crossing case that differs from ordinary sequential signed compression. Maximum observed error is1.953e-6 world units (tolerance1e-5), not bit-exact parity. Input guards and aliased point/output pass geomod_interior_faces.

The primitive is not connected to live CSG yet: region-limit preparation, prior-crater adjustment, recomputed cutter geometry and actual shallow-cut visuals still require integration and verification. Ordinary matching shallow regions continue to be refused rather than producing unrestricted cuts.

## Terrain mesh integration

`rf_geomod_terrain_cut_template_limits` now transforms the template, applies prepared shallow limits to its vertices and reconstruction kernel, and submits that actual mesh to the existing atomic star-cut publication path. Existing no-limit callers retain their original operation. Face planes, generated mapping and collision are rebuilt through the terrain owner; this is not a render-only displacement.

The original-template integration fixture checks a single positive-Z limit at40% depth in both solid and cavity sources: closed output edges, normalized generated collision planes, generated vertices below the reduced depth bound, and a collision ray hitting within that bound. Invalid limits preserve the published generation. The full original-template suite also passes its existing six-cut closure/volume/junction-ray checks. Two-limit live CSG coverage and repeated shallow-cut interaction remain open, as do authored region/history preparation and visual validation. No new emulator run or screenshot was produced for this step.

Reproduce with `build/pc/Release/rf_geomod_interior_tests.exe Installed_Game artifacts/geomod-holey01-csg.bin`; evidence log: artifacts/shallow-csg-tests.log.

## Authored-region preparation

`rf_geomod_regions_prepare` now supplies ordinary hardness plus up to two ordered shallow directions and signed depths. It skips a near-parallel second region only while one limit is selected, retains that region's hardness contribution, and rejects opposing or excess selected limits through allowed=0. The24 original selection cases now compare compiled shared C admission and successful limit vectors; all pass. The existing45 ordinary-region oracle cases retain bit-exact scale behavior. Region fixtures, failure rollback and installed parsing also pass (82 sections/903 regions).

Signed depths intentionally remain unnormalized because prior-crater placement consumes their signed vectors. Cutter handoff must subsequently implement original zero-first suppression and negative-depth direction reversal. The existing hardness-only API continues to refuse matching shallow regions, keeping live activation pending history adjustment and handoff integration. Unit-normal validation is a shared API input contract; it is not a claim that the original rejects every malformed basis.

## Live DEV admission integration

Live room0/template0 rocket terrain edits now use region preparation, prior-crater center alignment, signed-depth normalization and deformed-template CSG. A bounded128-entry admission ledger retains packed requested centers separately from adjusted centers/signed vectors/scale. Entries are recorded before CSG, so later geometry rejection does not silently erase admission history. Successful DEV terrain reset clears the count. Manual prototype box edits remain a separate DEV operation.

Duplicate comparison uses the actual5897c4 threshold0.04000000283122063, not nearest float(.04). The codec retains initial serialized solid bounds because the port collision overlay expands its bounds after edits; applying expanded bounds to old codes changed decoded positions and failed the repeated-hit check. This stable-bound policy is a deliberate port fix: subsequent original-code evidence confirms CSG can expand solid bounds without re-encoding history in the tested segment.

`tools/dev_shallow_admission_check.py` applies an opt-in process-local authored-region fixture (depth.75, limiting direction-X) without changing installed files. Two impacts produce one shallow terrain edit and one duplicate rejection. Actual output was inspected: textured room, weapon and dark exposed patch render; crater readability/retail visual parity remains unresolved. The ordinary900-frame route still completes3 edits with48 debris pieces expired at endpoint. Live repeated shallow cuts at distinct positions, multi-region cases, reset playback, save/load integration and wider campaign geometry remain open.

Native validation: render-20260915-194922 completes900 frames on stock64MiB with47 PC/Xbox checks passing and8565 free pages (33.457MiB). Actual framebuffer inspected; the shared dark crater readability issue remains visible. Owned PID8920 exited and all20 temporary disc entries were verified restored. This native run covers the ordinary three-cut route; the shallow fixture remains PC-only verification.

## Native shallow fixture and reset coverage

`tools/xemu_render_check.py --dev-room --shallow-fixture --spawn --level glass_house.rfl --archive levelsm.vpp --input artifacts/destruction/shallow-double.bin` now stages the same opt-in depth.75 region fixture on PC and Xbox. The temporary shallow-fixture.flag is recorded in the disc restoration manifest. Installed game inputs remain unchanged.

The400-frame run render-20260915-195231 passes47 PC/Xbox comparisons with8597 free pages (33.582MiB). Both impacts occur, but only the first is admitted. The native framebuffer was inspected: expected room/weapon/crater content is present; dark mound-like readability persists and is not accepted as retail parity. Owned PID56804 exited and all21 disc entries were verified restored.

The extended process-local checker generates a500-frame reset/refire replay. PC confirms the history index restarts at1, with three impacts, two admitted cuts, one duplicate rejection, and one current crater after reset. This specifically exercises clearing history when terrain reset succeeds.

The500-frame reset/refire native run render-20260915-195421 also passes47 comparisons, with8581 free pages (33.520MiB). The native endpoint shows the new crater and lingering blast smoke; geometry/weapon/room content was inspected. Owned PID47248 exited and all21 disc entries were verified restored. These tests cover the diagnostic fixture, not general campaign shallow-region acceptance.
