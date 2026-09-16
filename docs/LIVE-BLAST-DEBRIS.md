# Live blast damage and debris response

2026-09-16, ordinary actor blast integration. rf_weapon_blast_amount uses actor physics position with stored float deltas, extended falloff intermediates and final float result. It rejects radius<=float0.1 or nonpositive damage before victim queries. The scene now performs one CF5 cover ray per candidate instead of bullet CF0x27 rays to each sphere. Surface-radius subtraction is removed. Existing damage services/attribution and .01 normal epicenter offset remain; actor population routing, clutter and other original dispatcher branches are not claimed complete.

Tests embed eleven clear-cover original489010 results, threshold/nonfinite boundaries, and real shared geometry CF5 coverage. The face-owner case intentionally blocks explosions while bullet CF27 excludes it. Original query/type evidence and supplied service boundaries are documented in research/secondary-re/weapons-blast-contract.md.

Continuing debris impacts now call rf_geomod_debris_contact: randomized normal impulse is added to existing velocity, retaining tangential motion; new axis/spin consume the original six RNG draws. Terminal floor settling precedes this helper and consumes no draws.24 original-vector tests pass with error/RNG rollback. Existing contact offset, floor velocity-zeroing, step/gravity scheduling and wet scaling remain port policy or separate verification.

All eight tools/dev_rocket_check.py scenarios pass: flight, impact, reload, weapon switch, far/near/lethal self blast and visible projectile. The near recording now walks until306 rather than300: old sphere-surface policy damaged a player whose physics position lay outside the radius. New near point distance is approximately4.72854 for radius5, amount21.7168159, health89.57593 and armor88.70725. The test continues to require a survivable close hit; no algorithm/tolerance weakened to fit old results.

Native artifacts/xemu/render-20260916-085632:406frames, all50 checks pass on stock67108864 bytes with no expansion. PC/Xbox blast/vitals match. Debris matches16spawned,16active,67bounces,207renderedvertices and hash1697471385, owned50640bytes. Native framebuffer inspected: room, crater, smoke, floor debris, launcher and health/armor HUD present. This static frame alone does not establish full bounce animation or final crater fidelity; dark crater readability remains open. Owned emulator exited and all24 disc staging entries restored.

PC and NXDK builds pass. Approximate project estimate remains49%; GeoMod approximately67%. Current focus: explosions, debris and destruction gameplay.
