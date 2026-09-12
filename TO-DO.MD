# Red Faction Xbox / PC - open work

Shortlist:
- Complete the single-player actor, AI, damage and mission runtime.
- Connect retained NPC speed/body/stance owners to support queries, contact/fall/landing dispatch and physics stepping, then timed footsteps.
- Complete moving NPC animation, room membership, LOD selection and lighting.
- Finish controller/event interactions, traversal, particles and GeoMod.
- Complete audio playback ownership, shared RNG ordering and level transitions.
- Keep shared PC/Xbox behavior verified within the stock64MiB memory budget.
- USER: Review PS2 visual parity and test hardware after representative XEMU gameplay is stable.
- USER: Review the interactive PC movement and audio items listed below.

The stationary startup-NPC animation gate matches PC/stock64MiB XEMU;
footstep selection and support-material helpers are verified but not live.
Detailed source evidence and test scope are recorded in docs/NPC-MARKERS.md.

Immediate open items:
- [ ] Bind verified post-bounds contact preparation49f8ea/49fdbe to live physics scheduling; attach native-verified registered player/NPC response publication to scheduled pair processing, extend binding to other actor families, then dispatch responses and publish flags even on general-response deferral returns.
- [ ] Bind verified pair processor48ca60 to live scheduling and contact/response effects after PC/stock64MiB XEMU verification of registered NPC/player views and borrowed player model publication; attach native-verified normal/general actor-pair publication to48ca60 dispatch, then connect NPC support-velocity refresh, bind verified model dispatcher503120/5031f0 to native-verified owned static geometry queries, then bind original-verified54e140 coordinate preparation and prepared type2 geometry, bind native-verified static collision resource residency and queries to live scene owners, and decode remaining110c21 rendering batches while preserving original-verified authored NaN-plane rejection, and bind original-verified skeletal matrix-refresh/query composition to evaluated NPC poses through native-verified registered-model collision caches (78 owners/6501 refreshes with complete retirement), and connect native-verified actor/model contact publication (49afe0 through5031f0 and retained NPC geometry; player/NPC callers) to pair scheduling, adding demand evaluation for stale poses and dynamic LOD selection, then bind solid response49b570 to geometry/cache services and extend model response49afe0 beyond retained player/NPC callers, add trigger/projectile views and moving NPC orientation ownership, complete projectile classification, global-plane producer48bbe0 and live owner lookup.
- [ ] Compose registered-NPC death entry with collision teardown and remaining stages; connect the original-verified 8192-record collision pool after completing classification48be00 beyond the verified actor/actor branch and binding the original-verified discovery traversal to object-list ownership and creation/unhide scheduling.
- [ ] Bind complete death scheduling and effective class views after the composed base NPC death-animation/action-sound fixture passed PC and stock64MiB XEMU; extend beyond the single authored miner case.
- [ ] Connect the tested registered-model bone-clear adapter to full death dispatch and recover original override producers, then verify enabled overrides in XEMU; baseline stock64MiB replay passes with the new storage.
- [ ] Compose the verified ordinary-SP death stages using the retained NPC death fields, composed base/unarmed animation stage and tested registered-NPC tail adapter, completing remaining shared field ownership, accounting for the 6fc4d8 alternate-mode gate and remaining live resource callbacks.
- [ ] Reconstruct riot-shield impact-mark allocation/clipping/rendering and bind verified player-slot cleanup to the original-audited 50-mark/150-polygon/500-vertex pools.
- [ ] Connect the authored-tested file-tag/room-query/shared-lightmap source composition to death dispatch and bind room queue scheduling; isolated authored texture/growth draws now pass PC and stock64MiB XEMU, while campaign source ownership and queue scheduling remain open.
- [ ] Recover generic cached CSPH tag parent initialization/placement; the95-model original audit clears cached-tag precedence for all190 corpse eye/spine queries.
- [ ] Bind verified base corpse-motion lookup to live creation and recover weapon-switched declaration views; canonical availability masks are retained.
- [ ] Connect the authored-tested owned corpse constructor and class-input binding to live death dispatch and resource effects; bind the authored-tested corpse update adapter to live dispatch, replacement materials and moving room lookup.
- [ ] Enable authored-class sphere transfer and tested corpse transition/refresh in live dispatch and recover attached item creation/handle publication and bind the tested item world-pose callback to live owners.
- [ ] Bind verified corpse retention/fade/update/deletion and30-slot allocation to live creation and owned resource backends; five eligible bodies trigger fading.
- [ ] Connect native-verified burn retarget/release to live corpse inheritance after corpse ownership is reconstructed.
- [ ] Bind verified finalization418f80 and its owned-corpse creation adapter to live actors, burn/player owners, geometry queries and deferred deletion.
- [ ] Connect the native-verified registered NPC/player death-clearance query to complete death-start and moving-NPC orientation ownership.
- [ ] Complete death-start41fdc0 around the verified early player/timer stage41fe5f..41feed, death-animation handoff, configured-item drop linked-actor handoff4204a1..4205e8 and SP tail cleanup420b03..420bdc, bind real item allocation/query/bounds and connect the verified SP weapon-drop42ae10 resource callbacks and shared inventory removal4031a0, bind verified player-mode fields and player-specific cleanup including riot-shield impact marks, bind the verified shared dying-update41ee40 to live owners and recover their player/camera handoff; dead/dying queries, death-entry state, collision-pair retirement and death-action selection are verified; live clearance integration, playback and remaining death effects are open.
- [ ] Extend integrated player damage/pain/death audio to authored callers and recover death animation, camera, game-over and respawn ownership.
- [ ] Attach the verified event-to-player damage/flash/camera binding to authored activation with complete player pain audio/death reactions and camera-mode lifecycle; add weapon damage and verify HUD/pause presentation timing.
- [ ] Connect and verify NPC settling/fall/landing; deeper diagnostic sweeps find walkable floors below all24 opening-level short-probe misses.
- [ ] Complete NPC distance LOD selection, actor visibility/draw gates, lighting and moving room membership.
- [ ] Connect NPC AI/state reselection, weapon overlays and footstep consumption; broaden startup beyond audited opening classes.
- [ ] Connect verified impact amount and force-region suppression to NPC contact ownership and the shared damage backend using registered skeletal owners.
- [ ] Connect real event/fall/weapon damage sources to the registered NPC adapter, implement pain/death/burn/AI effects and broaden lethal/reentrant coverage.
- [ ] Extend verified NPC flinch/pain playback beyond the two-hit fixture to real damage callers, weapon reset, armed overlays, death and AI.
- [ ] Wire NPC world/support callbacks into the shared standing transition and normal/slow landing, then special class effects.
- [ ] Verify live NPC action transitions and sustained cache pressure on Xbox; selected clips load within budget and zero-reference payloads can now be evicted.
- [ ] Independently verify live ambient scheduling and gain decisions against the original runtime.
- [ ] Connect the verified burn attachment backend to persistent NPC owners and complete live burn resource ownership.
- [ ] Connect live Switch effects/playback (rejection PCM is prepared) and establish whether/when original linked initialization executes.
- [ ] Extend the verified Continuous_Damage-to-NPC/player path to authored activation, complete reactions and other target families; implement hostile-source AI separately.
- [ ] Resolve the L1S3 pressure-test route after fixing its 14 static ray errors, complete audio category settings, and recover light ownership.
- [ ] Verify grounded force re-entry/flag clearing, extend force ownership beyond the player and recover full frame/RNG ordering.
- [ ] Verify per-frame lift support identity, horizontal dismounts, use-input scheduling and linked-object use interactions.
- [ ] Complete APU failure cleanup auditing and real-hardware validation.
- [ ] Implement sound metadata persistence across level transitions and reconcile release scheduling and borrower completion after verifying cache reset and its count-based lookup limitation.
- [ ] Complete controller audio groups/loop metadata and validate interactive PC/Xbox playback.
- [ ] USER: Listen to the PC --campaign diagnostic and report dropouts or unexpected volume/pan changes.
- [ ] Complete moving-platform carry/response and extend door traversal beyond the staged fixture.
- [ ] Complete controller sound/alert/wakeup effects, rotation and remaining key-target actions.
- [ ] Complete event actions, delayed ticking and remaining target families beyond recursive startup dispatch.
- [ ] Complete particle rendering for animated textures, stretched sprites and mixed room/object passes.
- [ ] Connect dynamic particle ownership, event mutations and shared campaign random state.
- [ ] Extend native campaign coverage beyond the four verified gravity-level startup replays.
- [ ] Recover jump audio ownership and validate authored traversal.
- [ ] Verify the route from the authored player spawn and the upper climb exit after the near-base approach replay.
- [ ] Fit campaign texture residency within stock Xbox memory and add animated VBM/mip rendering.
- [ ] Recover original player-view behavior and address camera collision while connecting campaign player input.
- [ ] Replace diagnostic miner placement with the verified level player start through reconstructed player creation.
- [ ] Integrate class eye offsets into the player lifecycle and replace scripted stance eligibility.
- [ ] Capture and replay any further interactive exits; the historical tick-99 cause remains unproven.
- [ ] Reduce the remaining world-projection cost and avoid projection work on catch-up ticks.
- [ ] USER: Verify PC look, crouch, focus loss, resizing and Escape exit; basic movement is user-confirmed.
- [ ] USER: Check campaign jump (Space/A), crouch, controller reconnect and Back+Start exit when testing the interactive build.
- [ ] Complete entity registration and the single-player runtime under the stock 64 MiB limit.
- [ ] USER: Review PS2 visual parity and test real hardware after representative XEMU gameplay is stable.

Controller propagation, support refresh and position commit now bracket physics;
PC/XEMU door replay passes, while an authored lift carry replay remains open.

Campaign now declares88 global sounds before controller PCM; native door replay
retains only four waveforms and verifies124024 bank bytes with unchanged output.

Sound-table reader/archive loader pass20 cases with all88 owned rows intact;
native archive I/O and campaign registration now pass; level transitions remain.

Metadata-only bank registration passes PC and native136-byte-budget checks;
connect global sound declarations and campaign residency policy next.

PC selective voice release passes protected-source-page testing with independent
playback continuing; shared campaign residency policy still needs integration.

Xbox selective release and injected stopped-observation failure/retry pass three
native cycles; actual hardware-stall coverage and campaign residency remain open.

Native64MiB APU residency passes three playback/unload cycles and two reloads;
bank accounting drops57150 to136 bytes, while automatic campaign policy remains open.

Original counter-gated cleanup verified over55 voice slots; keep flag forwards
unchanged and does not change voice selection, while counter balancing remains open.

Original pre-level sound counter update verified across2600 slots; recover
counter balancing and cleanup scheduling before persistent residency integration.

Sound-table inventory finds one controller overlap across68 levels (L14S3
Switch_01.wav); startup/level ordering still needs verification before integration.

Initial audio gains now precede playback on PC/Xbox; muted-start checks and
stock64MiB integrated door replay pass with unchanged spatial telemetry.

PC campaign audio now uses shared voices on a bounded device-clock backend;
two refill/stop/reopen checks pass with9220 fixed bytes plus host overhead.

Closing/reversal spatial replay also passes stock64MiB XEMU with694 listener
refreshes and390 changed settings matching PC; PC device backend remains open.

Live Xbox spatial gains now follow gameplay listener pose; stock64MiB door
replay matches PC settings with APU output, while unity PCM remains diagnostic.

Gameplay listener-pose calculation is separated from projection and diagnostic
camera overrides; connect refresh at this once-per-frame pose boundary next.

Original refresh path passes80 end-to-end setter cases and confirms no
default-sample gain reapplication; listener pose scheduling remains to connect.

Original listener refresh505ec0/5058c0 traced; resolve pose timing and
initial-versus-refresh sample gain before connecting campaign spatial updates.

Production audio adapter and shared mixer now support logical-handle gain
updates without restart; native adapter calibration passes, listener calls remain.

Native APU intermediate gain/pan calibration passes at -6/-20dB volume and
10dB side attenuation; live campaign listener wiring remains open.

Device attenuation-to-channel amplitude adapter passes76 PC/NXDK cases;
native gain calibration and campaign listener wiring remain open.

Shared C device volume matches17584 original PC/NXDK lookups using202 bytes
of constants; Xbox gain conversion and live wiring remain open.

Original device volume tables and quantization pass202 entries/17584 lookups;
mode-flag meaning and conversion to Xbox gains remain open.

Native APU running-voice left/right/mute routing passes DSP checks without
restarting playback; original gain conversion and listener wiring remain open.

Campaign banks retain first-registration controller volume/range/rolloff within
the memory budget; global table ordering, loop flags and playback use remain open.

Shared far-cutoff C matches4221 original PC/NXDK cases; registration ordering,
category gain and live metadata loading remain open.

Controller registration range/volume and derived far cutoff traced in original;
verify category gain, first-registration ordering and filesystem playback metadata.

Shared positional audio now matches6688 original PC/NXDK cases, including
general directions after correcting normalization precision and dot-product order.

Shared positional C matches original PC/NXDK outputs for2592 axis-aligned
cases; general directions, metadata and live listener integration remain open.

Original positional audio passes2592 axis-aligned cutoff/gain/pan cases with
unchanged helpers; shared C comparison and general-direction cases remain next.

Original positional audio attenuation and sample-mode branch located; verify
505740 arithmetic and 522530 playback semantics before spatial integration.

All ten APU initialization allocation-failure boundaries restore pages in XEMU;
subsequent successful initialization and sixteen-voice playback still pass.

Production APU adapter passes sixteen overlapping voices with DSP output,
overflow rejection, slot reuse and stale-stop protection; shutdown restores pages.

Live campaign APU output passes stock64MiB XEMU door and closing/reversal
replays; hardware playback uses its own clock and reset releases PCM references.
PC gameplay/PCM checks remain matched; spatial audio and PC device output remain open.

APU static replay, eight stop/recreate cycles and backend reinitialization
pass stock64MiB XEMU with restored pages; campaign wiring remains next.

Isolated APU voice/DSP output and clean shutdown pass stock64MiB XEMU;
44 allocated pages return; campaign integration and PC device output remain open.

Live controller PCM now matches PC/stock64MiB XEMU; independent full-stream
reference passes; spatial parameters and audible device output remain open.

Owned deduplicated PCM bank passes PC lifetime/budget checks; two door
files retain93402 bytes; scene audio wiring and native output remain open.

Shared836-byte audio mixer passes144 PC/NXDK cases; live controller audio
ownership, spatial parameters and platform playback remain open.

Bounded PCM resource parsing passes81 PC/NXDK cases; door samples need
93024 PCM bytes; missing loop-name resolution, mixing and audio output remain open.

Corrected the staged approach axis; traversal and occupied closing reversal
now pass on PC/stock64MiB XEMU; authored-spawn route and effects remain open.

Live lower-door opening and occupied hold now pass on PC/stock64MiB XEMU;
traversal, live reversal coverage, sound and broader controller effects remain open.

Live player contact now starts both lower-door controllers on the same PC/XEMU
tick; motion ticking and retained activation effects remain unconnected.

Controller translation reversal matches4096 original/PC/NXDK cases;
live occupancy-gated hold/reversal and controller effect wiring remain open.

Door occupancy scanning now matches2048 original/PC/NXDK cases; live
source-trigger ownership, hold timing and closing reversal remain open.

Authored lower-door motion matches840 original/PC/NXDK ticks; both start
together, open at142/144; live occupancy/reversal and effect wiring remain open.

Owned trigger activation can now dispatch ordered controller/event backends;
live controller effects and motion ticks remain unconnected.

Live default-player trigger readiness now matches PC/XEMU at the lower door;
179 ready ticks, no activation yet; remaining filters and controller effects stay open.

Campaign player now shares a registered handle across object/entity view registries;
XEMU door replay passes; class classification and trigger polling remain open.

SP trigger-link routing matches1024 original/PC/NXDK cases; lower trigger8542
links controllers8593 then8591; player polling and live activation remain open.

Staged lower-door replay now records111 mover contacts on PC and stock64MiB
XEMU with exact final contact metadata; opening and authored traversal remain open.

Eight authored-spawn PC approaches reached only static contacts; the2250-frame
door-direct route stops near Z10, short of lower movers near Z-19; inspect route/stage next.

Campaign ground queries now include movers with per-query contacts and support
commits; L1S1/L2S1 XEMU replays pass; targeted mover contact/carry remain unverified.

Landing support/contact velocity rebasing matches 4096 original/PC/NXDK
cases; mover-aware ground records and scene support lifetime remain open.

Static/mover support commit matches 2048 original/PC/NXDK cases, including
rising-platform height correction; ground-query and support-state wiring remain open.

Campaign player sweeps now query registered movers and static world with authored
materials; L1S1/L2S1 XEMU replays pass; ground support and moving response remain open.

Body-sweep surface callback resolves authored material prefixes and shared texture
slots; 29 PC/NXDK checks pass; scene ownership and live sweep wiring remain open.

Owned mover/world geometry now adapts to body sweeps with source identities;
11 PC fixtures pass; real surface resolution and live scene integration remain open.

Body-sweep composition now matches 11 original/PC/NXDK ordering fixtures;
geometry ownership adapters, live player sweeps/support and response remain open.

Original full body-sweep ordering is captured in11 analytic fixtures; preserve
full displacement, shrinking limits/bounds and later-hit ties during integration.

Mover hit output and velocity/identity metadata match 2048 original/PC/NXDK
cases; ordered selection, physical response and live sweep wiring remain open.

Player sphere preparation in mover space matches 4096 original/PC/NXDK cases;
contact selection/response, mover velocity and live sweep wiring remain open.

Scene controller membership now binds registered movers and pose flags; L1S1/
L2S1 XEMU replays pass, with motion dispatch and mover collision still open.

Scene movers now register owned poses used by rendering; L1S1/L2S1 stock64MiB
XEMU replays pass, with controller membership and mover collision still open.

Combined activation matches the full original function in 1024 PC/NXDK cases;
live actor/mover commits, audio/AI backends and scene motion remain open.

Controller player-alert gating matches 2048 original/PC/NXDK cases; AI stimulus
execution, live snapshots and combined activation remain open.

Registered mover-bound collection matches 1024 original/PC/NXDK cases; live
mover ownership, pose updates, player effects and activation integration remain open.

Controller wake flags match 2048 original/PC/NXDK object-list cases; live mover
bounds, player effects and combined scene activation remain open.

