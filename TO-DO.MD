# Red Faction Xbox / PC - open work

First-pass policy: playable campaign systems take priority over exact1:1 RE.
Use practical shared implementations, verify gameplay and stock64MiB operation,
and defer visual polish. Focus: movement, combat/damage, AI, mission events,
then level progression. Fidelity remains a later completion requirement.
First-pass acceptance: consecutive campaign sections playable with movement,
working combat, responsive enemies, required mission events and level exits on
stock64MiB Xbox, with the shared PC build maintained. Use measured gameplay
failures to select work; avoid spending time on exact RE or visual polish unless
it blocks those systems. This priority change does not increase completion.

GitHub image policy: retain the current four images; upload no additional
screenshots until near-retail-quality replacements are available.

Shortlist:
- DEFERRED / TO COMPLETE: Improve Xbox performance beyond the current basic-testing baseline; revisit NPC pose evaluation.
- TO COMPLETE: Preserve remaining world changes across sections, including partial actor state and dropped items.
- ACTIVE: Implement scripted attacks and expand mission scripts and AI until consecutive campaign sections are playable end to end.
- TO COMPLETE: Expand the working pistol/rifle combat, pickups and first-person feedback to the remaining weapons.
- DEFERRED / TO COMPLETE: Restore full NPC/prop texture detail through better residency; verify PS2 parity for reduced textures.
- Continue campaign traversal/physics coverage beyond the repaired walking route.
- DEFERRED / TO COMPLETE: Finish lighting lifecycle updates, shadows and visual parity within64MiB; revisit after critical gameplay systems.
- Finish glare volumes/reflections, cross-level bitmap clocks, special visibility and parent ordering/tag resolution.
- Complete the single-player actor, AI, damage and mission runtime.
- Connect retained NPC speed/body/stance owners to support queries, contact/fall/landing dispatch and physics stepping, then timed footsteps.
- Complete moving NPC animation, room membership, LOD selection and lighting.
- Complete authored clutter creation, skin bindings, collision ownership and rendering fidelity.
- Finish controller/event interactions, traversal, particles and GeoMod.
- Complete audio playback ownership, shared RNG ordering and level transitions.
- Keep shared PC/Xbox behavior verified within the stock64MiB memory budget.
- USER: Review PS2 visual parity and test hardware after representative XEMU gameplay is stable.
- USER: Review the interactive PC movement and audio items listed below.

Progress reporting: include a very high-level working area (e.g. Lighting, AI, Weapons, GeoMod or Scripted events) and an overall percent estimate at the end of every turn report (USER preference) and revisit remaining 24-hour workdays when measured throughput supports a forecast; current rough estimate45% (35-45%), no defensible +/-48-hour completion forecast before end-to-end gameplay.

USER direction: final performance pass complete; return to systems implementation.
Native replay-20260914-104251 passes stock64MiB crossing/state/HUD checks and
matches the complete final image from103625. All37 PC tests pass. New NPC
per-frame counters identify pose advance/evaluation6.320ms, collision preparation
and its hash0.631ms, controllers/setup0.252ms, eye/state hashes0.165ms, residency
0.058ms, snapshots0.029ms, fixtures0.019ms and visibility gate0.010ms.
Scene phase means total71.751ms this run versus54.270ms previously; no additional
speedup or sustained high-teens FPS is claimed. Host variation and instrumentation
remain possible contributors. Defer further performance investigation per USER.
Next systems work: Attack event38 dispatch and actor/target behavior. Existing
original loader462150 routes type38 to4b86d0; factory4b69d0 constructs it via
4beae0. Inspect those bounded functions to establish the authored target mapping,
then connect to combat; avoid expanding this into an exact-RE parity project.

NPC subphase profiling verified in replay-20260914-103625: stock64MiB
180-frame crossing/state/HUD checks pass, with an exact final framebuffer
match against102028; all37 PC tests pass. Scene phase means total54.270ms
(~18FPS-equivalent, not measured gameplay FPS; loading/input pacing excluded).
NPC playback/pose/collision cache costs6.136ms, scripted movement1.408ms,
NPC room refresh0.291ms and attachments0.117ms. These are nested timings,
not additional frame costs. Next optimization target: NPC pose/playback and
collision preparation; room and attachment work is not the main cost here.
Instrumentation alone makes no speedup claim.

Motion track reuse rejected pending controlled evidence. Repeat native
replay-20260914-103040 passes state and exact final-image comparison, but
scene69.818ms/NPC9.615ms/platform18.308ms again establish no gain against
the earlier52.404/7.375/12.904ms baseline. Broad renderer slowdown makes
causality uncertain; nevertheless reverted the candidate rather than
claiming success. Original parser restored; candidate.patch retained under
artifacts/motion-track-reuse for later controlled investigation.
Correctness evidence:37 tests, six strict PC comparisons, two stock64MiB
native replays and matching final framebuffers. No new FPS gain.
Next: separate NPC playback from attachment/room updates and use a matched
baseline or within-run comparison for small timing changes.

Step profiling verified in replay-20260914-102028: stock64MiB180-frame
crossing/state/HUD checks pass, and full native final image matches101543.
Scene52.404ms (~19FPS-equivalent); instrumentation gives no speedup claim.
Mean nested step costs: forces/movers/player physics/support1.097ms,
light timers0.068ms, triggers/events0.806ms, particles0.087ms,
NPC scripts/animation/rooms/attachments7.375ms, collision checks0.279ms.
NPC updates dominate this block. First five stages have103 samples; latter
stages104 because the final frame skips stepping. Both builds and37 tests pass.
Next: inspect NPC animation/attachment updates for repeated work; retain
scripted movement and gameplay behavior. Evidence: artifacts/step-profile/
performance.json and native102028 image comparison.

Corrected VBlank guard verified in replay-20260914-101543: stock64MiB
180-frame crossing/state/HUD checks pass; full native final image exactly
matches100657. Scene64.047->52.770ms (~19FPS-equivalent work), platform
24.221->13.192ms, VBlank/reset/clear10.683->0.490ms. Section counters:
one explicit wait,119 already-crossed frame starts. GPU completion waits
and pb_finished full-queue retry remain. Counter is checked after event
wakeups to reject stale signals; section teardown resets tracking.
Both native candidate/restoration builds pass. Shared PC gameplay code
unchanged; current PC/native replay state agrees. No new37-test run needed
for this Xbox-only change. Evidence: artifacts/vblank-pacing/performance.json,
baseline-performance.json and native101543 image comparison.
TODO: validate high-rate scenes and hardware presentation; profile remaining
physics/events and world rendering before further gameplay expansion.

Unused first-person render allocations removed; replay-20260914-100657
passes180-frame stock64MiB crossing/state/HUD checks and exact full native
final-image comparison with100127. Counter[1,0,0,1283456] confirms no
body output/scratch allocation and1,283,456 bytes of omitted payload.
Last-frame renderer available pages6160->6481 (+321 pages,1,314,816 bytes);
this uses diagnostic44 sampled by the renderer, not load/teardown samples.
Both builds and37 tests pass; six strict PC image/state comparisons match.
Evidence: artifacts/hidden-player-memory/memory-report.json, comparison/report.json,
and artifacts/xemu/replay-20260914-100657/native-image-comparison.json.
Pose/collision geometry remains resident. No new FPS gain claimed.
Next: presentation pacing and physics/event costs.

Hidden first-person body mesh suppression verified in replay-20260914-100127:
stock64MiB180-frame crossing/state/HUD checks pass; full native final image
matches095052 exactly. All64 sampled actor-render records emit zero vertices.
Actor render phase5.769->0.298ms; scene65.000->64.259ms. Platform time rises
18.817->24.106ms, offsetting the CPU reduction; no overall FPS gain claimed
beyond the existing ~15FPS-equivalent work. Both builds,37 tests and six
strict PC image/world/player/model comparisons pass. Poses, eye/collision
updates and the visible first-person weapon remain active. Default and
third-person rendering remain enabled. Evidence: artifacts/hidden-player-mesh/
comparison/report.json, performance.json and native100127 image comparison.
TODO: reclaim unused first-person body mesh/render scratch allocations after
checking their ownership; inspect presentation pacing and physics costs.

Room-culling camera audit completed with tools/replay_visibility_views.py:
nine final views after90-frame left/right/up-right rotations near authored
exits in L1S1/L1S2/L1S3. Final player state and recorded inputs match in all
cases, with distinct final body states confirming the rotations. Eight
images match exactly; L1S3-right has three isolated ceiling-edge pixels
replaced by background. Both images inspected; accepted for first pass,
tracked as polish work rather than exact parity. Raw report remains
REVIEW_REQUIRED; explicit review is artifacts/visibility-views/review.json.
TODO: repair the earlier actor-view single pixel and these three pixels;
extend coverage to more rooms/intermediate frames and bind detail parents.
Initial audit incorrectly treated a mesh-count-inclusive hash as camera-only;
corrected to compare final body state and input records before the full run.
No new engine change or FPS gain in this audit. Existing native baseline
remains approximately15FPS-equivalent work.

Primary-room culling verified in replay-20260914-095052: stock64MiB native
180-frame crossing/state/HUD checks pass; complete final framebuffer exactly
matches094219. World rebuild23.356->13.538ms; platform24.058->18.817ms;
scene82.529->65.000ms (~15FPS-equivalent, not hardware FPS). Final batches
388->85, measured mesh methods2100->463. This eliminates unseen world work
without changing geometry residency or adding allocations. Both builds and
37 tests pass. Five PC transition final images match; actor8323 retains one
reviewed changed pixel, explicitly PASS_WITH_DIFFERENCES in comparison/report.json.
Broader camera and portal-boundary testing, detail-parent culling and that
one-pixel discrepancy remain open. Evidence: artifacts/world-room-culling/
performance.json and artifacts/xemu/replay-20260914-095052/native-image-comparison.json.

GPU state reuse verified in replay-20260914-094219: stock64MiB180-frame
crossing/state/HUD checks pass, and full native final framebuffer exactly
matches093743. Last frame388 batches require2100 methods instead of6596
(68.2% fewer in the measured mesh loop); triangle order/count unchanged.
Draw/GPU-wait phase11.106->10.519ms, platform24.212->24.058ms, total
scene83.482->82.529ms. This is a modest timing improvement, roughly12FPS
equivalent, with host variation; command reduction is directly counted.
Xbox build/restoration pass. Shared PC code is unchanged; prior37-test
result remains applicable, and harness PC/native state comparison passes.
Evidence: artifacts/gpu-state/performance.json and
artifacts/xemu/replay-20260914-094219/native-image-comparison.json.
Next: larger world projection/visibility and scene preparation costs.

Clip-code cache verified in replay-20260914-093743:180-frame stock64MiB
crossing/state/HUD checks pass; complete640x480 native final framebuffer
matches091453 exactly. Both builds and37 tests pass; six PC comparisons
match final pixels and world/model/player summaries. Transient cache1280B.
Measured world rebuild25.135->24.317ms; scene88.875->83.482ms. Platform
27.260->24.212ms also changes, so the overall gain is not attributable solely
to this cache. Treat as a modest reduction, roughly11-12FPS-equivalent
frame work, not hardware FPS. Next: larger world projection/submission costs.
Evidence: artifacts/world-clip-codes/pc-report.json, performance.json,
and artifacts/xemu/replay-20260914-093743/native-image-comparison.json.

Vertex-upload experiment rejected after replay-20260914-093224. Preparing
colors in a local vertex before writing GPU storage passes stock64MiB replay
state and selected HUD pixels, but upload time5.144->6.212ms shows no measured
XEMU gain (scene88.875->88.336ms is effectively unchanged). Restored the
previous renderer rather than retaining an unproven optimization.
Full native640x480 framebuffer exactly matches replay-20260914-091453,
also closing final-image coverage for the intervening world camera cache.
Evidence: artifacts/vertex-upload/performance.json and native-image-comparison.json
in artifacts/xemu/replay-20260914-093224. Repeatable tool:
tools/compare_native_replay_images.py. This verifies one final frame only.
Next: world projection/clipping remains the measured CPU target (~25ms).

Validation correction: replay-20260914-092055 and092818 omitted --capture;
their PASS proves state/timing checks, not framebuffer validation. The earlier
091453 capture is present. Vertex-upload validation now explicitly uses
--capture and will compare native output with that preserved framebuffer.

World camera cache verified in replay-20260914-092818: stock64MiB180-frame
crossing/state checks pass. World rebuild39.587->25.135ms
(36.5% lower); scene total123.634->88.875ms, about11FPS-equivalent work.
Platform time also falls36.471->27.260ms, indicating host/run variation;
do not attribute the whole scene gain to the cache or claim hardware FPS.
Both builds,37 tests, five PC transition comparisons and the visible-actor
comparison pass. Cache uses1024 transient bytes, with no persistent allocation
or geometry/texture reduction. Evidence: artifacts/world-cache/report.json,
artifacts/world-cache/performance.json and artifacts/world-cache/xemu.log.
Next: reduce remaining world projection and render submission costs.

World-phase profiling verified in replay-20260914-092055: stock64MiB native
180-frame crossing passes with 104 destination samples. World geometry
rebuild averages39.587ms; visibility traversal0.269ms, audio0.173ms,
camera setup/room location0.240ms. Total scene work123.634ms confirms
the prior scratch improvement; this instrumentation adds no new speedup.
Next optimization target: world geometry rebuild, then platform submission.
PC build and37 tests pass; Xbox build/harness restoration pass. Evidence:
artifacts/performance-world-detail.json and artifacts/xemu/replay-20260914-092055.

Active-range scratch optimization passes stock64MiB native crossing/state/
capture verification in replay-20260914-091453. Detailed scene work decreases
294.365->125.864ms; NPC51.192->7.913ms, clutter84.702->2.500ms. These are
single replay guest timings, roughly8FPS-equivalent work, not hardware FPS.
Both builds and37 tests pass; visible-model PC A/B pixels/summaries match.
Next measured costs: camera/visibility/world rebuild41.894ms, platform sink36.712ms,
physics13.288ms. Profile world rebuild before the next optimization.

Visible-actor scratch A/B replay passes with identical pixels and nonzero
NPC/clutter/world-weapon/first-person output;37 tests pass. Native corrected
performance run remains active in artifacts/performance-scratch-fixed-xemu.log.
See tools/replay_scratch_differential.py and artifacts/scratch-differential/report.json.

Scratch validation correction: initial build failed and old-PC replay claims
were invalid. Fixed batch reference/optional static scratch, enforced build exit
checks, and reran with a distinct executable hash. Fresh PC pixels/final summaries
match and37 tests pass. Corrected native run: artifacts/performance-scratch-fixed-xemu.log.
The earlier performance-scratch-xemu.log run failed at compile, before emulation.

Detailed timing passes in replay-20260914-090636: clutter85ms/NPC51ms are
major costs. Replaced whole384KiB per-batch diagnostic scratch fills with
active-vertex ranges; identical PC pixels/model summaries and37 tests pass.
Candidate native timing/state/capture run is active in artifacts/performance-scratch-xemu.log;
do not rebuild Xbox until harness restoration. No native speedup claimed yet.

Bounded HUD batching passes native crossing/state/capture verification in
replay-20260914-090327. Measured scene work drops222->198ms (about11%);
renderer time drops43.5->26.8ms. Still roughly5FPS-equivalent, not adequate.
Finer NPC/clutter/weapon/platform timings are added; both builds and37 tests
pass. Native detailed run is active in artifacts/performance-detail-xemu.log;
wait for disc restoration before Xbox builds. No GitHub images added.

Xbox baseline completed: replay-20260914-090014 passes;104 destination samples
average about222ms measured scene work, with44ms nested renderer time. The
165ms broad presentation phase needs finer NPC/clutter/platform timing.
Bounded64-draw HUD batching is implemented but unverified; native comparison
is running in artifacts/performance-hud-batch-xemu.log. Do not rebuild Xbox
until the harness restores the disc. See docs/XBOX-PERFORMANCE.md.

The command-buffer fix passes the original L1S2->L1S3 stock64MiB replay:
180 frames, exit9512 at60, final L1S3, and matching native/PC state.
Evidence: replay-20260914-085417/report.json. GPU-drained recycling prevents
the observed boundary overflow; optimization must preserve this regression.
USER reports roughly4FPS: prioritize measured Xbox performance now. The existing
renderer profile is partial and includes loading outliers; do not attribute the
whole frame cost to HUD resets without steady-state CPU/GPU measurements.
Next: collect comparable frame timings and batch solid HUD draws/command resets.

