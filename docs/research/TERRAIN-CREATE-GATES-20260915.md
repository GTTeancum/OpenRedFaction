# Original terrain creation gates (2026-09-15)

New bounded executable evidence identifies two admission rules missing from the current DEV terrain binding. It does not establish a per-material destructibility mask.

## Verified master gates

RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`; verifier `tools/verify_geomod_terrain_admission.py`, results `terrain-create-gates.json`.

- Actual4670c3..4670e4 rejects requested crater radius<1.0 before region hardness is applied. Six finite boundary cases include adjacent float32 values below/above1, zero and negative. Exactly1 passes. This is requested radius, not the hardness-scaled result: do not reject a radius5 crater merely because high hardness shrinks its effective radius below1.
- Actual4671f7..46726a scans recorded craters and rejects a same-template, same-room crater when squared center distance<=0.04000000283122063 (the stored float at5897c4, equivalent to rounded .2f*.2f). Eighteen cases cover matching/different template and room plus distances around0.2. Actual distance helper4faf00 executes; only original world-position reconstruction4b5900 is supplied as an identity-space boundary. The stored shape comparison is16-bit; room comparison32-bit. A different template or room bypasses this duplicate rejection.

Before this correction, the live scene admitted any positive campaign_rocket.crater_radius and invoked CSG without this nearby duplicate-record gate. For the current radius5 rocket, the minimum rule changes nothing; duplicate suppression may avoid redundant edits. Any implementation needs a bounded record keyed by template and room and must respect reset/current geometry ownership. Do not implement a global radius threshold after hardness or a cross-room proximity veto.

## Protected-surface investigation boundaries

The already verified45cff0 ordinary-region rule remains maximum matching hardness, default fallback,100 refusal, and1-hardness*.01 scale. The live API explicitly refuses matching shallow regions and ice. No new per-face protection flag was proven here.

The retained community header's `GRoom::is_geoable` is explicitly an **Alpine1.3 RF2-style addition**, not evidence that the original PC executable uses that field. Do not import its brush-protection policy into stock reconstruction. Raw467020 master dispatch checks room/template presence before the radius gate; the bounded tests above start after those checks and do not claim full original admission coverage.

Shallow45cff0 and downstream4dbdf0 constraints are the next useful bounded investigation: the source exposes one/two limiting direction vectors and overlap rejection, while the port currently returns NOT_FOUND for every matching shallow region. Full CSG filtering and campaign integration remain unverified. No source edits, builds or emulator runs were performed.

Integration: live rocket terrain admission now rejects requested radii below1 before hardness scaling. Existing blast effects/damage remain independent. Duplicate-ledger integration awaits coordinate quantization evidence; the harness supplies decoded positions and does not prove packed-center behavior.

Validation:24 original gate cases pass; PC rf_pc_play and NXDK build pass. Current installed radius5 rocket behavior is unchanged; no new emulator/visual acceptance run was claimed for this admission-only correction.
