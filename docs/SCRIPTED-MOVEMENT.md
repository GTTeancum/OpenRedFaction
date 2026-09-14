# First-pass scripted NPC movement

Goto (type5) and Goto_Player (type6) now issue commands to linked registered
NPCs through the shared runtime dispatcher. Authored event delays use the existing
scheduler; on replaces the retained command, off cancels it. Unknown/dead NPCs
remain unhandled targets. The command owns its destination and event UID.

This is a practical first pass, not an exact reconstruction of all original
Goto flags or behavior. Goto uses the authored event position; Goto_Player tracks
the current player body position. Horizontal speed is currently1.5 units/second,
with a0.25-unit arrival radius. It uses existing steering/angular integration,
ordinary position commit/publication and world/body sphere sweep. A blocked
step preserves position and leaves the command active. No teleport or obstacle
bypass is used. Goal commands can still be replaced/stopped after obstruction.

Full navigation-route following, grounding/slope/stair support, vertical travel,
NPC avoidance, authored speed/mode flags, arrival side effects, and locomotion
animation/footsteps remain open. Current actors retain their existing animation;
this does not yet constitute a complete escort sequence. Y remains unchanged.

## Evidence

L7S2 Goto4994 targets Gryphon4952 and has authored delay0.5 seconds. Its destination
and fields are read by the shared C level reader. The event is downstream of
Goal_Check5015 (door1>=2), which itself depends on two death-watch groups. The
contained dispatcher test retains the authored record and verifies exact delay,
forwarded fields and off-command delivery; its actor backend is a test callback.

`tools/replay_script_move.py` separately invokes the real authored Goto at frame30
through the process-local PC fixture. It does not bypass command scheduling,
movement or collision. The59-frame control has no command yet;180 frames yields
120 movement steps and3 units displacement. At600 frames Gryphon has355 successful
steps (8.875 units) and185 blocked attempts;601 frames leaves the exact same
position with186 blocked attempts. Both builds and36 CTests pass. This fixture
does not simulate defeating all guards or complete Gryphon's route.

The XEMU harness accepts --goto-uid, saves/restores campaign-goto.bin, and compares
all eight script state words and actor position/target words directly with PC.
No host input or desktop control is used. Native verification passes as recorded below.

Native600-frame verification passes in
`artifacts/xemu/replay-20260914-064211/report.json`: script state
[1,355,0,185,1,4952,4994,0] and every actor position/target word match PC.
6057 free pages (23.66MiB) remain; framebuffer inspected. This proves simulation
movement and stopping, not finished locomotion animation or an escort sequence.
The existing480-frame PC walking round trip also passes with exits at62/264.

## Authored waypoint following

Commands now connect start/destination through the existing navigation-volume
selection and graph-request helpers. Clearance comes from the retained body
spheres (maximum radius and vertical span). Selecting endpoints through authored
volumes is necessary: a direct sphere visibility query to the raw Goto position
rejected every destination candidate in this case. Navigation selection retains
its original contained/overlapping-volume and visibility fallback behavior.

The existing graph search retains up to four nodes. Movement consumes that
window and replans when it runs out, preserving owned endpoint storage and
borrowed level nodes. No per-frame allocation is added. Missing routes retry
at60-frame intervals while the existing collision-bounded direct approach remains
available. Goto_Player invalidates a route after its destination shifts over one
horizontal unit. Commands reset route state; walking remains horizontal for now.

Updated PC controls pass:180 frames gives120 movement steps;600 gives540 steps,
zero blocked steps, two successful route requests and five waypoint advances.
Previously this actor stopped after355 steps. At1500/1501 frames it instead stops
at (16.7452793,3.20673919,15.7683830), with638 movement steps and802/803 blocked
steps. That location is adjacent to Hangar door L (key3667 at X17.5); the paired
right door is key3694. Authored triggers3670 and4963 link to these door keys.
Only player contacts currently reach the live trigger-contact loop, so NPC door
activation is the next integration point to verify. This is not a completed route.