PC walking handoff coverage now passes five routes, including L1S3->L2S1
(event7079 at frame86, translation150.25,-176,-121, facing/movement retained).
The apparent reverse event7080 has no direct authored trigger; no walkable
return is inferred. Full traversals between these boundary fixtures remain open.
Native L1S2->L1S3 verification is running in artifacts/section3-xemu.log,
replay-20260914-084854; wait for harness restoration before Xbox builds.
Next gameplay gap to investigate: scripted Attack commands are not dispatched;
L1S1 contains six such events and L1S2 contains three. Confirm actor/target
semantics before connecting these commands to the existing combat runtime.

Expanded PC walking-exit tests pass four routes across L1S1/L1S2/L1S3.
New L1S2->L1S3 event9512 and L1S3->L1S2 event9324 crossings retain facing,
apply the authored(41,32,-118) translation/inverse and continue movement.
These fixtures stage outside exit volumes; full intervening gameplay is unproven.
Evidence: artifacts/walk-exit/report.json. Native section3 checks remain open.

Stock64MiB native dialogue audio passes240 frames: three starts match PC,
no device errors, and3939 nonzero DSP-ring samples. Evidence:
artifacts/xemu/replay-20260914-084040/report.json. This is device-output evidence,
not a linear recording/listening check. Empty voice names now skip loading.
Both builds and37 tests pass after this handling change.

Mission voice playback now uses on-demand PCM in the existing1MiB sound bank;
replacement/off requests stop the previous voice and audio failures keep subtitles.
PC240-frame tests confirm natural/explicit/repeated dialogue starts and distinct
PCM output; both builds and37 tests pass. Native device audio remains to verify.

Stock64MiB XEMU subtitles pass the120-frame authored message8356 replay;
the native framebuffer visibly contains the guard line. Evidence:
artifacts/xemu/replay-20260914-083326/report.json and framebuffer.png.
Next: voice playback through the existing1MiB sound bank with on-demand loading.

Live English subtitles now display through the shared scene HUD with bounded
storage, word wrapping, timed expiry and matching-event cancellation. PC event8356
captures at120/600 frames confirm appearance/expiry. Both builds and37 tests pass.
Native XEMU display verification and voice playback remain open.

Message events now deliver authored records through a shared presentation
callback, including delayed/on/off handling and speaker-link isolation.
Authored L1S1 callback tests and all37 tests pass; PC/NXDK builds succeed.
Scene subtitle ownership/display and audio registration remain the next work.

Mission dialogue lookup now matches all918 installed English records in66
tables through shared C code. PC and NXDK builds succeed;37 tests pass.
Temporary text-table storage is capped at64KiB and released after lookup.
Message event dispatch, timed subtitles and voice playback remain open.
See docs/MISSION-MESSAGES.md and artifacts/mission-message-reader.json.

Stock64MiB Xbox repeated Slay_Object test passes90 frames: one NPC8432 death
entry, health-1, matching PC. Evidence: replay-20260914-081317/report.json.
Mission-message inventory locates66 level-local text tables/918 entries; all
714 Message event IDs resolve. Six voice references are absent from indexed
archives, so subtitles need a fallback. Live message display/audio remains
open; see docs/MISSION-MESSAGES.md and artifacts/mission-messages.json.

Stock64MiB XEMU NPC removal passes90 frames in L1S3 with one actor excluded.
Evidence: artifacts/xemu/replay-20260914-080631/report.json; capture inspected.
Slay_Object now uses NPC lethal damage/death entry and animation without
removing corpse ownership. L1S1 repeated-event/control PC replays and36 tests
pass; both builds succeed. Native repeated-slay verification is running in
artifacts/slay-xemu.log. See docs/SCRIPTED-SLAY.md.

Remove_Object now removes registered NPCs from collision, targeting and
rendering, stops movement and preserves retirement across section revisits.
Death watchers recognize removed resolved handles as missing while unknown
authored objects still defer. L1S3 event9602 removes NPC9436 in a90-frame PC
replay; both builds and36 tests pass. Native verification is running in
artifacts/npc-remove-xemu.log; full object-family deletion remains open.

Stock64MiB Xbox tracking-camera replay passes at1000 frames, with a native
capture of Gryphon walking beyond the hangar and exact PC movement/contact
state. Evidence: artifacts/xemu/replay-20260914-075441/report.json.
Remove_Object now cancels/retire events and triggers, preserving allocation
until scene teardown; NPC/clutter removal remains open. Both builds and36
tests pass, including delayed cancellation, off/no-op and self-removal.
See docs/EVENT-REMOVAL.md; native cancellation testing remains open.

PC inspection camera follows Gryphon without changing player placement.
Frames980/1000/1020 visually show changing walk poses near the floor beyond
the hangar;1000-frame motion/contact state matches the ordinary camera.
Both builds and36 tests pass. Native tracking-camera capture is running;
check artifacts/npc-camera-xemu.log before claiming Xbox visual validation.

Make_Invulnerable now reaches the existing ordinary-damage immunity flag for
registered NPCs and the player. Authored4970/Invert4995 on/off timing passes,
as do36 tests and PC door controls; native protection-event testing remains.
Grounding replay: artifacts/xemu/replay-20260914-074151/report.json PASS.

Scripted run/fall actors now accept floor support, fall through body sweeps
and retry uphill motion along walkable slopes. PC hangar controls pass with
actual floor-height correction (Y2.987934),999 steps and no blocks. Both builds
and36 tests pass. Stock64MiB XEMU1000 frames now match PC position, movement and contacts.
The starting-tunnel capture does not verify walking visual quality.
Evidence: artifacts/npc-ground-replay.log; artifacts/npc-ground-xemu.log.

Scripted movement now requests authored walk/stand animations using existing
blends, with hidden/missing-map guards. Both builds,36 tests and PC door
controls pass; native replay and visual walking confirmation remain open.
The later security doors have authored player-use/delayed-event prerequisites;
keep those intact. See docs/SCRIPTED-MOVEMENT.md.

L7S2 PC hangar traversal passes with authored Shot1/Shot3 setup and Goto4994:
1000 frames,999 steps, zero blocked steps, two NPC contacts/two door starts
and370 occupancy hits. Hidden-actor and disabled-trigger controls both stop
before the door. Longer1500-frame replay stops nearX27.471 beyond the hangar.
Automatic cutscene dispatch and full visible mission play remain incomplete.
Stock64MiB XEMU1000 frames match PC movement, routes, contacts and occupancy.
Both builds and36 tests pass; native capture inspected (starting tunnel only).
Evidence: artifacts/npc-doors/report.json; tools/replay_npc_doors.py;
artifacts/xemu/replay-20260914-072409/report.json.

Shared UnHide events now queue deferred visibility changes for living NPCs.
Authored dispatch, cooldown, missing backend, unresolved links, visibility
publication and no-resurrection checks pass. The PC build and36 tests pass;
full Shot1/Shot3 door traversal and native runtime verification remain open.
See docs/NPC-VISIBILITY.md; artifacts/unhide-ctest.log.

NPC door contacts and occupancy now have shared scene adapters, including separate
controller backlinks per NPC. The L7S2 positive door route remains unverified:
Gryphon4952 is alive (health75) but starts hidden (flags0x0202c000), so occupancy
correctly excludes him. Shot1 Delay4953 -> UnHide4962 -> Gryphon is a missing
runtime prerequisite; Shot3 Delay4961 only enables trigger4963 and later moves.
The current Goto fixture moves the hidden actor; its earlier route results prove
simulation only, not an active visible mission sequence. Implement UnHide and
sequence both authored setup events before claiming the door route complete.
Evidence: artifacts/npc-trigger-diag.log (temporary logging removed from source).

Scripted NPCs now follow authored navigation nodes through the existing route
search and collision sweeps. Gryphon passes the first obstruction:600-frame PC
and stock64MiB XEMU match540 steps, zero blocked steps, two routes and five
waypoint advances;6056 free pages (23.66MiB). Both builds and36 tests pass.
Longer PC controls stop at the hangar door nearX16.745 (doorX17.5); trigger4963
rejects player-controlled actors, while live contacts currently poll only the
player. NPC contacts are next; full route/grounding/animation remain incomplete.
Evidence: artifacts/script-move/report.json and
artifacts/xemu/replay-20260914-065556/report.json; docs/SCRIPTED-MOVEMENT.md.

Basic Goto/Goto_Player commands now drive shared NPC horizontal movement with
steering, position publication and body/world collision sweeps. L7S2 Gryphon
Goto4994 delays0.5s, moves8.875 units, then stops at geometry:600-frame stock64MiB
XEMU state exactly matches PC (355 steps,185 blocked),6057 free pages (23.66MiB).
Both builds,36 CTests, PC59/180/600/601 controls and480-frame walking round trip
pass. This is a fixed1.5-unit/s horizontal approach, not complete locomotion:
route following, grounding/vertical travel, avoidance, authored modes and walking
animation remain open. See docs/SCRIPTED-MOVEMENT.md, artifacts/script-move/report.json
and artifacts/xemu/replay-20260914-064211/report.json.

Event on-links now reach registered translation mover activation through the
existing scene controller service. Conditional dispatch, source/actor/time, off
suppression and errors pass contained tests; both builds and36 CTests pass.
Direct authored mover/native motion verification remains open. L7S2 door1 is
actually a counter gating Gryphon movement (5015 -> Goto4994 -> NPC4952), not
a direct door link; prioritize that missing scripted NPC system next.
See docs/WHEN-DEAD.md and artifacts/mission-mover-* logs.

Authored L7S2 death-to-goal chain passes PC and stock64MiB Xbox: guard4887
death leaves door1=0, guard4907 death fires watcher5012 -> Goal_Set5011 once
at1016ms, door1=1; other six watchers remain inactive. Controlled shared damage
fixture, not a played combat route. Native90 frames match every watcher and
goal,6057 free pages (23.66MiB), framebuffer inspected. Both builds and36 tests
pass; firing-time assertion also passes. The counter2 check targets delayed
Goto4994 -> Gryphon4952; scripted NPC movement and the second guard group remain open. Evidence:
artifacts/death-chain/report.json; artifacts/xemu/replay-20260914-062247/report.json.

When_Dead now polls known NPC state and dispatches linked events/triggers once
per scene, including grouped deaths and the authored any-missing-object flag.
Unknown object classes defer detection. Both builds,36 CTests and the rendered
PC rifle round trip pass; authored death-chain/native verification, clutter
queries and mover actions remain open. Evidence/first-pass differences:
docs/WHEN-DEAD.md; artifacts/death-watch-* logs.

Defeated-NPC revisit now passes rendered PC and stock64MiB Xbox verification:
L4S5 guard3305 killed by rifle fire stays retired after L4S4 and return; rifle3415
stays collected,39 rounds retained,240 inputs and both handoffs match PC.
Native retirement [8,1,0,0],7223 free pages (28.21MiB); framebuffer inspected.
All1610 authored actor keys fit32776 bytes; both builds and36 tests pass.
Corpse poses, partial health and other world changes remain open. Next:14
When_Dead events in the opening two sections need runtime dispatch support.
Evidence: artifacts/xemu/replay-20260914-060602/report.json and
artifacts/pickup-return/report.json; see docs/WORLD-STATE.md.

Pickup revisit now passes a rendered PC and stock64MiB Xbox round trip:
L4S5 rifle3415 collected/fired, exit861 to L4S4, return592 to the pickup.240
inputs,39 rounds retained, zero new grants and exact taken-key match; native
framebuffer verified with7231 free pages (28.25MiB). Both builds and36 tests pass.
Also resolve48 authored .d4l exits to installed .rfl files;271/273 exit records
resolve. Missing targets L12S1/6289 -> L11S4 and L9S1/2446 -> L9S1A still need
reachability/intent checks. Native evidence:
artifacts/xemu/replay-20260914-053954/report.json; artifacts/pickup-return/report.json.

Pickup retirement now persists in the shared section handoff: owned level+UID
keys, preallocated grants and restored taken masks prevent repeat collection.
All593 authored items across68 levels fit the20488-byte owner; scene contact
and restore tests prevent duplicate ammunition grants. Both builds,36 CTests
and the PC rifle replay pass (affected tests repeated after capacity reduction).
Rendered out-and-back pickup replay and native XEMU retirement remain open.
See docs/WORLD-STATE.md and artifacts/pickup-persistence-* logs.

Native mission-state handoff now passes L8S1 -> L8S2 on stock64MiB XEMU:
120 input frames, authored VAT setter at30, forced exit5625 at61, all five
retained goal records exactly match PC; framebuffer verified,6910 free pages
(27MiB) at completion. Separate PC fresh-game control retains VAT=0 and local
sample1 is retired. Both builds and35 CTests pass. Fixed lab load limits with
64px NPC fallback,1MiB prop geometry,512 texture slots and VBM frame-zero
loading. Full lab traversal, world persistence and visual fidelity remain open.
Evidence: artifacts/mission-replay/report.json;
artifacts/xemu/replay-20260914-052507/report.json.

Mission goals are now connected to shared scene initialization and dispatch:
load-time declarations, increment/decrement, conditional on/off propagation and
delayed checks. Pending PC/Xbox section handoffs retain persistent counters. Both
builds and35 CTests pass; authored L8S1 VAT setter survives archive teardown
and gates the L8S2 check in a contained runtime test. Native mission-goal replay
and full authored traversal remain open. See docs/MISSION-GOALS.md and
artifacts/mission-dispatch-ctest.log.

Mission counter storage now builds for PC/Xbox with34 CTests passing: owned
names, signed thresholds, increment/decrement, explicit local/persistent lifetime
and bounded16900-byte capacity. Current C reader verifies53 goal events across9
levels, including14 cross-section references. Not yet connected to gameplay: next
confirm Goal_Create mapping and integrate dispatcher/loading loops. Evidence:
artifacts/mission-goals.json; docs/MISSION-GOALS.md; mission-goals build/test logs.

Walking round trip L1S1 -> L1S2 -> L1S1 now passes PC and stock64MiB XEMU:
two exits at62/264 frames, three scene loads,480 inputs, retained ammunition and
facing, matching PC state and verified framebuffer. Renderer free pages are6656
on first L1S1 visit and6667 (26MiB) on return; longer stress remains open. Evidence:
artifacts/roundtrip/report.json; artifacts/xemu/replay-20260914-045904/report.json.
Next implement persistent mission goals/state; broader arrivals and velocity/stance
carry remain to complete.

Authored doorway placement is now connected on PC/Xbox. Walking into the real
exit triggers passes both directions on PC, preserving facing and continuing
movement after exactly one transition at62 frames. Stock64MiB XEMU forward
walking/arrival passes180 frames with matching PC state and verified framebuffer;
5589 free pages (21.8MiB) remain during destination rendering. Both builds,34
CTests and four legacy dispatch replays pass. Native backward/repeated crossings,
other arrival routes, velocity/stance and mission-state carry remain open. Evidence:
artifacts/walk-exit/report.json; artifacts/xemu/replay-20260914-045205/report.json.

Xbox L1S1 -> L1S2 now passes a complete120-frame native replay on stock64MiB:
exit9019 hands off at61 frames, state comparisons pass and framebuffer is verified.
Old-level cleanup leaves12175 free pages (47.6MiB); destination rendering leaves
5583 (21.8MiB). Fixed stale membership-ready state reading freed controller handles.
Evidence: artifacts/xemu/replay-20260914-043300/report.json. Native backward/repeated
exits, walking triggers, doorway placement and mission-state carry remain open.

PC now follows authored exits in-process: L1S1 -> L1S2 and reverse complete
120-frame replays with cleanup before loading the next scene. Same-level requests
are ignored as first-pass policy; destination placement uses its authored spawn.
Xbox builds the shared handoff but still needs its loading loop connected and
native verification. Doorway alignment and mission-state carry remain open.
Evidence: artifacts/campaign-exits/report.json; both builds and 34 CTests pass.

Player-state handoff now preserves inventory, equipped pistol/rifle and vitals
through a 464-byte shared owned snapshot. PC replay acquires/fires the L4S5 rifle
then imports into L1S2 with byte-identical state (39 rounds, health/armor retained).
Fresh-game and catalog-mismatch cases pass; both builds and 34 CTests pass.
Evidence: artifacts/campaign-state/report.json. Automatic scene transitions,
arrival placement, mission-state carry and native handoff still remain open.

Shared campaign destination resolution now finds all 68 installed level entries
across levels1/2/3.vpp without geometry/material allocation, preserves outputs
on failure and transfers one archive handle on success. Both builds and all
33 CTests pass; see artifacts/campaign-resolver-destinations.log. Actual scene
handoff, player-state transfer and native cross-level verification remain open.