Controller start-sound selection and handle retention match 2048 original/PC/
NXDK cases; audio loading/backend, player/wakeup effects and live wiring remain open.

Controller activation eligibility/backlinks and motion selection match 4096
original/PC/NXDK cases; sound/player/wakeup effects and live activation remain open.

Scene controller/key links pass stock 64 MiB XEMU replays in L1S1 and L2S1,
including two key-owner fallback links; activation and mover binding remain open.

Actor resolution/contact/delay/activation passes the 13-event stock 64 MiB
XEMU replay with exact PC traces; live actor and mover-key registration remain open.

Owned triggers now retain authored volumes and contact timers; enter/leave/
re-entry polling passes PC integration, with live scene wiring and native replay open.

Authored volume preparation matches all 2367 original/PC/NXDK triggers,
including matrix/dimension axis order; live contact integration remains open.

Combined eligibility/contact/delay polling matches 4096 original/PC/NXDK
cases; connect live snapshots and remaining key/player activation gates.

Trigger actor facts now resolve through shared entity views and match 2048
original/PC/NXDK cases; live snapshot updates and contact polling remain open.

Registered trigger activation passes all 13 authored Particle_State events in
stock 64 MiB XEMU with PC-identical traces; natural contact/key gates remain open.

Explicit runtime trigger firing now reaches shared event dispatch and preserves
self-disable changes; natural actor/contact/key integration and XEMU replay remain open.

SP activation bookkeeping matches 2048 original/PC/NXDK cases with mutable
dispatch; player-field gates and live actor/link integration remain open.

Contact-delay cancellation, arming and expiry match 1504 original/PC/NXDK
cases; key gates, live actor state and final trigger activation remain open.

Both box-trigger contact branches match original/PC/NXDK execution, including
directional face coverage; live actor inputs, dwell/key gates and firing remain open.

Oriented segment/box contact matches 4016 original/PC/NXDK cases plus 80
invalid guards; directional trigger contact and live activation remain open.

Spherical trigger contact matches 4016 original/PC/NXDK cases plus 80 invalid
input guards; box contact and natural gameplay activation remain open.

Trigger eligibility matches 4096 original/PC/NXDK cases; live actor predicates,
contact detection and gameplay activation still need integration.

All 13 authored Particle_State events across six levels pass PC and stock
64 MiB XEMU replay, with exact timing/state traces and final memory recovery.
Natural campaign trigger activation and visible effects remain open.

Particle_State now runs through immediate and delayed event dispatch; PC
integration fixtures and the stock 64 MiB XEMU regression replay pass.
Visible authored Particle_State activation still needs native validation.

Stretched geometry and clipped submission match original/PC/NXDK fixtures; six
stock 64 MiB XEMU GPU fixtures match all 1536 PC pixel samples exactly.
Campaign-spawned stretched effects and mixed room/object passes remain open.

Level particle rendering now selects retained animation frames using particle age.
All 20 emitter levels and the stock 64 MiB scene replay pass; retained explosion
frame pixels and page recovery pass XEMU. Campaign effect spawning remains open.
Current L1S1 particle ownership is 238920 bytes; maximum authored level is 484864.

Live L1S1 mist now renders on PC and stock 64 MiB XEMU; the inspection replay
submits 1308 polygons with matching cumulative draw-packet hashes.
A new PC diagnostic capture shows the mist source; full visual parity is open.

Particle/emitter room collection matches 128 original/PC/NXDK fixtures, including
parent transforms and queue limits; initial live frame submission now passes.

Queue append and sphere rejection match 256 original/PC/NXDK cases, including
all 2048 slot bytes; initial live emitter/particle collection and submission pass.

Ordinary render queue distance keys and dispatch order match 44420 original/
PC/NXDK entries. Plane grouping and room-height splits each match 22210
entries; mixed-object queue loading and room-surface integration remain open.

World billboard transformation, center acceptance and clipped submission match
the original executable in 1024 PC/NXDK cases. Live draw-queue integration
remains open, including emitter grouping and blending with scene objects.

Static level particles now tick on PC and stock 64 MiB XEMU: the inspection
replay creates 28, expires 18 and retains 10, with matching final 64 tick records.
Initial mist drawing passes PC/XEMU; broader particle rendering remains open.

Native moving-camera visibility passes 664 frames on stock 64 MiB XEMU,
with final camera/eligibility telemetry matching PC and 13084 owned bytes.

Camera setup, lazy portal projection, cache reset and room traversal now run
together and match original/PC/NXDK replays, including changed-camera reuse.
Owned initial room predicates and primary-list mapping now match all 94 levels;
the moving-camera scene now updates visibility. Static level particle ticking
passes PC/XEMU; runtime room mutations remain open. Authored collision skip flags are connected.
Level visibility uses 13084 bytes in L1S1, max 37212, including traversal scratch.

Owned portal graphs cover all 94 levels (2862 portals); L1S1 uses 1408 bytes,
max 6260. Visibility traversal keeps its fixed 7196-byte scratch budget.

Both original emitter passes use the same collection and room eligibility;
12 loop replays confirm order and 189 calls, independent of emitter enable.
Emitter bounds finalization matches 2048 original/PC/NXDK cases.

Owner-gated particle simulation matches 2048 original/PC/NXDK cases, including
768 frozen cases with previous-position updates; campaign lookup wiring remains open.

Shared level runtime loads all 87 emitters across 20 levels with verified
room/texture bindings and exact budgets (L1S1 238932 bytes; max 484908 bytes).
PC ownership/cleanup and native static-level ticking pass; dynamic campaign wiring remains open.

All emitter positions resolve to rooms; persistent shared texture bundles pass
20 levels with exact budgets (L1S1 16624 bytes; largest 262600 bytes).

Level A00 reader decodes 87 emitters across 20 installed levels; template
conversion matches 174 original/PC/NXDK replays with preserved-field checks.

Shared fixed emitter pool matches 320 original/PC/NXDK allocations, 192 releases
and two full-pool failures; 128 slots use 29184 bytes with FIFO reuse.

Fresh emitter defaults match all 128 original static entries on PC/NXDK;
slot reuse remains distinct from first-use zero initialization.

Shared emitter initialization matches 1024 original/PC/NXDK cases, including
immediate emission before caller enable assignment and retained runtime fields.

Parentless and attached emission each match 2048 original/PC/NXDK cases,
including transforms, inherited velocity, packet, particle, RNG and timer.
Connect resolved campaign parent views, scheduling, simulation and rendering.
Combined emitter update matches 2048 full original/PC/NXDK cases, including
393 phase changes, 489 emission attempts and post-emission parent-room refresh.
Emitter-linked unowned simulation matches 2048 original/PC/NXDK cases, including
1083 expirations and 455 bounds expansions; owned/world interactions remain open.

Real boom01 frames 0/7/15 pass six PC/XEMU draws: 1536 samples differ by at
most one RGB unit, and guest pages return to baseline after image release.
Connect live emitter/frame scheduling and campaign depth/placement to both passes.

PC particle rasterization now matches all twelve native XEMU probe colors;
CTest checks quad interiors/shared edges and unchanged depth for those cases.
Connect both particle passes to campaign effects and real animated textures.

Xbox particle backend passes twelve stock-64-MiB XEMU pixel probes for blend,
fog, alpha and depth; native testing fixed a missing secondary-color enable.
Connect Xbox/PC campaign passes and validate animated texture sampling/depth mapping.

Particle draw-vertex conversion now matches 4096 original/PC/NXDK cases,
including ARGB, optional color transform, fog rounding and depth/UV scaling.
Connect the separate 32-byte particle format to texture binding and GPU drawing.

Particle geometry submission now matches 2048 full original/PC/NXDK cases,
including clipping, projection and depth override (1178 submitted, 870 rejected).
Connect texture residency, color/fog conversion and native backend drawing.

UV-only particle polygon clipping now matches 2048 original/PC/NXDK cases,
including temporary-vertex reuse, duplicate intersections and six input guards.
Connect clipped vertices to projection, depth override and native drawing.

Billboard preparation now matches 2048 original/PC/NXDK packets, including
radius-dependent submission depth and clip masks (832 culled, 32 partial).
Connect clipped polygons to projected/depth-adjusted backend submission.

Particle blend/depth/fog state selection now matches 3780 original/PC/NXDK
cases, including preserved state and capability fallbacks; 3780 original cached
repeats issue no writes. Particle default selection matches 1031 cases; texture
source 2 matches 4096 cases plus 31 unsupported-source guards. Connect clipping,
residency and backend submission before claiming rendering.

Particle point projection matches 648 original/PC/NXDK cases, including
cached flags, clamp behavior and reciprocal-only depth adjustment. Polygon
clipping, remaining texture-state recovery and backend integration remain open.

Camera-space particle billboard geometry now matches 1024 original/PC/NXDK
cases, including aspect correction, rotation and submission-order UVs.
Connect clipping, depth handling, blending and texture residency to drawing.

Particle animation frame selection now matches 2880 original/PC/NXDK cases
for lifetime, loop and hold-last modes, plus six invalid-input guards.
Connect selected frames to bounded bitmap residency and particle drawing.

Native 64-MiB XEMU replay 20260910-085856 matches all 2057 particle-step
fixtures against PC, including rejection cases. Five native x87 checkpoints
report 0x027f, confirming the 53-bit harness setting for this Xbox build.
Integrate owner/collision/wind/damage paths and visible scene effects next.

Shared fixed particle pools now match 3679 original/PC/NXDK replay operations,
including 1829 allocations/recycles, saturation and emitter detachment.
Seven complete state snapshots agree. Storage uses 1600 120-byte records;
connect emitter execution, particle simulation and scene rendering next.

Emitter phase duration and RNG advancement match 1140 original/PC/NXDK
cases: one draw even at zero variance, draw/32768 and 0.1-second clamp.
Emitter update decisions now match 2560 original/PC/NXDK cases, including
592 phase toggles and 609 emission requests. Creation, actual emission,
parent attachment and spawn-timer reset remain open.

Central explosion timing now matches 700 original/PC/NXDK update cases,
including 164 expirations and process-before-release ordering. Trail updates,
emitter creation/processing and scene-clock integration remain open.

Central explosion size gating and velocity/radius/lifetime scaling match
600 original/PC/NXDK cases. Shared preparation returns an owned copy and
random-position extent without mutating definitions. Position sampling,
emitter creation, scheduling and live rendering remain open.

Native 64-MiB XEMU replay 20260910-081353 verifies all nine loaded explosion
definitions and charge_explode against PC hashes; campaign/resource checks
pass. Original emitter lookup passes 146 cases and confirms the authored
explosion random bits 2 sparks name is absent, leaving rocket hit mask 63.
Connect bitmap residency, scheduling and live execution next.

Owned vclip definition loading now passes 126 PC/NXDK named checks across
63 declarations and 63 scratch-budget rejection checks, including embedded
particle metadata. Case-only duplicate glassboom/Glassboom resolves first.
charge_explode retains rocket hit and Medium Explosion names. Explosion recipe
loading, Foley resolution, playback and active simulation remain open.

Native particle resource lifetime was verified in 64-MiB XEMU replay
20260910-075740: six frame loads match PC hashes and physical pages return
to baseline. This is pre-scene testing, not peak campaign residency.

The shared Explode request emitter now matches 432 original cases on PC and
compiled NXDK. Effect-name lookup, scene dispatch and request consumers remain
open; scene explosions are still counted as unsupported.

Explode request routing is recovered: 288 original-action cases verify geometry
request gating followed by general explosion arguments. Shared effect lookup,
request execution, geometry changes, rendering/audio and damage remain open.

Delay (48) now expires and propagates in native Xbox: L1S1 UID 9495 reaches
Explode UID 9466 after five simulated seconds in 320-frame replay
20260910-071141. Explode effects remain unsupported; no visual explosion yet.
Original Delay execution passes 72 cases; 93 startup graphs still pass.

Delayed Invert/Set_Gravity now tick after physics on the shared fixed-step
clock. Three expiry-chain fixtures and native 64-MiB XEMU clock replay
20260910-070755 pass; other scheduled event types retain pending deadlines.
No reachable authored startup currently schedules these two delayed types.

Invert (type 3) now dispatches opposite actions with the original actor/source
semantics. Original 192 cases, five shared recursion fixtures and 93 authored
graphs pass; L1S1 exercises it in 64-MiB XEMU replay 20260910-070306.
Mover/auxiliary effects and delayed event ticking remain open.

Recursive startup now follows event links and enables/disables trigger targets.
All 93 authored graph checks and sibling/re-enable/cycle fixtures pass; native
64-MiB XEMU L1S1/L17S1 replays 20260910-065717/065759 match PC. Unsupported
actions, delayed ticking and other target families remain open.

Owned event target resolution is connected to scene loading: all 4,901 links
across 93 levels pass, with 1,964 resolved and 2,937 awaiting other families.
Native 64-MiB XEMU replay 20260910-065321 matches the PC ordered target hash.
Recursive propagation and generic runtime dispatch remain open.

Original generic target routing now passes 720 execution fixtures, including
event off-source behavior, trigger enable/disable, stale handles and mover
suppression with fallback. Shared dispatch and event-link resolution remain
open; the evidence is in event-target-dispatch-verification.json.

Ordered event-link walking now matches original 4b8b00 in 700 PC/NXDK cases,
including callback list mutation and mode handling. Event target resolution,
generic target dispatch and recursive scene propagation remain open; startup
still reports outgoing event links as pending. No new visual behavior claimed.

L18S1 world loading is fixed: absent mover sections are valid and now produce
an empty retained mover owner. The old -3 error was RF_NOT_FOUND, not RF_FORMAT.
All four rendered PC and stock 64-MiB XEMU gravity-level checks pass for 16
idle ticks. The Xbox preflight also accepts absent group/mover sections.
Existing malformed mover data is still rejected. Native reports 20260910-064208,
064231, 064257 and 064322 cover L18S1 and L17S1-3 respectively; these are startup
checks, not campaign completion or visual-parity evidence.

Startup auto dispatch now activates registered events and applies Set_Gravity
in the scene, while counting unsupported actions/targets, scripts, propagation
and delayed work. All 93 shared startup fixtures pass. Rendered PC L17S1-3
apply 4/3/4 gravity; L18S1 applies 9.8 after the loading fix above. Default 64-MiB
XEMU replay 20260910-062922 passes with matching startup diagnostics.

Runtime trigger targets now resolve against registered events/triggers while
retaining raw UIDs: 2,809 of 4,471 links resolve, including all four gravity
links; 1,662 await other object families. The native ordered-target hash and
jump replay pass in 64-MiB XEMU (20260910-062500). Startup dispatch is open.

Campaign triggers now share the event registry with bounded ownership and
retained raw links. All 2,367 triggers pass ownership checks across 93 levels;
64-MiB XEMU replay 20260910-062110 passes with both owners active. Resolving
links and dispatching startup actions remain open.

Campaign loading now owns decoded events and registered common runtime
objects. All 4,446 records across 93 levels pass ownership/budget checks;
64-MiB XEMU replay 20260910-061740 passes with 184 L1S1 events (219,436
owner bytes plus 12,300 registry bytes). Trigger dispatch remains unconnected.

Shared event-name lookup now recognizes all 90 original types and matches
544 original cases on PC/NXDK. This supplies IDs for general registration;
recognizing an event does not implement its action or startup dispatch.

Authored trigger initialization now matches 2,367 installed records plus
243 flag cases on PC/NXDK. Timing conversion remains exact across three
x87 precision modes; 0.01f truncates to 9 ms as in the original. This supplies
startup state from level records, but registry/dispatch integration is open.

Shared auto-trigger activation now matches 360 original cases on PC/NXDK,
including dispatch-before-bookkeeping, timer wrap and activation-count wrap.
It accepts externally resolved global/script eligibility and link dispatch;
registration, callback effects and live level-start integration remain open.

All four authored gravity links now pass the unchanged original auto-trigger
to setter chain, now including original UID-to-handle conversion and disabled
checks. The sweep belongs late in level startup; 30 seconds is a subsequent
trigger cooldown, not a delay before gravity changes. Registrations remain
synthetic in this harness; shared runtime startup wiring is still required.

Set_Gravity (type 44) now has a shared action handler: 2,048 original on/off
cases match PC/NXDK, with six additional shared API checks. Original construction
and the existing 3,922 scheduling cases also pass. Live event registration and
trigger dispatch remain open; this does not yet enable campaign gravity events.

Gravity now has shared scalar/vector state used by falling and jump setup.
The setter matches 1,024 finite original cases through both entry paths on
PC/NXDK, including signed zero and negative gravity, without changing jump
impulse. Level setup resets it to 9.8; runtime gravity event dispatch is open.

Campaign jump height now loads from game.tbl with a bounded shared reader;
24 PC/NXDK parsing cases pass. The normal jump replay passes in 64-MiB XEMU
(20260910-013412), and a separate PC asset copy configured to 2.50 reaches
2.49997 units. Gravity still uses the installed default; audio remains open.

Immediate crouch now precedes jump, matching 430c70. PC restriction replays
cover crouched rejection, held-button release without auto-jump, same-tick
crouch/jump rejection, and same-tick stand/jump acceptance. The simultaneous
case passes 64-MiB XEMU (20260910-013000); the climb case passes at
20260910-013030. Climb-mode requests are rejected;
a new press on the staged exit frame is accepted in restored mode 1, which
does not establish grounded upper-platform traversal.

Campaign jump replay now passes on PC and stock-64-MiB XEMU
(20260910-012451): initial airborne rejection, two accepted jumps, 1.32991
units ascent, landing while held without auto-repeat, and release/repress.
Space (PC --campaign) and A (Xbox campaign mode) are mapped; physical-device
checks remain USER. Audio is recorded only; jump height now comes from game.tbl, while gravity
still uses the verified installed default pending its configuration lifecycle.

Campaign support now runs after physics using a fresh probe and the recovered
selector, then consumes jump flag 2. Both 64-MiB XEMU replays pass with exact
PC state: approach 20260910-012038 (220 ticks, 6.43122 climb units) and stance
20260910-012117 (64 ticks). Jump input is still unconnected, so takeoff and
landing after a jump remain unverified.

