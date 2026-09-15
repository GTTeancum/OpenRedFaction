# L2S3 main shaft traversal

The shared campaign now has an uninterrupted11950-frame PC replay from the
L2S2a spawn through the L2S3 encounters to the platform at approximately50m.
`python tools/replay_area3_main_shaft.py` regenerates and checks the route;
its prefix comes from `replay_area3_shaft.build_input(True)` and the recorded
Area3 hall prefix. There are no staged poses, forced exits, health changes or
inventory injection. All actions are bounded ordinary process-local inputs.

## Gameplay evidence

The existing authored exit5150 is crossed at global7275. The player acquires
the authored shotgun, kills rifle guard2114 and collects the newly implemented
rifle drop. Two weapon-cycle presses select that rifle on the upper landing.

- Guard1773 above the shaft: automatic rifle hits at11028 and11040; killed
  before his lethal return shot. Aiming before clearing the doorway strikes
  its ceiling, so the route explicitly approaches the opening first.
- Guard2067 below the shaft: killed by hits at11120 and11126 after turning down.
- Main ladder780: approach along the narrow rim, jump-assisted entry, then
  climb with normal movement input.
- Guard1757 on the higher platform: hits at11576 and11582 kill him before his
  return shot. The route aims during the climb, stops to fire, then resumes.

Final PC position(97.586578,50.151196,61.483517), walking mode,5health and
30rifle rounds. Seven L2S3 combat kills, twelve automatic rifle rounds with six
hits/three kills; earlier shotgun use remains three shells/twelve pellets.
Friendly miner2061 remains at100health. Native scope/results are recorded below.

This is campaign progress, not completion. Remaining work includes the upper
climb and exit, guard coverage beyond this route, less awkward ladder entry/top
transitions and representative human play. The replay deliberately preserves
its low health rather than modifying combat to force a pass.

## Geometry evidence

A local extraction of static geometry confirms the narrow rim at y2.5:
faces4951/4954/4971 span z63.75..64.25; the doorway floor4974 spans z64.2..64.8.
The left ladder platform faces4958..4963 sit around x96.5..97.0. This explains
why sidestepping before clearing the doorway was blocked and why walking straight
into the shaft dropped the player toward the lower guard. Extracted vertex data
is local-only at `artifacts/area2-traversal/shaft-surfaces.json`.

## Correct onward route