Load_Level(type22) now dispatches through a shared deferred request backend,
including delayed activation. Scene wiring retains destination, entrance, raw
words/flags and source IDs without destroying resources during event dispatch.
All14 authored exits across L1S1/2/3 dispatch and resolve existing destination
files;33 CTests and both builds pass. This does not yet load a second scene.
Next connect teardown/reload on PC/Xbox, resolve same-level exit behavior and
carry player inventory/vitals/placement across section boundaries. Evidence:
artifacts/level-transition-authored.log. No seamless/native transition claim.

L3S1/L3S4 now load and run120-frame PC replays within unchanged NPC4MiB and
clutter1MiB budgets. Full textures are tried first; oversized NPC sets fall back
to128-pixel maximum dimensions and props to128 then64. Box reduction preserves
source-format metadata and accounts simultaneous source/output memory. L3S1
NPC peak2659440 bytes/props612216; L3S4 NPC2255564/props458112. Full-quality
L4S5 framebuffer stays byte-identical. This is first-pass resource policy;
close-up detail/PS2 parity and better residency remain to complete. Both builds
and32 CTests pass. Stock64MiB XEMU L3S4 PASS:120 frames, rifle9479 collection,
three shots/39 remaining and texture/HUD hashes match PC;5883 free pages.
Evidence: artifacts/xemu/replay-20260914-033621/report.json and npc-memory-*.log.
L3S1 native validation and end-to-end progression remain open.

Campaign class filtering now follows the original unresolved-class skip path.
L3S1 retains20 valid actors from22 records; both missing camera1 records are
skipped without substitution. L1S1 keeps all78 actors. Retained record order,
class/UID mappings, archive-independent raw data and exact budget guards pass.
Both builds and29 CTests pass; the original creation-order oracle passes5184 cases. L3S1
now reaches NPC materials but exceeds4MiB (UltorGuard_Parts_03f.tga); L3S4 has
the same residency blocker. These sections are not yet playable. Next reduce
NPC texture residency without raising the stock64MiB target.

Live pistol/rifle switching and authored rifle acquisition now pass six PC
replays: independent ammo, three-shot fire, held-cycle suppression, burst cancel,
unowned rejection and no-reserve reload. Both first-person resources are retained
under1MiB each. Both builds and28 CTests pass; stock64MiB XEMU120-frame rifle
PASS: pickup3415, selection, three shots,39 remaining and HUD match PC;7234
pages free. PC1800-frame death/respawn/fire/reload PASS: retained ownership,
three reserve rounds transferred,42 loaded/197 reserve. Evidence:
artifacts/xemu/replay-20260914-031805/report.json and artifacts/weapon-select/report.json.
Next:5.56mm ammo-box collection, native rifle reload and broader weapon/mission progression. Fitted weapon placement and exact rifle audio remain polish.

Shared burst firing is implemented and the live pistol uses the same scheduler.
Authored rifle rules produce three shots six ticks apart, with45 ticks between
burst starts; partial magazines, release, cancellation and semi-auto holds pass.
Both builds,28 CTests, six pistol replays, NPC death and6000-frame exhaustion
checks pass. Stock64MiB XEMU660-frame PASS:18 shots, one reload,14 loaded/109
reserve match PC;6884 available pages. Evidence:
artifacts/xemu/replay-20260914-025927/report.json. Live rifle switching,
acquisition and audio remain next.

Weapon selection preparation: the live first-person loader now reads its model
and idle/fire/reload clips from weapons.tbl rather than fixed pistol filenames.
Shared assault-rifle loading/playback passes after archives close:34 bones,
637 vertices,1036732 resident/1048328 peak bytes, below1MiB. Both builds and28
CTests pass; pistol replay framebuffer matches the prior native-run PC reference.
Next connect equipped-weapon state, input cycling, rifle acquisition and burst
fire; there is no live selectable rifle or native rifle validation yet.

Health/armor pickup gameplay is connected: Medical Kit and Suit Repair restore
up to a provisional100 cap; full values leave items available, dead players cannot
collect, and successful collection removes the item once. Shared static pickup
resources load once; armor now has a HUD bar. PC replays cover actual enemy damage
followed by healing/repair and full-health rejection;28 CTests and both builds pass.
12mm_ammo class/grant handling is connected and integration-tested, with a later-
level authored ammo-box replay still open. Stock64MiB XEMU300-frame PASS:
repair pack9868 restores15.6 armor once; health/armor/pickups match PC, armor HUD
pixels match,6837 pages available. Native capture inspected. Evidence:
artifacts/xemu/replay-20260914-024732/report.json.

Live authored Handgun pickups now render using the retained pistol world model
and replenish player reserve. A two-unit body-distance check plus precise world/
moving-solid occlusion gates collection; full inventory leaves the item in place.
Successful collection removes it once for this scene. PC cases cover visible/full,
partial replenishment,16-round grant and no second collection; panel obstruction
and moving-clear tests pass. Both PC integration and28 CTests pass; stock64MiB
XEMU210-frame walk-and-collect PASS:Handgun9427 collected once, ammo/HUD
match PC, world model absent afterward;6869 available pages. Evidence:
artifacts/xemu/replay-20260914-023510/report.json. Other item classes, pickup
sounds, script/registry publication and original starting grants remain open.

Item definitions and capped inventory grants are implemented in shared C.
Handgun gives12mm handgun ownership/16 rounds;12mm_ammo grants32 SP rounds.
Tests cover ownership, ammo-only, capacity limits, zero-benefit grants, invalid
state and oversized quantities. Definitions retain model, weapon, SP count and
no_pickup. Next wire live models, proximity/occlusion, grant and disappearance;
no in-game pickup collection claim yet. See docs/LEVEL-ITEMS.md.

Authored pickup records now load through a bounded shared C reader/owner.
All63 campaign levels scan exactly:593 pickups, peak28824 retained bytes.
Truncation, budget failure, repeat close and source-archive closure checks pass;
both builds and28 CTests pass. L1S1 has authored Handgun pickups with16 rounds.
Next bind items.tbl classes/models and collect them into the live inventory;
no live pickup objects or native residency validation yet. See docs/LEVEL-ITEMS.md.

Player combat now owns a448-byte weapon inventory and consumes finite reserve
on reload. Starting/respawn supply is provisionally a full16-round pistol plus
the authored125-round reserve capacity. Partial reloads transfer only what is
available; empty reserve prevents reload/shot audio. Reserve is shown on HUD.
PC6000-frame exhaustion PASS:141 shots,125 rounds transferred over8 reloads,
34 later dry-fire presses with no additional audio. Six rule/conservation
cases and27 CTests pass. Stock64MiB XEMU660-frame PASS:18 shots, one reload,
14 loaded/109 reserve, matching PC HUD/state;6936 available pages. Both builds
and player death/respawn checks pass. Evidence:
artifacts/xemu/replay-20260914-021130/report.json. Pickup replenishment and
mission-specific starting grants remain open.

Live pistol combat now uses the authored16-round magazine,40 SP base damage,
66-tick reload and30-tick primary cooldown. Semi-automatic input fires only on
a fresh press; presses during cooldown/reload are consumed rather than queued.
Reload completion, respawn ammo and HUD use the same definition. Damage type
is now authored bullet rather than prototype bash. The24-byte definition uses
the existing NXDK-safe parser and is loaded once; pickups remain open. Six PC rule cases,27 CTests and kill/death regressions pass.
Stock64MiB XEMU127-frame reload-boundary PASS:16 rounds restored, bullet
damage selected, HUD/state match PC,6936 available pages. Both builds and
player death/respawn regressions pass. Evidence:
artifacts/xemu/replay-20260914-020444/report.json.

First-person pistol resource owner is implemented in shared C:40 bones,847
vertices,8 materials and resident idle/fire/reload clips. PC resident934656
bytes, conservative peak946252 bytes within1MiB, including live pose buffers.
Idle/fire/reload playback now produces skinning matrices without frame-time
allocation or archive I/O and returns completed actions to idle. Tests cover
closed archives, pose changes, reload completion,100 action restarts and budget
cleanup. Live scene integration now renders the gun and drives shot/reload
animation from gameplay; PC idle/fire/reload/return captures pass. Camera
placement, depth band and lighting are provisional. Stock64MiB XEMU80-frame
reload PASS: pose hash,1560 emitted vertices and weapon budget match PC;
6937 available pages at completion. Native framebuffer inspected.
Both builds,27 CTests,four weapon cases and death/respawn regressions pass.
Evidence artifacts/xemu/replay-20260914-015216/report.json. Muzzle flash,
other weapons and authored tuning remain open.
See include/rf/player_weapon.h and tests/player_weapon_tests.c.

First-pass player pistol audio now emits authored Glock Launch and Glock
Reload groups through the existing local-player mixer/device path. Requests
occur on successful shot or reload start, including automatic reload; holding
reload does not retrigger it. PC traces contain HandGun_Fire01.wav and
FP_glock_reload.wav, with2 PCM loads totaling87548 bytes within the existing
1MiB bank cap. Missing audio/voice exhaustion records errors without halting
gameplay. Separate first-pass sound RNG and diagnostics add40 bytes. Both
builds,26 CTests and five recorded audio cases pass (including silent full-clip
reload). Stock64MiB XEMU180-frame audio-device PASS: shot and reload voices
match PC, no device errors and nonzero DSP DMA output. See
artifacts/xemu/replay-20260914-013131/report.json; physical audibility not tested.
Evidence artifacts/weapon-audio/report.json and captured WAV files.

Stock64MiB XEMU360-frame combat regression PASS after the surface-ray and
friendliness changes:22 shots,8 hits,1 recorded kill;8 return hits,health61.6.
State/HUD match PC;7115 available pages at completion. Evidence:
artifacts/xemu/replay-20260914-011423/report.json and framebuffer.png.
Historical visual failure: that capture showed the killed guard upright.
The PC fix stops looping locomotion once at death, uses the retained death
motion setup and skips the standing controller for dead NPCs. Generic death
now completes and holds its final grounded pose at360/480 frames. Both builds
and26 CTests pass. Stock64MiB XEMU360-frame PASS with inspected grounded
body and PC-matching frozen death state.7115 available pages at completion.
Evidence artifacts/xemu/replay-20260914-012508/report.json and framebuffer.png.
Evidence artifacts/combat-death/report.json; death audio remains deferred.
This replay does not prove door-gap shots or authored allegiance transitions.

Next weapon integration: the installed12mm handgun uses fp_glock.v3c(52232
bytes), idle/fire/reload RFA clips(5880/5240/8712 bytes), Glock Launch and
Glock Reload foley groups. Authored .v3d/.mvf names are logical labels, not
missing assets. Table values are16 rounds,1.1s reload,0.5s primary fire wait
and40 SP base damage; current12-round prototype tuning remains provisional.
Asset inventory evidence: artifacts/player-weapon-assets.json. No player gun
rendering or audible weapon feedback is claimed by this inventory audit.

Player hitscan now selects the nearest NPC bounds, then checks actual static/
moving collision faces along the segment to that target. Mover AABBs no longer
act as solid shot blockers. Shared scene tests cover empty bounds corners,
panel obstruction, target-before-panel and a moved panel. Both builds,26 CTests
and PC combat/kill/death replays pass. Native door-shot validation remains open.

Set_Friendliness(type30) now dispatches immediate and delayed mission events
through resolved live actor links. The scene updates player/NPC affiliation
and clears NPC combat alerts so scripts can stop an ongoing attack. Missing
and stale objects are skipped; non-entity support remains open. Shared-scene
integration tests cover duplicate/stale links, missing backend, delay and
hostile/friendly transitions. Both builds and26 CTests pass; authored mission
sequence validation in XEMU remains open. Adds8 bytes to the trigger owner.

First-pass enemy awareness now lets visible armed hostile actors acquire the
player without being shot, using staggered30-frame range/cone/geometry checks.
Neutral/friendly actors require provocation. PC240-frame hostile encounter:
2 guards,8 hits,health61.6; neutral miner area stays passive at100 health.
Adds32 global diagnostic bytes, no per-NPC growth or perception allocation.
Both builds,26 CTests and PC death/respawn/travel regression pass.
Stock64MiB XEMU240-frame PASS:2 acquired guards,8 hits,health61.6;
awareness/combat state and HUD match PC.7115 available pages at completion.
Evidence artifacts/xemu/replay-20260914-010332/report.json.
See docs/ENEMY-AI.md; pursuit and weapon behavior remain open.

First-pass player recovery now blocks controls after fatal damage and shows
a death/respawn prompt. After60 frames, fresh E/X restores the starting player
body, pose, health/armor and ammunition, clearing reactive enemy alerts.
World/NPC/mission state persists: full level restart/checkpoints remain open.
A1132-byte retained snapshot avoids respawn allocations. PC tests cover death,
held-Use rejection, health/ammo restoration and resumed firing; movement input
was initially accepted without travel: the snapshot preceded speed/pose
initialization. Capture now follows initialization; the strengthened PC test
requires real displacement and measures about5.2 horizontal units after respawn. Both builds and26 CTests pass. Stock64MiB XEMU1500-frame lifecycle
PASS: one death, one respawn, resumed firing and matching PC state/HUD.
Evidence artifacts/xemu/replay-20260914-003751/report.json;7115 available
pages at completion (not a peak memory measurement).
Evidence artifacts/player-life/report.json; docs/DEATH-LIFECYCLE.md.
Corrected snapshot native replay PASS: artifacts/xemu/replay-20260914-004948/
report.json,1500 frames with PC-matching body/lifecycle/combat and15 HUD pixels;
7115 available pages at completion. Prior no-travel limitation is resolved.

First-pass armed retaliation now uses8 retained bytes per NPC (624 for78):
a damaging hit alerts an armed visible actor; after30 frames it attacks every
60 frames within40 units, checking the existing moving/static geometry ray
path. Hidden/dead/unarmed actors do not attack. Damage goes through retained
player health/armor and flash handling; the HUD adds a health bar. Player fire
and further enemy attacks stop at zero health. PC240-frame provoke:3 hits,
health85.6; killing the attacker stops at1 return hit;1500-frame sustained
retaliation reaches zero after20 hits. Idle stayed passive before the awareness pass documented above. Both builds and26
tests pass. Evidence artifacts/enemy-first/report.json. Stock64MiB XEMU
240-frame PASS with matching3 return hits/health85.6 and inspected HUD:
artifacts/xemu/replay-20260914-002734/report.json, framebuffer.png and
health-pixels.json.7115 available pages observed at completion, not a peak.
This earlier retaliation-only pass is extended by awareness above; pursuit
and squad behavior remain open.
Full level restart/checkpoints, firing animation/audio, faction handling and
weapon tuning are open; first-pass in-place respawn is described above. Attempted walking cover routes remained visible/in range; no verified
cover-transition claim. Ray obstruction uses the existing geometry path.

Shared combat HUD now renders a centered reticle, green damaging-hit feedback,
12 remaining-round markers and a reload progress bar through both renderers.
Draw-only, stack vertices, no retained allocation. PC native pixel checks cover
ready/hit/reload states. Image inspection exposed that the previous combat
target9858 is hidden; shots now exclude object flags2/0x4000. The old counter
evidence did not demonstrate hitting a visible actor. Tests now use visible
guard8456 (22 shots/8 hits/1 death at360 frames), and hidden9858 explicitly
produces zero hits. Both builds and26 tests pass. Evidence:
artifacts/combat-hud/report.json and artifacts/combat-first/report.json.
Initial native capture001553 passed game-state checks but contained no HUD:
the shared world-depth bias clipped screen overlays beyond the GPU far plane.
Xbox solid screen overlays now use direct depth, also fixing scene damage
flash routing. Native capture harness now checks actual HUD pixels against PC,
so submission/state success cannot mask missing overlays. Retest PASS:
artifacts/xemu/replay-20260914-001847/report.json and framebuffer.png.
Stock64MiB,34 frames,1 shot/1 hit,11 rounds;13 HUD pixels match PC within
2/channel. Native capture visually inspected and added to README.
Next prioritize enemy combat behavior; original HUD styling stays deferred.

