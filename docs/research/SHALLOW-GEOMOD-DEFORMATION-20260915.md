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
