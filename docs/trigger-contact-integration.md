# Campaign trigger contact readiness

The scene now polls the existing reconstructed 4bfc60 contact stage against the
registered campaign player after the physics position commit. This is readiness
instrumentation, not firing: activation counters, key handling and controller
motion are not consumed. A ready count can repeat while the player stays inside.

Loader evidence (original executable SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836):
465510 stores the low-byte value in local_3c, constructor input +68; 4bf970
copies this to runtime +2c4, the filter inspected by 4c06d0. The third raw
fields word enters constructor +24 and runtime +304 (attachment reference).
The live path currently accepts only filter zero without an attachment/script
or owner/class-dependent flags. Unsupported polls are counted explicitly.
Use input is currently zero. Ready is before the original key/activation gates.

rf_scene_trigger_contacts exposes six words: polls, ready polls, last ready UID,
lower-door UID8542 ready polls, skipped unsupported polls, last status.
These are shared PC output and XEMU memory observations; no per-tick allocation.

Validation:
- PC/NXDK builds and all six CTests pass.
- verify_trigger_poll.py: 4096 original/PC/NXDK cases pass. This validates the
  composed gate/contact/delay function, not original whole-scene scheduling.
- replay_door_contact.py --native, 180 staged frames, stock64MiB XEMU:
  artifacts/xemu/replay-20260910-164443/report.json. Both targets produce
  [8950,358,9599,179,1969,0]. UID8542 is ready on 179 simulation ticks.
  Existing mover collision and complete final contact metadata still match.
- PC authored-spawn 180-frame idle control: zero UID8542 readiness, no errors;
  artifacts/door-contact/spawn-idle.txt.

Next: connect ready activation to the ordered controller/event links and live
controller ticks, preserving key gates and effect ownership. Door opening and
traversal are not yet demonstrated. Other filters need resolved entity/attachment
ownership; this instrumentation must not become a silent replacement for them.

## Owned activation dispatch

rf_runtime_trigger_fire_links composes owned resolved targets, typed 4c0320
routing and 4c0220 SP bookkeeping without allocation. The caller supplies the
event/controller backend and resolved contact/key/player gates. It skips
unresolved targets, stale handles and other types; suppression uses the low
byte and applies only to controllers. Callback failures stop subsequent effects
but do not undo prior effects or the trigger bookkeeping.

PC --runtime-trigger-links checks 258 suppression values, blocked activation,
two ordered controllers followed by an event, ignored type5/unresolved targets,
pre-bookkeeping callback state, cooldown/count/limit/clock and backend failure.
Existing runtime-trigger-fire and six CTests pass; both platforms build.
verify_trigger_links.py retains 1024 original/PC/NXDK exact callback traces;
verify_trigger_fire.py checks the original bookkeeping independently. The new
owned wrapper is PC-tested, not yet exercised by live Xbox controller effects.

## Authored lower-door numeric replay

verify_authored_door_motion.py executes original469800 and46a8f0 with the
retained L1S1 keys for controllers8593 and8591, their authored timing, mode2
and initial flags80002002. Both start already activated on the same60Hz tick.
The original source-trigger handle is absent, sound handles are disabled and
key event links are absent as authored. Original obstruction/occupancy helpers
execute against this empty fixture; this does not exercise a player in the door.

840 ticks match the complete mapped numeric state on PC and NXDK. Both
controllers first move at tick1. Controller8593 reaches the open position at
tick142 and8591 at144. Their arrival bookkeeping follows at143/145; dwell
ends at203/205; final closed idle is325/326. This small authored numerical
difference must not be mistaken for the earlier separate-start defect or
removed by forcing both controller trajectories to have identical endpoints.

The remaining original gates are46a280 (hold open while occupied, guarded
by bits1/2/2000 and mode!=1) and46a1e0/46bae0 (closing obstruction reversal).
Live motion integration still needs these, arrival effects, sound ownership
and attached-object commits. Report: artifacts/authored-door-motion.json.

## Occupancy scan

rf_trigger_occupancy reconstructs the actor/item scan used by46a280 and the
actor-only portion of46a1e0. Source lookup and controller mode gates remain
external. Actors with flag4000 are ignored; an overlapping actor returns
immediately, without scanning items. Otherwise all overlapping items request
wake in list order. Snapshot inputs are borrowed and no memory is allocated.

Box containment uses507a50 inclusive boundaries. Non-box shapes follow the
original sphere branch: stored position differences, absolute components sorted
a>=b>=c, partial=c/8+b/4, magnitude=a+partial+partial/2, strict magnitude<radius.
This intentionally differs from the ordinary trigger sphere-contact helper.
Negative and zero radii do not admit an occupant. NULL volume means a missing
source trigger. Callers must not reuse this scan as actor eligibility.