The shared support selector now matches all 2,048 original cases on PC/NXDK.
Static call-chain recovery places ordinary local input before simulation,
post-physics support before the owned-player update, and jump-flag consumption
in that later update. Wire this sequence into campaign replay next; the full
original frame loop has not been executed by the harness.

Jump integration exposed a support-update dependency: the diagnostic landing
loop can cancel takeoff while its probe still sees the floor. Original
487f73..487fc9 passes 2,048 dispatch cases and bypasses support when actor
flag 2 is set. The owned-player update clears that flag; its ordering with
physics must be matched before live input is enabled. No landing delay added.

Original default jump binding is now verified as edge-triggered: type 0,
key 0x39, with no secondary keyboard or mouse binding. All 48 original-query
cases pass; press/hold/hold/release/repress dispatches only on the two presses.
The actual press counter is consumed by 51f140. Live PC/Xbox binding and body
state wiring remain open; see tools/inspect_jump_binding.py.

Shared jump-request gating now matches 864 original dispatcher cases on PC
and compiled NXDK. Control kind 5, parent kind 4, actor bit 0, override state
34 and keyboard state 0x29 can reject the request. The default binding type is now verified below; the outer input-lock byte
637086 is identified, but its lifecycle remains to be integrated.

Original jump/fall execution now passes 6,144 fixtures plus a null-entity check.
The shipped maximum jump height is 1.33, giving impulse 5.105683327. Mode 2
(climbing) rejects jump; do not add an assumed climb boost to solve the upper
exit. Shared C now matches all cases on PC and compiled NXDK; action dispatch,
audio playback and live jumping remain open.
See tools/inspect_jump.py and docs/CAMERA.md.

A near-base approach now passes 220 ticks in stock-64-MiB XEMU
(20260910-004650), with exact PC timeline/body matching: 0.80861 units of
retained walking outside the region, then 6.43122 units of climbing and exit.
Use replay_climb_controls.py --approach and xemu_replay_check.py --approach.
The start is staged outside the authored region, not the level's player spawn.
Ground-ray inspection found the lower floor at -14.875 and upper surfaces at
-4.875 offset from the region; static and initial-mover samples agree. Upper
traversal still needs its intended route/action identified, not an assumed snap.

Look-directed climbing now passes a 180-tick PC/XEMU replay using only
commands reachable from the existing controller sticks (no move-Y injection).
Run tools/replay_climb_controls.py, then xemu_replay_check.py on its inputs
with --climb. XEMU 20260910-003732 matches PC exactly and ascends 5.20467 units.
The start remains staged inside the region; physical device behavior, authored
approach and top-platform traversal remain unverified. Original 430c70/4280b0
confirms immediate region exit with vertical-velocity reset, so avoid assuming
the staged top-boundary re-entry alone proves the transition code is wrong.

User confirmed interactive movement on 2026-09-09 and reconfirmed being able
to move on 2026-09-10. This confirms basic movement input only; the latest
confirmation does not identify the platform. Campaign jump, look, crouch,
controller reconnect and exit behavior still need user checks.

Campaign stance now applies collision crouch immediately and uses the recovered
owned-player animation selector after the verified first initialization frame.
The 64-tick press/move/release replay passes on PC and stock-64-MiB XEMU
(replay-20260909-231211), including exact stance and animation memory rings.
The normal 664-tick diagnostic state and raster remain unchanged. Eligibility
matches 3,456 original PC/NXDK cases; motion selection matches 3,392 cases.
Blocked standing, full ownership/input locks, vehicles and inventory remain open.
See docs/CAMERA.md and tools/verify_player_stance_live.py.

Campaign region lookup, climb entry, motion and exit now run on both targets.
L1S2 staged ascent replay 20260910-003213 passes on stock-64-MiB XEMU:
62 climbing ticks, 4.3717055 world units upward, one entry and one sideways
exit; all 1152 timeline words and existing player records match PC exactly.
9428 pages (36.8 MiB) remain at the final rendered frame. Native raster differs
from PC by at most one channel value. This is not original-game/PS2 parity.
Shared material/lightmap limits are 12/4 MiB after eliminating GPU duplication;
these initial limits do not prove every campaign level fits. Authored approach,
controller mapping and top-platform traversal remain open. A vertical-input
probe at the top boundary re-entered repeatedly, so that transition needs work.
The two-key L1S1 post-run door self-test is no longer applied to the L1S2 fixture;
full L1S2 group/door simulation remains unverified. See docs/CAMERA.md.

Owned movement-region storage now survives archive closure with a checked
single allocation. All 147 records across 39 levels pass PC/NXDK ownership,
exact-budget and repeatable-close checks. The live scene must retain this owner
until all player region/orientation pointers are released.

Shared climb motion now matches 384 original PC/NXDK cases, including vertical
input, external force, support velocity and repeated passes. The initial reuse
of run arithmetic exposed an x87 ratio-rounding difference; climb now preserves
the original extended ratio and all 384 run cases remain exact. Live campaign
region/transition/movement wiring remains open.

Shared climb exit now matches 128 original cases on PC/NXDK, including blocked
standing, walk capability, default movement/fallback, speed and step-offset reset.
Entry still passes all 768 cases. Retained live regions, climbing movement and
campaign entry/exit wiring remain open; see tools/verify_climb_exit.py.

Shared climb entry now matches 768 original cases on PC and compiled NXDK,
including sound callback state, speed restoration and descriptor fallback.
It allocates nothing and borrows region/descriptor storage; climb exit and
live campaign integration remain open. See tools/verify_climb_entry.py.

The conditional climb effect is identified as sound dispatch through 48a9c0.
Shared C now routes player-owned/camera-zero sounds non-positionally and other
sounds positionally, matching 648 PC/NXDK original-code cases. Audio playback,
sound-ID asset resolution and shared climb transition integration remain open.

Original climb entry now passes 768 execution fixtures, confirming class flag 4,
region ownership fields, effect-18 ordering, speed restoration and descriptor
fallback. Shared climb transition/effect implementation and live entry/exit remain
open; see tools/inspect_climb_entry.py and docs/CAMERA.md.

The v180 movement-region reader now loads all 147 authored records in the 39
installed levels containing section 0xD00, with PC/NXDK byte-identical decoding.
It allocates nothing and preserves reader/output on malformed data. Connecting
the retained regions to climb entry/exit remains open; see docs/CAMERA.md.

The stance environment query is now identified as an ordered oriented-box
movement-region scan leading to climb mode 2, rather than room-water detection.
Shared C matches the complete original scan in 1,521 PC/NXDK cases; level region
loading and climb entry/exit must be connected before replacing the empty query.

Shared C now implements the outer stance gate, matching 4,608 original-code
cases on PC and compiled NXDK. Campaign stance now respects live movement/speed
modes for both press and release. Ownership, environment lookup and blocking
values remain explicit fixture defaults pending their live lifecycle integration.
See tools/verify_player_stance_gate.py and docs/CAMERA.md.

The original release handler now passes 162 execution fixtures covering blocked
and clear query results, 0�8 spheres, absent/matching/nonmatching player lists,
and zero/nonzero crouch bytes. A blocked result preserves the full actor, player
and sphere records and skips ground/speed updates. The query result is supplied;
this does not close the live low-ceiling campaign traversal check.

Original player contact response now executes in 768 fixtures covering normal
retention/clearing, incoming correction, moving-contact boost, free tangential
motion and grounded direction filtering. All nine tracked branches are reached;
shared C now matches all 768 cases on PC and compiled NXDK, with 52 compiled
guard cases preserving state/output. Original direction composition is verified
in 512 action-reader fixtures, and the campaign profile now enables flag 0x80
with the matching contact adapter. Its 480-tick movement/turn replay passes in
stock-64-MiB XEMU with 64 retained contacts and PC-identical final state.
Full input ownership/locking, blocked-standing integration and damage remain open. See docs/COLLISION.md.

Original cache-gate and spawn-copy execution now confirms that a warm class
cache bypasses pose sampling for both NPCs and players, and creation copies
cached sphere centers into the entity without changing its other sphere fields.
Four gate cases and 180 copy cases pass. Sphere and stance sampling now runs
before body allocation and has no body dependency. Campaign playback now uses
armed candidates with independently sampled NPC class data; persistent class
ownership, weapon lifecycle and player contact response remain open. See docs/CAMERA.md.

Campaign initial controller and class poses now match original execution.
Separating the cache also removes a legacy -0.25 render-fixture translation
from two standing sphere centers; original sphere queries confirm the correction.
The normal 664-input replay remains identical and all eight PC sweeps pass.
Stock-64-MiB XEMU neutral replay 20260909-223324 passes with initial animation,
eye and stance comparisons added. This is still an opt-in diagnostic profile.

Original static-solid plane rejection now prevents the campaign turn overflow:
all 480 yaw ticks pass on PC and 64 MiB XEMU at a 1,161,048-byte world peak,
with the same 2 MiB allocation. All eight PC input sweeps and the normal
664-tick Xbox replay pass. Mover culling and room visibility remain open;
see docs/PROJECTION-PERFORMANCE.md for the measured image differences.

Opt-in campaign placement now loads miner1 by class, uses the authored start,
and seeds verified body/eye angles. A 120-tick neutral replay matches PC in
stock-64-MiB XEMU, including final body and world/camera hashes. PC diagonal
movement passes 480 ticks; the stationary-turn overflow previously found at
tick 342 is now resolved by static backface rejection. The regular interactive profile is restored; full player
factory, armed initial animation/class-cache ownership and binding are still open.

Shared C spawn-look initialization now matches original instructions exactly in
601 cases on PC and compiled NXDK, including all 94 level starts. The helper
still needs to seed the live player before campaign placement replaces the fixture.

Original spawn-look initialization executes across all 94 level starts; 89 have
nonzero body yaw (Live Mines: 2.2433788776 radians). The diagnostic zero-angle
initialization cannot be retained for campaign placement. Matrix extraction and
movement-descriptor eye filtering are now reconstructed; see docs/CAMERA.md.

Class-based skeletal asset loading now works without a placed level entity:
all 168 authored class/skin selections and the existing 78 Live Mines bindings
pass. This removes the resource loader's UID dependency for player creation;
the live scene still needs the new class/spawn/ownership path connected.

Shared C local-player binding now matches 81 original cases on PC and compiled
NXDK, including the state visible when weapon selection is invoked. Entity
allocation, full weapon selection and live scene ownership remain open.

Shared C now implements the verified player-start prefix (skin normalization,
local reset and position override), with all 24 cases matching original execution
on PC and compiled NXDK. It allocates nothing; entity creation and live ownership
integration remain open. See src/core/player.c.

Original player-start loader and single-player factory argument preparation now
match shared C across all 94 installed levels. This verifies the transform source
and matrix ordering, plus the original factory prefix's local reset, skin
normalization and one-shot position override (24 branch cases). Generic entity
allocation and player ownership remain open; the live diagnostic still uses
its miner placement. See docs/CAMERA.md and tools/verify_player_start.py.

Xbox file replay now compares recorded commands with PC without controller input.
The 480-tick yaw sweep passes in 64 MiB with matching world/camera hashes, input
history and final body; a completed GPU submission exceeds 1.96 MB within the
existing 2 MiB allocation. The diagonal-turn replay also passes all 480 ticks,
including a 1.91 MB completed GPU mesh and exact PC state. Both reproduced
capacity-failure trajectories now pass on Xbox. See docs/INPUT.md.

PC input replay reproduced turning exits at the 1 MiB staging boundary. First-person
views now use the full existing 2 MiB buffer when necessary; both 480-tick failing
sweeps pass with no allocation increase. The historical tick-99 input was not
recorded, so its exact cause remains unproven. Both wide-view Xbox replays now
pass; failure snapshots retain operation stages and input history.

Presentation profiling found a redundant trailing VBlank wait. Removing it cut
measured presentation/integrity time from 27.23 to 13.62 ms; overall throughput
was 15.25 ticks/guest second in the follow-up run, still below target. The pacing
harness now uses its own snapshot-backed HDD base to avoid other emulator locks.
Full 664-tick stock-64-MiB Xbox verification 20260909-211014-307540 passes; the
ongoing controller ISO is restored.

Single-pass retained projection now stages into the idle actor half of the existing
2 MiB buffer. Measured world projection fell from 60.32 to 31.50 ms/tick and total
throughput rose from 9.81 to 14.62 ticks/guest second. PC traces remain exact and
staging failure/overlap checks pass. Full 664-tick stock-64-MiB XEMU run
20260909-210345-153679 matches PC hashes, rings and final body. Projection and
presentation still dominate.

Xbox profiling identified world projection as the largest tick cost. Reusing fan
corners and classifying unclipped triangles reduced measured projection from
94.31 to 60.32 ms, with throughput rising from 7.37 to 9.81 ticks/guest second.
All 94 level spawn images, 32 moving views, the 664-frame PC trace and the
1920x1440 showcase remain exact. Full 664-tick stock-64-MiB XEMU verification
20260909-205633-876144 also passes with matching PC hashes, rings and body state.
See docs/PROJECTION-PERFORMANCE.md.

Shared 60 Hz pacing now bounds catch-up and skips presentation on PC and ongoing
Xbox sessions; deterministic fixtures remain unpaced. Unit checks and 8,000 compiled
NXDK clock samples pass. Live RAM shows only about nine ticks per guest second,
so per-tick work must be reduced; pacing alone has not fixed movement speed.
The overload rule preserves presentation. A 20-second rerun passes, but an earlier
RF_RANGE termination remains unresolved; its report is correctly retained as FAIL. See docs/INPUT.md for timing policy and telemetry.

PC now has a Windows input/presentation frontend using the shared scene provider
and software rasterizer. Its 664-frame headless replay matches body/input/camera
evidence and the final reference image exactly; both showcase resolutions remain
pixel-identical. Window interaction needs manual testing, and real-time catch-up
remains open. See docs/INPUT.md for the launch command and limits.

Xbox controller polling now drives movement, look and crouch through a shared
provider; user confirmed movement, and XEMU memory records real stick input.
The bounded neutral test correctly failed its neutral expectation after user
interaction. PC replay, clean-stop and compiled adapter checks pass. See docs/INPUT.md.

Yaw now commits body orientation and world inertia; movement, collision sweeps
and eye view use the turned body. All 664 complete original look/eye/camera
replays and stock 64 MiB XEMU run 20260909-201342-341995 pass with PC-matching
body and camera traces. Next connect actual player input and runtime ownership.

Pitch control is now bound to the moving first-person diagnostic camera.
All 664 original look/eye/camera replays and live 64 MiB XEMU run
20260909-200813-666493 pass; look/eye rings match PC and body trace is unchanged.
Next integrate yaw/body orientation physics and actual player input. No capture.

Eye-matrix builder 4a0d70 is reconstructed: 1,200 compiled NXDK cases match
original execution exactly; PC differs only in tiny vertical-pitch residuals.
Bind look-angle state, yaw-only body and eye matrices into the live camera next.
Original global FPU initialization and live XEMU integration remain open.

README now uses a 1920x1440 PC render of the staged miner by the partly open
Live Mines guard-station doorway, explicitly labeled PC. Add live Xbox
showcase-profile validation before presenting this composition as Xbox output;
its identification as the requested main hall remains unverified. See docs/SHOWCASE.md.

Look-angle scalar update now matches 1,200 original cases on PC and compiled
NXDK, including consumed deltas, pitch clamp and original yaw wrapping.
Reconstruct the nonstandard 4a0d70 eye-matrix builder before binding separate
look to the scene. No new rendered result; see docs/CAMERA.md.

First-person diagnostic now binds loaded eye offsets to body/controller state.
All 664 PC poses (including 30 transition frames) match original eye/camera
instructions byte-for-byte. XEMU run 20260909-194149-709809 passes in 64 MiB;
new native tunnel screenshot matches PC within one channel level. Next connect
actual player identity/input and separate look orientation; the route is scripted
and has no weapon view. See docs/CAMERA.md for limits and commands.

Shared PC/Xbox scene now computes standing/crouching eye offsets from loaded
poses, independently of diagnostic root translation. Original/PC comparison
and live 64 MiB XEMU run 20260909-193237-875652 pass all six words and 664 frames.
Live Mines record 3 (UID 8431) is the first serialized miner1 before ordinary
player spawn. Bind player view next; broader class-cache lifecycle remains open.
No new visible result, so no screenshot.

Initial controller audit now passes eight original/PC creation-field cases:
armed players blend stand to attack_stand; non-player and unarmed cases stay
standing. Actual class eye setup is cached on first use, so identify the first
miner1 instance before binding offsets. The full spawn routine remains open.
See docs/CAMERA.md and tools/inspect_initial_player_motion.py.

Memory harness revalidation `20260909-191614-644905` PASS: live XEMU confirms
stock 64 MiB, 664 frames, ten PC-matching actor rings, final body state, and
room transition with zero misses. Available memory after upload is 41,054,208
bytes. The supplied strtod assertion does not recur. Original player camera,
initialization and campaign memory peaks remain open; see docs/XEMU-MEMORY.md.
No screenshot captured because there is no new visible result.

Camera effects now use an explicit original-compatible RNG stream and match
1,800 original/PC/NXDK cases including final RNG state. Initial player eye
state and game-wide RNG seeding/consumer order remain open before view binding.

The complete first-person camera effect now matches 1,800 original cases on
both PC and compiled NXDK, including cone sampling and orientation rebuilding.
Bind initialized player eye offsets and the game's random sequence next; the
rendered camera is still the diagnostic follow view. See docs/CAMERA.md.

First-person camera effect reset/timer/decay now matches 600 original cases on
PC and compiled NXDK. Reset must expire the timer at now, not disable it.
Recover random cone direction and orientation rebuilding next, while resolving
initial player eye offsets before camera rendering integration.

Moving-actor room membership now passes the 664-frame XEMU follow run
`20260909-185445-343865`: room 53 to 52, no lookup misses, full PC-matching
room telemetry and unchanged body/render hashes. Integrate the original player
view and general entity update order next; local-player room audio remains open.

The locator now binds retained level geometry and source face IDs, with no
per-query allocation. All 94 levels pass 8,214 original/PC and 8,214 compiled
NXDK comparisons after source closure. Connect moving-actor room state and
verify its changes in live XEMU next. See docs/CAMERA.md for coverage limits.

The original containing-room traversal and deterministic retry generator are
now reconstructed: 600 full original/PC/NXDK locator cases match, with 50 actual
retries. Bind the locator to retained level geometry and validate real room/face
identities next; multi-room runtime membership remains open. See docs/CAMERA.md.