Playable-first combat now connects RT (PC F) and Y reload (PC R) to a shared
hitscan prototype:12-round clip, unlimited reserve,12-tick cooldown,72-tick
reload,25 base damage and100-unit range. Static world geometry and conservative
live mover bounds obstruct shots; nearest living NPC body bounds select hits.
Uses retained damage dispatch and generic death entry/animation. PC staged
miner9858:360 frames,22 shots,8 hits,one death; manual reload180 frames gives
7 shots/7 hits; idle produces none. Both builds and26 tests pass. RFI4 size40
adds held fire/reload while retaining old24/28/32-byte replay formats.
Evidence artifacts/combat-first/report.json. Stock64MiB XEMU360 frames PASS:
artifacts/xemu/replay-20260914-000617/report.json;22 shots/8 hits/1 death/2
rounds left exactly match PC.7180 available pages at completion is an endpoint,
not a peak guarantee. Legacy390-frame raw input walk still passes with no shots.
This is new first-pass policy, not1:1 weapon reconstruction. Weapon inventory/
tuning, first-person gun presentation, shot/impact audio, precise
mover/actor hit geometry, clutter obstruction and enemy return fire remain. Damage-side peripheral
notifications and special effects are not yet bound; bullet health/death is.

Walking failure fixed: actor_follow_view now passes only the retained world
prefix into world regeneration; the previous frame's appended NPC geometry
is excluded from the world-only budget check. Frame291 previously submitted
2132760 old bytes against a2097152 world budget in the3MiB shared mesh.
PC now completes30/390/780/1170 frames with unchanged3MiB mesh cap and
forward/back/forward movement. Both builds and26 tests pass. Ambient smoke
checks retain voice/load counters and stable bank sizes across return trips;
stale global-RNG hash and unrelated Foley-byte golden values were removed.
Evidence artifacts/ambient-walk-check/restart-report.json. Stock64MiB XEMU
passes390 frames with PC comparison: artifacts/xemu/replay-20260913-235513/report.json.
No allocation increase. Next prioritize playable combat integration.

USER priority change: shelve further lighting work and return to it later.
Lighting remains incomplete: lifecycle event routing, alternate/moving solids,
queued regeneration across visibility changes, directional/ray paths and
rendered changing-light/PS2 parity validation are open. Preserve current work.
Immediate focus is the repeatable frame291 walking failure, followed by
campaign traversal/physics; diagnose its actual origin before assigning blame.
Reproduced on current1497019 PC build with tools/replay_ambient_walk.py:
30 frames pass;390-frame forward walk exits at291, RF_RANGE, view stage1.
Evidence artifacts/ambient-walk-check/pc-390.txt. Failure is in the camera/
view preparation interval; physics is not yet proven to be the failing owner.

Scene clock visibility now uses live pool source position/end/radius instead
of reconstructing the authored placement. Shared source-volume preparation
preserves the already-biased segment radius and takes retained authored cone
angle metadata. Original/PC/compiledNXDK match384 moved point/cone/segment
cases (actual4d91d0), plus4096 world and4096 transformed-view cases; five
new invalid-source guards preserve outputs. Both builds and26 tests pass.
Evidence artifacts/light-volume.json and light-volume-view.json. Release/move
event routing, alternate solids and rendered changing-light validation remain
open; this change establishes their current-source geometry path.

Light release now exposes original4d9130 visibility notification before unlink,
after reference decrement and generation advance, retaining the live source,
list and active cache during the callback. Class0 notifies only for update
low byte1; other classes notify on final release regardless of update value.
Original/PC/compiledNXDK match105 cases including48 callbacks; the existing
1024-operation/2048-cache oracle, both builds and26 tests pass. Evidence
artifacts/light-release-notify.json. Callback is synchronous and must not
mutate/reenter its pool; existing release keeps its no-callback behavior.
Corrupt INT32_MIN references are rejected before mutation. This does not yet
bind scene release/move notifications or demonstrate changing rendered light.
Original4d93d0 base-color changes advance the selection-cache generation only;
do not infer base dirty2 from every generation or authored clock change.
Next connect explicit lifecycle invalidation to visibility/scene ownership.

Scene base/projected-shadow regeneration now uses renderer room visibility
and cached mapping-room IDs; offscreen jobs retain all dirty bits without
mapping decode or stale uploads. Stock64MiB XEMU passes180 frames, matching
PC330 completed jobs/36 ambient/333 callbacks/RGB hash820094536, with5521
pending requests: all5851 accounted for. Final7197 available pages (~28MiB)
is an observed endpoint, not a peak guarantee. Regeneration workspace771272
bytes excludes RGB1130876, overlay88000 and allocator overhead. Both builds
and26 tests pass. PC updates lighting even when replay drawing is skipped.
Native evidence artifacts/xemu/replay-20260913-232705/report.json.
Fixed a stale material-array binding exposed by native testing after scene
setup combined actor/clutter records. The full-level --lightmap-all stress
mode remains separate; previous five-minute run reached4670 jobs before its
first draw, so visibility demand is essential. Stage/counters persist on
failure and are sampled every10s in regeneration tests. Initial-world face
order, projected mode1 and authored shadow modes are bound; directional
source/64-source diagnostic, moving-solid/GeoMod routing and ray mode remain
open. No PS2 parity or changing-source visual claim. Next validate queued
jobs across changing visibility and bind actual base-light invalidation.

Additional PC movement check: the existing ambient forward-walk replay fails
at frame291 with RF_RANGE, scene stage1, both with regeneration enabled and
disabled. Track under traversal/physics. A shorter270-frame walk and camera
rotation keep the same330 visible mappings. Staged lower-door1/180-frame
replays both pass with715 completed/5136 pending mappings, but also retain
the same visible-room set; they do not prove resume after a visibility change.
Evidence artifacts/lightmap-walk-baseline.txt, lightmap-demand-walk-270.txt
and lightmap-demand-door-180.txt.

Shadow workspace now reuses one allocation across jobs, resetting active
mask strides/guard rows while preserving geometry scratch and inactive masks.
Live Mines retains141952 PC bytes;5851 cached/uncached jobs and64 reopened
jobs agree. Retention changes473 mask hashes and66 packed regions versus
fresh allocation. Investigation confirms original409f90 is MOV EAX,ECX;RET,
not a clearing constructor:1024 original/PC/NXDK filter passes match with
nonzero carried scratch, including full scratch contents. Fresh zeroed jobs
are therefore not an oracle for persistent scheduling. Full-game stack reuse
and call ordering remain unverified. Compiled NXDK reset/isolation/capacity
guards, both builds and26 tests pass. Next bind retained shading/shadow jobs
to scene dirty2/4 dispatch and validate actual changing lighting in XEMU.
Evidence artifacts/lightmap-shadow-filter-warm.json, geometry-shadow-storage.json
and campaign-shadow-l1s1.json; no new rendered-parity claim.

Retained shading workspace now keeps three accumulation planes and special
polygon/vertex/normal scratch in one budgeted allocation. Live Mines reuses
16196 PC bytes across5851 mappings; all previous mask/RGB/packed hashes and
counters remain identical, including cached/uncached and64 reopened jobs.
Compiled NXDK validates64 capacity combinations, disjoint arrays, zeroed
scratch, exact/short budgets, allocation failure, overflow and repeated close.
Both builds and26 tests pass. This removes per-mapping shading allocations;
fixture reference buffers and shadow storage still allocate separately. Next
retain shadow workspace across jobs and bind complete dirty2/4 scene work.
No new rendered/native full-scene memory claim. Evidence
artifacts/geometry-lightmap-storage.json and campaign-shadow-l1s1.json.

Shared base regeneration now writes retained RGB at atlas offsets, composing
ambient fill or seeded ordinary/special masked accumulation and RGB resolve
without allocation. All5851 Live Mines mappings match the prior temporary-
buffer route byte-for-byte, including untouched atlas neighbors; the fixture
uploads these retained colors and still matches packed1555 results. Cached
and uncached jobs agree. Compiled NXDK passes256 composed rectangles and7
guards; both builds and25 tests pass. These are integration checks against
previously verified stages, not new original-scene or XEMU rendering proof.
Next bind retained mask/channel/polygon workspace and this worker into live
dirty2/4 scene scheduling; source overflow and ray shadows remain open.
Evidence artifacts/lightmap-regenerate-rgb.json and campaign-shadow-l1s1.json.

Scene frame sinks now bind retained base RGB and allocation-free dirty1/8
lightmap uploads on PC and Xbox, waiting for prior GPU reads on Xbox.
Stock64MiB XEMU completed180 Live Mines frames:5851 initial uploads,
no repeated uploads, no deferred base requests and zero update errors;
PC final-frame replay agrees on upload totals. Retained RGB1130876 bytes
plus overlay scratch88000 bytes excludes allocator overhead. Both builds
and25 tests pass. Current authored lights are class0, so this run does
not exercise changing live overlays or establish visual lighting parity.
Dirty2/4 base/shadow requests remain queued until their worker is bound;
next integrate base regeneration, then test changing rendered lighting.
Evidence artifacts/xemu/replay-20260913-222748/report.json.

Live-light overlay now reads retained linear baseRGB and writes existing
packed images directly, with native Xbox swizzle and no packed staging copy.
All256 original/PC/NXDK rectangles (5184 shaded pixels,1..4 mixed sources)
match complete resident images including untouched texels; baseRGB remains
unchanged. Four image guards, prior four rectangle guards, both builds and25
tests pass. This is the locked computation, not live scheduling: caller still
selects class lights, synchronizes GPU use and resets final dirty state. Next
connect retained base/overlay owners to scene mapping dispatch and XEMU timing.

Retained raw RGB owner preserves original section1200 bytes needed by the
initial dirty1/live-light path4f2cfe, avoiding lossy reconstruction from1555.
Verified1515 images across94 PC levels and all23 Live Mines images byte-for-
byte in compiled NXDK. Exact/short budgets, late cleanup and repeated close
pass; both builds and25 tests pass. Live Mines RGB owner1130876 bytes;
peak tested PC owner2753420 bytes, excluding allocator overhead. Native full-
scene memory not yet measured. Dirty1 class-light composition requires these
base colors even when dirty2/4 shadow regeneration is skipped; next connect
retained RGB, computed base updates and class-light overlay to scene scheduling.
Evidence artifacts/lightmap-rgb-owner.json.

Native stock64MiB XEMU now renders one packed lightmap allocation before and
after a16-texel rectangle update. All64 framebuffer samples match PC bilinear
rendering within2 channel levels, including48 unmodified texels. Prior draw
completion via pb_busy, CPU sfence and normal texture rebinding suffice in this
XEMU test; hardware cache behavior is not proven. Initial edge check corrected
to account for bilinear footprints rather than loosening tolerance. Full native
pixel suite and25 PC tests pass; build succeeds. Evidence:
artifacts/xemu/particle-pixels-20260913-221410/report.json.
Next schedule real campaign lighting work and uploads at the verified frame
boundary, measure full-scene memory/timing and inspect meaningful rendered output.

Computed RGB now writes existing packed lightmap image rectangles without a
second texture allocation:linear PC/swizzled Xbox. All5851 Live Mines regions
compare each written PC texel against the linear1555 conversion, including
ambient-only regions.1024 compiled NXDK cases plus4 image/extent guards verify
full backing images, untouched neighbors and dirty/lock behavior;225 rejected
cases preserve image/state. Both builds and25 tests pass. Writes require caller
GPU-read completion and texture-cache invalidation, still unbound/unverified
natively. Next schedule these updates in the live renderer and measure XEMU
memory/timing before claiming rendered lighting parity.

Authored ambient binding now reads v180 section900 (4618b0) and optional
room colors after liquid payloads (4ed520). Live Mines globalRGB96/96/96 and
room overrides feed all5851 mapping results, including446 ambient-only regions.
Cached/uncached hashes/counters agree;1024 level and1024 room records plus2048
truncation guards pass in compiled NXDK;both builds and25 tests pass. Zero-source
regions use original direct ambient fill. Directional switch1 is retained and
explicitly rejected by this fixture until its source is bound; original other
values select ambient. Next retain image/RGB owners and submit dirty/swizzled
updates in live rendering with measured stock64MiB memory.

Campaign fixture now carries masks through actual ordinary/special lighting,
RGB resolve and linear1555 packing:1521 ordinary and3884 special lit mappings.
Shadows alter49626 RGB bytes in782 regions;769 retain differences after1555
packing. Cached/uncached hashes and counters agree for5851 mappings;64 fresh
jobs repeat and25 tests pass. Zero ambient and directional scale1 are explicit
fixture inputs, not authored ambient reconstruction. No GPU submission/XEMU
claim; peak shadow scratch excludes new lighting scratch and adjacency.
Next bind campaign ambient, retained RGB/image owners and dirty upload/swizzle
in live rendering; preserve zero-source handling and stock64MiB accounting.

Retained shadow-face snapshots preserve all5851 Live Mines mask hashes and
counters while cutting this host's full-level fixture from17.78s to8.78s.
Snapshot storage is415408 bytes plus existing73252-byte peak shadow scratch;
not total/native scene memory. Compiled NXDK cached path matches256 original
composition cases; count guard, both builds and25 tests pass. Evidence corrects
prior room-routing assumption:4f4efe reads solid+70 and4f5520 follows face+54
for occluders; room routing selects receivers only. Cache snapshots must refresh
on topology/plane/position/flag changes. Next retain jobs/dirty scheduling and
compose masked accumulation/RGB upload, then rendered XEMU validation.

Real-asset PC shadow harness completes all5851 Live Mines mappings using
loaded texture formats, authored light selection/modes, receiver groups and
actual projected callbacks:7537 callbacks,17804 accepted occluders and169812
changed mask bytes;64 reopened jobs reproduce exactly. Peak PC shadow scratch
73252 bytes (not total scene memory);25 tests pass. Full initial world-face
scan visits51.4M faces in18s on this host, so this is an integration fixture,
not a viable per-frame policy. No renderer submission or XEMU evidence yet.
Next preserve original room/surviving-face routing and retained mapping jobs,
compose masked accumulation/RGB upload, then measure rendered native memory.
Harness: tools/verify_campaign_shadows.py; evidence artifacts/campaign-shadow-l1s1.json.

Selected authored pool IDs now resolve to retained constructor shadow modes
in selection order without extra persistent storage. PC/NXDK validate22293
lights across93 level records, reversed selection, in-place output and short
capacity rollback. Live Mines modes:156 disabled,75 enabled. Both builds and
25 tests pass. Snapshot binding assumes authored IDs are not released/reused
and shadow settings are not externally changed; dynamic source mode ownership
still pending. Next connect selected receiver jobs and masks to campaign draw,
then verify native XEMU memory and lighting rather than data-only comparisons.

Dirty shadow-mask dispatch matches2048 original/PC/NXDK schedules across
all dirty bytes, renderer modes0..7 and source modes0..3, including1910
accumulation requests. Four compiled NXDK guards and25 tests pass; both
builds succeed. Logical mask resets preserve guard/padding. Deterministic
callbacks replace projected/ray bodies in this scheduling comparison; this
is not native XEMU rendering evidence. Next bind live campaign source modes,
receiver jobs and retained storage; reconstruct ray-test body4f4280 and
measure live memory/rendering.

Bounded shadow storage owns receivers, face/clip/UV scratch and1..63 masks
in one allocation, with zero scratch and255 masks/private guard rows. Exact/
short budgets, overflow, isolation and repeated close pass; compiled NXDK
layout peaks270408 bytes for tested63x64x64 masks and128-vertex clip scratch
(using allocator hooks, not native heap). PC per-source256 comparisons now
use this owner unchanged. Both builds and25 tests pass. Next bind live
campaign jobs/dirty source modes and measure native XEMU memory/rendering.

Complete per-source projected shadow operation now matches256 original/
PC/NXDK compositions:384 endpoint passes,75 back-facing clears,2472 face
visits,257 accepted occluders and28865 changed mask bytes. Three NXDK guards,
both builds and24 tests pass. Original source/preparation/traversal/border
chunks form oracle; back-facing clear modeled as width*height zero bytes.
Synthetic geometry, caller-seeded masks and initialized retained scratch;
source lists/dirty modes and campaign ownership still external. Next own
receivers, masks and scratch under budget, bind live campaign and verify XEMU.

Shadow source sampling matches2048 original/PC/NXDK records, including
1170 line sources and838 endpoints whose float interpolation differs from a
direct endpoint copy. World/cached-local selection, bounds,1x255 versus2x127
passes and3 guards verified. Both builds and24 tests pass. Selected cached
coordinates are supplied, not transformed by this adapter. Next compose
source samples with preparation/traversal/final border and explicit receiver,
mask/scratch lifetime, then native campaign rendering and memory validation.