verify_trigger_occupancy.py executes original46a280 with real geometry and
flag helpers, supplied source lookup and captured item wake effects.2048
original/PC/NXDK cases match (857 occupied;540 wake requests). Fixtures include
flagged actors, early return, multiple items, box boundaries, signed sphere
radii and unknown shapes. Both builds and six CTests pass. Live scene ownership
and controller reversal are not claimed.

## Translation reversal

rf_group_translation_reverse reconstructs46bae0 for translation controllers.
An idle current/target index is an unchanged no-op. Active reversal computes
the stored segment length, complements/clamps traveled distance, resets phase
with the original directional timing clamp, flips2000, exchanges key indices
and clears speed/velocity. Position, pending pose and timer stay unchanged.
This is the state transition after the external obstruction decision; it does
not itself decide whether a door should reverse.

verify_group_reverse.py:4096 original/PC/NXDK cases with actual key lookup,
length, clamp and vector helpers. Full runtime and original object mutation
match, including zero length, negative timing, idle indices and out-of-segment
distance. Both builds and six CTests pass. Report is ignored local output
at artifacts/group-reverse-verification.json.

## Live controller activation prefix

Default player contacts now call owned trigger firing and dispatch into actual
registered translation controllers. rf_group_activation_begin updates their
motion state and the player controller backlink. Per-controller20-byte sidecars
retain source, actor, pending activation effects, start count and frame. They
are allocated once and freed with scene mover ownership (100 bytes for L1S1).
This intentionally does not claim the sound/alert/wakeup tail has executed;
those requests remain pending. Motion ticking is still unconnected.

Event targets now enter rf_runtime_event_fire, sharing the supported startup
action backend and delayed event state. Unsupported actions/targets contribute
to telemetry, and unsupported delayed events stay pending. Lower-door event
9826 is Set_Friendliness with a1-second delay, not a motion command; its entity
action remains unsupported. This is not complete event-target coverage.

The scene clears fired bit64 per frame as4bf740 does, before player polling,
and excludes triggers marked for removal. The rest of4bf740 deferred key and
attachment behavior remains open. Raw fields0/1/2 must all be absent on this
path, with no script or unsupported actor filter. Global inhibit/player-script
gates are absent in the current reconstructed player lifecycle.

180-frame staged PC/XEMU replay: artifacts/xemu/replay-20260910-170030/report.json
passes stock64MiB, full collision metadata, event ticks and activation telemetry.
LIVE_ACTIVATION=[4,6,2,6,5,0,1,1]: four trigger fires, six controller calls, two
starts, six event calls, five pending/unsupported effect observations, no
errors, both lower controllers start at frame+1=1. Trigger readiness now
respects activation cooldown: [8414,4,8542,3,2327,0]. Collision is unchanged.
Both platform builds and six CTests pass; runtime event probe also verifies
direct event source/actor, gravity dispatch and wrong-type rejection.

## Live motion and mover pose integration

The scene now advances registered translation controllers after player trigger
contacts/event ticks, polls occupancy against the current registered player,
re-arms the target-key dwell timer for occupied holds, and invokes reversal
on the recovered closing branch. Arrival calls the first key event target when
resolved; remaining key targets and sound requests remain explicitly pending.
Only the player exists in the live occupancy snapshot; future actor/item
ownership must extend this snapshot and item wake handling.

Controller views feed the owned mover propagation and commit helpers; collision
views sync afterward. The renderer already consumes these same owned poses.
General-object attachments and rotation contributions remain counted pending;
rotation entries must not supply invalid translation contributions. Retained
view storage is24 bytes/controller on Xbox plus8192 bytes of pose slots; the
existing20-byte/controller effect sidecars remain. There is no per-tick heap
allocation. Physics/controller/render scheduling is still reconstruction scene
integration, not a claim of original whole-frame equivalence or platform carry.

Evidence:
-180-frame forward staged replay, PC/stock64MiB XEMU, native report
 artifacts/xemu/replay-20260910-170532/report.json: both doors reach authored
 open positions,179 ticks,2 arrivals,40 mover contacts, exact PC/native full
 contact and position words.
-420-frame idle staged replay, final code, native report
 artifacts/xemu/replay-20260910-170632/report.json:419 ticks,8 occupied holds,
2 arrivals, no reversal/errors; both doors remain at their authored open keys.
 Motion words [419,419,8,0,2,2,1,0] include one pending rotation binding.
- Authored-spawn PC180-frame idle control activates zero controllers.
- Both builds and six CTests pass. The harness now asserts authored open-key
 positions for180-frame/default or long idle fixtures, not only PC/Xbox equality.

The PC native render capture was inspected. Its close camera is obstructed and
unsuitable for a meaningful README screenshot. Successful traversal, live
closing reversal, usable demonstration camera and activation/audio/AI effects
remain open. Do not claim a complete playable door interaction from these tests.
