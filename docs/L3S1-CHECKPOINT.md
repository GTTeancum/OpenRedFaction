# L3S1 checkpoint route

The checkpoint control is in the elevated booth. It operates a different opening
in the lower hall; walking east from the control leads into a solid booth wall.
This was a route error, not evidence of a broken door or collision routine.

## Authored geometry and events

Trigger861 is near(-32.7604,.17047,-17.5667) and requires Use. It links key859
of Checkpoint_Door and Goto_Player event2196, which targets guard892 after.3sec.
Mover1219 is centered(-32,-2.5,-21.75), with local bounds x+/-.0625,
y+/-1.5 and z+/-1.75. Thus its closed opening spans z-23.5..-20 in the lower
hall, outside the booth's z-17.35 approach. Its two keys differ by2.875 vertically
and use3-second directional travel times.

The Station_Door on the ladder side room is mover836, keys837/838, operated by
trigger853 near(-44.25,-2.75,-19). Its short travel times permit the return path.
Original moving-groups/movers inventories and the installed level geometry are
the source of these coordinates; no authored values were changed.

## PC route evidence

The15060-frame normal-input continuation returns west across the booth, enters
the ladder before issuing downward input, descends, and returns through the side
room into the lower hall. Endpoint(-41.901833,-2.888007,-21.110769),55health,
42loaded rifle rounds. Both ladder entries and exits are recorded. The first
return attempt issued downward input too early; it did not establish a defect.

The15220-frame continuation walks east through the actual checkpoint opening,
ending(-29.992422,-3.118479,-21.107328). Guard892 approaches through the authored
Goto_Player behavior and lands one10-damage shot, leaving45health. The player
remains alive with42loaded rounds. The brief edge impact entering the hall clears
with continued normal walking; no jump or collision change was necessary.

The15290-frame continuation clears guard892 with two rifle hits. It finishes
at(-29.988010,-3.118479,-21.107328),45health and36loaded rounds. Guard109
remains dead and allied miner789 remains at100health. This uses the existing
broad-box bullet collision; more precise hit detection remains open below.

Reproduce with `python tools/replay_l3s1_checkpoint.py`; `--lower-hall` and
`--passage` select the earlier endpoints. All three generated inputs match their
executed fixtures byte for byte. No pose, health, inventory or forced-event
injection was used.

## Xbox verification

Local artifact `artifacts/xemu/render-20260915-024010` passes the15290-frame
stock64MiB run and all33 PC/native comparisons, including all77 player-body
words. Both natural section handoffs match (7275 and13085). Endpoint free
memory is6117pages,23.89453125MiB; this is not a minimum-memory measurement.
All18 staged disc entries were restored and the owned emulator exited.

## Next encounter: measured bullet collision defect

The15510-frame main-room probe wounds allied miner910, provoking retaliation.
Opt-in diagnostics reproduce the original combat journal and final state exactly.
At local frame2350/global15435 the selected miner bounding box reports a hit,
but the actual post-spread shot misses all three transformed body spheres:

| Sphere | Radius | Closest shot distance |
| --- | ---: | ---: |
| Lower | 0.600 | 1.596088 |
| Middle | 0.400 | 1.345558 |
| Upper | 0.150 | 1.338604 |

Guard781's frame2356 hit also misses all body spheres; frame2362 intersects
its middle sphere. This establishes disagreement with existing body collision
shapes, not mesh-perfect visual hitboxes. Both player and enemy hitscan currently
use `combat_box` alone. A shared narrow-phase body test, nearest-target ordering,
world obstruction and updated encounter replays remain to implement/verify.
Do not count the main-room probe as an accepted clean campaign route.

Set `RF_REPLAY_TRACE=1` and `RF_REPLAY_TRACE_FROM=15435` for the PC replay
to emit `SHOT_RAY` and `SHOT_SHAPE`. These describe broad candidates before
world obstruction/damage; correlate with `COMBAT_EVENT` for actual damage.
`tools/analyze_shot_shapes.py <log>` checks finite shot segments against the
transformed spheres. Local evidence is in `artifacts/combat-shape-trace` and
the original input in `artifacts/l3s1-main-room-safe` (the probe name does not
mean the miner was unharmed). Diagnostics do not change hit selection.