Campaign material-to-shadow image binding borrows geometry-local pointers
from existing shared slots, with no pixel read/load/allocation. Verified5919
references across68 PC level fixtures plus28 loaded Live Mines references
(17 unique images), and256 compiled NXDK tables/2048 entries with3 guards.
Both builds and24 tests pass. Missing slots are explicit NULL; other load
errors propagate, preserving output. Current retained images only: no new
animation clock/override policy. Next connect these bindings to source passes,
receiver/mask/scratch lifetime, then native rendered/memory verification.

Retained shadow traversal matches256 original/PC/NXDK passes over2048
serialized synthetic faces:194 eligible/projected,129 filter-accepted,4750
changed mask bytes;3 NXDK guards. Actual original texture classification,
linked-corner decoding and per-face cull/clip/project/filter chunks are replayed
in supplied file order, retaining mask and scratch between faces. No allocation.
Both builds and24 tests pass. Next bind campaign texture-frame tables, source
space/endpoint selection, receiver/mask/scratch lifetime and final border;
then run native memory/rendering validation on representative gameplay.

Collapsed shadow-volume edges now retain original x87 indefinite planes;
culling and clipping treat that exact four-word encoding as non-restricting.
Verified1024 preparations including two-texel extents,2048 volumes (512
collapsed-edge cases),4096 occluders,2048 clips and512 combined passes against
original/PC/NXDK. Both builds and24 tests pass. Other malformed/nonfinite
inputs still reject. Next bind source space/endpoint selection and retained
occluder traversal with bounded mask/scratch ownership, then native rendering.

Combined shadow cull/pass preparation matches1024 original/PC/NXDK
cases with extents3..64:494 facing,530 rejected,4 preserved-output guards.
Original corner/facing/mapping-bound expansion/volume are replayed; source
center/radius bounds independently checked. Both builds,24 tests and pass/
volume regressions pass. Two-texel collapsed-volume gap subsequently resolved
by exact indefinite-plane handling above. Next bind source space/endpoint
selection, retained
occluder iteration and bounded mask/scratch ownership, and validate native.

Combined occluder clipping/projection/filter/mask pass matches512 original/
PC/NXDK cases:294 reach projection,251 pass filtering,7424 changed mask bytes.
Thirteen collapsed projections expose original filtering even below3 vertices;
retain initialized UV/filter tails and preserve projection-vs-acceptance flags.
Two shared guards plus NXDK scratch guard, both builds,24 tests and prior
projection/filter/2D clipping regressions pass. Next connect source/mapping
preparation and retained face traversal, apply border once per source, own
bounded masks/scratch, and verify native64MiB memory/rendering.

Shadow-mask border finalization matches1024 original/PC/NXDK masks,
including79 two-pixel extents and205 inactive passes;4 guards preserve data.
Original4f5588 copies sides first, then interleaves top/bottom writes. Gate is
reached-projection, not successful raster acceptance. Both builds and24 tests
pass. Next compose full source/occluder traversal with explicit retained
scratch and mask lifetime, then native memory/rendered validation.

Resident lightmap image binding now passes5851 compiled NXDK mappings
and all5851 PC receiver groups/28356 UV vertices using loaded L1S1 images.
Serialized image section confirms128x128 dimensions. Three binding guards
preserve both outputs, and out-of-range image IDs fall back to image zero.
Both builds and24 tests pass. No pixel allocation/read in the binding itself;
receiver probe owns loaded images. Next compose bounded masks/scratch and
source/receiver/occluder traversal, then verify native rendering and64MiB.

Retained receiver grouping passes5851 L1S1 PC mapping groups/28356 UV
vertices and35 compiled NXDK groups, plus3 NXDK guards and an empty query.
Peak polygon/UV scratch1352 bytes (32-bit layout); no allocation or normal
owner needed. Both builds and24 tests pass. Supplied128x128 image metadata,
static surviving IDs and per-mapping scans remain explicit limitations; this
is not native rendered verification. Next bind actual image dimensions and
mask/scratch lifetime, then compose the complete retained shadow pass.

Retained shadow receiver conversion matches7302 L1S1 original/PC/NXDK
faces and28356 UV vertices, with7302 PC capacity guards and2 NXDK guards.
No allocation; actual serialized origins/UVs tested with supplied128x128 image
metadata. Both builds and24 tests pass. Next group surviving receiver faces
by mapping, bind actual image dimensions and bounded mask/scratch ownership,
then compose the retained shadow pass and verify native memory/rendering.

Retained shadow face snapshots match all7418 L1S1 original/PC/NXDK faces,
reusing collision bounds without allocation; largest face scratch192 bytes.
Original supplied-plane4dfe20 rejected0 L1S1 faces.7418 PC capacity guards,
2 NXDK guards, both builds and24 tests pass. Caller still supplies surviving
IDs and later runtime mutations. Next compose source/mapping preparation,
retained occluder/receiver traversal and bounded mask ownership, then native
memory/rendered verification.

Retained image format now drives shadow texture exclusion via the existing
format classifier.4096 original/PC/NXDK face cases pass through actual static
bitmap lookup/classification (1266 accepted,2830 rejected), without supplying
510710 results;5 guards, both builds and24 tests pass. NULL image means no
bitmap; caller must select/load the correct animated frame. Next bind retained
face decoding, receiver/mask ownership and native scene updates.

Shadow occluder selection now matches4096 original/PC/NXDK faces (1323
accepted,2773 rejected), with5 guards. Preserve strict bounds overlap,
signed16 fields, flag/texture exclusions and mixed coplanar/volume thresholds.
Both builds and24 tests pass. Texture classification was supplied to the
original replay; bind retained face decoding and actual texture ownership,
then compose full masks and verify native memory/rendering.

Combined shadow receiver clipping/area/filter/raster matches1024 original/
PC/NXDK passes (771 accepted,253 rejected,54219 changed mask bytes). Both
builds,24 tests and standalone2048 clipping cases pass. Preserve549f10's
aliased output quirk: original count changes without final vertex copy, so
area reads retained subject/tail. Harness supplies zero-initialized scratch;
live scratch ownership must be explicit. Reject empty receiver sets because
the original path references uninitialized raster vertices. Next bind occluder
selection and receiver/mask owners, then native rendered verification.

Projected2D polygon clipping matches2048 original/PC/NXDK cases (1521
clipped,437 unchanged,90 empty), including winding and epsilon boundaries.
Five shared guards plus one NXDK scratch-capacity guard pass; both builds
and24 tests pass. Caller-owned bounded scratch replaces original globals.
Next compose polygon intersection/area acceptance and occluder selection,
then mask resource ownership and native rendered updates.

Projected shadow polygon area now matches4096 original/PC/NXDK cases,
including513 late-degenerate whole-polygon zero results and3 guards. Both
builds and24 tests pass. Preserve triangle-fan ordering, Heron product order,
mixed side-length precision and early rejection. Next reconstruct2D polygon
clipping/filter thresholds and bind occluder/mask ownership before native
rendered integration; area tests alone do not validate complete filtering.

Mapping-driven shadow corners/center/facing now match2048 original/PC/NXDK
cases (1016 facing,1032 rejected), including all axes,2..64 extents and6 guards.
Preserve retained inverse U versus stored inverse V in this path. Both builds
and24 tests pass. Next compose retained source/mapping preparation with volume
construction and occluder selection, then projected filtering and mask ownership;
these calculations have not yet driven native rendered shadows.

Six-plane shadow volume construction now matches2048 original/PC/NXDK
volumes (12288 planes), including orientation and final receiver replacement,
with4 guards. Reuses verified visibility plane math; both builds and24 tests
pass. Next bind UV corner/facing preparation, occluder selection and projected
polygon filtering before full mask ownership/native rendered updates.

Combined shadow polygon ray construction/intersection/projection/deduplication
matches1024 original/PC/NXDK polygons (797 accepted,227 rejected,2595 removed
duplicates), with4 guards and exact partial scratch on ray rejection. Both
builds and24 tests pass. Next reconstruct six-plane volume construction and
projected polygon area/filtering, then bind bounded mask resources and verify
native rendered updates. Supplied clipped vertices are not live scene binding.

Shadow ray/plane intersection matches4096 unhooked original/PC/NXDK rays:
1788 hits,1024 parallel,1284 behind-origin and512 explicit beyond-segment
hits, plus4 guards. Both builds and24 tests pass. Preserve point writes for
negative parameters and unchanged points for parallel rays. Next compose
clipping, ray construction/intersection, projection and polygon filtering into
the shadow-mask pass; native rendered verification remains open.

Shadow polygon/plane clipping matches2048 unhooked original/PC/NXDK cases,
with35103 emitted vertices (15741 intersections), epsilon-boundary cases,
123 empty results and4 guards. Both builds and24 tests pass. Caller supplies
disjoint bounded buffers; no allocation. Next compose the six-plane shadow
volume, ray-plane intersections and projected polygon filtering before live
mask/resource integration and native XEMU rendered verification.

Shadow intersection-to-mask projection matches4096 original/PC/NXDK points,
including asymmetric U/V rounding, both edge clamps and8 input guards.
Both builds and24 tests pass. Original mask bank has64 slots at4096-byte
spacing;4f4590 clamps projected coordinates to [1,width-1]/[1,height-1]
before rasterization. Keep the standalone filler padding contract for arbitrary
polygons; live projection requires extents >=2. Next bind ray-plane intersection,
clipping, duplicate-vertex removal, source modes and bounded mask ownership.

Shadow polygon rasterization now matches2048 unhooked original/PC/NXDK
cases (99066 changed bytes, including7119 guard-row bytes), with6 guards.
Both builds and24 tests pass. Preserve inclusive right/bottom writes and
wrapping byte subtraction with explicit width*(height+1)+1 capacity; do not
bind this routine to a tightly allocated width*height mask. Next reconstruct
projection/clipping and resolve live mask allocation/ownership before native
rendered verification. Original caller bounds and bank are traced above.

Shadow UV-to-plane reconstruction now matches4096 unhooked original/PC/NXDK
points across all six axis orders, with8 invalid-input guards. Both builds
and24 tests pass. Direct division preserves the original shadow rounding;
ordinary texel sampling retains its distinct reciprocal path. Next reconstruct
mask clipping/rasterization and bind source shadow modes/resources; compiled
NXDK CPU replay is not native XEMU rendered verification.

Complete class-light rectangle sampling/shading/packing matches256 original/
PC/NXDK rectangles (5184 pixels), including dirty8 clear and retained base RGB.
Both builds and24 tests pass; connect selection, texture locking/swizzling,
mask ownership and full dirty dispatch before native rendered verification.

Class-light shading plus base-RGB/1555 addition matches2048 original/PC/NXDK
pixels; both builds and24 tests pass. Keep the base-RGB and direct packed
class-light passes distinct when composing source selection and live uploads.

Zero-light ambient RGB fill now matches512 original/PC/NXDK rectangles,
including128 scaling, byte wrapping and dirty8. Both builds and24 tests pass;
bind ambient/source/mask/RGB owners and the full update dispatch next.

Ambient initialization now matches512 original/PC/NXDK grids, including room
overrides and global fallback; both builds and24 tests pass. Bind retained
ambient/RGB/source/mask owners and zero-light direct fill before live updates.

Mapping polygon grouping now matches5851 L1S1 PC groups and35 compiled-NXDK
groups, with4736 bytes peak polygon/vertex scratch plus normal work. Both builds
and24 tests pass. Bind initial RGB/mask/source ownership and verify native
lighting residency/updates after loader survival/plane repair is resolved.

Retained corner binding now matches28356 L1S1 original/PC/NXDK lightmapped
corners (UV, position, smoothed normal); both builds and24 tests pass. Group
selected mapping polygons with bounded scratch, bind live RGB/mask resources,
and verify native residency/updates after loader face acceptance is resolved.

Bounded adjacency ownership now matches1922603 ordered links across94 PC
levels; L1S1 uses143096 bytes. Subset/duplicate-corner/budget checks, both builds
and24 tests pass. Bind loader-surviving faces and corner-normal sampling, then
verify the owner and lighting updates natively within64MiB.

Original vertex adjacency suppresses duplicate faces and preserves first-insertion
order, verified over66560 insertions. Static inventory estimates143076 bytes
for L1S1 adjacency; implement the bounded owner, preserving loader rejection/
removal semantics before binding smoothed corner normals to sampling.

Retained-geometry mapping access now decodes on demand without a duplicate
table;301871 PC records across94 levels and2048 NXDK/original cases pass.
Both builds and24 tests pass. Bind selected face/corner-normal ownership and
RGB/mask resources before driving native rendered updates.

Combined special-grid coverage, sampling, lighting, fallbacks and border copies
match256 original/PC/NXDK grids (6237 texels), exercising every sample outcome.
Both builds and24 tests pass. Bind retained face/corner-normal owners, mapping
RGB resources and masks, then verify native rendered updates within64MiB.

Special-grid border replication matches450 original/PC/NXDK grids across
all2..16 dimension pairs, including sequential two-texel behavior. Four guards,
both builds and24 tests pass. Compose coverage, selection, accumulation and
RGB fallbacks into the complete grid before retained-resource integration.

Special sample selection plus interpolation matches2048 original/PC/NXDK
cases across every tolerance pass, including334 first-in-range vertex fallbacks
and578 exhausted searches. Three guards, both builds and24 tests pass; compose
the full special grid, border copying and retained topology/lighting resources.

Special two-edge position/normal interpolation matches4096 original/PC/NXDK
samples, including zero/near-zero spans and unclamped factors; two guards, both
builds and24 tests pass. Recover edge/nearby-vertex selection and retained
face/normal ownership, then integrate the complete special sampler.

Special texel coverage matches2048 original/PC/NXDK cases with5504 edge
hits,485 row-prefix skips and482 continuations without new crossings. Both
builds and24 tests pass. Reconstruct two-edge/nearest-vertex sample selection
and interpolation, then bind special sampling to native lighting resources.

Special lightmap edge crossing matches8234 original/PC/NXDK cases, including
3436 crossings and endpoint/precision boundaries; three guards, both builds and
24 tests pass. Complete texel coverage orchestration and edge/vertex sampling;
this arithmetic is not yet bound to rendered lighting.

Special lightmap corner-normal smoothing matches4096 original/PC/NXDK
corners; both builds and24 tests pass. Reconstruct special face/vertex ownership,
texel coverage and edge/vertex sample selection, then native lighting binding.

Complete mapping decoding matches2048 original/PC/NXDK records; PC inventory
passes301871 static mappings across94 levels. L1S1 uses4324 special mappings
of5851, so prioritize special polygon sampling before native lighting binding.
Both builds and24 tests pass; decoded L1S1 metadata alone would add631908 bytes.

Ordinary lightmap sampling plus softened/masked accumulation now matches512
complete original/PC/NXDK grids (10368 samples); both builds and24 tests pass.
Bind retained mapping/RGB owners, mask generation and native texture updates;
special polygon sampling remains unfinished.

Ordinary lightmap sample positions match7776 original/PC/NXDK points over384
mapping grids; both builds and24 tests pass. Bind softening/masks and retained
mapping fields to accumulation, then complete special sampling and rendering.

Raw mixed-light accumulation with visibility weights matches4096 original/
PC/NXDK cases; a recovered weight-rounding detail also fixes shared VFX math.
Both builds and24 tests pass. Bind mapping sample positions, softening/masks,
source accumulation and retained RGB/renderer updates.

Complete lightmap RGB resolve matches512 original/PC/NXDK mappings and36992
pixels, including the unfiltered rim and pitched output; both builds and24 tests
pass. Reconstruct source accumulation and bind retained RGB/renderer updates.

Lightmap neighborhood filtering matches4096 original/PC/NXDK cases at native
53-bit x87 precision, including2153 boundary cases; both builds and24 tests
pass. Bind full mapping resolve, source accumulation and renderer texture updates.

Accumulated-light RGB conversion matches8192 original/PC/NXDK cases,
including integer normalization and overflow behavior; both builds and24 tests
pass. Reconstruct light accumulation/filtering, then bind runtime texture updates.

Runtime lightmap RGB rectangle upload matches2048 original/PC/NXDK cases,
including black texels, failed locks and pitch; seven buffer guards pass.
Both builds and24 tests pass. Reconstruct4f26a0 preceding light accumulation
and dirty-bit stages, then bind retained RGB resources and renderer uploads.

Authored light timers now run after physics in the campaign replay: stock64MiB
XEMU matches PC across179 updates/41349 visits, clock hash and2412 bytes of
persistent traversal scratch. Both builds and24 tests pass. L1S1 lights stay
steady; exercise changing/classed lights, alternate solids and rendered effects.

