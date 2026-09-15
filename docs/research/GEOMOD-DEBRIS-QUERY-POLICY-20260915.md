# Original GeoMod debris query policy (2026-09-15)

**Two actionable mismatches exist in the live debris binding:** its query flags differ from the original wrapper, and it converts intersection fraction into world distance where the original subtracts the fraction directly.

## Executed evidence

`tools/verify_geomod_debris_query.py` executes complete original490890,48fc10,49c5c0 and their actual arithmetic/initialization/flag-helper callees. Only final solid intersection4df1c0 is supplied synthetic hit results. Nine cases cover radii0.5/1/5 crossed with fractions0/.25/1. All pass. Binary SHA and measured queries are in `debris-query.json`.

- 48fc10 passes external query mode1 to49c5c0. Actual499190 maps this to1;49c62a adds bit4, producing internal **0x5**. Captured query start/delta are the supplied segment, and radius is zero.
- 49c5c0's result+4 is the intersection fraction (its initial limit is1.0). 48fc10 copies it directly to its output+4. 4908d6..4908dc subtract that exact value from the count accumulator. Radius5/fraction.25 therefore subtracts.25, not1.25. There is no segment-length multiplication in the executed chain.
- The face bit8 rejection in490890 remains as already verified. The synthetic solid boundary supplies a face with flags0; this new harness targets wrapper semantics, not solid intersection correctness.

## Current shared binding and proposed correction

`scene_debris_prepare` calls `rf_geometry_collision_world_ray` with0x460 and supplies `min(radius,sqrt(length)*fraction)` to `rf_geomod_debris_count`. The old count oracle replaced48fc10 and named this field distance; its passing results do not validate this conversion.

Preserve original fraction semantics in the binding/helper naming and test with nonunit radii. Preserve raw original behavior even though subtracting fractions from a radius-based accumulator is dimensionally surprising. Separately restore a dedicated debris query policy matching0x5. Do not globally change rocket or body collision flags.

The existing recovered collision filter documents why flags matter:0x460 rejects face bits0x40/0x80 and certain owner states, whereas0x5 does not apply those gates. Bit1 also requests first-hit traversal. Actual eligible surfaces/order must be checked under the world facade, whose original ownership/traversal context is not reproduced by this synthetic query boundary.

## Limits

This is debris count/surface-query evidence, not complete terrain CSG eligibility. It does not execute4df1c0, original room liquid fallback, repeated room search, or an original visual frame. It does not establish whether changing to0x5 alone reproduces original world traversal. The supplied hit fractions are valid0..1 values. No source edits, builds or emulator runs were performed.
