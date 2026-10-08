# Common script-chain propagation after effects

Written 2026-10-08 against `8c20f4f6b624b8903568d3a87fd5113effaf1d69`.
Source review only; no builds, tests, fixtures, game runs or campaign traversal.

## Concrete missing behavior

`startup_event_action` receives effect phases 0/1 and a separate common
propagation phase 2. Its generic phase-2 handler was below several effect
handlers that returned early. The earlier seven-type effect-link fix therefore
left animation, alarm, single-shot, invulnerability and other early adapters
swallowing their outgoing authored event links. Attack/Goto and later handlers
were already protected by the existing generic guard and are not new fixes.

A read-only inventory of the three original SP archives finds 58 event-to-event
edges from 38 such effect events across 15 levels. Examples:

- L11S3 Play_Animation10624 -> Play_Animation10638 -> Play_Animation10639,
  together with UnHide10653 and Make_Invulnerable10663. The linked actors are
  Capek, Eos and Parker in the authored laboratory sequence.
- L13S1 Play_Animation8375 -> Goto8280/8276/8277 and Set_Friendliness8376.
- L2S1 Make_Invulnerable7191 -> Ignite_Entity7190.
- L6S2 Alarm6509 -> Goto_Player6525 and Message6551.
- L20S1 Play_Animation12687 -> Explode12436, Message12052 and Delay12692;
  Shoot_Once12546/12547/12549 -> Delay12551/12550/12552.

These are actual authored references, not invented demonstration graphs. This
change establishes code dispatch, not successful end-to-end mission completion.

## Original reference

RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `4b8b70`: immediate activation calls the ON/OFF virtual effect at
  `4b8c06/4b8c0f`, then queries `4b8c40`, then invokes common propagation
  `4b8b00` at `4b8c30`.
- `4b8ce0`: delayed expiry calls the effect at `4b8d03/4b8d0c`, queries the
  same eligibility helper, invokes propagation at `4b8d39`, and clears its
  timer at `4b8d40`.
- The `4b8c40` byte/jump tables route types 7, 9, 11, 12, 24, 43, 46, 49,
  53, 67, 79 and 86 to the true return at `4b8c5e`; type1 takes that default.
- `4b8b00` traverses links in stored order and distinguishes ON exactly by
  the low byte equaling 1, forwarding the existing source and actor arguments.

## Implementation

The common phase-2 entry now precedes all effect adapters. It calls the same
`startup_target` loop with the same source, actor, mode, registry resolution,
recursion bound and error-stop behavior. The original activation/timer code
still invokes the effect before propagation, exactly once each. No effect is
replayed as an OFF operation by the propagation phase.

UnHide keeps its existing deferred-object/synchronous-event split. Conditional
Goal_Check, Switch, Invert, removal and other special propagation ownership stay
with the existing `propagates` gate. No wire layout, resource allocation, callback
interface, vehicle behavior or authored input is changed.

Verification remains with the parent hourly batch. Overall implementation stays
approximately 88%; objective/event progression is newly repaired but runtime
unverified, with no percentage uplift claimed from this patch alone.

## Delayed camera and monitor chains (post14:00)

The common link-phase repair does not help when a supported timer never gets
stepped. Shake_Player18 was absent from delayed runtime admission and had no
scene callback. Actual L11S3 event12108 (`lab_explode`,5.5s) links Play_Sound12116
and Endgame12150; its timer could remain pending forever. Eight of24 authored
Shake_Player records have delays. The existing camera feedback implementation
already models the required effect, so this slice wires that real owner rather
than substituting a generic delay or skipping the camera request.

Original loader462523 passes values[0]/values[1] to4b8040, which stores them at
+2b8/+2bc. Shake ON4bb660 sends the first value multiplied by the exact binary
float0.01 at5897b0 and the unchanged second value to local camera40e0b0.
The scene now uses that arithmetic and `rf_scene_player_feedback`, including
its real player handle, shared camera effect deadline and render consumer.
OFF leaves the current effect unchanged. The original4b8c40 table admits
common outgoing propagation for18, so normal ordering remains effect first,
links second. RFEC retains a queued shake as VISUAL; live camera presentation
continues the existing first-pass omission policy on load, without affecting
objective timers or adding an unsaved gameplay owner.

Force_Monitor_Update60 now also services its delayed timer. Original4b9af0 only
invalidates monitor image caches; this renderer has no such retained image to
invalidate. Its effect is therefore an explicit empty-cache operation, while
normal common link propagation reaches linked Monitor_State configurations.
This covers actual delayed L6S3 events6644 and6649 without claiming rendered
monitor images. See research/FORCE-MONITOR-UPDATE-20260925.md for that source
boundary. No new tests/fixtures or helper builds were run; parent15:00 owns
verification. No changes are made to explosion geometry or vehicle scope.