Retained-world lighting traversal now reuses per-room trees and their stacks,
matching2048 original/PC/NXDK cases (504 state-changing). Six invalid-binding
checks and an empty-tree case pass; both builds and24 tests pass. Allocate
persistent room/root scratch, bind scene light updates and rendered dirty state.

Retained lighting metadata is installed in the campaign scene: L1S1 owns
7418 faces and5851 shared mapping bytes in272919 bytes. A180-frame stock64MiB
XEMU replay matches PC counts and full metadata hashes; both builds and24
tests pass. Bind solid/tree views, live timers and dirty-state rendering next.

Render/lighting view stack matches1024 original/PC/NXDK sequences and
6144 nested steps with exact restoration; both builds and24 tests pass.
Bind dispatch services and renderer-mode gates to retained native solid/
dirty-state owners, then connect live light timers and rendering.

Main/alternate-view lighting dispatch matches512 original/PC/NXDK cases;
all21 callback failure positions restore entered views. Both builds and24
tests pass. Reconstruct view-stack state and bind services to retained
native solids/dirty ownership, then connect live light timers.

Alternate-view light volumes match4096 original/PC/NXDK cases, with
unchanged radii and transformed positions/axes/endpoints. Both builds and
24 tests pass. Finish main-then-view dispatch and view-stack ownership,
then bind retained native dirty state and live timers.

Complete world-space solid lighting updates match2048 original/PC/NXDK
cases without substituted callbacks;504 change state. Both builds and24
tests pass. Reconstruct alternate-view dispatch and bind retained native
face/shared dirty-state ownership before enabling timer-driven rendering.

Real point/cone/segment volume callbacks match4096 original/PC/NXDK
preparation and bounds cases; both builds and24 tests pass. Compose with
whole-solid/root/tree updates and alternate views, then bind native dirty
state and timer-driven lighting.

Lighting root/detail collection and repeated-root traversal match1024
original/PC/NXDK cases each; both builds and24 tests pass. Compose whole-
solid fallback and alternate-view dispatch, then bind actual light-volume
predicates and native dirty-state ownership.

Geometry-tree lighting updates match1024 original/PC/NXDK traversal cases,
including pruning and shared face state; both builds and24 tests pass.
Finish root/detail-room collection and alternate-view dispatch, then bind
native lighting state and the verified light-volume tests.

Face lighting dirty-state updates match2048 original/PC/NXDK cases and
callback failure preserves earlier progress; both builds and24 tests pass.
Connect root selection and right-before-left geometry tree traversal, then
bind native face/shared-lighting state and timer-driven rendering.

Authored spotlight angle-to-plane preparation matches all2411 installed
lights plus1024 synthetic cases on original/PC/NXDK; fixed negative widths
for180-degree lights. Both builds and24 tests pass. Apply volumes in room/
tree traversal and alternate-view dirty-state dispatch, then native lighting.

Spotlight plane construction and composed box testing match2048 original/
PC/NXDK cases each; both builds and24 tests pass. Finish stored cone-width
calculation, alternate-view dispatch and room/tree dirty-state traversal
before connecting timer-driven visibility to native lighting.

Spotlight visibility box rejection matches4096 original/PC/NXDK cases,
including the double -0.001 plane boundary; both builds and24 tests pass.
Construct spotlight planes, traverse room geometry and apply dirty-state
updates before binding timer-driven visibility and native lighting.

Original light registry append/dispatch passes384 executable cases with
131106 visits, including count changes during iteration. File-order loading
is confirmed statically; timers follow actor/physics update487a40. Bind
that ordering and frame gates before claiming shared RNG/frame fidelity.

Retained light clocks now initialize identically for all22293 lights in93
levels on PC/NXDK; peak owner255100 bytes remains below256KiB. Both builds
and24 tests pass. Original433260 timer loop uses registry646098 in index
order; verify registry population/frame gates before scheduling live clocks.
Stock64MiB XEMU replay-20260913-165531 passes180 frames and PC telemetry
comparison with142620-byte L1S1 owner; native clock hashes remain to add.

Timer-to-pool light updates match4096 original/PC/NXDK cases, including RGB,
class-dependent invalidation and shared RNG; failed updates preserve state.
Both builds and24 tests pass. Retain elapsed clocks, bind frame scheduling and
visibility dispatch, and complete light-driven native rendering.

Authored light timer stepping matches4096 original/PC/NXDK cases, including
interpolation, single-transition overshoot and RNG consumption decisions;
both builds and24 tests pass. Bind retained elapsed state and shared RNG/frame
scheduling, apply pool intensity updates, then finish light-driven rendering.

Native campaign scene now owns231 L1S1 lights in137072 bytes;180-frame
stock64MiB XEMU run matches PC source/runtime/field hashes and RNG. Fixed
first-light base-color rounding; both builds and24 tests pass. Bind retained
timers, visibility updates and rendering; visuals are not yet light-driven.

Retained authored-light ownership now loads all22293 sources in93 levels
identically on PC/NXDK, with exact/short budgets and archive-independent data.
Peak232680 bytes; both builds and24 tests pass. Install in the native scene,
step retained timers/RNG, perform visibility updates and bind edge rendering.

Authored light activation matches all22293 original/PC/NXDK records plus512
randomized cycle cases; fixed rejection of authored zero-width cone angles.
Both builds,2048 shading cases and24 tests pass. Bind retained activation,
shared RNG/timers, pool replacement/visibility effects and native rendering.

Authored v180 light reader now matches all22293 records across93 levels on
PC/NXDK; every record also passes truncation guards. Both builds and24 tests
pass. Convert authored matrices/degrees/flags/cycles, bind pool creation and
retained timers, then verify original loader and native scene activation.

VFX pool ownership matches1024 original/PC/NXDK operation sequences plus2048
original/NXDK cache checks;1100-slot capacity/exhaustion/reuse checked onNXDK.
Both builds and24 tests pass. Bind authored light loading, visibility callbacks,
global/null-room selection and native edge lighting; pool costs105644 bytes.

VFX point/cone/segment source initialization matches2048 original/PC/NXDK
cases and3 guards; both builds and24 tests pass. Bind original pool lifetime,
registration/generation/visibility effects and authored light loading before
native scene use; directional creation and edge-light integration remain.

VFX cache-to-selection-to-transform-to-RGB composition matches1024 complete
original/PC/NXDK cases; all prepared source fields also match NXDK/original.
Both builds and24 tests pass. Bind live light sources/invalidation, global
state, edge caches and native VFX draws; scene still uses diagnostic lighting.

VFX room/object cache rebuilds match2048 cases per original path on PC/NXDK;
2048 original/NXDK mutation sequences confirm cache hits/invalidation. Both
builds, box/sphere regressions and24 tests pass. Bind source lifetime and
invalidation, global/null-room state, active lists and native edge lighting.

VFX box candidate filtering matches2048 cases per original room/object path
on PC/NXDK, including expanded corners and segment intersections;3 guards,
sphere regression, both builds and24 tests pass. Reconstruct live cache
updates/global active-list state and bind edge lighting before native use.

VFX spherical candidate filtering now matches2048 original/PC/NXDK cases,
including ordered indices, tangencies/adjacent floats and segment projection;
3 guards, both builds and24 tests pass. Bind room/object cache rebuilding,
box filters, global gate state and edge-light caches before native use.

VFX source transforms plus mixed shading match2048 complete original/PC/NXDK
cases, including every transformed source field. Bind live active-list
selection and mesh edge-light caching before native renderer integration.

Composed mixed VFX lighting now matches2048 complete original/PC/NXDK
4daff0/4da8b0 cases across directional, point, cone and segment sources.
Connect live scene selection/order/transforms and mesh edge-light caching;
the harness supplies active lists, so native lighting remains unverified.

Composed VFX point-light shading now matches1024 complete original/PC/NXDK
4daff0/4da8b0 cases with0-16 ordered lights and2 guards. Join directional,
cone and segment sources plus active scene ownership before native use.

VFX final light RGB conversion matches4096 original/PC/NXDK cases and3
guards; fixed an x87 blue-channel precision mismatch in the negative-gain
branch. Bind active-light accumulation to conversion and material colors next.

VFX four-profile falloff plus RGB addition matches4096 original/PC/NXDK
cases,4096 in-place checks and3 guards. Connect accepted point lights, retain
cone-boundary precision, and complete active-light ownership/conversion next.

VFX cone-light geometry4daf30 now matches2048 original/PC/NXDK cases and
3 guards, including softened normals and cone-axis terms. Complete falloff,
cone-boundary attenuation and active-light accumulation before native drawing.

VFX point-light geometry4dae50 now matches2048 original/PC/NXDK cases and
3 guards, including coincident fallback, radius rejection and softened branch.
Reconstruct cone geometry/falloff and active-light accumulation next.

VFX scene-light boundary mapped:4da8b0 active-light accumulation differs from
model lighting; reconstruct point/cone geometry, falloff dispatch and active
light ownership before4daff0 RGB conversion. Static evidence only, no new
rendering claim; see docs/VFX-RENDERING.md scene-light integration boundary.

VFX edge color preparation now matches2048 original/PC/NXDK cases, including
brightness minimum, unlit white and type2 tint, plus2048 in-place checks and
3 guards. Connect scene lighting, edge caches and renderer passes next.

VFX bitmap metadata and retained primary/secondary image selection now match
2048 original/PC/NXDK duration cases plus2048 NXDK/original image selections;
7 guards, both builds and24 tests pass. Bind lighting/draw submission and
original-map overrides; native VFX appearance remains unverified.

VFX effect-time and normalized texture clocks match4096 original/PC/NXDK
cases and4 guards; bind retained bitmap metadata and actual draw passes next.
PC XInput walkthrough exited at frame20232 with RF_RANGE in animation stage5,
scene stage1; user requested continuing other work rather than diagnosing now.
Preserved logs: artifacts/pc-xinput-walk.stderr.log and stdout.log.

Retained standalone VFX material banks now resolve newer mesh material IDs;
7 asset banks,120 PC/NXDK bindings and29 budget/I/O/parse rollback guards pass.
Bank footprints20-914 bytes exclude textures; connect texture clocks, lighting
and draw passes. Xbox and PC core/probe builds pass; live PC session retained.

Embedded VFX materials now sample retained blend, brightness and frame opacity;
80 original/PC/NXDK scalar checks across14 meshes and126 missing/global/range
guards pass. Both builds and24 tests pass; renderer submission remains open.
PC frontend now polls XInput while focused; USER: verify controller movement,
look, A jump, X use, B crouch, disconnect and Back+Start exit in the live build.

Borrowed VFX material views now evaluate blend/brightness/opacity without
allocation, including unaligned arrays and original pre-interpolation blend
clamps;2048 original/PC/NXDK cases and7 guards pass. Both builds and24 tests
pass. Bind older embedded frame opacity and retained renderer materials next.

VFX material scalar timing/interpolation matches2048 original/PC/NXDK
cases each for54a930/54a9e0/54aa80, including zero rate;3 guards pass.
Bind retained material tracks, lighting and texture passes to face lists;
both builds and24 tests pass, native VFX appearance still unverified.

Persistent VFX geometry now produces sorted authored face lists;112
PC/NXDK list comparisons across14 meshes include372 visible face records.
Capacity guards,2048 original world-face cases and24 tests pass; bind
parent poses, projected-vertex caches, lighting and texture draw passes next.

VFX instance updates now select frames once and resolve keyed tracks once
per mesh;112 PC/NXDK updates retain identical buffers with at most3 key
queries each.2048 original dispatch cases and24 tests still pass.
Hoist repeated matrix/center work, then bind parents and renderer buffers.

Persistent VFX local vertex/UV ownership passes28 PC/NXDK budget/lifetime
cases and112 updates over14 installed meshes; buffers cost136-1156 bytes
per mesh beyond the borrowed definition. Both builds and24 tests pass.
Bind parent poses, projection caches, face lists and material submission;
optimize repeated matrix work and support empty meshes before full playback.

World-space VFX face preparation matches2048 original/PC/NXDK cases
with real projection/clip code generation:1232 rejected,816 visible.
Bind persistent geometry/cache/list ownership and lighting/material draws;
both builds and24 tests pass, native VFX playback remains unverified.

Composed VFX face preparation matches2048 original/PC/NXDK cases:
758 clip rejects,651 back faces,639 visible;2 guards and24 tests pass.
Connect original projection/cache ownership and persistent face lists to
material submission; both builds pass, no native VFX rendering claim.

VFX material-biased depth/list append matches2048 original/PC/NXDK cases
and3 capacity/input guards; secondary sort fields remain caller-owned.
Compose projection/culling and bounded persistent lists before material draw
submission; both builds and24 tests pass, no native VFX visuals yet.

VFX gap sort53c840/53c950 matches2048 original/PC/NXDK lists and2 guards,
including equal/epsilon-adjacent keys; both builds and24 tests pass.
Generate original depth keys and compose bounded face-list ownership next;
projection/material draw submission and native VFX playback remain open.

VFX facing test5478f0 matches2048 original/PC/NXDK cases and2 guards,
including distinct perspective/flat zero-dot behavior; both builds/24 tests pass.
Compose projected clip masks, normals and facing with material-biased depth
ordering before render submission; no new native VFX rendering evidence.

Animated VFX face normals match2048 original/PC/NXDK cases and2 guards;
renderer callbacks resolve to553ee0/554bf0/555080 and extra texture110.
Complete554a80 projection/facing/depth list and material passes; both builds
and24 tests pass, but VFX render submission/native playback remain open.

Located VFX render entry53ee90 and draw targets516f50/516f70/516f90;
trace material/clip/submission callbacks next (docs/VFX-RENDERING.md).
Retain active-state and persistent geometry/UV ownership before live drawing;
render dispatch remains unimplemented and these findings are static evidence.

Skeletal VFX tag-parent branch matches2048 original/PC/NXDK cases,2048
in-place checks and2 guards using real cached bone lookup without hooks.
Bind retained parent owners/virtual tags and complete UV/material submission
before native playback; both builds and24 tests pass.

Direct-parent VFX geometry matches2048 original/PC/NXDK cases,2048
in-place checks and5 guards; center/vertex change, extra bounds stay unchanged.
Reconstruct tag-based parent503230/4fe710/4ff020 branch, then connect
UV/material submission and native playback; both builds and24 tests pass.

Combined VFX geometry dispatch matches2048 original/PC/NXDK cases from
53f060 entry to54010a, with3 inactive/invalid guards; all24 tests pass.
Integration exposed/fixed absent rotation keys: use stored frame0 quaternion,
not identity.14 authored ownership cases still pass; bind parent transforms
and UV/render submission next, then validate native stock64MiB playback.

VFX key-time conversion matches2048 original/PC/NXDK cases and4 guards;
owned keyed geometry passes328 authored vertex/time/budget checks.
Old keyed files use frame0 fallback position/rotation/scale; absent fallback rejects
instead of inventing values. Verify complete53f060 dispatch/parent composition
and bind rendering; both builds and24 tests pass, no new native XEMU claim.

Resolved keyed VFX geometry matches2048 original/PC/NXDK cases, including
base-then-key transforms and scale-dependent bounds; both builds/24 tests pass.
Bind owned key sampling and verify missing-track defaults/key-time conversion
before complete53f060 playback and native renderer integration.

Non-keyed VFX transform animation matches2048 complete original/PC/NXDK
interior/terminal branches and2048 owned-view fixtures;14 installed meshes
correctly select keyed or morph paths, so no authored non-keyed coverage.
Connect keyed/base transforms next, then parent poses and rendering.

VFX scale/rotation/translation stages match2048 original/PC/NXDK cases,
including2048 in-place checks and4 failure guards; both builds and24 tests pass.
Compose keyed/base and frame transforms into53f060 playback, then parent poses
and renderer submission; this helper validation is not native XEMU evidence.

Attachment ordering audit: original4881a0 passes1000 randomized16-object
forests; positive-tag dispatch passes54 original cases including real486c90.
Negative-tag poses now match1536 original/PC/NXDK cases (384 singular).
Shared bounded parent ordering now matches1000 original/PC/NXDK forests.
Installed clutter declares no turret use4; fresh props have attachment parent-1.
Static-prop/glare ordering is live and matches180-frame stock64MiB XEMU.
Moving-parent fixture passes880 PC/stock64MiB XEMU cases with full restoration.
Orphan retirement is bound;180-frame stock64MiB XEMU preserves all220 valid glares.
Forced-loss stock64MiB XEMU retires3 owners and clears all scene/registry references.
Bind authored dynamic parenting and turret ownership; extend retirement with future resources.