Native verification: artifacts/xemu/replay-20260914-065556/report.json passes600
frames. Route words [2,2,0,5,4,3,0,0], movement [1,540,0,0,1,4952,4994,0]
and actor position/target words exactly match PC.6056 free pages (23.66MiB)
remain; native framebuffer inspected. Both builds,36 CTests and the updated
59/180/600/1500/1501-frame PC controls pass. Native1500-frame hangar-door stop
has not been checked separately. Trigger4963's filter3 rejects player-controlled
actors in rf_trigger_eligible, consistent with the needed NPC contact path.
# First-pass locomotion selection

The process-local inspection camera now follows an authored NPC UID, using a
world sweep to shorten its offset near geometry. It runs after player/combat
updates and changes only rendering view inputs; the real player stays put.
PC replays opt in with `RF_REPLAY_FOLLOW_UID=4952`. The native harness accepts
`--follow-uid 4952` and saves/restores `campaign-follow.bin` with its other flags.

PC captures at980/1000/1020 frames were individually inspected: Gryphon changes
walking pose in the room beyond the hangar, with feet near the floor. The1000
frame movement, position, trigger and occupancy telemetry exactly matches the
ordinary camera replay. Captures: `artifacts/npc-doors/follow-980.png`,
`follow.png`, `follow-1020.png`. No foot-sliding/speed-match claim is made from
these three stills. Native inspection-camera validation is pending.

Update: native tracking-camera replay passes on stock64MiB XEMU in
`artifacts/xemu/replay-20260914-075441/report.json`. Its1000-frame capture was
inspected and shows Gryphon in the walking pose beyond the hangar, matching
the PC view. Movement and contact telemetry match PC; the camera remains an
explicit diagnostic aid, not automatic campaign camera behavior.

Scripted run/fall actors now commit accepted floor contacts through the existing
ground-probe/support helpers and fall with collision-checked gravity when no
support is found. Hidden actors and other movement modes are excluded. Uphill
contacts with normalY>=0.5 project the proposed step along the support plane,
then repeat the full-body sweep so walls and ceilings still block movement.
This is practical shared locomotion, not full original physics: stairs, moving
platform transport, fall damage, landing effects and general navigation remain.

The initial ground integration stopped at L7S2 face904 with normal
(-0.023749,0.997374,0.068417), exposing a gentle uphill floor as a horizontal
sweep obstruction. The slope retry fixes that regression. All three PC door
controls pass; visible Gryphon ends atY2.987934 instead of spawnY3.206739,
with999 successful steps and zero blocked steps. Both builds and36 tests pass;
combined native grounding/animation validation is pending.

Update: stock64MiB XEMU1000-frame replay now passes in
`artifacts/xemu/replay-20260914-074151/report.json`. Corrected position
(23.013216,2.987934,16.036537),999 steps, zero blocked steps, trigger contacts
and occupancy match PC. The native framebuffer was inspected but keeps the
starting-tunnel camera; walking visual quality remains unverified. This replay
predates the subsequent Make_Invulnerable scene binding.

Scripted movement now requests authored logical state2 (`walk`) after a
successful movement step, and state0 (`stand`) when blocked, arrived or
cancelled. Requests use the existing quarter-second transition and do not
restart an already selected state. Hidden actors do not receive these requests;
missing walk mappings retain the existing stand fallback. Armed/stance states,
animation speed matching, footsteps and grounding remain open.

Contained tests cover walk/stand selection, preserving an in-progress blend,
hidden actors and missing mappings. Both builds and36 CTests pass, as do the
three PC hangar-door controls (`artifacts/npc-walk-replay.log`). This change still
needs native replay and visual walking confirmation; earlier XEMU door evidence
predates the locomotion selector.

The next L7S2 obstruction nearX27.471 is not evidence of a route-search failure:
Door SecurElev key4880 and CheckPt Door02 key3768 are linked to player-use
triggers4930/5346 and Delay5324 (3s), itself reached from Delay5325 (120s) through
When_Dead5323. Preserve that mission interaction rather than bypassing the doors.