Containing-room face testing now matches 1,200 original cases on both PC and
compiled NXDK, including tie priority and edge-line retries. Reconstruct
4e0c20 direction generation and 4e1630 traversal/owner selection next, then bind
the locator to entity room refresh. No new image for this unbound geometry API.

Room refresh 48a190 now matches 384 original cases on PC and compiled NXDK.
It updates room/query position and clears the dirty flag, not physics/orientation.
The locator and notification are explicit callback boundaries; reconstruct
4e1630 containing-room traversal before binding this to campaign entities.
See `docs/CAMERA.md`. No new visible result or XEMU run for this unbound API.

Latest memory harness rerun: `20260909-182441-657096` PASS, 664-frame follow
profile in stock 64 MiB XEMU. Snapshots now carry UTC time, map hash and QMP
memory size; report map hash matches the complete snapshot. Four PC CTests pass.
Original camera/input and class initialization remain open; see
`docs/XEMU-MEMORY.md` for the current command and limits. No screenshot captured.

Support-gate audit: 256 original predicate cases pass in
`tools/verify_actor_support_gate.py`. Full entity integration must carry actor
flag 8 and attachment/category conditions;
the fixture's position movement gate remains simplified. See the latest
support-query audit in `docs/XEMU-MEMORY.md` before extending traversal.

Accepted crouch/stand changes now query and commit static support immediately
after sphere replacement, before animation and rendering. Run
`20260909-173908-972553` passes all stance-query memory comparisons in 64 MiB
XEMU. The standing-clearance copy test still isolates clearance/center changes;
full general query gates and live ledge traversal remain open.

Longer physics routes now advance eight independent 600-update paths from the
rendered miner body, using real geometry and collision responses. Positive X
loses and regains support three times; the other routes exercise sustained
contacts. PC/XEMU route hashes and endpoints agree. This is not yet animated
traversal; the visible scene still stops at 64 frames.

The animation producer now accepts a caller-selected stream length without
restarting its controller/cache or reallocating its mesh. The new PC
`--long-animation` check passes 600 frames with an identical 64-frame prefix and
bounded timing storage. The scene's fixed-size telemetry/callbacks still need
extension before combining this with the longer moving-body route.

The shared scene now runs 664 continuous animated/body frames in the PC
`--live` profile: three support losses/recoveries, 663 physics updates and no
capped updates. Telemetry uses 64-slot rings with absolute frame IDs. NXDK builds
pass, but this new live mode still needs XEMU memory validation; the fixed close
camera loses sight of the miner before the route ends.

XEMU live-scene validation now passes in run `20260909-175428-274474`: 664
rendered frames, three support losses/recoveries, ten PC-matching telemetry
rings and all 308 final body bytes matching. This supersedes the pending XEMU
validation notes above. A suitable route camera remains open.

The live profile now uses a fixed endpoint inspection camera, selected from the
known diagnostic route, and PC preview supports `--scene-live-last`. The miner
is visible in the final walking pose. This is not a player/follow camera and
does not keep the entire route visible; the tunnel bend occludes a start view.

Endpoint capture `20260909-180017-502708` passes XEMU memory and PC framebuffer
comparison (10 pixels over three channel levels). A new native screenshot shows
the later walking pose. The live checker now includes retained movable-world
geometry to match Xbox at this camera; full-route camera motion remains open.

Retained world reprojection now accepts an explicit changing camera without
reloading geometry or reallocating its mesh. The PC `--moving-camera` check
passes 32 translated/rotated views after archive closure, matching fresh builds.
Connect this API to the live actor camera and validate that path in XEMU next.

PC `--follow` now reprojects the retained world and actor using one per-frame
view across all 664 frames. The actor stays inside the view (at least 430
projected triangles per frame), and the body hash stays 533762320. The shared
code builds with NXDK, but Xbox follow-mode selection/GPU capacity and native
visual validation remain open; camera collision is not implemented.

Follow mode now passes 664 frames in stock 64 MiB XEMU, run
`20260909-181012-800513`, including full world/camera hashes, actor memory and
native framebuffer comparison. Peak world mesh is 484,848 bytes versus the
current 8 MiB CPU/GPU reservations; shrink those before expanding runtime work.
The final still is very similar to the last posted image, so it is not reposted.

Follow CPU/GPU buffers are reduced to 2 MiB each, saving 12 MiB of simultaneous
reservations. XEMU run `20260909-181406-561630` passes with unchanged world,
camera, actor and body hashes. Reported available memory after upload rises to
41,062,400 bytes. No new image was captured for this allocation-only change.

First-person pose transfer at 40d88c..40d8be is now reconstructed in C and
matches 258 original cases on PC/NXDK. It preserves distinct body and eye
orientations. Player lookup/eye derivation, 40db70 camera effects and 48a190
commit remain open; the rendered scene still uses the diagnostic follow view.

Capture and post screenshots only when there is a new visible result.

Latest memory-harness run: `20260909-165657-464529` PASS; 64 MiB confirmed,
all 64 surface/traction records match PC and all 240 material coefficient bytes
match the authored table. The miner selects Rock on landing at frame 22.
Ground traction now comes from the support face texture prefix, removing the
fixed value. Run `20260909-170239-776884` also passes blocked/clear standing
using the scene transition helper on a copied crouched body near a real ceiling.
The rendered route still lacks blocked-standing traversal and support loss;
the original selector continues after a refused stand attempt without checking
its return value. Do not invent a forced-crouch animation rule.
The reconstructed stance selector is now connected before animation advancement;
run `20260909-170656-795906` matches all 64 selector/effect records on PC/XEMU.
It applies crouch at frame 40 and stand at 48. Eligibility remains scripted.
Initial miner inertia is corrected: positive authored mass plus its populated
model sphere list preserves the zero parameter tensor, rather than identity.
Original preparation checks and XEMU run `20260909-171406-829227` pass, including
144 initial/final tensor bytes. Initial animation pose remains provisional.
Live actor playback now starts at the original constructor's phase zero and
skips a redundant stand-to-stand transition. Run `20260909-171941-116367`
passes initial animation telemetry, all actor comparisons and native framebuffer
comparison. Full initial selector inputs remain unresolved; the stand choice
is still diagnostic. A new pose screenshot was captured for this change.
Live movement animation now uses the reconstructed selector with the same
steering and numeric movement mode as physics; the fixed frame-16 walk request
is removed from this path. Run `20260909-172348-379119` matches all 64 locomotion
records in XEMU. Ordinary unarmed candidates and crouch eligibility remain
explicit fixture inputs; gameplay priorities/AI selection remain open.
Animation/controller advancement now shares physics' 1/60 second runtime step.
Run `20260909-172853-167404` matches all 64 timing/phase/generation records and
existing actor checks in stock 64 MiB XEMU. Frame zero retains a separate 1/30
initialization step; full startup scheduling remains unresolved. Extended fixture
eligibility ends at 56: crouch starts at 41 while moving, 47 from idle, and
standing resumes at 56. These supersede the earlier timing observations above.
See `docs/XEMU-MEMORY.md` for reproduction and limits.

Working approach: complete the first miner integration across loading, rendering
and physics before expanding isolated subsystem coverage. Use the existing
diagnostic pose if spawn selection is unresolved, label that assumption, and
replace it as evidence becomes available. Verify integrated PC/XEMU behavior;
add isolated comparisons only for failures or fidelity risks on this path.

Integration now runs in 64 MiB XEMU: UID 9858's class/material configuration,
frame-zero model pose and three collision spheres are connected to an owned
body during the 64-frame rendered scene. All 468 configuration bytes and body
hashes match PC; the 600-frame door regression also passes. Pose/inertia remain
explicit diagnostic assumptions; collision response and entity registration
are next. The integrated sphere/world diagnostic now checks six two-unit axis
sweeps per retained sphere and captures raw guest contacts for PC comparison.
Prepared falling translation now matches original instructions on PC/NXDK and
feeds a passive falling fixture using the miner's actual spheres and level.
The fixture now applies original contact position/time clipping and static
velocity response, then consumes the remaining frame without applying gravity
again. Actor pose/room commit and rendered movement remain open. The first hit
advances zero distance after the margin; a second pass consumes the remaining
0.014501864 seconds using the response velocity.
The shared scene now advances the live body between rendered frames and commits
pose through the verified 48a230 setter. The 64-frame diagnostic performs 63
updates, 91 passes and 28 contacts without hitting its pass limit; every rendered
mesh and final framebuffer comparison passes against PC in XEMU. It no longer
replays or holds a stored trajectory. Grounded-state selection and full runtime
registration remain open: passive physics currently lets the miner drift along
the slope instead of choosing the original grounded movement behavior.
The ground-query preparation is now checked by executing original 4a0840 up to
its world-query call against all 64 captured poses and actual spheres (256
prepared cases). It selects sphere 0 by lowest center Y, uses a 0.05 start lift
and a 0.10 falling probe depth. Grounded depth retains extended precision until
the final position store. This verifies query preparation only; world-query
results, landing transition and grounded movement remain open.
The shared C ground preparation now feeds a stationary-world query each rendered
frame. Run 20260909-152946-815112 passes in stock 64 MiB XEMU: all 64 raw query
records match PC, with 42 walkable contacts starting at frame 22. Captured probe
preparation matches original instructions; it uses the recovered query flag 4.
This observation does not yet change the passive body's movement mode.
Static landing is now integrated: the miner transitions from fall (3) to its
class run descriptor (1) at frame 22, then remains stationary for 41 updates.
The passive fixture no longer drifts. Original/PC/NXDK landing comparisons pass
for 42 captured contacts, including preserved horizontal velocity; the original
grounded dispatcher confirms the zero-input idle case. General grounded motion,
support loss, moving platforms and landing effects remain open. The live fixture
rejects unsupported class descriptors and nonzero grounded force/velocity.
The nonzero force/velocity rejection and idle-only assignment have now been
replaced by shared prepared grounded motion (49f7c3..49f89f). Original/PC/NXDK
comparisons pass on 256 cases covering drag, steering acceleration, force and
support velocity. The live miner remains zero-input, but every grounded tick
now executes the real proposal and common sweep/remaining-time path. Authored
steering, general support maintenance and moving-object policy remain open.
The shared loader now reads all 16 movemodes.tbl descriptors, and the complete
433a50 translation transform matches original/PC/NXDK across 256 axis-selection
cases. Live run/fall descriptors are loaded from the archive and exposed in guest
RAM; the grounded zero-input path uses the transform. Class acceleration scaling,
clamping, live input and eye/parent pose ownership remain open. The miner scene
explicitly rejects eye/parent translation references until those poses exist.
Class movement values now come from entity.tbl: miner1 speed 6, slow factor .5,
fast factor 1.5 and acceleration 20. The shared scale/transform/clamp stage
matches 256 original/PC/NXDK cases and is connected to grounded updates. All four
values match guest RAM in XEMU. Real input and grounded support maintenance are
still required before claiming controlled actor movement.
Ground support now uses the active mode and authored speed for probe depth.
Static support commits preserve velocity, and moved grounded actors switch back
to fall when no walkable support is found. Original support commits match
PC/NXDK in 42 cases; original no-hit/steep transitions pass two prepared cases.
XEMU confirms 320 original/PC query-preparation checks including its 64 live
mode-dependent records. The passive run performs two post-landing support
commits and no support losses; support loss still needs a moving live fixture.
A process-local +X .25 input pulse now drives frames 24..47 through authored
steering. Run 20260909-160442-543906 passes in 64 MiB XEMU: all 64 input and
rendered geometry records match PC, with 40 ground-support commits and zero
support losses. Its 320 original query-preparation checks also pass. The new
framebuffer shows the displaced miner; animation is still scripted, and this
is not a gameplay controller. Live support loss/recovery remains open.
The sustained -X contact route now exercises 29 actual contacts and 92 proposal
passes across 63 updates in 64 MiB XEMU (run 20260909-161509-037641). Every
captured response matches PC and original 49d7e0 before damage dispatch. The
route stays grounded, so it does not close support-loss/recovery. Input and
64 rendered geometry records match PC; all frames finish below the pass limit.
Original collider behavior is now narrowed: 423b90 caches class spheres once,
while 4289d0/428a60 switch cached stance centers rather than following every
animation frame. Shared center switching and standing-clearance endpoint
preparation match 288 original/PC/NXDK cases. Runtime use still requires the
correct class standing/crouching pose cache, clearance query and ground refresh;
do not replace the current cache with per-frame bone-derived spheres.
Stance centers now switch in the live scene: crouch at frame 39, clear-space
stand at frame 48, with ground refresh on either change. The 200-byte cache
and all 64 stance records match PC in stock 64 MiB XEMU (20260909-162703-696777).
The crouch pose is derived by stop-looping, weight selection and .2-second
advance from a copy of the diagnostic initial playback state. Initial pose
selection is still provisional; blocked standing, crouch movement descriptor
selection, timing/effects and gameplay input remain open.
Live stance transitions now call the original-verified numeric movement setter:
standing mode 1/speed 6, crouched mode 0/speed 3, with the run physics descriptor
unchanged. All 64 setting records match complete original setup wrappers, PC
and NXDK from XEMU run 20260909-163137-872744. Miner response remains 10 because
its class lacks flag 0x800. Test steering bypasses the gameplay input/target-speed
controller, so slower actual crouch movement is not yet established.
Dispatch correction: descriptor 1 selects 49e400, not the prepared 49f7c3
linear branch used by earlier live tests. Original idle checks could not detect
that difference. The live scene now uses verified run convergence, slope
handling and speed 6/3. All 384 original/PC/NXDK cases include real dispatch.
Two 64 MiB XEMU routes match PC; sustained input slows during crouch and
accelerates after standing. Traction remains an explicit diagnostic 1 pending
ground bitmap/material binding; contact-route history does not prove contacts
on the corrected trajectory (zero observed in these new routes).
See docs/XEMU-MEMORY.md for guest snapshots and the NXDK strtod fix.
The complete miner1 block supplies `$Use: "ai response"` with radius 2 (kind 9)
and movement name run (index 1); do not use absent-field defaults for this actor.

## Shortlist

- [ ] Create Live Mines entity 9858 through shared class/model-derived physics and validate its runtime state in 64 MiB XEMU.
- [ ] Connect the verified falling proposal to original contact response and actor pose/room commit, then show the miner moving in XEMU.
- [ ] Extend the shared level-directory loader into a complete campaign world loader.
- [ ] Recover the gameplay camera and entity eye transform instead of using raw spawn coordinates.
- [ ] Draw the loaded static geometry and resolve its materials, lightmaps, visibility, and unknown records.
- [ ] Fingerprint the original executable and reconstruct its subsystems as portable C/C++ using Ghidra and documented evidence.
- [ ] Implement streaming asset access and level loading within a stock Xbox's 64 MiB unified memory.
- [ ] Build an Xbox renderer and corresponding PC renderer that preserve at least PS2 visual parity.
- [ ] Restore the complete single-player campaign, including Geo-Mod, AI, physics, scripts, audio, menus, and saves.
- [ ] Restore authored trigger/event activation, including the level's door-pair links.
- [ ] Validate performance and correctness in 64 MiB XEMU before USER hardware testing.
- [ ] USER: Review visual comparisons against the PS2 version when representative scenes are available.
- [ ] Consider multiplayer only after single-player completion.

## Reconstruction evidence history (current integration status above)

Immediate integration target: miner1 UID 9858 in Live Mines. The body initializer
is ready for prepared input, but class material/flags and named model collision
spheres are not yet resolved into that input. Installed miner1 declares mass
100, flesh material, and csphere_0/1/2; do not substitute the scalar collision
radius for those authored model-derived spheres. Recover their class descriptor
construction, then connect the existing resident level record and body owner.
Shared class-dependent physics flags match 9,216 original cases on PC/NXDK;
the original class parser still needs to supply the descriptor fields.
The shared CSPH reader now exposes all 114 model spheres across 95 installed
V3C files. miner.v3c supplies radii .60/.40/.15 with parent indices -1/15/8;
apply the recovered bone pose path and named class overrides before spawning.
Shared animated sphere placement now matches 318 original queries, including
18 miner standing/crouching sphere poses. Named overrides now match 270
original/PC/NXDK cases, and the bounded table reader checks all 151 declarations
across 63 installed classes. Resolve material coefficients and authored class
flags, then connect the posed/overridden spheres and entity 9858 to its body.
Material source is now located: materials.tbl supplies flesh elasticity .2,
friction .8 and density 800, stored by original 0x467e68..0x467ea2.
The bounded material reader now loads all five coefficients and preserves fixed
material indices independent of declaration order; all ten installed materials
and six malformed fixtures pass, and NXDK builds. Connect it to runtime archive
loading. Authored mass/material/class flags now read successfully for all 63
installed classes with seven malformed-input guards. Recover post-parse class
defaults/mutations and connect these inputs to the miner's runtime body.
Creation-order tracing shows positive authored mass skips sphere mass/tensor
preparation; first-actor body creation precedes class-sphere cache construction
and replacement. Preserve this order and resolve the initial animation state.
Shared body sphere replacement now preserves mass/tensors, updates bounds and
spring flag, and enforces peak old-plus-new allocation budget. PC lifetime/guard
checks pass and NXDK builds. Original installation block 42405d..424168 now
matches linked NXDK in 180 cases, with 160 allocation failures preserving the
old owner. Pose/class construction, later crouch setup and XEMU integration
remain open.
Class +1b4 is optional `$Use:` kind (turret=4), not movement mode; its initial
absent-field value is static zero, confirmed for all 75 original descriptors
and three patterned constructor runs. Add optional use-kind metadata and trace
4246e0 class/model setup before treating class physics inputs as resolved.