Volume opacity/beam geometry match original/PC/NXDK; original mode is06110c42.
Volume projection/clipping matches1024 original/PC/NXDK polygons.
Projected animated volume beam passes2048 PC/XEMU pixel checks;84 pages recover.
Camera-to-volume opacity matches2024 original/PC/NXDK cases, with12 input guards.
Composed nonactor volume routine matches256 original/PC/NXDK cases plus7 failure checks.
Volume records now share ordinary particle sorting;180-frame stock64MiB replay passes.
Opt-in scene-volume fixture passes90 animated beams/360 vertices on stock64MiB XEMU.
Verify authored volume placement and mixed transparent-object ordering.
Actor volume sizing/RNG matches2210 original/PC/NXDK cases plus6 guards.
Actor volume aim geometry matches1018 original/PC/NXDK cases plus17 guards.
Composed actor volume branch matches512 original/PC/NXDK cases and4 NXDK failure guards.
NPC volume adapter now reads retained command/occupant state; stock64MiB prop-beam regression passes.
Retained NPC volume service passes6 restored-state cases on stock64MiB XEMU.
Special volume-owner gate matches162 original/PC/NXDK cases plus6 callback failures.
Two-tag glare constructor matches60 original/PC/NXDK cases plus3 callback failures.
Two-tag owned lifecycle passes PC and16 compiled NXDK checks.
Extracted rod factory stage passes80 compiled checks and512 original/PC/NXDK factory cases.
All431 installed clutter definitions disable rod glare; defer live binding until an authored caller requires it.
Verify authored NPC/vehicle beams, bind player state and complete transparent scene ordering.

AI action/state setters407e20/407e80 match2048 original/PC/NXDK cases.
Event arbitration4087a0 matches2048 original/PC/NXDK cases; bind event/destination/stance services.
Shared destination40ac90 matches2048 original/PC/NXDK cases; bind route services and retained scene ownership.
Navigation preparation40aae0 matches2048 original/PC/NXDK cases; compose it with retained movement/query owners.
Direct-route40b0d0 matches2048 original/PC/NXDK cases; bind global navigation references and reconstruct searched routes.
Concrete search matches2048 all-member and2048 temporary-start original/PC/NXDK graphs; compose endpoint setup and live route ownership.
Bounded endpoint insertion/cleanup matches1536 and nearest selection2048 original/PC/NXDK cases; bind visibility and search wrapper.
Node preparation and alternate-edge geometry each match2048 unhooked original/PC/NXDK cases; compose them into route services.
Visibility query4ce740, concrete flat-solid collision and room hierarchy each match2048 original/PC/NXDK cases; bind combined route ownership.
Concrete request4cebd0 matches2048 clear and2048 obstructed original/PC/NXDK cases; bind retained scene graph storage and NPC owners.
Persistent NPC route endpoints and prepared-request service pass38 ownership checks in stock64MiB XEMU; bind AI destination preparation and route following.
Waypoint advance40c2a0 and full steering40cb50 each match2048 original/PC/NXDK cases; reconstruct arrival/re-pathing40bb70 and follower dispatch40b6d0.
Authored rotation limits/acceleration are retained for all63 classes;84 PC/NXDK reader cases and three PC level budgets pass; bind steering/physics consumers.
Body rotation filter433c80 matches2048 original/PC/NXDK cases; bind NPC eye orientation and complete angular integration before live steering.
Eye-angle update49cf40 matches2048 original/PC/NXDK cases plus6 guards; bind retained NPC angles and eye matrix reconstruction.
Physics eye-matrix composition matches2048 original/PC/NXDK cases plus3 guards; bind NPC angle/matrix owners and per-frame physics scheduling.
Retained NPC look/steering services pass38 restored-state PC/stock64MiB XEMU cases; bind AI follower and angular delta/physics scheduling.
Composed angular prediction49f566 passes2048 original/NXDK cases with bounded PC matrix residuals; bind angular preparation, collision prediction and commit.
Ordinary angular velocity preparation matches2048 original/PC/NXDK cases plus7 guards; bind drive predicates, special-mode dispatch and prediction.
Angular dispatch precedence matches800 original/PC/NXDK cases; bind SP player/control references and remaining special rotation services.
Retained ordinary NPC angular preparation/prediction passes38 PC/stock64MiB XEMU cases; connect verified linked_handle player lookup and collision/commit scheduling.
Player/control lookup48aa30 matches2048 original/PC/NXDK cases; bind registered SP player list to automatic angular drive selection.
Automatic SP angular drive selection passes38 driven and38 undriven PC/stock64MiB XEMU checks; finish special dispatch, collision and movement commit.
Opening-level angular audit finds142 ordinary and3 rigid-body spawns; prioritize ordinary collision/commit and retain Driller/APC49e180 as required follow-up.
Ordinary49d0a0 commit matches1024 original/NXDK cases with bounded PC matrix residuals; bind accepted NPC collision results and pose publication.
Retained ordinary NPC commit and completed-position publication pass76 PC/stock64MiB XEMU checks; connect collision substep resolution and continuous scheduling before later room/eye refresh.
Ordinary partial-contact motion prefix matches2048 original/NXDK cases with bounded PC matrix residuals; complete427550 contact dispatch and49fe40 flag/time tail before live scheduling.
Complete ordinary49fe40 orchestration matches2048 original/NXDK cases at explicit contact callbacks; reconstruct427550 gameplay effects and bind retained contact/time owners before live scheduling.
Actor contact41a000 decisions match2048 original/PC/NXDK cases; bind429790 destruction effects and remaining427550 surface/item/vehicle dispatch.
Contact destruction429790 ordering matches1024 original/PC/NXDK cases; retain class174 sound and bind live damage/selection/playback services.
Authored squash-sound groups are retained per class and pass180-frame stock64MiB XEMU; bind contact destruction to damage and positional playback.
Retained contact destruction damage and squash playback pass restored-state L1S1 stock64MiB XEMU; complete427550 dispatch and live collision scheduling.
Registered NPC-to-NPC41a000 dispatch reaches damage/squash playback in four restored-state XEMU checks; add player/object branches and full427550 scheduling.
Object-contact handles NPC/player damage and riot_shield class identity in PC/stock64MiB XEMU; complete pickup services, lethal lifecycle and continuous scheduling.
SP pickup eligibility459560 matches2048 original/PC/NXDK cases and3 callback failures.
SP pickup completion order/timer matches1024 original/PC/NXDK cases and6 callback failures; implement grant/reaction/audio services and bind scene pickup ownership.
Normal-SP acquisition4030d0 matches2048 original/PC/NXDK cases with actual401470 no-op notification; bind inventory creation, equip/reset and full pickup grants.
Startup grants now compose with concrete42ab20 animation selection (220 loaded PC cases); connect retained inventories/selections to live scene consumers.
All63 class defaults now retain optional melee (four authored classes); include it when creating startup inventories.
VFX quaternion key evaluation matches2048 original/PC/NXDK cases with56 owned checks; compose keyed/frame transforms, parent pose and rendering.
VFX translation/scale key evaluation each matches2048 original/PC/NXDK cases with112 owned checks; complete quaternion rotation and composed transform playback.
VFX vertex-animation sampling matches2048 original/PC/NXDK cases with1664 owned authored checks; complete transform-based/keyed motion and renderer submission.
VFX UV sampling matches2048 original/PC/NXDK cases and4572 owned authored checks; complete vertex/transform animation, material state and renderer submission.
VFX frame selection and signed vertex expansion each match2048 original/PC/NXDK cases; owned vertex access matches3128 authored checks; complete transform/key/UV interpolation and effect clock lifecycle.
VFX texture binding resolves20 authored materials to16 shared images (356344 bytes); connect effect resource ownership and verify actual textures in native XEMU playback.
Owned VFX meshes load all14 authored payloads within explicit budgets with132 failure checks; bind bitmap ownership, normalized direction and animation playback.
VFX transform keys match1031 original/PC/NXDK cases including7 authored meshes; compose bounded owned meshes and connect animation interpolation/playback.
VFX frame decoding matches1255 original/PC/NXDK cases including231 authored frames; normalize owned direction and bind bounded resources/playback.
VFX mesh bounds/edges match1038 original/PC/NXDK cases including14 authored meshes and8113 failure guards; decode animation frames and bind validated face-to-edge references.
Embedded VFX materials match1034 original/PC/NXDK cases including10 authored records with1886 failure guards; bind bounded bitmap ownership and decode vertex/transform tracks.
Standalone VFX materials match1034 original/PC/NXDK cases including10 authored records; bind bounded bitmap ownership and vertex/transform tracks.
VFX mesh timing matches2048 original/PC/NXDK cases; all14 authored mesh prefixes parse with validated face indices; complete material sections and vertex/transform tracks using per-mesh sample counts.
VFX mesh-face decoding matches2190 original/PC/NXDK cases including all142 authored projectile faces; complete remaining mesh sections, vertex tracks and material resolution.
VFX header matches1031 original/PC/NXDK cases; bounded archive directories index29 records across7 projectile assets with42 NXDK failure checks; reconstruct object/track decoding and playback.
Projectile model catalog retains all44 authored entries (5 static,11 VFX,28 model-less), passing16 PC/NXDK reader cases and51 original filename classifications; bind both static and VFX model backends under a shared budget.
Moving creation physics matches640 original/PC/NXDK cases; projectile model/sphere/physics ownership now passes12 PC/NXDK integration cases and2 allocation failures; bind bounded scene model services, room ownership and remaining firing/impact effects.
Projectile152-byte creation descriptor matches2048 original/PC/NXDK cases; fixed50-slot pool matches4096 original/PC/NXDK operations; registration/rollback adapter passes52 PC/NXDK lifecycle checks; bind scene model services and finish full4c77a0 lifecycle before composing425830 firing; see docs/FIRING-RUNTIME.md for audited boundaries and deferred-retirement requirements.
Shot ammo consumption4257c0 matches2048 original/PC/NXDK cases; bind it to successful-shot dispatch with timing and projectile ownership.
Muzzle-origin41b5a0 now matches2048 original/PC/NXDK cases plus7 NXDK failure guards; target-aim math41b4c0 matches2048 original/PC/NXDK cases; retained NPC target/lock service passes156 checks in stock64MiB XEMU; composed live muzzle service matches60 armed NPC poses in XEMU; bind other target types, replacement-class hand ownership and firing dispatch.
Authored NPC primary/secondary/melee startup grants now populate retained inventories and selected animations; complete active firing resource services, weapon models and combat scheduling.
Third-person model/tag accessors and full418e60 hand/grip placement each match2048 original/PC/NXDK cases; reconstruct421c40 visibility/draw dispatch and bind actual transforms/model loading.
Third-person model residency now follows NPC inventories and matches180-frame stock64MiB XEMU (8200 bytes in L1S1); complete textures and draw dispatch.
Primary weapon poses match60 initial NPC poses in XEMU; visibility/model selection matches4096 original/PC/NXDK cases; draw-state setup matches2048 original/PC/NXDK cases; recoil also matches2048 original/PC/NXDK cases; full draw loop matches1024 original/PC/NXDK cases; 10 callback-failure cases pass; live weapon renderer emits648 vertices with exact PC/stock64MiB XEMU hashes over180 frames; authored hand counts/tags now match all78 NPCs in XEMU; finish lighting/LOD, player overrides and active recoil.
Retained pain dispatch now uses44 reset descriptors and selected action audio; verify representative armed pain paths and finish active sound/effect reset services.
Armed and unarmed death/recovery scene fixtures pass; animation consumers validate retained skeletons without rejecting equipped weapons.
All78 inventory/selection/firing owners match PC in180-frame stock64MiB XEMU; retained storage78936 bytes, body subsystem peak538012 bytes within640KiB.
Reserve addition428d90 matches2048 original/PC/NXDK cases; bind reload query/service and full ammo pickup acceptance.
Ammo pickup scaling/message amounts match8192 original/PC/NXDK cases; bind special weapon IDs, difficulty ownership and inventory/message consumers.
Retained4872-byte weapon supply catalog matches all44 original-audited mappings in stock64MiB XEMU; create scene inventories through verified initialization.
Retained collision voice1478 and global Sub Hit playback pass four restored-state PC/stock64MiB XEMU checks; bind full surface dispatch and continuous scheduling.
Surface route gates match4170 original/PC/NXDK cases; reconstruct APC debris/damage, Driller damage/player feedback and timed-contact effects before full dispatch.
APC bumper placement/geometry/damage ordering matches1024 original/PC/NXDK cases; bind actual asset/geometry services and complete timed-contact dispatch.
Timed water-contact effects/cooldown match2048 original/PC/NXDK cases; large/huge splash definitions and Foley IDs pass stock64MiB XEMU; implement VFX emission before full427550 composition.
Combined427550 routing reads retained NPC contact packets and passes four PC/stock64MiB XEMU cases; bind concrete effect/object services and connect the continuous physics caller.
Driller geometry/self-damage requests match1024 original/PC/NXDK cases; implement linked-player feedback and bind real geometry ownership.
Player contact direction/flag accumulation match2048 original/PC/NXDK cases; compose with the Driller linked-player camera/feedback loop.
Driller linked-player feedback binds retained player/camera/direction state and passes four PC/stock64MiB XEMU cases; compose after real geometry/damage and full contact scheduling.
AI scalar401cc0 and occupant-chain4077a0 each match2048 original/PC/NXDK cases; retain scalar/override sources.
Shared408ac0 selection matches2048 original/PC/NXDK cases across107 paths plus21 failure checks.
AI animation resets4091d0/409210 match2048 original/PC/NXDK cases plus4 failure checks.
Scene AI reset service passes304 restored-state cases across38 NPCs on stock64MiB XEMU.
Shared recovery408f20 matches2048 original/PC/NXDK cases plus5 callback and8 duration guards.
Authored unholster delays are retained; three PC levels and180-frame stock64MiB XEMU pass.
Scene unholster service passes38 authored starts and38 pending-call checks on stock64MiB XEMU.
Implement remaining AI services and bind retained override/state fields before live special landing.

The stationary startup-NPC animation gate matches PC/stock64MiB XEMU;
footstep selection and support-material helpers are verified but not live.
Detailed source evidence and test scope are recorded in docs/NPC-MARKERS.md.

Clutter collision integration: 12 shared collision models and 168 registered
prop owners now match PC/stock64MiB XEMU across 1008 diagnostic queries;
live player/NPC collision scheduling and full factory effects remain open.

Factory audit: opening149 collision-enabled props are collide_weapon only;
retain factory flag0x40000 and original non-projectile rejection. Bind glare
(149 declared placements) and explosion resources (158) before full creation.

Static tag lookup53c23f now matches516 original/PC/compiled NXDK cases;
retain first-submesh attachment names/poses and bind factory glare services.

Static attachment owners pass1173 compiled NXDK cases and now match
stock64MiB XEMU in shared scene storage; bind tag poses and glare creation.

Static tag world placement now matches512 original/PC/compiled NXDK
poses exactly; bind registered prop lookup/placement to glare creation.

Registered prop tag services now match original and stock64MiB XEMU
(1541 lookups/394 poses); bind glare instance creation and lifetime next.

Original parented glare constructor413d20 passes60 audited cases; implement
its shared type10 allocation, maximum-size radius, state initialization and list lifetime.

Shared parented glare constructor matches60 original and18 callback-failure
PC/NXDK cases; bind type10 ownership and reconstruct glare destruction.

Original glare type10 allocation/unlink/retirement passes32 cases; shared
owner must preserve flags0x6030000 and omit ordinary prop room binding.

Concrete528-byte glare owner matches32 original body states on PC; verify
compiled NXDK allocation failures, then bind factory and campaign resources.

Glare owner passes35 compiled NXDK cases; full owned factory adapter passes
PC lifecycle checks; adapter now passes NXDK; bind class resources.

Correction: glare radius is max(class sizes), not RNG; constructor now
executes that rule and full owned adapter passes14 compiled NXDK checks.

Glare metadata reader matches56 authored classes on PC/compiled NXDK;
retain class definitions and bitmap resources, then bind campaign creation.

Glare class ownership passes13 PC/compiled NXDK checks for56 authored rows
(17492 retained/39157 peak bytes); bind bitmap resources and campaign creation.

Glare bitmap audit finds38 unique textures, including two animated thruster
VBMs; use all-frame ownership (1771472 bytes for all textures) when binding.

Owned glare materials preserve62 frames across38 shared textures and56
class bindings; verify native Xbox residency and bind campaign glare effects.

Campaign glare resources now match stock64MiB XEMU across all62 decoded
frames at1803012 retained bytes; bind authored prop glare instances next.

