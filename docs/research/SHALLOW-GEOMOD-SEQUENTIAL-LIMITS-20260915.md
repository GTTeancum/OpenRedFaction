# Sequential shallow-limit execution (2026-09-15)

`tools/verify_geomod_shallow_both.py` executes both original4dbdf0 deformation branches,4dc103..4dc220, including actual projection/vector/distance callees with no hooks.100 cases cross a25-point grid with four ordered orthogonal/oblique limit pairs and nonzero center. All match the documented numerical construction within1e-5; exact original float outputs are retained in shallow-both.json. This is an independent formula check, not shared PC/NXDK bit-exact validation.

**Both activation tests use the original pre-deformation offset.** The second projection uses the point already changed by the first branch. Each active branch projects the current point onto the limiting plane, takes the unsigned Euclidean point-to-projection distance, and adds `unit_limit * distance * depth/effective_radius` to that projection. Consequently a point moved across the second plane by the first branch can be reflected onto the second limit's positive side; replacing this with signed compression or recomputing the activation test is observably different.

Example with center0, radius5: point(-2,-5,4), first limit(0,-1,0)/depth2 then second(.8660254,-.5,0)/depth3 yields approximately(-.9856407,-2.5856407,4). The first result(-2,-2,4) lies on the negative side of the second limit even though the original point passed its positive-side gate. The second branch still runs. Swapping limit order changes results for oblique pairs; preserve selection order from45cff0.

The fixture supplies normalized limiting vectors, their depths, effective radius and original offset at the established stack locations. Actual caller normalization, every non-cardinal precision edge case, and45cff0's previous-crater adjustment remain separate. Full shallow activation should wait for the latter or explicitly restrict to a verified history-free subset; these probes alone do not justify unlimited campaign shallow support.

RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No shared edits/builds/emulators or visual parity claims.

Primary integration: shared C point deformation now passes all100 original comparisons within1.953e-6 world units; input/output guards pass, and the final NXDK build succeeds. This remains a preparation primitive, not a live terrain or visual acceptance claim.