Original event activation now passes 2,160 cases, delayed-tick prefix 1,620
cases, and three re-activation steps. Timer replacement, disabled caller-field
updates and on/off ordering are documented in docs/TRIGGERS.md. Reconstruct
shared event state transitions, preserving callback order, then event actions.
Shared C activation/timer prefix now matches 3,780 original cases on PC and
compiled NXDK with non-mutating callbacks. Extended checks now total 3,922
cases, including 12 callback mutations and 130 float-threshold neighbors.
Register event states and implement actions; recursive callback behavior and
exhaustive timing precision remain outside current verification.
The three door-event constructors pass six original-code patterned-storage
cases; timer/link initialization and action targets are identified. Trace the
remaining factory/loader initialization before registering event runtime states.
Set_Friendliness action now passes 24 original-code target-state cases; UnHide
on/off request bytes pass eight cases and two common off no-ops pass. Implement
shared actions and recover deferred UnHide processing; do not unhide immediately.
Deferred UnHide now passes 108 original-code no-player cases and five persistent
ticks, confirming 500 ms shared timing, on/off priority and retained requests.
Player eligibility now passes 96 branch cases; the type-specific factory's
zero-delay arming and request reset pass 12 patterned cases. Connect the existing
collision-ray wrapper and recover actual visibility effects before integrating
the action; see docs/TRIGGERS.md.
Original hide/unhide direct writes and dispatch now pass 80 target cases plus
two null calls, including attachment toggles and recursive child visibility.
Connect the existing weapon-reset reconstruction, recover remaining intercepted
effects and map entity fields into shared runtime before gameplay visibility.
Shared C UnHide initialization, request methods and deferred scheduler now match
223 original cases on PC and NXDK; connect its target callback to registered
entities, collision eligibility and full visibility effects.
Owned entity inputs now preserve decoded and raw records after archive closure
for all 1,610 entities in 66 levels (max 101,209 accounted PC bytes); connect
Xbox residency and runtime registration before dispatching events to entities.
Xbox entity residency now passes stock 64 MiB XEMU: 78 entities, 355,013 total
logic-data bytes and 66 PC-matching lifetime checks; runtime entity construction,
registration and event dispatch remain open.
Original entity loader 464010 is identified; 200 scalar-overlay cases verify
UID/relationship/friendliness writes, and door-target initial flags are mapped.
Recover complete factory initialization before constructing live entity states.
Shared C spawn-field extraction now matches all 1,610 entities on PC/NXDK with
bounded raw parsing; integrate these fields into runtime construction.
Shared entity creation-flag conversion now matches 1,152 original factory-prefix
cases on PC/NXDK; allocation and remaining entity initialization are still open.
Original entity constructor and type-0 allocator now pass 15 patterned cases;
trace generic factory overlays and class/asset initialization before live spawning.
Generic factory identity/default overlays now pass 168 original no-model cases;
integrate verified defaults while recovering physics, model and class setup.
Complete original no-sphere physics initialization now passes 120 cases without
intercepted calls; recover sphere allocation/mass generation for entity physics.
Original fallback-sphere preparation now passes 180 cases with real array growth
and copies; implement shared sphere preparation and recover existing-sphere inertia.
Shared C fallback mass/sphere fields now match 4,020 original cases on PC/NXDK,
with nine input guards; add sphere ownership and existing-sphere inertia next.
Shared budgeted sphere ownership preserves authored 24-byte records and passes
11 PC lists; wire Xbox residency and recover generated inertia for existing spheres.
Shared existing-sphere mass/tensor accumulation now matches 640 original cases
on PC/NXDK with 25 invalid-input guards; recover matrix inversion and connect
the complete initializer before constructing live entity physics.
Shared tensor inversion now matches 1,007 original cases and composed sphere
preparation matches 640 on PC/NXDK; connect body initialization and recover
geometric-model mass generation before live entity physics.
Shared world-space tensor updates now match 1,200 original cases on PC/NXDK,
and 360 full no-sphere initializer layouts are verified; integrate those body
fields with sphere ownership and recover general-radius bounds next.
Shared sphere radius/world bounds now match 1,000 original cases on PC/NXDK;
combine the verified fields, tensors and owned sphere lists into body initialization.
Shared budgeted fresh-body initialization now matches 480 original cases on
PC/NXDK; wire Xbox residency and entity construction after resolving class/model
physics parameters and the remaining original constructor fields.

Trigger loader 465510 and event loader 462150 are identified in the original
level dispatcher. The bounded v180 trigger inventory parses 2,367 triggers and
4,471 links across 93 levels. Live Mines trigger 8542 references first keys of
both inspected door panels (8593/8591), while 8522 references the other pair
(8603/8605); additional links remain unresolved. The shared C trigger reader
now matches all 2,367 records and 4,471 ordered links against the independent
Python inventory, including truncation, link bounds and exact EOF checks. PC
and NXDK builds and all four CTest checks pass. Recover original activation/UID
resolution before replacing diagnostic activation.
See docs/TRIGGERS.md for the read evidence and ordered linkage table.
The bounded event inventory now exhausts 4,446 records and 4,901 links across
93 levels. Door extras resolve to Set_Friendliness (9826), Goto_Player (8694),
and UnHide (8695). Reconstruct their target resolution and runtime actions;
the shared C event reader now matches all 4,446 records and 4,901 links across
93 levels, with truncation, link bounds and exact EOF checks. NXDK builds;
owned event inputs now retain every record/link after archive closure across
93 PC level checks (max 212,044 accounted bytes), with exact/short budgets and
repeat-close checks passing. Connect event ownership to Xbox; event runtime
registration, scheduling and actions remain open. Owned trigger
inputs now retain all 2,367 records and 4,471 links after archive closure in PC
checks across 93 levels (max 55,696 accounted bytes); exact budget, short-budget
failure and repeat-close checks pass. NXDK builds; connect ownership to Xbox
runtime, ordered registries and activation.
Both owners now run in the Xbox level path under a shared 512 KiB budget.
64 MiB XEMU retains 61 triggers, 184 events and 387 links (253,804 bytes),
matching PC hashes through 66 lifetime checks and archive closure. The
600-frame door diagnostic still passes; connect runtime registration and
authored trigger/event execution to replace explicit diagnostic activation.
Original handle pool blocks now pass 1,032 allocations, eight releases and
1,047 lookups: 1,024-slot FIFO reuse, generation wrap and stale-handle rejection.
The shared C registry now matches original PC/NXDK traces across 2,100
operations and occupies 12,300 bytes on Xbox. Connect it to owned object
lifetimes and constructors. Resident Xbox movers/controllers now use registry
handles for collision IDs, memberships and parents, with five of each passing
64 MiB XEMU lifetime and 600-frame door checks. Registered PC memberships
preserve all 1,421 links across 68 levels. Register trigger/event runtime
instances next; mover-first creation order remains diagnostic.
Original dispatch 4c0320 now passes four door-list execution cases, preserving
interleaved mover/event order and mover suppression. The fixture uses synthetic
registered handles and intercepts downstream actions. The verifier now also
executes original post-load UID conversion 4611a1..461231 with the real object
and key-owner lookup functions for both door lists. Shared C UID resolution
now matches 2,000 original lookup cases on PC and compiled NXDK, including
duplicate precedence and special UIDs. Connect persistent ordered registries,
entity backlinks and owned trigger/event data, then activation and dispatch.

Ordered controller binding now matches 2,000 original calls on PC/NXDK, and
forced translation updates all 1,406 owned mover poses/collision views on PC.
Position assignment now matches 8,030 original calls on PC/NXDK, including
pending-source aliasing; controller commit lists now match 2,000 original calls
on PC/NXDK. Normal commit now synchronizes all 1,406 real-level collision views
in PC checks, with NXDK synchronization verified separately. Connect persistent
authored controller ownership and rendering, then trigger/events and XEMU.
Translation initialization now matches original state/selected-key blocks for
2,441 authored key positions on PC/NXDK; build persistent owned controllers
using the preserved base pose and validated selected start key. Owned controller
inputs now retain all 1,223 groups after archive closure (max 114,176 bytes per
level). Live Mines now retains five groups through 64 rendered frames and
archive closure in stock 64 MiB XEMU; connect persistent runtime poses,
registration/attachments and rendering to this owned data. Controller base
poses now match 1,223 original first-key setups, including flags 06000001 and
zero radius from the empty collision-sphere list. Persistent runtime entries
now retain 1,112 initialized translations and 111 explicit pending rotations;
Live Mines runtime lifetime passes 64 MiB XEMU. Bind authored memberships to
registered objects, then activation, propagation/commit and rendering. Owned
mover membership lists now match all 1,421 authored references on PC using
diagnostic handles. Resident Xbox mover objects now retain those memberships
and synchronized flags through 66 lifetime checks in 64 MiB XEMU; connect
activation/propagation/commit and rendering, then general-object attachment
and actual handle registration. The four authored Live Mines door cycles now
match 160 connected original activation/tick/propagation/commit steps on PC
and compiled NXDK. The same 160 ticks now run on resident controllers/movers
in stock 64 MiB XEMU with exact PC traces and collision-view synchronization;
connect moving geometry to rendering and the frame loop. Shared transformed
mover projection now passes 4,218 pose cases; load/remap mover materials and
append their draw geometry to the scene before frame-loop integration. Shared
world/mover material residency now maps 5,919 texture references across 68
levels, deduplicating names and owning images after source closure. Live Mines
uses 17 unique slots for 28 references (16 loaded, one missing), with all mover
textures already present in the world table. PC budget/ownership/image checks
and PC/NXDK builds pass; connect this bundle and its local-slot mappings to
scene geometry, then validate visible movers and frame-loop motion in XEMU.
World/mover composition is now connected in both PC and Xbox scene paths.
The combined builder matches separate projection/remapping on all 68 levels;
Live Mines submits 80 additional mover triangles and passes 64 MiB XEMU plus
PC framebuffer comparison. The miner camera occludes these additions (frame
unchanged); add a door-focused inspection view, retain render geometry and
connect committed resident poses to the actual render loop. Current scene
movers use authored file poses and release source geometry after projection.
The door-focused view now visibly renders mover 8544 with textures in stock
64 MiB XEMU and passes PC framebuffer comparison (964 triangles, no actor in
view). Connect committed door motion to retained render geometry and frames;
inspect the visible fine diagonal surface artifacts before claiming fidelity.
Shared world/mover reprojection now supports a fixed caller-owned vertex
allocation; 204 PC pose cases across 68 levels match separate projection and
remapping, including failure preservation for late invalid poses and short
capacity. Retain mover geometry/mappings in the scene owner and call this
updater from committed door motion during the renderer's frame loop.
The retained scene owner now passes 204 PC pose/lifetime cases across 68 levels
after archive closure, source-level poisoning and image closure, with at most
307,984 bytes of mover geometry/mappings. Keep this owner in the Xbox scene
and draw committed door ticks; the existing compatibility loader still closes
it after initial projection. GPU streaming currently requires a fixed world
vertex boundary and must accept changing world counts within its fixed capacity.
Committed mover poses now drive the Xbox draw loop with retained source data
and fixed CPU/GPU capacity. The visible diagnostic now updates all door panels
together at 1/60-second steps, correcting the earlier quarter-second jumps and
sequential panel cycles. Original/PC/NXDK checks pass 2,400 smooth motion ticks;
the open-door 120-frame XEMU check passes exact mesh hashes and framebuffer
comparison. Connect actual trigger/event pairing and the gameplay clock, retain
actors in the same running world, then restore collision obstruction and audio.
The full synchronized 600-frame cycle now passes stock 64 MiB XEMU with exact
PC mesh traces and all 2,400 controller/mover ticks; the open-door snapshot is
verified separately. Reprojection and timing remain diagnostic, not performance-
qualified gameplay rendering.

Loaded-world sweeps pass 44,082 real-level queries and 162 Live Mines queries
in 64 MiB XEMU with exact PC results; recover transformed/moving-solid queries
and actor movement response before driving gameplay collision.

Zero-radius collision tree traversal now matches 1,800 complete original calls;
recover tree construction and world room selection, then connect loaded geometry
and mutable metadata to the running game.

Live Mines collision queries now use initial file/room filter metadata and
match all 7,418 original face-query results; recover later room mutations and
world traversal before using them for gameplay.

Collision face bounds now match the original finalizer for all 7,418 Live Mines
faces, including its 0.0001 expansion; continue recovering room state and
runtime filter metadata before world traversal.

Face access now preserves the original 32-bit file flags across 460,720 faces;
recover post-load flag changes and room ownership before enabling gameplay
collision metadata and world traversal.

All 7,418 Live Mines faces now bind to collision views and match original ray
results on PC and NXDK; recover runtime filter metadata and world traversal
before using the query for gameplay visibility or actor movement.

The combined zero-radius face query now matches 6,000 complete original calls
on PC and NXDK; connect real level faces and world traversal, then recover
texture-check modes and swept actor collision.

The candidate face filter now matches 28,356 original cases on PC and NXDK;
join it to geometric tests, recover later texture rejection and hit updates,
then implement world traversal.

Projected polygon containment now matches 6,858 original cases on PC and NXDK;
combine it with plane/box tests and recover face filters and world traversal.

The one-sided segment/plane test now matches 11,876 original cases on PC and
NXDK; recover polygon containment and face filtering before querying the world.

The original segment/box collision primitive now matches 15,678 original cases
on PC and NXDK; reconstruct world traversal and support queries described in
docs/COLLISION.md to complete crouch/stand physics.

The stance gate matches 2,292 original-code cases and crouch eligibility matches
8,751 cases through its visibility boundary; reconstruct the target-eye ray
query and crouch/stand collision effects before driving gameplay state selection.

The Xbox scene now plays registered authored stand/walk/crouch states from the
miner's 19-motion set; replace the diagnostic request schedule with gameplay
state selection and add authored action/weapon-specific playback.

Bulk registration now resolves 180 authored state groups, including 19 miner
base motions; feed these state sets into actor playback and retain explicit
failure for the unresolved edf_ship group.

Motion-cache lookup and reference acquisition now match 1,041 original-code
cases and supply diagnostic registration identities; finish payload release
and bulk authored state registration before replacing the fixed file setup.

Motion identity/loop-flag registration now matches 2,098 original-code cases
and drives the passing XEMU fixture; connect bulk authored state registration
and motion-cache ownership to replace the remaining fixed four-file setup.

Authored state binding now opens 1,137 compiled motions and reports the two
missing EDF1_Idle references; register these handles and drive real actor states.

State-motion metadata now matches all 1,139 base/weapon declarations across 63
classes; connect those mappings to registration/playback and resolve missing
EDF1_Idle plus the already missing edf_ship model without inventing substitutes.

Live Mines now retains its world and textures while miner UID 9858 animates for
64 frames on stock-memory XEMU, with the final pose matching PC; replace the
scripted miner sequence with authored actor states and recover the gameplay eye.

Placed diagnostic models now explicitly clip at the world's 0.1-unit near plane;
1,200 original polygon fixtures and a real miner boundary test pass, with the
existing combined image unchanged and numeric 64 MiB XEMU passing. Recover the
original near-plane setup and extend the frozen scene into animated actors.

Live Mines and miner UID 9858 now draw together on PC and stock-memory XEMU
with a passing framebuffer comparison; restore the gameplay camera, reconcile
near clipping and extend the frozen diagnostic into an animated actor scene.

Two Live Mines miners now produce posed geometry at authored transforms using
the world preview camera; combine world/actor draws and reconcile near clipping
before treating their camera paths as fully equivalent.

All 78 Live Mines entity records now bind by UID to installed skeletal meshes
and authored skins with exact transforms; feed these bindings into world drawing
and resolve each actor's animation state instead of the scripted miner fixture.

Animation scratch now uses a fixed 27 KiB heap allocation, reducing its Xbox
stack frame to 19,152 bytes; the measured stream-frame subtotal is 31,196 bytes.
Measure library/kernel stack demand and connect real actors to world drawing.

Parker's skin now completes 64 frames on stock-memory XEMU and matches the PC
framebuffer. Audit diagnostic stack usage and the earlier NV2A assertion, then
connect authored skins and real level entities to combined world drawing.

All five miner skin bundles now pass material and budget checks, and Parker's
authored skin renders in the PC preview. Connect skin selection on Xbox and
real level entities, and verify the original runtime skin-switch semantics.

The skeletal compiled-name helper matches 1,960 original-code cases and resolves
49 entity classes to installed meshes; 41 capacity guards pass. Recover the
separate .v3d model path, investigate missing edf_ship.v3c, apply skin lists and
connect real level actors to world drawing.

Entity asset metadata now matches all 63 table classes and 168 class/skin
selections. Recover compiled model filename resolution and apply skin lists;
investigate level class names camera1 and Bucket Bot before actor creation.

The bounded level-entity reader matches 1,610 records across 66 installed
levels. Resolve entity classes/skins to model assets and feed real transforms
into scene drawing; Live Mines miner1 UID 9858 is a nearby placement candidate.

Supplied world-camera/entity transforms now drive the running model stream on
64 MiB XEMU, with moving and fully culled placement checks on PC. Load real
level entity transforms and combine placed models with the world draw path.

Entity-local camera and view rotation now match 2,000 original placement-block
cases. Connect supplied world/entity transforms to scene model drawing and
recover the remaining view-stack and secondary-view state.

The miner inspection now streams 64 changing frames through reusable CPU/GPU
buffers on 64 MiB XEMU. Add sustained scene playback, world placement, original
lighting/material effects and performance measurements before PS2 comparison.

A textured, posed miner now renders on PC and 64 MiB XEMU, with a passing
framebuffer comparison and translucent faceplate. Add visible animation playback,
world placement, original lighting/material effects and PS2 visual comparison.

Resident model batches now assemble triangle routing, clipping and emission,
and real miner poses exercise index output on PC and 64 MiB XEMU. Independently
compare full assembled batches and connect their materials/vertices/indices to
the Xbox and PC draw paths for the first visible animated model.

Clipped-vertex projection and fan emission now match 2,400 original projection
cases and 600 output polygons, including 2,200 generated vertices. Connect these
stages to routed resident model triangles and verify the assembled draw inputs.

Multi-plane polygon traversal now matches 600 complete original-code triangle
fixtures byte for byte, including temporary-slot lifetime and output order.
Extend integrated coverage to near/custom planes and varying colors, then
recover clipped-vertex projection and bounded triangle-fan index emission.

Clipping temporary-vertex allocation and release now match 2,043 original
operations, including the 47-success allocation limit. Assemble the per-plane
polygon walker using the recovered pool, intersections and attributes.

Generated-vertex clip classification now matches 3,000 original cases and
shares the verified fresh-projection mask calculation. Assemble temporary
vertex allocation/release and polygon traversal around the recovered stages.

All seven clip-plane intersection branches now match 2,800 original cases.
Assemble intersection, attributes, clip classification and temporary-vertex
lifetime into per-plane polygon traversal before draw submission.

