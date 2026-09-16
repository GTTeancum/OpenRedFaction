# Subsequent-blast debris relaunch implementation (2026-09-16)

Existing active fragments now receive the original48fe30 relaunch prepass before new fragments are created. `rf_geomod_debris_relaunch` in coregeomod/header accepts fragment position/radius/resistance, blast center/radius, a detail marker and RNG. Matching returns velocity and bouncecount; it leaves age/spin/flags outside the helper's state. No match consumes no RNG and preserves result; invalid/numeric failure preserves result/RNG/matched.

## Original evidence and important distance distinction

The oracle `artifacts/future-vehicles-re/debris_relaunch_marker.py` now covers162 original instruction cases, extending the earlier81 axial cases with diagonal positions.4faf30 stores binary32 differences and4fa7a0 computes an **approximate magnitude**, sorting absolute components a>=b>=c then evaluating `a + (t + .5*t)`, `t=.25*b+.125*c`, without a final float spill. Admission is strictly less than blast radius. Replacing this with Euclidean distance would be wrong: position(1.2,1.2,1.2) is admitted at radius2 despite Euclidean distance greater than2. Both signs and a diagonal outside the approximate cutoff are covered.

For unmarked admitted fragments, original504e40/573e83/573528 yields floor(random[3,5)), then original490150 supplies launch velocity. Exactly three CRT draws occur. Existing count0/1/4 does not change admission. Marker values0, mapped nonzero and unmapped0xdeadbeef prove the marker is a zero/nonzero exclusion without dereference. The shared helper preserves that exclusion. It validates inputs before exclusion as a defined safer API boundary.

## Scene integration

`scene_debris_prepare` runs the existing-fragment scan before biasing the spawn origin and before probe/count work. Relaunch uses the **unshifted contact point**; new fragment placement retains the existing normal*.1*radius bias. Traversal starts at the circular pool's next replacement index and visits80slots, reproducing oldest-to-newest active-fragment order after wrap. Inactive slots do not consume random numbers. Only matched velocity and bouncecount change; no age, lifetime, angle, axis, spin, active flag or alpha reset occurs. Thus fading fragments can still disappear on schedule after relaunch, as in the original prepass.

Chunks now carry an explicit `detail_marked` field cleared when newly spawned/reset. Current DEV chunks remain unmarked; general authored breakable-detail settlement does not yet assign that field. The helper supports exclusion, but complete authored detail integration is **not** claimed. The additional80uint32 fields cost320bytes in the stock64MiB-target pool. The change does not modify camera, collision queries, bounce math, or terrain validation.

## Validation

Built only `rf_geomod_debris_relaunch_tests` in the existing PC Release build. It passes54 captured original response vectors (distinct marker/position/seed combinations), exact velocity/count/RNG comparisons, predecessor/equal/successor float radius boundaries, numeric-overflow and invalid-input rollback. Expected velocities/counts come from original executable traces, not a copy of the helper formula. CMake registers `geomod_debris_relaunch`. Existing unrelated geomod compiler warnings remain (signed comparison and potentially uninitializedden); no new test warning was reported.

The general scene executable and Xbox build were not built/launched by this agent. Parent owns live repeated-blast validation and native integration checks. No Git operation or screenshot was performed.

## Call boundary and live verification handoff

Original executable has two direct48fe30 call sites: **4673d8** inside admitted GeoMod467020 and **476693** in a separate caller not reconstructed by this change. The467020 path reaches4673d8 after template resolution, admission and history duplicate checks. It computes the debris radius as `local_38 * selected_template[+60]`; local_38 is the recovered scale (normally requested size/template radius, or1 for the fixed-size flag). It also suppresses debris for its mode/flags gates. Therefore this integration deliberately stays inside the admitted terrain-destruction path and receives `hardness.scale * terrain_template->radius`; it is **not an all-explosions/damage-radius feature**. The scene uses the admitted contact point as relaunch center before its spawn bias; exact selected-center differences in other template/admission modes remain outside this change.

Original relaunch probe is now retained at **tools/future_re/debris_relaunch_marker.py**; the tracked regression cites that path. Rerun passes162 original cases, including diagonals. The binary is read-only and no game process launches.

Added lightweight scene diagnostic **rf_scene_debris_relaunch[8]**: passes, active candidates, matched relaunches, previously settled fragments assigned nonzero velocity, accumulated state hash, last prepass seed before, seed after, last matched slot. The hash includes prior/new counts, unchanged age, new velocity and RNG transition; the prepass occurs before new allocations, so freshly spawned fragments cannot inflate the resumed-settled count. DEV reset/open clears it. Combat trace prints the same8values as `DEBRIS_RELAUNCH` after each prepass.

`tools/replay_debris_relaunch.py` prepares an ordinary-input490frame candidate using the verified first-rocket walk/look sequence, fire316, modest second aim change and fire390. It creates `artifacts/debris-relaunch/{inputs.bin,recipe.json}` without launching anything. **Not yet live-verified**: acceptance requires at least two prepasses and a positive cumulative settled-resumed field, not merely a higher spawned count. The second-shot timing/yaw may need adjustment if the earlier fragments expire or the new impact is outside their radius. General PC build/play remains held for the parallel save worker; no new native session was launched.

## Executed live verification

The initial second-frame390 recipe fired only once because launcher cooldown was still active. Default now450: impacts331 and472; native run artifacts/xemu/render-20260916-091658 reaches550frames and passes51 PC/Xbox checks at67108864 base memory with no expansion. Both report DEBRIS_RELAUNCH_STATE [2,5,1,1,601898194,3964544563,3961591796,0], proving one already-settled slot0 receives nonzero velocity before new fragment creation. All25 disc entries restored; owned PID2512 exited. Native framebuffer inspected: textured room, dark double-cut crater, residual smoke and launcher HUD. This endpoint cannot establish animation quality; original-code helper evidence and scene telemetry establish the state transition.

This evidence predates the subsequent fragment-birth placement/resistance correction, which requires fresh verification. The active worktree may therefore differ from this run; archived XBE/PC hashes are recorded in its report.json.
