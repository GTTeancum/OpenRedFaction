# Shallow GeoMod prior-crater alignment — 2026-09-15

## Executed evidence

`tools/verify_geomod_shallow_prior_cuts.py` executes the entire original `45cff0` for **69 cases**. `shallow-prior-cuts.json` retains inputs, outputs and independent geometric expectations. RF.exe SHA-256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

Only region container accessors `40a490/40a480` and template getter `4375b0` are supplied. Actual region membership, normalization, plane projection, history iteration, comparisons and center updates execute original instructions. The template getter returns a fixture with base radius 5; its caller uses the **new request's template index**, not an old record index. Fixtures use x87 control word `037f`. This is CPU behavioral evidence, not an emulator or visual parity claim.

## Actionable result

Prior shallow cuts change the **new crater center**, not its newly selected limiting vectors. This preserves alignment to an existing shallow crater's plane when the new impact lies slightly inside its permitted shallow depth. Implement this preparation before transforming template geometry; merely applying the two deformation vectors to the unadjusted impact center misses this behavior.

The original scans the global admission history in order (`647c9c` count). It reads the 36-byte auxiliary record at `646a28 + i*36`: center, first depth-vector, second depth-vector. It reads old scale from `64861c + i*32`. These are the admission records described in TERRAIN-HISTORY-OWNERSHIP-20260915.md, not only completed CSG cuts.

Algorithm, for requested center P and each newly selected limit:

1. Initialize one correction vector per new limit to zero. Stop scanning once every required correction is nonzero (`45d25f..45d298`). A found correction is never replaced by a later record.
2. Skip an old record if its first limit vector is zero (`45d29a`). Otherwise examine its first vector and, if nonzero, second vector.
3. Require `distanceSquared(oldCenter,P) < (newTemplateBaseRadius * oldScale)^2` (`45d2c6..45d2fa`). There is no old room or old template equality comparison in this function.
4. For each old limit, normalize it and retain its original length as old depth (`45d351..45d373`). For each still-unfilled new limit, normalize that vector too.
5. Require `dot(oldUnit,newUnit) >= 0.95` (`45d3f3..45d403`). The constant at `589458` is a **double**, unlike the float 0.95 used in shallow-region selection.
6. Require `dot(oldUnit, normalize(P-oldCenter)) > 0` (`45d409..45d422`). The positive direction is that of the stored negated region-Up vector.
7. Project P onto the plane through oldCenter with oldUnit normal (`4fb1a0`, called `45d43d`). Require squared projection distance strictly smaller than old depth squared (`45d448..45d459`). Equality does not adjust.
8. Save `projectedPoint-P` as the correction for this new limit (`45d45b..45d4a4`). Each slot is independent, but every test/projection uses the **original requested center**, not a center already corrected by the other slot.
9. Add the first and second correction vectors to the request center (`45d4fb..45d50e`). Retain the new limit vectors unchanged.

An old record with center `(0,1,0)`, limit `(0,-2,0)`, scale1 and template base radius5 adjusts a new center `(0,0,0)` to `(0,1,0)`. Old center Y=2 is exactly the depth boundary and does not adjust. Old center below the request is the wrong side and does not adjust. With matching X and Y limits, the two corrections add to `(1,1,0)`. History Y=.5 followed by Y=1 yields .5; reversing them yields1.

## Coverage and limits

The 69 cases cover both sides, zero/exact/inside/outside depth, spherical overlap, old scale, history order, angular matching, two new limits in one or separate records, old second vector, and an absent first vector. Comparisons matched the independent geometric model within 2e-5 world units. Newly selected limits remain unchanged.

A float-stored old scale of 0.2 illustrates extended precision: 5 times that binary float is slightly greater than1, so a center distance of1 can pass the strict overlap comparison. Do not silently replace the original intermediate precision with an asserted exact-decimal threshold. General floating-point edge parity remains subject to the executable's active FPU mode.

The fixtures use spherical authored regions and synthetic history; actual region asset loading and live history ownership must still be connected and validated by the main agent. Original admission history can include cuts which later fail CSG. The port's bounded terrain publication capacity must not be mistaken for the original history policy. This report does not prove the appearance or correctness of a live authored shallow level.

## Shared implementation

`rf_geomod_shallow_align` consumes borrowed admission-ordered auxiliary history (up to128 entries), retains requested-center evaluation for both corrections, and returns the adjusted center without modifying history or signed selected limits. All69 original/shared C comparisons pass within2.959e-8 world units. Translated-center, admission-order, malformed-history rollback, capacity and no-history aliasing fixtures pass geomod_interior_faces. This does not yet maintain the live queue or packed duplicate history; those require separate requested and adjusted center ownership.