Clipping UV/RGB interpolation now matches 3,000 original-block cases under
explicit x87 rounding setup. Recover plane intersection factors/positions and
assemble per-plane polygon clipping with bounded temporary vertices.

Triangle clipping-input assembly now matches 6,000 original records, preserving
own UVs and reused positions/lighting. Recover edge interpolation, temporary
vertex lifetime and per-plane traversal before clipped triangle submission.

Triangle rejection/direct/clipping routing now matches 3,000 original cases.
Recover polygon clipping and bounded index emission, then connect the routed
triangles to material binding and Xbox draw submission.

Model triangle facing now matches 2,000 original renderer cases, including
two-sided flags and degenerate inputs. Connect frustum rejection, clipping and
index emission to the processed animated batches.

Real miner poses now feed resident render batches on PC and 64 MiB XEMU,
passing 47,616 vertex visits with bounded reusable buffers. Compare those render
outputs against original code and connect triangle submission for visible models.

All 599 installed model batches pass controlled resident-render processing,
including UVs and duplicate copies. Connect real evaluated poses and emulator
batch buffers, then triangle clipping and visible submission.

Resident batch processing now assembles deformation, projection, lighting and
reuse, matching 250 original-code batches. Exercise real animated geometry
through it and connect triangle clipping/material binding/draw submission.

Fresh model projection, depth and visibility now match 2,000 original-code
cases. Assemble deformation, projection, lighting and duplicate reuse into
resident batch processing, then connect triangle submission.

Duplicate-vertex processing now matches 2,000 original branch executions.
Recover fresh projection and connect cache lifecycle, deformation, lighting
and model draw submission using the resident batches.

Visible fresh-vertex normalization and lighting now match 2,000 original
call-site cases. Connect duplicate-vertex cache reuse and draw submission;
the missing normalization step is now explicit in the recovered render path.

The original camera-distance metric and detail choice now match 2,000 tested
cases. Supply live camera/scale state and connect selected model geometry to
draw submission; the current visible scene still contains static geometry only.

File thresholds now drive the resident loader through the recovered selector,
including the shared diagnostic on 64 MiB XEMU. Connect the live camera metric
and model draw submission before implementing in-scene detail switching.

Original model detail selection now matches 2,000 executable cases, including
flag overrides and exact threshold boundaries. Retain file thresholds and
connect the camera metric to selection-driven geometry residency and drawing.

The shared animation diagnostic now uses a budgeted resident geometry loader
on PC and 64 MiB XEMU. Independent traversal validates all 170 model LODs,
including exact-budget success and one-byte-under rejection. Connect resident
batches to renderer deformation, lighting and draw submission for visible models.

All 599 model batches now resolve through their LOD texture tables to owned
material indices. Connect these mappings and renderer-specific deformation
to draw submission, then validate the first visible character output.

All 744 miner LOD0 vertices now deform over 64 animation frames on PC and
64 MiB XEMU, matching 47,616 original collision-deformation evaluations.
Connect renderer-specific vertex processing, materials and lighting to drawing.

The shared miner diagnostic now prepares skinning matrices from real skeleton
and animation data across 64 frames, matching original code on PC and 64 MiB
XEMU. Connect actual vertex batches, material slots and lighting to drawing.

Skinning-matrix preparation and generation caching now match 2,000 original
cases across 8,000 bones. Connect actual stored transforms and animation poses
to prepared matrices, then feed deformation, lighting and model drawing.

Complete model lighting setup now matches 2,000 original-function cases.
Connect world candidate lights and model inputs, finish second-stream/pose
preparation, then integrate the recovered deformation and lighting into drawing.

Selected-light direction now matches 2,000 original cases, including flag
0x400 and singular inputs. Assemble fixed lighting and global gates with the
verified selection, direction and attenuation helpers for renderer integration.

Selected-light attenuation now matches 2,000 original cases. Finish light
direction normalization/rotation, fixed lights and global gates before
connecting the recovered setup to model rendering.

Nearest local-light selection now matches 2,000 original executions. Finish
fixed-light setup, selected-light direction/attenuation and global gates, then
connect game-derived lighting inputs to model rendering.

The model lighting helper now matches 2,000 complete original executions,
including non-finite vectors. Connect original light setup and second-stream
preparation before validating fully lit model rendering.

The bounded vertex-reuse reader matches all installed records, including
24,861 valid backward references. Integrate the renderer's cached position,
clip/depth and lighting reuse while recovering second-stream preparation.

Rendering's fresh-vertex deformation now matches 2,000 original executions
for both streams; recover second-stream preparation, duplicate reuse and
lighting before connecting the deformed batches to drawing.

Collision vertex deformation now matches 2,000 original executions: weights
use byte/256 and stop at the first zero, without sum normalization. Connect
prepared bone transforms and independently recover rendering deformation.

All active model bone indices fit their skeletons; all 12 non-finite normals
and 19,060 vertices with non-255 weight sums are triangle-referenced. Recover
the original deformation consumer before choosing weight or normal treatment.

Vertex/triangle readers now match all 85,866 vertices and 99,214 triangles in
the 95 models. Recover runtime bone-weight interpretation and handling of
12 source vertices with non-finite normals, then integrate skinning and drawing.

The bounded model batch directory now exposes eight aligned data regions for
all 599 batches across 170 LODs in 95 models. Decode the shared format descriptor,
verify positions/UVs/indices and skinning, and connect material slots to drawing.

Per-model material residency now connects actual decoded textures to owned
records with deduplicated local slots and a combined allocation budget.
All 95 models / 733 materials pass field and exact-budget checks; the largest
bundle peaks at 1,837,928 bytes. Connect bundles to model drawing and weapon
presentation, then add cross-model sharing/eviction and a total scene budget.

TGA format and material-alpha classification now match original instruction
blocks and accompany all decoded images. Connect per-model texture residency
and handles to owned materials, then integrate model rendering and presentation.

The shared named texture loader decodes all 385 model-referenced TGAs correctly.
Map original texture-format transparency, add per-model residency and handle
ownership, then connect decoded textures to model rendering and presentation.

Disk material conversion now covers all 733 installed records with bounded
scalar ownership. Resolve real texture handles/transparency and integrate model
instance materials with rendering before validating visible weapon presentation.

All 733 serialized materials across the 95 installed V3C models now stream
through the bounded reader and match independent traversal. Recover conversion
from their 84-byte disk layout to runtime materials before connecting ownership.

Instance materials now own bounded copies of their three arrays with cleanup
and failure checks. Connect this ownership layer to actual model loading and
texture overrides, then validate local weapon presentation in Xbox runtime.

Material fixed-field copying and array-size planning now match 2,000 original
cases through completion or first allocation. Finish bounded ownership of the
three copied arrays before instance materials can be published and rebound.

The 200-byte model material constructor now matches 1,000 full original
executions. Recover material copying and bounded instance allocation, then
connect texture overrides and local weapon presentation to owned model instances.

Model material counts now match 2,000 complete original executions across
wrapper types. Recover per-instance material allocation/copy and texture binding
before connecting the remaining weapon presentation adapter to live models.

The no-op presentation cleanup dependency is removed, and resource/backend
and mode-kind gates match 4,000 expanded original cases. Recover the remaining
resource/material binding operations and live model ownership for local presentation.

Weapon presentation control flow now matches 4,000 original-code cases using
already-loaded model handles. Implement its cleanup/resource/mode adapters and
actual model ownership, then connect local presentation and transition to gameplay.

Current-weapon lookup now matches 3,000 original-code cases and runs before
empty-ammo decisions in PC/XEMU; the original profile starts at the handler
entry. Recover local-player presentation, transition and earlier selection
gates to complete actual weapon switching and gameplay integration.

Ammo exhaustion now drives the selection tail, queues one replacement and
clears followup state in the shared 64-frame PC/original/XEMU diagnostic.
Recover earlier selection gates, local transition and presentation before
actual weapon activation and full gameplay locomotion can be assembled.

Selection followup now executes its actual three-field reset, matching 4,000
expanded original-code cases with 872 resets. Recover local-player transition
4aa0b0 and preceding selection gates, then integrate selection in Xbox runtime.

The selection tail now matches 4,000 original-code cases through paired
remapping, duplicate suppression, ownership checks and request queuing.
Recover the preceding 4a4a50 gates and actual message/apply/followup adapters,
then connect selection to gameplay and validate it in the Xbox runtime.

The weapon-selection queue mutation now matches 640 complete original-code
cases and builds for PC/NXDK. Recover selection eligibility and activation,
current-weapon presentation and firing adapters before connecting the empty
weapon decision to live gameplay; no new visible result is available.

Empty-weapon decisions now match 4,000 original-code cases across paired
firing dispatch, ammo messages and replacement selection, plus the XEMU diagnostic.
Implement current-weapon presentation updates and execute the selected actions,
then finish sound adapters and remaining gameplay locomotion physics.

Ammo lookup and ordered replacement-weapon selection now match 4,000 original
cases and an ammo-exhaustion XEMU profile. Complete the empty-weapon handler,
weapon switching/presentation, sound adapters and remaining locomotion physics.

Effect-pair switching now matches 4,000 original executions and runs through
weapon reset in XEMU; the false local-release callback was removed after
identifying a read-only lookup. Implement sound/player reset adapters, effect
ownership/rendering and following physics/state selection for full locomotion.

Valid-entity weapon reset now matches 2,400 original cases through completion
or explicit external-operation boundaries, and active-weapon nonloop stops run
in the shared XEMU diagnostic. Implement sound/effect/player reset adapters and
following physics/state selection, then assemble full gameplay locomotion.

The pre-candidate 800ms timer/reset gate now matches 5,000 original executions
and runs in the shared XEMU profile with 16 invalid-weapon resets. Recover valid
weapon reset effects and following physics/state selection, then assemble the
full locomotion selector with actual entity creation and registry updates.

Entity lookup, weapon/seat traversal, attachment readiness and combat eligibility
now match 2,007 original-code fixtures and feed the shared 64-frame XEMU
diagnostic. Implement actual entity creation/registry updates, preceding 800ms
reset, following physics/state selection and populated reset/audio adapters.

Locomotion candidate selection now matches 3,014 original executions, including
integrated sidestep/roll effects, asymmetric missing-state fallbacks and extended
precision speed thresholds; the expanded 64-frame diagnostic matches 64 MiB
XEMU. Recover combat predicates, preceding 800ms reset, following physics/state
selection and populated reset/audio adapters to assemble the full selector.

The shared diagnostic now matches original instructions and 64 MiB XEMU for
64 frames of controller blending, real sidestep actions, candidate selection,
playback, cached bones and eye transforms. Recover populated reset/audio
adapters, roll runtime coverage and actual entity initialization, then finish
the full locomotion selector and gameplay-camera integration. Original action
names identify 17/18 as sidesteps and 19/20 as rolls; `rf_turn_*` is historical.

The assembled candidate helper matches 3,004 complete original executions for
absent weapon-entry and sound fixtures, with all six outcome branches covered.
Recover populated reset/audio adapters and actual entity initialization, then
validate the full helper in Xbox runtime and finish gameplay-camera integration.

Remaining turn-candidate branches now match 4,800 original-code cases, including
activity-based restart suppression and fallback candidate selection. Recover
the preceding reset/decision logic and sound playback, then integrate the
complete helper into entity animation selection and the gameplay camera.

Turn direction gating now matches 5,208 original-code cases, including x87
dot-product boundaries and local-vector transformation. Integrate remaining
candidate/reset decisions with recovered turn effects; the full gameplay
selector, camera and animated character rendering remain open.

Selected-turn effects now match 2,400 integrated original-code cases across
action restart, five deadlines, candidates, turn flag and movement settings.
Recover the preceding candidate/reset decisions and valid-sound playback;
the complete gameplay selector and camera integration remain open.

Movement settings now match 6,000 original-code cases for speed, mode and the
conditional response field. Integrate the recovered action starts, five timers
and movement-setting effects into candidate/turn helper 0x41f9f0, then finish
the complete selector and actual player initialization.

Shared game clocks and deadlines now match 9,800 original-code cases, including
nested pauses and wrap boundaries. Connect the turn helper's five 1,200 ms
deadlines, recover its movement effects and finish candidate selection;
platform clock wiring and runtime timer validation remain open.

Entity action starts now match 6,000 original-code cases through loaded motion
restart and sound-class dispatch. Recover sound selection/playback, turn-helper
timer and movement effects, and the remaining candidate-selection logic before
integrating the complete gameplay selector.

Action-activity and remaining-time queries now match 7,000 original-code cases;
activity depends on end minus cursor, not slot weight. Recover candidate/turn
helper 0x41f9f0, including action starts and timer/movement side effects, then
finish the middle selector and actual player initialization.

Animation priority selection now matches 5,010 original-code cases, including
forced states, linked-entity predicates and the x87 velocity threshold. Recover
the middle candidate-selection and physics/AI branches, then connect actual
player entity fields to the complete selector and gameplay camera.

The movement selector tail now matches 6,000 original-code cases using entity
movement-vector, mode and direction fields. Recover upstream priority rules,
candidate selection, physics/AI side effects and actual player initialization
before connecting these decisions to the gameplay animation controller.

The shared controller-to-eye diagnostic now passes 64 frames in stock 64 MiB
XEMU, matching PC and original-code hashes through requests, interrupted blends,
overrides, playback and cached skeleton evaluation. Recover locomotion selection
and actual player initialization; animated character rendering remains open.

Logical state requests and membership checks now match 10,000 original-code
cases, including interrupted blends and standing fallback. Recover locomotion
selector 0x41f400 and actual player initialization flags, then integrate the
controller with playback and eye evaluation in the Xbox runtime diagnostic.

The post-selector animation controller now matches 2,406 original-code cases,
including transition timing, overrides and loaded-motion weight assignment.
Recover locomotion selector 0x41f400 and its state-transition setter, then
connect actual player flags and controller updates to eye initialization.
PC tests and NXDK compilation pass; this controller is not yet exercised in XEMU.

Standing/crouching/restored-standing pose queries now match 600 original bone
matrices and 24 eye transforms across eight starting states. Resolve controller
0x41f400 and actual player initialization flags: creation updates animation by
1/30 second before eye setup, so phase and cursor cannot be chosen arbitrarily.

Character reset and deferred animation-stop controls now match 1,800 original
cases, including preservation of slots/references until the next update.
Verify the full standing/crouching eye-offset setup sequence using these controls
and resolve initial-pose sampling before applying it to the gameplay camera.

Loaded-motion insertion, weight assignment and restart now match 3,200
original-code cases; the runtime diagnostic uses recovered insertion instead
of constructing slots directly. Recover lazy resource registration/loading and
the actual player initialization calls, including bone overrides and displacement
producers, before connecting the gameplay camera.

The shared 64-frame blended-skeleton check now passes in stock 64 MiB XEMU,
matching PC and original-code hashes for bones, eye transforms, playback and
cache state. Recover bone overrides, displacement producers and player setup;
animated character rendering and gameplay-camera integration remain open.

Per-bone generation caching now matches 960 repeated/invalidated original pose
queries, preserving queued displacement on cache hits. Verify animation in
Xbox runtime, then recover bone overrides, displacement producers and actual
player initialization before changing the gameplay camera.

Pending root displacement now matches original consumption and propagation
through 8,000 bone matrices and 320 eye transforms, including a synthetic
second root. Recover the producers of displacement, bone overrides and player
setup, then verify the integrated animation path in Xbox runtime.

Playback now feeds archive sampling, per-bone primary attenuation, pose blending,
parent composition and eye-tag evaluation, matching 4,000 original bone matrices
and 160 eye transforms after complete updates. Recover player setup, root
displacement and bone overrides, and add an Xbox runtime animation check before
using this path for the gameplay camera; the rendered diagnostic is unchanged.

Complete forward playback now matches 10,240 original-code updates across
160 scenarios of 64 consecutive frames, including mixed loops, completion,
freeze handling, compaction, primary state, generation wrap and sticky events.
Connect this state to archive sampling and skeleton evaluation, then recover
actual player initialization and camera animation inputs. No new rendered
behavior yet; publish native game/emulator screenshots when visible progress
is available. Xbox compilation passes; runtime integration remains open.

Non-loop cursor advance and primary-candidate selection within the mixed-loop
branch now match 3,000 original-code cases. Assemble the complete update from
the verified stages and test consecutive frames, preserving the original
slot-index versus motion-ID distinction in fade-bypass lookup.

The non-loop completion pass now matches 1,360 original-code cases, including
freeze-slot retention and primary-state clearing. Assemble and verify the full
animation update across consecutive frames, including primary selection during
advance, zero-weight removal and frozen-instance early exit.

Animation-slot removal now matches 2,040 original-code cases, including ordered
compaction, selected-index repair and reference-count clamping. Integrate this
with non-loop completion and freeze handling, then connect the playback state
machine to archive sampling and skeleton evaluation.

Loop phase-to-cursor mapping and two-marker event crossing now match 3,000
original-code cases, including floor rounding and exact marker boundaries.
Integrate phase/cursor updates, then recover non-loop completion, frozen slots
and inactive-slot removal for complete animation playback.

Shared loop-phase advance now matches 1,600 original-update comparisons,
including wrap flags and dominant-slot selection. Recover end freezing and
inactive-slot removal before
integrating continuous playback with the verified skeleton path.

Character tag placement now matches 1,000 original-wrapper comparisons; this
wrapper rotates and translates the evaluated tag without an extra scale step.
Recover actual player setup inputs and animation slot state, then integrate and
validate the animated camera path in XEMU.

The shared single-motion skeleton path now matches 300 original bone matrices
and 12 animated miner eye transforms for stand/crouch poses. Recover looping
slot updates, root displacement, overrides, model scaling and camera state
selection before applying this model-space eye to gameplay or the Xbox preview.

Archive-backed sampling now matches 131,965 full-track evaluations while holding
only two rotation keys and two position keys. Cache disk reads and integrate
these samples with slot weights, parent composition and eye attachment evaluation.
Open validates all tick ordering; 62 malformed-file fixtures are rejected.

Local pose blending now matches 1,600 original-code matrix comparisons across
one through sixteen contributors. Connect per-track samples and normalized
weights to parent composition, root motion and the model's eye attachment;
the complete 0x51b500 evaluator and playback update still remain open.

