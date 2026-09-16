# Debris initial count and birth RNG order (2026-09-16)

The original initial count is **3 + CRT_rand % 3** (3,4,5), whereas relaunch uses floor(random[3,5)) (3,4). They are intentionally different. No correction is needed to the scene's initial count expression.

`tools/future_re/debris_birth.py` executes prepared48ffa0..4900e4 for one new fragment, including actual CRT RNG, unit-direction generation, radius/resistance arithmetic, signed remainder, spin, complete490230 mesh/lifetime construction and490150 launch. Only pool allocation, the no-hit placement query and bitmap dimensions are supplied. It stops before the enclosing spawn-loop cleanup.20 cases cover five seeds and blast radii.5/1/2/4. Results: `artifacts/future-vehicles-re/debris-birth.json`. All cases consume35 draws.

| Stage boundary | Original address | Draws consumed |
|---|---|---:|
|Placement direction and distance complete|490043|3|
|Radius/resistance complete|490081|4|
|Initial bouncecount complete|4900bc|5|
|Spin axis complete|4900cb|7|
|Spin rate complete; enter mesh build|490230|8|
|Lifetime and24 vertex coordinates complete; enter launch|490150|33|
|Launch complete|4900e4|35|

The ninth draw sets lifetime to random[1,4); it comes after spin rate, before the24 coordinate draws. Existing scene order and rf_geomod_debris_build already follow this order. New-fragment matrix initialization4fce70 is identity and consumes no draws.

Two narrower numeric differences were found:

-Placement: original48ffbe multiplies random distance[0,.5) by the blast radius before scaling the unit direction. Current scene distance omitted the radius factor. This changes spawn spread materially except at radius1; it does not change random count/order.
-Resistance: original490072 operates on the unspilled radius calculation after storing the radius at+38. Current scene reconstructs resistance from the rounded stored radius. For seed0, original resistance is0.000411354994866997 versus reconstructed0.00041134655475616455. Three of five sampled seeds differ. The radius itself is correct; the helper must retain unrounded r^3*.2f+.05f for resistance=(raw_radius-.05f)*5, then round.

Original also sets flag2 when the unrounded radius is below.15f, before computing resistance. This is an actor-impact suppression flag used by48f4e0; the current DEV fragment pool does not implement debris actor damage, so this report does not propose unrelated actor behavior.

Proposed bounded correction is a transactional eight-draw birth helper producing displacement/radius/resistance/count/spin, followed by existing mesh build and launch helpers. Store resistance in each chunk so subsequent relaunch does not reintroduce rounded-radius reconstruction. Original lifetime/count policy is preserved. Shared edits are currently held while primary builds native relaunch validation; no executable/core changes are claimed here yet.

## Implemented correction and focused verification

`rf_geomod_debris_birth` now returns the original first-eight-draw displacement, radius, resistance, count, spin axis/rate and recovered flag2. It retains unrounded radius for resistance and scales displacement by the actual admitted GeoMod radius. `scene_debris_spawn` uses it before existing mesh/lifetime construction and launch. The scene stores resistance in each chunk and uses that retained value for subsequent relaunch, eliminating rounded-radius reconstruction. The recovered actor-impact flag is returned for future actor integration; this change does not add debris actor damage.

New `tests/geomod_debris_birth_tests.c` passes20 captured original vectors bit-for-bit, including all birth fields, RNG after8draws, lifetime and RNG after33draws, launch velocity and final RNG after35draws. It covers blast radii.5/1/2/4 and five seeds, plus invalid-input transactional checks. Only the focused `rf_geomod_debris_birth_tests` target was built/executed. No general PC play or Xbox launch was performed by this agent.

Parent's native550frame relaunch run at `render-20260916-091658` passed51checks **before this birth correction**. That evidence is valid for the preceding relaunch implementation only. The corrected spread/resistance can change actual contact paths and RNG later through bounce timing; fresh PC/native sequence validation is required and owned by primary. Do not attach the earlier exact reactivation telemetry to this new build.

## Fresh PC/Xbox validation

Corrected birth build passes51 comparisons at550frames in artifacts/xemu/render-20260916-092108, base67108864/expanded0. Two impacts retain one settled-fragment reactivation: [2,5,1,1,1395913357,3485410575,3269723840,0] on both backends. tools/verify_debris_relaunch.py checks nonzero prior-fragment behavior rather than merely equal endpoints. All25 disc entries restored; owned PID36020 exited. Birth/relaunch CTests pass. Remaining original birth-placement filter/hit-point mismatch is a separate follow-up; this run does not include that correction.