The50m endpoint is a diagnostic detour above the intended crossing. The
[JPaterson000 walkthrough](https://gamefaqs.gamespot.com/ps2/367196-red-faction/faqs/11726)
directs the player across the third platform into the second elevator shaft;
it does not require climbing the damaged upper section of the first ladder.
Source geometry supports this: at y35, face1980 crosses the first shaft along
z63.25..64.25, and faces1895/4458/4459/4466/4467 connect the shafts near z60.
Face2354 crosses the second shaft along z63.25..64.25. Ladder1827 at x110.78
then provides the intended climb toward the exit. `python tools/replay_area3_main_shaft.py --crossing` now passes12110frames on PC:
position(110.387421,35.912914,60.889561),5health,30rifle rounds and contact with
second-shaft ladder1827 (region index3). The generator exactly matches the
executed input. This corrected branch is PC-only so far.

Vertical/offset jumps toward damaged region1833 were unsuccessful and are not
counted as evidence of a movement defect. No jump-strength or region changes
were made based on that mistaken route assumption.

## Xbox result and scope

`artifacts/xemu/render-20260915-010427`: PASS11950frames, all32 selected
comparisons, including all player-body words, seven L2S3 combat kills and three
automatic rifle kills. 4160free pages (16.25MiB) on the
stock64MiB target. All18 disc entries restored; owned XEMU closed.
This native replay takes the diagnostic50m branch. The intended35m crossing
and second-shaft climb are not covered by this native result.

A further local PC extension (`shaft-second-climb.bin/.log`) reaches
(110.387390,73.221077,61.440014) alive at12600frames and activates guards2086/2087
through the authored approach event. They remain alive. The extension overclimbs
the exit landing; stepping off at its height and clearing the corridor remain
open. Repeated top-edge ladder transitions remain visible in the counters.

## Exit landing and contact diagnostics (2026-09-15)

`python tools/replay_area3_main_shaft.py --second-climb` reproduces the12600-frame
second-ladder climb. `--exit-landing` instead stops climbing at12443, then uses
slower northward movement with pressure toward the outer wall from12460..12529.
The12620-frame PC replay finishes at(109.886497,67.870689,58.859035), walking,
with5health,30rifle rounds and both corridor guards alive. Position stays stable
for the final40frames. All seven earlier L2S3 kills remain verified.
The generated inputs match the executed local replays byte for byte.

This establishes an ordinary-input ladder dismount; no physics, health,
inventory or authored geometry was changed. Faster straight and crouched
approaches fell off the narrow rim. A subsequent jump reaches the corridor,
but the first attempt dies before completing its turn toward the guards.
The successful follow-up combat and transition are recorded below.

PC-only opt-in diagnostics: set `RF_REPLAY_TRACE=1` and
`RF_REPLAY_TRACE_FROM=<global frame>` for per-frame pose and CONTACT_TRACE.
Fields after the frame are cumulative sweep queries, cumulative hits, solid,
face, sphere, normalXYZ, fraction, state_124, velocityXYZ. The contact remains
cached until another hit; compare the hit count before treating it as fresh.
Tracing starts before that frame's input is applied. A traced/untraced replay
comparison retained identical final pose, combat, life, rifle and climb results.

Static face2385 is the one-metre-wide right walkway at y67, x110..111 and
z58.25..60.75. Face4318 is the upper corridor wall (z58.25, y69.96875..81);
face4812 extends below the corridor floor at66.96875. The failed falling
approach contacts both wall spans instead of establishing a landing.
These observations do not establish a collision defect.

The native harness now accepts explicit runs up to60000frames and3600seconds,
matching the existing runtime frame cap; defaults remain180frames/180seconds.
Replay input remains streamed on Xbox. These extended routes and limits have
not yet received a new native run; the previous11950-frame Xbox result above
is still the latest native campaign evidence.

## Corridor combat and natural exit (PC)

`--exit-combat` reproduces12910frames. Turn toward the guards before the
landing jump at12711; lower the aim during the jump and recover it while landing.
Normal rifle alternate fire kills2086 and2087. Final position is
(108.289345,67.850273,57.197807), walking,5health and5rifle rounds.
COMBAT begins59,28,9; RIFLE_ALT begins37,10,5. Both guards have-30health.
The prior seven kills and friendly miner2061's100health remain verified.

`--exit-walk` extends that exact prefix to13200frames. Normal forward movement
crosses authored exit6604 at13085, transitioning L2S3 to L3S1 without a forced
exit or staged state. Final position(-58.652866,-3.119846,-20.917120), alive,
5health and5rifle rounds. Per-section combat counters reset at transition;
the identical12910-frame prefix separately establishes the nine L2S3 kills.
All generated branches match their executed input files byte for byte.

Evidence: artifacts/area3-exit-combat and artifacts/area3-exit-walk contain the
inputs, logs, report and native PC framebuffer. This is PC evidence only.
Next: verify the13200-frame route on stock64MiB Xbox, then continue L3S1 gameplay.
The tight low-health replay does not replace representative human playtesting.
Overall first-pass estimate: ~48%; this turn extends verified campaign coverage,
not engine fidelity or visual polish.

## Full-route Xbox verification (2026-09-15)

`artifacts/xemu/render-20260915-014953`: PASS13200frames on stock64MiB.
All32 selected final-state comparisons pass, including all77 player-body words.
Native transition counters are[2,6604,13085,12743], with destination L3S1.rfl;
PC independently reports the two expected transitions at7275 and13085.
Both finish with5health and5rifle rounds. Endpoint free memory is6278pages
(24.5234375MiB), not a minimum-memory guarantee for the complete route.
The harness restored all18 saved disc entries and closed its own emulator;
a subsequent process inventory found no project XEMU session.

This establishes natural full-route arrival and selected final-state parity.
Combat counters reset on section change: the final L3S1 snapshot alone does
not independently prove all nine preceding kills. The12910-frame PC prefix
provides that kill evidence; the earlier11950-frame native run independently
covers seven. Further L3S1 traversal and combat are PC-only so far.