Shared factory corona-cache/glare stage passes512 original/PC/NXDK factory
cases and251 callback/cache checks; bind scene instances and skin overrides.

Skin glare metadata now matches504 PC/NXDK cases, including16 authored
overrides; apply Yellowish runway-light WarmWedge overrides to live instances.

Opening campaign now owns157 parented glares with67 skin overrides and
clean native retirement; reconstruct glare update/visibility/frame selection/rendering.

Original corona/reflection pass dispatch matches197 PC/NXDK cases; bind
visibility-marker production and actual corona/reflection/volume geometry.

Glare visibility-marker/volume queue collection now matches336 original/PC/
NXDK cases; bind live room collection and reconstruct actual draw routines.

Glare candidate occlusion now matches408 original/PC/NXDK cases plus12
error cases; reconstruct cached world search and bind visible effects.

Full original glare search audit passes2048 cases and identifies three
cache fields; implement shared search with explicit cached-query initialization.

Shared full glare visibility search passes1537 original/PC/NXDK cases;
bind stable scene solid/face/actor services and corona scheduling/rendering.

Preferred-face solid queries pass4400 original/PC/NXDK geometry cases;
bind stable face tokens and finish texture-sensitive glare query handling.

Texture-alpha thin collision passes6060 original/PC/NXDK cases; reconstruct
UV/bitmap sampling and propagate it through scene solid queries.

Collision UV interpolation passes8192 original/PC/NXDK cases; bind authored
corner coordinates and reconstruct bitmap sampling for texture alpha tests.

Bitmap sampling passes3072 original/PC/NXDK cases; adapt owned image
storage and compose authored UV/alpha services into scene collision.

Owned texture sampling passes2048 PC/Xbox cases without extra image
allocation; compose authored corner UV/alpha sampling into scene queries.

Authored UV/sample composition passes460720 faces across94 levels; bind
actual texture/frame selection and live preferred/room/flat collision paths.

Actual opening material sampling passes7418 faces with16 loaded textures;
resolve USERBMAP runtime ownership (116 faces) and bind live collision sampling.

Original USERBMAP audit passes50 constructor/3 loader cases; reconstruct
fallback510470 pixels and verify runtime face replacement before parity claims.

Owned original fallback pattern passes1024 pixels on PC/NXDK at4096 bytes;
bind verified loader fallback policy and check runtime USERBMAP replacement.

Initial USERBMAP fallback passes180-frame stock64MiB XEMU scene validation;
propagate texture-alpha sampling through live collision and verify replacements.

Textured sphere/face sweeps pass12000 original/PC/NXDK cases; carry the
alpha service through tree/room/preferred traversal into scene queries.

Textured collision trees pass5000 original/PC/NXDK cases plus5 guards;
carry indexed sampling through room/preferred queries and bind scene resources.

Textured local room queries pass5000 original/PC/NXDK cases plus8 guards;
finish transformed/preferred/flat propagation and bind live scene sampling.

Textured transformed room queries pass5000 original/PC/NXDK cases plus8
guards; finish preferred/flat propagation and connect live scene resources.

Textured flat collision passes5000 original/PC/NXDK cases plus6 guards;
finish preferred-face sampling and bind scene-owned texture query services.

Textured preferred-face and room/flat fallbacks pass4400 original/PC/NXDK
cases; bind scene-owned materials, source-face IDs and glare visibility queries.

Indexed collision-to-material binding passes7418 opening faces on PC/NXDK;
retain scene sampler views/bitmap scratch and validate live world/mover queries.

Scene-owned texture queries match PC/stock64MiB XEMU: 118 queries,118
direct samples,32492 retained bytes and clean teardown; add targeted native
alpha-contact coverage, preferred-cache services and live glare scheduling.

Scene preferred-face round trips pass112 native PC/Xbox hits; connect glare
cache tokens and validate alpha contacts on L1S2 (19 flagged references;
L1S1 world has none), then complete visibility scheduling and rendering.

Authored L1S2 alpha contacts now match stock64MiB XEMU: 6 transparent/32
opaque samples across687 queries; bind glare visibility caches and scheduling,
and verify runtime bitmap replacement before visual-parity claims.

Glare solid-query adapter passes932 native PC/Xbox calls including600
cached calls and cross-solid face reuse; bind registered actor/mover views,
lookup callbacks and full visibility scheduling before rendering glares.

NPC visibility views/model callback match38 native owners and31 direct
hits; bind original488b20 post-render flag0x10 publication/clearing and
room/owner callbacks before enabling full glare visibility scheduling.

Shared object render dispatch matches352 original PC/NXDK cases and4
callback failures with actual flag4000 skip; bind family rendering, then verify
object flag0x10 lifetime and glare scheduling without an assumed frame clear.

NPC render dispatch now applies live hidden/4000 gates and publishes0x10;
native L1S2 matches720 family submissions/1656 skips and3 NPC occluders.
Finish marker lifetime, room/owner callbacks and full glare scheduling.

Glare actor equality uses the retained room token at object word0; constructor
evidence resolves its ownership and1537 cases execute the getter unchanged.
Bind moving-room refresh and remaining full-search visibility callbacks.

NPC room-token/linked-actor facts match38 registered owners on stock64MiB
XEMU; connect moving-room refresh and stable full-search lookup views, then
complete glare visibility scheduling and rendering.

NPC post-update room refresh passes stock64MiB XEMU with a restored room
crossing; bind remaining movement publication/post-refresh family effects and
full glare lookup/scheduling against the synchronized room owners.

Prop room ownership now uses the same nonzero room tokens as NPC/player owners;
bind the remaining glare lookup and render scheduling services.

Registered prop visibility views/model queries now match stock64MiB XEMU;
bind full scene lookup, marker publication and glare scheduling.

Prop rendering now publishes verified object flag0x10 through shared dispatch;
complete marker lifetime, scene glare lookup and visibility/render scheduling.

Moving-solid visibility ownership now matches synchronized collision poses in XEMU;
bind mover render markers and complete scene glare search.

Original mover dispatch passes36 gate cases; split per-solid mesh submission
from transactional world generation before binding its render marker.

Mover mesh generation now uses transactional type9 dispatch and publishes live
visibility markers; bind full glare scene search and audit marker clearing.

Full glare search now resolves scene-owned geometry and caches in stock64MiB XEMU;
verify original owner order/frame scheduling, marker lifetime and corona drawing.

Alternating corona visibility refresh now matches1152 original/PC/NXDK cases;
reconstruct attenuation/parent-room gates and bind the actual draw schedule.

Inactive/occluded corona fade matches962 original/PC/NXDK cases; reconstruct
visible attenuation and common sample publication/drawing next.

Corona common draw tail matches400 original/PC/NXDK cases plus8 errors;
recover visible attenuation and bind billboard/oriented geometry backends.

Visible corona attenuation matches536 original/PC/NXDK cases; bind camera
angle setup, flash submission and complete corona geometry/scheduling.

Corona camera setup passes517 original/PC/NXDK cases; compose parent gates,
refresh/fade/attenuation and flash/draw services into the corona routine.

Corrected corona tail:411e00 packs mode0x06010c41, not graphics state/distance;
compose the corona routine using the verified integer geometry mode.

Composed corona routine matches259 full-original cases plus12 errors on PC/NXDK;
bind scene parent/flash services and billboard/oriented geometry rendering.

Immediate open items:
- [ ] Connect native-verified scene-owned navigation nodes/tags/adjacency (opening333 nodes,760 connections,48360 bytes) to verified reset40a950/full ordered query40c2c0, bind original/PC/NXDK-verified full4991c0 visibility orchestration to verified factory-ordered actor/clutter/corpse owners (bind original/PC/NXDK-verified compact clutter records: opening39834 bytes, then bind original/PC/NXDK-verified shared full4104a0 class/model factory (512 cases plus10 NXDK errors; factory-facing class metadata verified for429 selectable classes; compact class ownership verified at55379 bytes for429 classes; material/glare/emitter lookup verified across510 cases; class resource binding verified for429 classes plus51 synthetic cases; archive-backed resource name catalogs verified at3589 retained/45199 peak bytes; archive-to-class loader verified for all431 authored rows at55610 retained/225520 peak bytes with one reusable row; scene-owned classes/catalogs and170 opening clutter records now match PC/stock64MiB XEMU at99033 retained/229109 peak bytes, with2 unmatched authored Pole Light 1 placements; original type4 generic allocation verified for256 model-boundary cases plus2 early failures and64 actual model-failure destructor/slot-recycle cases; shared489fe0 model attachment verified across512 original/PC/NXDK cases plus4 callback errors and3 guards; shared static/cached animated model creation sphere preparation verified across512 original-block/PC/NXDK cases plus3 guards; shared creation physics owner now verifies640 original/PC/NXDK cases including flags0/20 and existing corpse compatibility; concrete532-byte clutter base owner verifies266 PC/NXDK cases and126 complete original allocator cases, including cleanup and FIFO recycling; shared whole-static-model bounds verified for all427 installed V3M models plus258 synthetic cases (42 multi-submesh models), preserving95 animated-model checks; owned static-model filename/bounds/CSPH metadata verified for all427 V3M files (max576 retained/9368 peak bytes), static filename conversion matches1960 original cases; archive-backed static base adapter verifies all427 models across1290 PC/NXDK cases (max1348 retained/9900 peak bytes); existing static render geometry ownership now passes PC archive byte/budget checks for415 models/733 LODs (12 models containing23 alternate batches explicitly excluded; static projection52e1b5 now matches4000 original/PC/NXDK cases plus6 NXDK guards; complete static vertex loop52e11b now matches512 original/PC/NXDK batches including lighting/RGB/reuse and nonfinite normals; static stored-plane routing matches4096 original/PC/NXDK cases and clipping inputs match2000 cases plus guards; static triangle clipping/emission composition matches512 original/NXDK batches exactly (441 generated vertices/1302 indices), with PC float differences below0.00034 screen pixels and two RGB one-step differences still open; owned static render resources now pass415 PC models/733 LODs and35 compiled NXDK cases over the opening12 models (90904 retained bytes excluding textures); scene-shared12 static resources/168 placement bindings now match PC and stock64MiB XEMU at109416 retained/118208 peak bytes with full data hash3534861336; retained-row material loader matches file-backed PC conversion for13 models and combines opening23 rows into11 textures at843404 retained/845704 peak bytes; scene-owned clutter23 runtime materials/11 textures now match PC/stock64MiB XEMU including every decoded pixel at843456 retained/847688 peak bytes; shared static preview adapter now passes PC direct/near-clipped/stored-plane rejection and capacity tests with poisoned skeletal world caches; campaign now draws authored static placements through shared local views and transferred base textures with highest LOD and cached room culling; native180-frame spawn and60-frame door replays match PC prop vertex hashes (1506/1998 vertices), reuse444880 bytes of NPC scratch and add680 room bytes; owned post-conversion primary replacement loader now preserves all base material fields except texture handles across23 real rows and four override patterns, including case deduplication and exact/short budgets; per-placement authored skin material bindings now match independent opening archive selection and60-frame stock64MiB XEMU (12 appearances,168 bindings,67 Yellowish placements,23 rows/12 textures,909700 retained/915680 peak bytes); shared static render owners now also retain authored CSPH names/parents/spheres (415 models/223 spheres and60 compiled NXDK cases verified), with opening resources matching stock64MiB XEMU at110968 retained/119760 peak bytes; shared static base adapter now matches archive-backed owner/physics bytes across415 models and830 PC comparisons, with9 compiled NXDK lifetime/allocation-failure cases; campaign now retains168 registered clutter base owners with149 physics-enabled bodies/173 spheres, matching independent authored counts and180-frame stock64MiB XEMU at94372 bytes with complete retirement; rendering reads owned transforms/rooms; bind full class factory effects, family lists, original global creation order and live collision participation next; original factory visibility, glare bindings, dynamic LOD/lighting and device-state parity remain unverified); geometry/textures/shared render-model handles, full factory/effect ownership and live integration remain open; authored flags verified for431 declarations and all512 masks; class defaults/model kind verified across256 original parser-prefix cases) using PC/NXDK-verified clutter model/skin metadata (429 selectable classes,494 authored selections; original410b60 verifies first-match lookup), bind original/PC/NXDK-verified410d30 skin/material/glare selection and successful creation order) and model/world geometry, then AI reselection408ac0; complete ground dispatch, moving region entry, factory and attachment lifecycle remain open.
- [ ] Connect native-verified registered-NPC body sweeps and swept-bounds/contact preparation49f8aa/49fd84 to NPC physics scheduling; bind complete original/PC/NXDK-verified49d7e0 SP contact orchestration to live stance/ground (compose verified4a0a5c support finish with native-verified retained NPC ground queries/fall transitions, native-verified registered NPC49cd80 force suppression/nonlethal damage binding and class128 impact audio, completing lethal damage effects and associated-player feedback, and original/PC/NXDK-verified419830 landing orchestration with live sound/player-flag, native-verified registered NPC standing/crouch with required ground services and post-ground clock publication, remaining normal/slow/special and relative callbacks), player-flag, crush and impact-damage backends using native-verified NPC speed/motion request bindings and resolved actor75c attachment, attach native-verified registered player/NPC response publication to scheduled pair processing, extend binding to other actor families, then dispatch responses and publish flags even on general-response deferral returns.
- [ ] Bind verified pair processor48ca60 to live scheduling and contact/response effects after PC/stock64MiB XEMU verification of registered NPC/player views and borrowed player model publication; connect native-verified registered player/NPC48ca60 normal/general/model dispatch (27 staged calls, including reversed model pairs) to live pair scheduling, then extend native-verified NPC support-velocity refresh to accepted live support ownership, bind verified model dispatcher503120/5031f0 to native-verified owned static geometry queries, then bind original-verified54e140 coordinate preparation and prepared type2 geometry, bind native-verified static collision resource residency and queries to live scene owners, and decode remaining110c21 rendering batches while preserving original-verified authored NaN-plane rejection, and bind original-verified skeletal matrix-refresh/query composition to evaluated NPC poses through native-verified registered-model collision caches (78 owners/6501 refreshes with complete retirement), and connect native-verified actor/model contact publication (49afe0 through5031f0 and retained NPC geometry; player/NPC callers) to pair scheduling, extending native-verified stationary NPC demand evaluation to moving actors while establishing any nonzero pending-root producer (direct reference audit found none), and dynamic LOD selection, then bind solid response49b570 to geometry/cache services and extend model response49afe0 beyond retained player/NPC callers, add trigger/projectile views and moving NPC orientation ownership, bind original-verified full object-family classification48be00 to live eligibility and owner facts, bind original-verified four-plane producer48bbe0 to projectile discovery and live owner lookup.
- [ ] Compose registered-NPC death entry with collision teardown and remaining stages; connect the original-verified 8192-record collision pool after binding original-verified full classification48be00 (including actual trigger creation eligibility48c8e0) to live family/owner facts, binding original-verified allocation-order object lists (4872eb/4867bc), connecting verified authored section dispatch and original/PC/NXDK-verified shared entity-record creation stage (including immediate endgame secondary allocation) to actual per-handler factories and post-load player creation, then binding the original-verified discovery traversal to object-list ownership and creation/unhide scheduling.
- [ ] Bind complete death scheduling and effective class views after the composed base NPC death-animation/action-sound fixture passed PC and stock64MiB XEMU; extend beyond the single authored miner case.
- [ ] Connect the tested registered-model bone-clear adapter to full death dispatch and recover original override producers, then verify enabled overrides in XEMU; baseline stock64MiB replay passes with the new storage.
- [ ] Compose the verified ordinary-SP death stages using the retained NPC death fields, composed base/unarmed animation stage and tested registered-NPC tail adapter, completing remaining shared field ownership, accounting for the 6fc4d8 alternate-mode gate and remaining live resource callbacks.
- [ ] Reconstruct riot-shield impact-mark allocation/clipping/rendering and bind verified player-slot cleanup to the original-audited 50-mark/150-polygon/500-vertex pools.
- [ ] Connect the authored-tested file-tag/room-query/shared-lightmap source composition to death dispatch and bind room queue scheduling; isolated authored texture/growth draws now pass PC and stock64MiB XEMU, while campaign source ownership and queue scheduling remain open.
- [ ] Recover generic cached CSPH tag parent initialization/placement; the95-model original audit clears cached-tag precedence for all190 corpse eye/spine queries.
- [ ] Complete live corpse creation dispatch; registered NPC corpse sources/lookups now use selected motion/declaration views, including weapon overlays.
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