Elapsed-to-tick conversion now matches 8,005 original-instruction checks.
Reconstruct the remaining 0x51ba80 update state machine: weighted shared loop
phase, per-motion cursors, end freezing and inactive-slot removal; then connect
cached archive playback and skeleton evaluation.

The shared archive-backed RFA reader now validates all 1,009 installed motions
and reads all 489,545 rotation keys and 147,716 position keys bit-for-bit without
whole-file allocation. Connect indexed reads to cached playback, engine-time
conversion and skeleton evaluation; trailing regions in 57 files remain unclassified.

Per-track blend-weight envelopes now match 575,460 original-code comparisons
covering all 26,393 installed track envelopes plus 500 synthetic envelopes.
Connect weights and sampled transforms through the original skeleton blending
path after implementing playback caching and engine-time conversion.

Rotation-track sampling and easing now match 570,824 original-code comparisons
covering all 26,393 installed tracks plus 300 synthetic tracks. Implement
playback caching, engine-time conversion and animated skeleton
evaluation before integrating camera setup. Single-key sampling at/before its
tick deliberately avoids an original out-of-bounds read by decoding directly.

Packed quaternion interpolation now matches 1,288,315 original-code comparisons
at five blend positions across all 256,663 distinct installed adjacent/single-key
pairs plus 1,000 synthetic pairs. Xbox runtime arithmetic equivalence remains
to be checked when the animation path is integrated into the diagnostic.

Packed rotation decoding is reconstructed in shared C, with all 65,536 signed
component values checked in each of four positions against original instructions.
The decoder, packed interpolation and track evaluation are available; integrate
them with motion loading and animated poses.

Position-key sampling is reconstructed in shared C with 755 bit-exact original
comparisons covering stand/crouch and synthetic tracks. Recover engine-time
conversion and full
motion loading before integrating the camera pose.

Original state initialization confirms stand slot 0 and crouch slot 8; the
base miner1 motions resolve to installed ult2_stand/ult2_crouch RFA assets.
The motion-envelope audit accounts for all 1,009 files and preserves additional
regions in 57 files. Recover keyframe encoding, timing and weapon override
selection before using these motions to set camera offsets.

Attachment transforms now preserve the original unnormalized quaternion path,
with 856 exact original-instruction comparisons including all 755 installed
records. Bone transforms retain normalization; do not use the bone API for
attachment setup. Animated parent poses and model integration remain open.

Shared C now computes aligned LOD attachment ranges and reads all 755 installed
attachment records bit-for-bit; miner's `eye` references bone 8 in each LOD.
Integrate skeleton and attachment tables, validate upper parent bounds, and
recover animated bone poses before using this local attachment as a camera.

Shared C now traverses V3C section structure directly in VPP archives, including
zero-sized SUBM chunks, while leaving LOD blobs on disc. Bone verification uses
structural boundaries instead of signature searches. Decode LOD geometry,
attachments and animation data next; the directory alone does not load a model.

The suspected bone remapping is a stable depth-sorted evaluation order, now
reconstructed in shared C with 200 exact original-instruction comparisons
(95 asset skeletons plus 105 synthetic cases). It does not change file indices.
Use this order when integrating animation poses; full V3C parsing and animation
sampling/blending remain unfinished.

Parent transform multiplication at 0x51c620 is reconstructed in shared C and
matches 2,852 original-instruction comparisons bit-for-bit on PC. Original bone
remapping and full hierarchy/animation evaluation remain open; isolated matrix
products do not yet establish a correct animated eye pose.

Local bone normalization and transform construction are now reconstructed in
shared C; 2,852 PC results match the original instructions bit-for-bit, covering
all 2,452 decoded asset bones and 400 scaled synthetic quaternions. Continue with
parent composition, animation evaluation and proper V3C traversal before wiring
the reconstructed eye pose into gameplay.

The shared raw BONE payload decoder now preserves fields across 95 detected
installed sections (2,452 bones), with no runtime allocation and validation of
finite transforms and acyclic parent indices. Finish structural V3C section
parsing, name processing and original quaternion conversion before pose use;
the verification tool's signature scanning is only diagnostic discovery.

Player creation now identifies the default `miner1` class, and unhooked original
filename resolution confirms `miner.vcm` -> `miner.v3c`. The asset audit resolves
45 of 46 entity-table character references; investigate `edf_ship.vcm` use and
fallback before treating its missing .v3c as a blocker. Parent-chain pose
evaluation is traced to 0x51b2e0/0x51b500 but remains unreconstructed.

Character tag lookup is now reconstructed in `src/core/model.c` and included in
PC/NXDK builds; 306 comparisons against original 0x51d5b0 and its unhooked
default-locale comparator pass. This covers ordered lookup, not model loading
or animated poses. Continue into 0x51b2e0, asset name resolution and player model
selection; runtime pointer layouts must not be used as file layouts.

Camera evidence update: original offset initialization at 0x423bd0 samples the
model's `eye` tag into class standing/crouching fields using identity rotation
and zero origin. Eight isolated original-instruction cases pass in
`tools/inspect_eye_setup.py`, with explicitly synthetic model hooks. Missing
tags and the info +0x724 flag 0x20000 behavior are verified. Recover player model
selection, animation indices, asset parsing and pose evaluation before feeding
these offsets into the campaign camera; no guessed eye height is authorized by
this evidence. See `docs/CAMERA.md` for addresses and remaining uncertainties.

## Accepted requirements

- Original Xbox is the primary platform; only stock 64 MiB hardware is supported.
- A PC build must share the Xbox-compatible game implementation and resource limits.
- C/C++ reconstruction and recompilation using NXDK; Ghidra is available for analysis.
- Single-player campaign first; multiplayer is a stretch goal.
- Resource reductions are allowed only while maintaining PS2-level visual parity.
- XEMU is the primary emulator; CXBX-R may assist with diagnosis.
- Never control the desktop or send host input; use source, files, logs, non-interactive commands, and process-contained test harnesses.

## Milestone 0 — evidence and toolchains

- [x] Set the persistent project goal.
- [x] Inspect the initial workspace: only an empty Git repository was present.
- [x] Locate NXDK at `C:/nxdk` and Ghidra at `C:/Programming/ghidra_11.3.2_PUBLIC`.
- [x] Locate the game, record file hashes, PE architecture/version/imports, and asset archive inventory.
- [x] Record compiler, SDK, emulator versions and reproducible commands.
- [x] Evaluate Open Faction and Dash Faction as references, recording provenance and reuse terms.
- [x] Compile and validate minimal PC and Xbox artifacts; distinguish scaffolding from recovered game behavior.

## Milestone 1 — reconstruction foundation

- [x] Create a reproducible headless Ghidra project from a hash-identified executable.
- [ ] Map startup, filesystem/archive loading, memory allocation, game loop, rendering, audio, input, and level transitions.
- [x] Start a recovered-function register with original addresses, evidence, dependencies, confidence, and behavioral tests.
- [x] Implement bounded archive access and inspect the first campaign level's section boundaries.
- [x] Load level directories and spawn transforms in shared C, validate all 94 installed levels on PC, and validate Live Mines in 64 MiB XEMU.
- [x] Load budgeted resident static geometry in shared C, validate all 94 installed levels, and verify Live Mines geometry in 64 MiB XEMU.
- [ ] Reconstruct level payload parsing and original archive mounting/override semantics.
- [ ] Establish native emulator visual capture without any desktop capture or window input.
- [ ] Establish memory accounting that includes executable, heaps, stacks, GPU surfaces, audio, and transient loads.

## Milestone 2 — playable campaign slice

- [ ] Load and draw the first campaign area on both platforms.
- [ ] Restore collision, player movement, controller mapping, weapons, enemies, events, and Geo-Mod.
- [ ] Restore HUD, sound, dialogue, cutscenes, saves, and level transitions needed by that slice.
- [ ] Compare matched PS2 scenes and capture native emulator output for review.

## Milestone 3 — campaign completion and validation

- [ ] Extend subsystem coverage through every campaign level and document completion evidence.
- [ ] Run memory stress, loading, save/reload, destruction, AI, and frame-time regression scenarios.
- [ ] Validate on 64 MiB XEMU, then prepare a reproducible USER hardware test package.
- [ ] Fix hardware findings and finish PC regression coverage.

## Current status

Original inputs are at `D:/Programming/GitHub/OpenRedFaction/Installed_Game`;
source work is in this repository on D:. RF.exe and tables.vpp match the
Red Faction 1.20 NA baseline. Inventory: 39 files, 15 VPP archives, 8,410 entries.

Both a 32-bit PC archive diagnostic and an NXDK XBE/XISO compile. The shared C
VPP reader matches the complete installed archive directory inventory and passes
malformed-input/bounds tests. One function, the filename checksum at `0x0052be70`,
has been reconstructed and verified against original x86 for 9,671 test inputs
in the C locale. Ghidra identified 7,248 candidate functions including libraries;
this is not a measure of completed reconstruction.

The diagnostic boots and validates tables.vpp on 64 MiB XEMU with both installed
4627 debug and Complex 4627 v1.03 BIOSes using the xemu rendering backend.
The headless `-display none` path faults before entry in this emulator build;
its exact cause remains a non-blocking harness investigation. The harness uses
isolated config/EEPROM, temporary HDD writes, and monitor-only guest telemetry.
UT99 reference harness location resolved to
`C:/Programming/GitHub/UnrealTournament_1.40/UT99-Xbox/Tools`.

All 94 installed RFL level files have been scanned for section boundaries.
The first campaign level is `L1S1.rfl` (Live Mines), version 180, 3,572,594 bytes,
with 29 payload sections plus an end marker. Static geometry is 1,637,649 bytes
and lightmaps are 1,130,684 bytes on disk; neither number is a runtime memory estimate.

No playable game, PS2 visual comparison, or hardware validation exists yet.

### Latest progress

Moved the authoritative source project, references, and analysis databases to
`D:/Programming/GitHub/OpenRedFaction` after the user's location correction.
Original inputs remain in its `Installed_Game` subfolder and are ignored by Git.
The C-drive task workspace contains relocation pointers; its old build outputs
are retained under `local/legacy-build-c` on D:.

The shared C level loader now validates v180 section directories, bounds, header
offsets, end markers, and finite spawn transforms without loading whole RFL/VPPs.
All 94 installed directories and spawn transforms agree with independent binary
reads; two CTest suites pass. Live Mines' directory, geometry/lightmap section
sizes, and exact spawn position are verified in 64 MiB XEMU from a disc containing
the unmodified 68.8 MB levels1.vpp archive.

Static geometry now retains original payload bytes and builds texture/room/face
offset indices under an explicit allocation budget. Live Mines has 6,658 vertices,
7,418 faces, 29,110 corners and 54 rooms, using 1,667,605 requested bytes for this
geometry representation. All 94 installed levels load on PC; the largest requests
2,131,016 bytes. Accessor-bound and allocation-budget checks pass, as do nine
corrupted geometry cases. XEMU verifies counts and first/last decoded positions
with geometry resident. This is not yet a rendering, collision, or gameplay test.

The initial untextured static geometry preview now renders on PC and Xbox from
the stored Live Mines spawn. Three frames execute in 64 MiB XEMU; a native guest
framebuffer capture matches the PC reference within the diagnostic edge tolerance
(191 of 307,200 pixels exceed three channel values). Fixed a W-buffer override
after back-buffer selection. This provisional camera and flat face coloring do
not establish original rendering fidelity; textures, lightmaps and gameplay remain.

The shared C TGA decoder now reads 24/32-bit raw and RLE images directly from VPP
entries with a fixed 4 KiB input buffer and explicit output allocation budget.
All 16 Live Mines TGA references resolve across maps1/maps2/maps3/maps4/maps_en;
RGBA8 base images total 2,117,632 bytes. USERBMAP remains unresolved. Pixel output
matches Pillow for 41 installed images covering all supported observed header
variants and every RLE image; 17 synthetic origin/truncation/packet cases pass.
One installed paletted TGA remains unsupported.

The shared material loader now uses bounded geometry-name access and explicit
archive search order. It allocates 2,118,040 bytes including 17 material slots;
the exact budget passes and one byte less fails with cleared state. It loads the
16 images inside stock 64 MiB XEMU and retains USERBMAP as an explicit missing
slot. Guest pixel checksum 0xa3aeb67d matches independent decoding. The material
images remain resident alongside geometry and the three-frame GPU diagnostic.
The shared preview now clips source UVs with geometry and carries u/z, v/z, 1/z
and material indices. Its vertex allocation is now 1,460,280 bytes. The PC reference
samples original textures with perspective division and nearest/repeat sampling;
artifacts/live-mines-textured-pc.png shows the first base-textured tunnel view.
Xbox now uploads swizzled base textures and samples projective UVs. Selecting
border-color source fixes initial corrupt sampling because images contain no
border texels. The textured tunnel is visible, but the strict PC comparison fails:
29,235/307,200 pixels exceed three channel values (mean maximum-channel error 1.346).
Resolve sampling differences before claiming PC/Xbox equivalence. Lightmaps,
filtering, animation and archive override rules remain open. Fixed stale Xbox
header dependencies caused by Windows/POSIX path mismatch; a rebuilt XEMU frame
with the expanded vertex stride passes the untextured comparison.

Next: bind decoded materials to geometry on both renderers, load lightmaps, recover portal/room visibility,
and investigate trailing records that the community geometry loader leaves opaque.
Original geometry-loading function semantics still require Ghidra reconstruction;
the new loader implements the observed file layout and preserves unknown bytes.

Sampling diagnosis: optional RF_PREVIEW_SNAP_1_16 quantizes only the PC diagnostic
screen coordinates; it reduces the textured mismatch to 1,222 pixels (0.398%),
still FAIL against the existing tolerance. This is an unverified raster precision
hypothesis, not a hardware rule or a relaxed acceptance threshold. Default PC
output remains unchanged. A 1/32 sample-center shift was less effective and removed.

Lightmap inspection now covers exact image boundaries in all 94 installed level
sections. Live Mines has 23 RGB lightmaps, expanding to 1,507,328 RGBA bytes;
largest installed level expansion is 3,670,016 bytes. The shared C lightmap loader
now loads all 94 levels under exact output/record budgets, rejects one byte less,
and clears allocations on failure. Live Mines' 23 decoded pixel hashes match
independent expansion of original RGB bytes. PC/NXDK builds pass. Geometry mapping
indices now resolve through the first word of each 96-byte record and all indices
are in range across all 94 levels. Live Mines' lightmaps remain resident in 64 MiB
XEMU alongside base materials and geometry; the base-textured frame is unchanged.
Lightmap UV propagation, blending rules and GPU sampling remain open. Open Faction
uses doubled modulation, but original-binary investigation shows this is conditional.
Original initializer 0x515730/constructor 0x411e00 emulates to solid mode 0x400024
at 0x1808328. Original state function 0x54f160 uses modulation in texture-source
case 4 and doubled modulation in case 5, with runtime byte 0x1cfcc1d selecting a
4-to-5 remap. Trace that flag and lightmap conversion before fixing the lighting
equation. See docs/RENDER_STATE.md; no unconditional lighting pass was substituted.

The runtime flag is now identified as TextureOpCaps MODULATE2X: original write
0x545f46 extracts bit 0x10 from 0x1cfcb58, and the original logger calls it D3Dm2x.
The shared mesh now clips lightmap UVs and carries projected coordinates plus
resolved image indices (56-byte vertices, 2,044,392 bytes for Live Mines).
The second texture stage now samples lightmaps with linear clamping and combines
them with linearly wrapped base textures using doubled modulation. Draws split at
material/lightmap boundaries. The first lit tunnel renders on PC and 64 MiB XEMU.
Strict comparison still FAILS: 5,248/307,200 pixels exceed three channel values
(mean maximum-channel error 0.609). PC/NXDK builds and existing CTest checks pass.
Original texture conversion, fog, alpha, mipmaps and PS2 comparison remain open.

The shared projection now uses the same explicit 1/16-pixel grid on both targets;
the former PC-only diagnostic flag has been removed. The latest lit-frame
comparison PASSES: maximum channel difference 1, zero pixels over the existing
three-value tolerance. Schema-7 XEMU telemetry measures GPU image requests
3,624,964 bytes and vertices 2,044,392 bytes, with 44.25 MiB available after uploads.
This frozen-scene snapshot is not a full-game peak or PS2 parity result.

Camera investigation now identifies first-person entry at 0x40ddf0, camera
position access at 0x40d760 and projection setup at 0x547150. First-person entry
uses the entity's computed eye position/orientation; the raw spawn lacks that
transform. Original projection constants are fingerprinted in camera-evidence.json.
No guessed eye-height or original near/far interpretation was introduced. See
docs/CAMERA.md for the entity/model and projection work still required.

Camera update dispatcher 0x40d850 is now exported; first-person mode copies the
player eye point to camera position before additional effects. Corrected thiscall
metadata for original vector helpers 0x409f40/0x409f70 makes that data flow explicit.
An original-instruction operand scan records candidate eye-field references for
further tracing; the standing/crouching eye writer is still unresolved.

The writer is now identified as 0x4194e0. Its non-linked branch is reconstructed
in src/core/eye.c: states 8/9/10 select crouching, offsets are transformed by the
entity orientation and blended during transitions, then added to position.
400 deterministic cases match original x86 execution exactly in the PC test.
The animated eye-tag branch returns explicit RF_NOT_FOUND; model-derived offsets,
entity state loading and actual camera integration remain open.


Xbox image sharing (2026-09-10): images/lightmaps now decode directly into
contiguous swizzled RGBA8 memory, borrowed by the GPU as A8B8G8R8; PC retains
row-major RGBA8. Owner lifetime and close still release the image allocation.
No duplicate GPU texture allocation remains except the 1-pixel fallback.
120-tick L1S1 campaign replay 20260910-002409 passes on 64-MiB XEMU.
The earlier run 002039 has 10891 available pages versus 9798 in the previous
235522 campaign run; this is a comparison across runs, not a controlled memory
benchmark. Final native framebuffer comparison and pixel tests are recorded
in docs/PROVENANCE.md. The 4-MiB material and 8-MiB GPU-image guards still need
replacement with measured shared campaign budgets before L1S2 can run.

