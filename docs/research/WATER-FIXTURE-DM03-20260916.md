# Authored wet-room runtime fixture: dm03

## Reproducible selection

Use installed `levelsm.vpp / dm03.rfl`, room18 `sewerpipe`, water texture greenwater01.vbm. This is an MP geometry fixture, with no campaign route required. Inventory contains only two wet MP maps: dm03 (two opposing water faces) and ctf06 (82 water faces). dm03 has52 rooms,1738 faces,1628 vertices and370996 bytes of static geometry; smaller fixture complexity favors it.

Run `python tools/replay_water_fixture.py`. It writes `artifacts/water-fixture/fixture.json` and ordinary RFI6 `input.bin`; it invokes only the existing reconstructed collision probe and does not launch a game, emulator or desktop input. Geometry payload SHA256 is `1135e6d98d6626044b5082e1838c75e17a861419843fa319332fb1e466e6e400`.

## Pose and geometric evidence

Camera position **(-226.5,-38.25,-80)**. Orientation rows:

```
right    (0,   0,  -1)
up       (0.8, 0.6, 0)
forward  (0.6,-0.8, 0)
```

This is an under-grate position, looking diagonally down at actual water. A dry ledge above the grate was investigated and rejected because the real shared world query hits the grate first. Do not reuse the rejected (-221,-35.8,-77.5) pose.

- Liquid face1737 belongs to room18, flags260 (0x104), plane approximately(0,1,0,39.500103). Opposing face1722 faces down. Surface height is -39.500099; the polygon spans x[-228,-210],z[-82,-78], with a clipped eastern corner.
- Real shared world ray from the chosen pose along delta(3,-4,0), flags4, reaches solid floor491 in room18 at fraction.4375, point(-225.1875,-40,-80), with no earlier solid obstruction.
- Downward world ray reaches floor498 at y=-40; upward world ray reaches grate underside500, room19, at y=-36. The chamber has4 units of vertical clearance here.
- Shared face sweep against the authored upward liquid polygon gives radius0.1 fraction.287524998, contact(-225.63742,-39.500099,-80), before the solid floor. Radii0/.125/.25 also hit water before the ray floor. Consecutive duplicate authored corners are removed only in this probe's fixed-eight-corner wire representation; geometric shape is unchanged. The main level geometry is never modified.
- Prior full authored-world verification already records dm03 two liquid entries and one water-then-solid case; the new probes establish this particular pose/line, not just a random face center.

Camera remains above water, but the player's feet/body can be immersed. Body settling and view height must be checked in the first reconstructed run. No player fit, live swimming, scene camera acceptance or visible ripple is claimed yet.

## Minimal runtime hook needed

The existing spawn-replay frontend accepts RF_REPLAY_ARCHIVE=levelsm.vpp and RF_REPLAY_LEVEL=dm03.rfl, but there is no generic exact-pose override. Add a bounded wet-fixture frontend selection that writes this pose into level.player_position/player_orientation before rf_scene_set_campaign_spawn. Reuse the ordinary five-slot DEV weapon loadout and input simulation, and leave campaign route/event progression disabled.

One scene separation is unavoidable with current source: rf_scene_dev_room_enabled both loads the Rocket Launcher/slot4 supply and invokes scene_terrain_open, which hard-requires glass_house.rfl/598faces/91rooms. A wet fixture must retain DEV weapon supply while skipping that glass_house-only terrain initialization. Do not set the existing DEV flag against dm03 without splitting/gating that assumption. Primary owns this frontend/scene integration; this worker changed neither.

The generated180-frame recording has no movement/look, advances weapon at frames10/20/30/40 and fires once at frame60. It assumes the existing initial handgun and four successful next-weapon selections. Runtime trace must confirm slot4, actual shot, ROCKET_LIQUID then ROCKET_IMPACT, and ripple activation. Capture frames around first entry and later ripple evolution through the application's existing replay capture output. Water-to-floor distance is only.5 units, so both contacts can occur within one step and nearby blast damage may affect later camera/player state; preserve the liquid event/effect even when terminal impact happens immediately afterward.

No original runtime, screenshots, source asset modifications or builds were used. The recording is ready for the missing fixture hook; it is not represented as an already playable/replayed acceptance test.

## Reconstructed PC execution

The explicit RF_REPLAY_WATER_TEST hook now places the player and enables DEV weapon supply for dm03 while skipping the Glass House-only terrain fixture. tools/verify_water_gameplay.py regenerates ordinary RFI6 inputs and executes65/180frame checks. The player settles; recorded look-down input=-1 atframes1..56 is necessary because initial body heading alone does not retain the intended downward firing view. Real liquid contact occurs64, solid floor contact66, exactly one ripple starts and expires by180. The close blast kills the player; frame80 inspection shows explosion/death overlay, so later ripple visibility is not proven by that image. A safer view and wet Xbox/audio remain open.

This run found tiny rocket triangles failing generic plane validation at distant world coordinates. Rocket rendering now rotates into projectile-local coordinates and translates the copied camera inversely, preserving all faces and terrain validation thresholds. PC wet replay now completes; dry ripple checks and NXDK build pass. No original visual references were used.

## Stock-memory Xbox execution (2026-09-16)

Shared rf_scene_water_test_place now supplies the identical pose to PC and Xbox; the native frontend reads the explicitly staged water-test.flag. Run:

```
python tools/xemu_render_check.py --water-test --spawn --level dm03.rfl --archive levelsm.vpp --input artifacts/water-fixture/input.bin --seconds 180
```

Retained run artifacts/xemu/render-20260916-090120 passes42 comparisons at180frames, base memory67108864 and plugged memory0. Both backends report ROCKET_LIQUID_STATE [1,4,1097789167,1056964608], ROCKETS [1,1,0,0,0,0,0,0], RIPPLE_LIFECYCLE [1,1,0,0], and one blast. Native capture was inspected: tiled room floor and death/respawn HUD from the close blast; this endpoint does not prove active-ripple appearance or wet sound (audio was disabled). The earlier active-ripple precision mismatch remains open. All25 staged disc entries restored exactly and owned PID34112 exited.

The harness now records water_test and applies nonzero scenario assertions to180-frame wet runs. tools/verify_water_xbox.py independently accepts the retained native report; this additional verifier was added after the native run and does not imply another emulator execution. PC event trace establishes entry64 and solid66; native endpoint proves the counters, not exact event-frame timing.
