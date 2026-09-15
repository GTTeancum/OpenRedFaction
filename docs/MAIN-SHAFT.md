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