2026-09-11: Coplanar thin-ray handling now matches original 4dec10 misses;
all 14 L1S3 grid errors resolved with 8,572 other rays unchanged. PC/NXDK
builds and original-vs-compiled collision regressions pass. L1S3 traversal
remains open; native930-frame XEMU replay20260911-021158 now passes with
PC state parity and nonzero DSP output on verified64MiB memory.

2026-09-11: Fresh L1S3 route-link inspection places930/1530 endpoints inside
trigger357, linked to Continuous_Damage9488; scene skips its value_byte2 mode.
Eastward trigger9322 links a return to L1S2, not Door4. Recover missing trigger
mode/damage behavior before treating the descent as valid campaign traversal.

2026-09-11 correction: Trigger357 value2 is an allowed-linked-actor filter.
UID26 is APC; original eligibility rejects an unlisted player. Eight explicit
filter cases pass original/PC/NXDK. Prior pit-volume evidence must not be
used to implement unconditional player damage or death.

2026-09-11: Authored actor contact filters0..4 are connected. Runtime link
membership/rebinding tests,93-level startup graphs and original filter
comparisons pass. Native930-frame replay20260911-022235 matches PC on64MiB.
NPC registration, point-event filter5 and special ownership gates remain open.

2026-09-11: Continuous_Damage original on/off trace passes450 cases each,
including1080 damage requests and72 secondary effects. Next reconstruct
target dispatch and verify loader inputs/repeat scheduling before integration.

2026-09-11: Shared Continuous_Damage request dispatch matches990 original
on/990 off traces on PC/NXDK; four guards and eight CTests pass. Scene
integration awaits damage backend, actor ownership and scheduling evidence.

2026-09-11: Verified24 original loader/helper cases and12 real-vtable
Continuous_Damage activation/tick sequences. Authored words map to amount/kind;
each activation dispatches once. Upstream repeated activation remains open.

2026-09-11: Shared SP health/armor numeric phase matches8,192 original
41a350 prefixes plus four port guards on PC/NXDK; builds/eight CTests pass.
Recover kill attribution/effects and outer4892c0 routing before live damage.

2026-09-11: Shared SP lethal attribution matches4,096 original prefixes
plus two guards on PC/NXDK. Burn-source and first authored-UID lookup order
are verified. Remaining damage effects, outer routing and ownership stay open.

Damage sound routing: complete4196f0 now matches4,096 original cases on PC/NXDK
plus five nonfinite guards, including original timers, sound call order and
callback flag mutation. See docs/DAMAGE.md and tools/verify_damage_sound.py.
Live damage playback, pain animation and entity/death ownership remain open.

Outer damage evidence:8,192 original4892c0 SP traces verify eligibility,
force bypass, difficulty scaling, object dispatch and post-effect health
cleanup. Implement the C wrapper next; delegated effects and the alternate
6fc4d8 path remain open. See docs/DAMAGE.md and verify_damage_wrapper_trace.py.

Shared SP damage dispatch now reconstructs4892c0 for normal SP globals.
8,192 original traces and two port guards match PC/NXDK state, return values
and callback arguments. Both builds/eight CTests pass. Remaining work includes
entity damage effects, owned registration and connecting Continuous_Damage.

Remaining entity-effect order:8,192 original41a505..41a7ab traces verify burn
creation gates/source resolution, pain thresholds, kind6 voice, player feedback
and AI notification. External effects remain supplied; next reconstruct shared
orchestration and effect ownership. See docs/DAMAGE.md and
tools/verify_damage_effects_trace.py.

Armor immunity42cca0 is now shared C, with4,096 original/PC/NXDK cases
including full flag words and floating boundaries. Damage dispatch8,194 cases
and eight CTests still pass; PC/Xbox builds pass. Owned predicate adapters
and shared burn/effect orchestration remain open; see docs/DAMAGE.md.

Shared entity damage effects41a505..41a7ab now match8,192 original traces
on PC/NXDK plus seven guards; both builds/eight CTests pass. Next assemble
vitals, attribution and effect orchestration, then bind actual burn, animation,
AI and entity owners. Callback mutation remains outside current trace coverage.

Damage effect callback mutation coverage now passes16,384 original/PC/NXDK
cases plus seven guards. All nine effect boundaries exercise mutations
(1,251 actual callback mutations); no core correction needed. Assemble the
damage phases next; real effect/lifetime ownership remains open.

Composed SP entity damage now joins vitals, lethal credit and effects;16,384
original paths match PC/NXDK state, return and call order. Effects-only suite,
both builds and eight CTests pass. Next bind persistent entity/class/burn
ownership and actual downstream effects, then connect event damage.

Burn creation42e910:576 original traces verify rejection,64-byte record
fields,free-to-active circular list transfer and non-aborting particle/audio
failures. Actual pool initialization/release, attachment lookup and templates
remain open before a gameplay burn owner; see docs/DAMAGE.md.

Burn pool evidence:8 records of64 bytes,432 original release cases plus
complete initialization verify owner clearing and circular list recycling.
Emitter absence is0;voice absence is-1. Corrected creation failures to0 and
all576 cases pass. Implement shared pool and recover42ee80 update next.

Burn owner update:4,096 original tail cases verify frame-scaled self damage,
audio and fade dispatch;30 real timer cases verify225ms shared rearm/wrap.
Shared pool implementation, full spread/emitter loop and fade42f2f0 remain
open. See docs/DAMAGE.md and tools/verify_burn_tick_trace.py.

Shared burn pool initialization/release now preserves8x64-byte records,
slot-index rings and owner cleanup order.437 PC/NXDK cases pass; both builds
and eight CTests pass. Pool storage is524 bytes excluding effects. Next add
creation and per-frame update, then bind actual emitter/audio/owner adapters.

Shared burn creation now matches612 original cases on PC/NXDK plus two
ring guards,including full-capacity free ring and partial emitter/audio failure.
Release/init437 cases,both builds and eight CTests pass. Next implement
update/fade and bind actual attachment/particle/audio/entity ownership.

Burn fade42f2f0:8,192 original traces verify5/12/17-second boundaries,
byte87 wrap/scaling and release order.4174c0 requires type7 owner before
flags29c writes;actual owner conversion remains required. Implement shared
fade/update and reconcile particle fields next;see docs/DAMAGE.md.

Shared burn fade now matches8,192 original cases plus six guards on PC/NXDK.
Creation614,pool437,both builds and eight CTests pass. Full per-frame loop
and live particle/model/audio/entity ownership remain open; type7 transition
cannot be replaced by a missing-owner guard. See docs/DAMAGE.md.

Original burn traversal audit:90 cases confirm18 missing-owner free-ring
cycles after release and72 terminating missing-emitter paths. Shared update
must resume saved next active token after release,an explicit defect correction;
verify head/interior/tail removal before integration. See docs/DAMAGE.md.

Shared burn traversal now fixes missing-owner cycling by resuming saved next
active token.90 corrected-original paths plus two guards match PC/NXDK;
both builds/eight CTests pass. Full body and fade-triggered release integration
remain open; this is traversal infrastructure,not completed burn simulation.

Shared burn owner audio/damage/fade dispatch passes4096 original cases on PC
and NXDK plus6 input guards; full attachment/spread and live ownership remain.

Burn attachment1024 and spread3125 original traces pass; radius is2 units
(squared limit4), while coincident leg tags expose original NaN normalization.
Shared attachment/spread implementation and persistent ownership remain open.

Shared burn attachment placement passes1025 PC/NXDK cases with zero measured
position error; coincident tags return an explicit error before NaN emission.
Spread, world transform and full live burn ownership remain to be connected.

Shared burn spread passes3128 PC/NXDK checks with original filter/range/damage
ordering; world transform, timer gating and persistent live adapters remain.

Complete original burn body passes54 ordering/mutation scenarios; spread
damage can reject a later target or switch the owner tail to fading.
Compose shared phases against this oracle before persistent live integration.

Composed shared burn body passes54 complete original scenarios on PC/NXDK,
including timer/world transform and mutation-visible spread/owner ordering.
Pool fade/release composition and persistent campaign adapters remain open.

Pool -> owner tail -> fade -> release passes540 PC/NXDK patterns and2082
record retirements, including all-eight removal, exact rings and timer rearm.
Persistent campaign resources and live owner-reference clearing remain open.

Burn model binding needs bone-only case-sensitive substring search; shared
lookup passes2048 PC/NXDK cases and fourth query is head. Live model resolution,
fallback orchestration and persistent NPC damage owners remain to connect.

Burn fallback bone resolution passes2048 PC/NXDK cases with byte-verified
hyphen/space names; Ghidra symbol underscores were not literal query bytes.
Live model owner, pose binding and persistent campaign damage remain open.

Installed burn bone binding passes95 model sets on PC/NXDK;31 resolve all
four bones, and miner resolves11/12/15/8 including spine03 fallback.
Live owner/animated pose integration and gameplay eligibility remain open.

Animated burn-bone query passes520 original PC/NXDK cases including264 real
miner pose queries plus26 guards; live emitter/owner adapters remain open.

Burn emitter movement boundary passes1024 PC/NXDK original cases plus4
guards; actual room locator, parent synchronization and emission remain.

Emitter cached-room policy verified across240 original paths; positive motion
and face crossing gate fresh lookup. Recover4cd9e0 before live room binding.

Complete original room-crossing traversal passes1500 geometry cases; distinct
508570 plane rules and1.0001 fraction limit must be preserved in shared code.
Boundary/oblique/real-level checks and cached-room binding remain open.

Shared room-crossing traversal passes1500 original PC/NXDK cases plus2 guards;
edge/oblique/multiple-root/real-level checks precede cached-room integration.

Room crossing matches3506 additional PC/NXDK boundary/angled-segment cases;
oblique planes, multiple roots and real levels remain before live integration.

Retained L1S1/L1S2/L1S3 room crossings match complete original4cd9e0 in2,160
NXDK CPU-harness queries, including oblique faces and multiple primary roots;
cached-room policy composition and live emitter ownership remain open.
Evidence: tools/verify_loaded_room_crossing.py and docs/DAMAGE.md.

Shared cached-room tracking and emitter room-token adapter now match complete
original4cd970 across6,480 retained L1S1/L1S2/L1S3 scenarios on PC and NXDK;
both builds and eight CTests pass. Connect live emitter ownership next.
Evidence: tools/verify_loaded_room_tracking.py and docs/DAMAGE.md.

Concrete burn attachments now connect evaluated bones, cached-room movement and
full emitter update with no allocation. Original/PC/NXDK match72 scenarios each
on L1S1 and L1S3, including40 created particles per suite; both builds and eight
CTests pass. Persistent NPC/resource ownership and live scheduling remain open.
Evidence: tools/verify_burn_resolved.py and docs/DAMAGE.md.

Burn fade now writes directly into active particle runtimes;8,192 original/PC/
NXDK comparisons preserve packed color and enabled bytes. Prior fade and540
retirement scenarios still pass, as do both builds and eight CTests. Connect
actual emitter/audio release and persistent NPC ownership next.
Evidence: tools/verify_burn_fade_resolved.py and docs/DAMAGE.md.

Burn resource release now stops/frees actual emitter slots and detaches surviving
particles;256 original/PC/NXDK cases verify list order and unchanged particle
counts. Both builds,437 prior pool checks and eight CTests pass. Audio backend
and persistent NPC ownership/scheduling remain open.
Evidence: tools/verify_burn_resource_release.py and docs/DAMAGE.md.

Original audio control-stop now preserves signed generation/8-bit slot semantics
separately from mixer handles;2,500 original/PC/NXDK cases pass, including zero
and stale handles. Recover control-voice allocation/refresh before connecting
burn audio to device adapters. Both builds and eight CTests pass.
Evidence: tools/verify_audio_control_stop.py and docs/controller-audio.md.

Original control-voice allocation now sweeps30 slots before selection and assigns
generations only after successful playback;1,024 original/PC/NXDK comparisons
pass. Positional start/refresh and real sample/device adapters remain. Both
builds,2,500 stop regressions and eight CTests pass.
Evidence: tools/verify_audio_control_start.py and docs/controller-audio.md.

Positional voice start/refresh now compose shared spatial math and control
allocation;1,024 start and2,048 refresh original/PC/NXDK cases pass, with prior
control regressions and eight CTests. Both builds pass. Connect real sample/
device adapters and persistent burn-owner scheduling next.
Evidence: tools/verify_audio_control_position.py and verify_audio_control_refresh.py.

Sample prepare/playback dispatch now preserve the separate sample gain category
and looping bypass rules;6,000 original/PC/NXDK cases pass. Actual543760 loading
and522530 device adapters, signed-device/mixer handle mapping and native status
ownership remain. Both builds and eight CTests pass.
Evidence: tools/verify_audio_sample_start.py and docs/controller-audio.md.


Current build06d8038 native regression: replay20260911-053608 passes930 L1S3
frames with PC state parity, QMP-verified64MiB and nonzero DSP output;
9530 available pages at completion. Eight PC CTests pass. Normal disc flags
restored and ISO rebuilt. See docs/XEMU-MEMORY.md. This confirms the existing
live path only; burn NPC ownership and sample-control device integration remain
open. No new visuals or hardware listening claim.


Device-source playing queries now exist for PC and Xbox, using each platform's
source clock. Real PC audio checks and native64MiB APU20260911-054025 pass expired
one-shot, muted loop, unrelated voice, unknown handle and closed-device cases.
Queries do not release PCM or certify speaker completion. Both builds/eight CTests
pass. Connect signed control-device ownership and sample loading next; persistent
NPC/burn ownership remains open. See docs/controller-audio.md.


Live ambient playback now uses a fixed244-byte signed identity map instead of
casting unsigned mixer handles; generation32768 no longer becomes a false failure.
Zero identity, stopped/reused source guards and original nonnegative counter wrap
pass nine CTests and720 original/NXDK comparisons. Four campaign startup fixtures,
PC1170-frame ambient restart and native64MiB replay20260911-054619 pass with PC
parity/nonzero DSP. Both builds pass. Controller/game-control device ownership,
native status integration, sample loading/category wiring and persistent NPC burn
ownership remain open; see docs/controller-audio.md.


Controller playback now routes metadata loop mode to the mixer and native device,
reports device start failures, and shares nonnegative device identities with ambient
voices. Arrival stops resolve full unsigned handles and clear spatial tracking.
Native64MiB lift-cycle replay20260911-055316 and PC600-frame replay pass with two
loop-mode starts/two stops,243 mixed loop voice blocks and zero observed wraps
(trips end before sample endpoint). PC/Xbox state and DSP checks pass, as do both
builds/nine CTests, door traversal and1170-frame ambient restart. The historical
two-track door PCM oracle fails on newly included ambient starts; independent
full-mix reference coverage remains open. Game control-table/category/native-status
integration and persistent NPC burn ownership remain pending.


The current door-plus-ambient PCM oracle now matches every572800 output bytes via
independent Python WAV decoding/resampling/mixing and propagated source cursors.
It captures actual scene scheduling/gain decisions as inputs; independent original
runtime verification of those decisions remains in the shortlist. Relative door
travel timing is retained; current contact startup adds one ambient-only block.
Observer-disabled PC repeat and native64MiB replay20260911-060021 pass with matching
PCM hash and guest state, plus nonzero DSP. Both builds/nine CTests pass. The earlier
historical two-track oracle failure is resolved for the current mixed PCM stream.


Entity factory armor/health assignment now matches1,536 original/PC/NXDK cases:
SP class armor copy, low-byte network zero armor, negative/unordered class health
becoming100 plus object flag4, and opaque class764->entity840 transfer. Both builds
and nine CTests pass. This is numeric construction only; intervening factory calls,
class metadata binding, persistent NPC registration and live burn/damage ownership
remain open. The campaign still registers only the compact player view.


Required class Life/Envirosuit/FOV metadata now loads through a bounded archive
adapter. All63 installed classes match original numeric stores/FCOS, PC archive
loading and NXDK text reading; five malformed guards preserve output. The prior
class764->entity840 word is identified as cosine of half authored FOV. Both builds,
nine CTests and1,536 creation-assignment cases pass. Persistent NPC construction
can consume verified class values next; registration/model/physics ownership and
live damage/burn integration remain open.

Current Xbox regression after7830833: replay-20260911-062100 PASS,180 staged
L1S1 door/audio frames, stock64MiB, PC state parity and nonzero DSP output.
Nine CTests pass; persistent NPC construction/ownership remains open.

Persistent NPC construction inputs now survive archive close and share class
vitals/physics under a1MiB campaign budget; three level lifetime/budget probes
and native64MiB replay-20260911-062422 pass. Actual NPC construction, model
ownership, registration and live damage/burn behavior remain open.

Entity model-kind selection is original-verified across86 cases and retained
with class model names; native64MiB replay-20260911-062852 passes. Actual
model/resource loading and actor initialization remain the next NPC work.

Campaign now retains shared skeleton bones by compiled model identity within
a256KiB budget. Three level lifetime/failure probes pass; per-NPC poses,
animation/geometry/material ownership and actor registration remain open.
Native64MiB replay-20260911-063337 passes600 L1S2 lift/audio frames after
shared skeleton integration; no new visual behavior is claimed.

Per-NPC inactive playback/pose storage now has distinct bounded arrays and
original-verified constructor fields (128 cases). Three level owner probes pass;
connect authored motion resources and initial state/action selection next.
Native64MiB replay-20260911-064148 passes after per-NPC pose allocation.

NPC base state mappings and borrowed motion descriptors now persist by class;
three level owner probes and181 installed group checks pass. Weapon/actions,
named timing markers and original startup selector inputs remain open.
Native64MiB replay-20260911-064631 passes after retained base motion loading.

Base45 action mappings and sound labels now persist with class state bindings;
1168 action declarations and495 installed action slots verify. Weapon-specific
mappings, numeric sound IDs, named timing markers and live initial selection remain.
Native64MiB replay-20260911-065339 passes with retained base action bindings.

Weapon ID order/name lookup is original-verified (44 weapons,140 lookups), and
class weapon-group masks now persist. Bind each declared group to motion
resources next; live initial selector and playback remain open.
Native64MiB replay-20260911-070422 passes after weapon group-ID integration.

Declared skeletal weapon groups now retain state/action file bindings within
the1MiB budget;4284 weapon slots across63 level groups verify. Stable shared
motion identities and initial actor/weapon selection are required before ticks.
Native64MiB replay-20260911-070902 passes after weapon-group loading.
