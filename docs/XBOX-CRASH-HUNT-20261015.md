# Xbox crash hunt — October 15, 2026

The user authorized targeted runtime crash hunting and virtual gameplay input,
including forcing environment/encounter placement. Xbox stock 64 MiB remains the
runtime target. No PC binaries, host input or image capture are used.

## L9S4 creature-cluster stress

Read-only authored census selected levels2.vpp/L9S4.rfl,Reeper1197 at distance8,
near Reeper6837,baby Reepers1198/1199,and several bats. Selection evidence is
artifacts/crash-hunt/l9s4-encounter-selection.json; original32 actors are retained.
Run20261015-224043-L9S4.rfl uses finite rifle loadout2,ordinary bounded aim and
game-owned combat phases. It completed1800 live inputs with44 player shots,
eight enemy attacks,five recorded kills,six weapon switches and up to four
simultaneously alerted actors. No deaths or reported guest error. All applicable
combat qualification counters passed. This extends bounded coverage to a
creature-heavy authored encounter; no visual/audio or overall-stability claim.
A grenade-loadout follow-up uses the same actors and placement for blast/effect
coverage without changing health or replenishing ammunition.

Grenade follow-up20261015-224331-L9S4.rfl completed1800 inputs:1709 live,
91 dead,two scene starts and one ordinary death/reload recovery. Sampled
per-scene maxima:four priming actions,three releases,three detonations,one blast
kill,six enemy attacks and four simultaneously alerted actors. The generic
12-shot firearm gate is false; repeated explosive release/detonation and enemy
activity are directly observed. No reported guest error. The1445 grenade
collision contacts are unusually high and retained as an unverified behavior
lead,not assumed to be a stability failure. No terrain edits/debris were observed;
this level has not established destruction coverage. Compact telemetry and
lifecycle-summary.json are retained. No new code or visual/audio claim this batch.

## L11S2 noise investigation invalidated by a navpoint change

Run222430 reproduces RF_FORMAT at input836 and locates campaign_script_step.
Run222846 narrows it to scene_noise_live for slot16. Focused run223300 records
UID10373,owner validation success,active1/follow2,no authored route/event owner,
retry0,retained count0,route index1,AI state12,and matching fixed noise target.
The live invariant requires state1 when retry is zero. All three runs retain
25 player shots and18 enemy attacks at the failure.

Source cause:campaign_navpoint_changed clears script_move.retry on every active
actor when any node opens. For noise investigation that field is the30-tick
listening delay paired with state12,not a failed navigation-search backoff.
The callback erased the delay but left the state waiting; next simulation
validation correctly rejected the inconsistent pair.

Fix:preserve a positive delay for an active noise investigation when a node opens.
Its normal countdown owns the state12-to1 transition. Ordinary failed-route
retries and retained-route invalidation keep their existing behavior. A compact
counter records preserved delay,UID and state. Broad temporary call-site tracing
has been removed; focused noise telemetry remains. Xbox build passed. The
identical1800-input authored combat sequence is the runtime validation.

Validation223634 completed all1800 live inputs in171.36seconds on stock64MiB,
with57 player shots,18 enemy attacks,seven weapon switches,two simultaneously
alerted actors and no death or reported guest error. The delay-preservation
receipt[3,10373,24,12] confirms the navpoint callback was exercised three times
while the guard was waiting. Final noise receipt has retry0/state1,owner status0,
and matching fixed target. This validates the repaired transition and passes
the former input836 failure point under the same authored encounter. It does not
prove general campaign stability or resolve the separate Drone visual issues.

## L11S2 authored-cluster startup failure

Selected levels2.vpp/L11S2.rfl near guard10362,distance8,finite rifle loadout2,
aimed game-owned combat1800 inputs. Authored census has32 actors; local cluster
includes guards10361/10362/10363 and mutant10357. Spatial selection evidence is
artifacts/crash-hunt/l11s2-encounter-selection.json; proximity alone does not prove
LOS or simultaneous AI activation.

Runs220722 and220932 failed before input0 with RF_RANGE. Application load-stage
logging locates scene stage14:campaign_clutter_materials_open. Run221242 traces
stage5(the64px retry),budget1048576,persistent1296,scratch15808,104 material slots,
metadata staging peak186994,texture-loader allowance1031472. Failure is
counter_01.tga(entry196652bytes,archive index0). The reduced-material loader still decoded
the full image before reduction. Reused existing rf_image_tga_limited for reduced
TGA loads at supported limits<=128; its box averaging matches rf_image_reduce.
VBM and unrestricted decode remain unchanged. Xbox build passed.

Run221435 advances beyond counter_01 but still fails at later texture
2pt_Switch02_Off.tga(entry49196bytes). Thus the former1MiB cap also cannot retain
this authored material set at64px. Increased only clutter's bounded cap to2MiB,
matching the existing NPC-material cap; no actor removal or extra texture reduction.
Added successful material resident/peak counters to application logs. Full stock
64MiB runtime validation is required before declaring this startup failure fixed.

Run221708 clears startup with51 appearances,104 materials and81 textures;
clutter resident1068528bytes,peak1095568bytes,texture pixels1040384bytes. This
confirms the old1MiB cap was below the measured working set. The stock64MiB run
then reaches836 live inputs,25 player shots,18 enemy attacks,four recorded kills,
and two simultaneously alerted actors before a separate RF_FORMAT failure. The
initial material-load failure is resolved for this encounter; the complete run
is not a pass. Enemy-failure counters remain zero,so existing gameplay stage and
preview failure diagnostics are added to the same application log for the next
identical reproduction. No additional emulator failure-time inspection protocol.

Run222020 reproduces the second failure at exactly input836 with the same
25 player shots/18 enemy attacks. Profile receipt[835,6] places it after
presentation in the simulation-step block; rf_preview_failure and enemy-failure
arrays remain zero. Next add failure call-site receipts within that block before
changing gameplay. No claim that L11S2 completed1800 inputs. Startup and repeated
combat have progressed farther, but the new deterministic error is open.

## Airborne Drone contact gap fixed

A normal forward-and-jump replay adds one jump edge at input12,with no shooting,
health changes or host input. Before the fix,run20261015-215707-L3S4.rfl completed90
inputs but only four player-first Drone queries. Last-live player mode3/index3
was rejected. rf_movement_fall selects descriptor3 for an ordinary player; the
shared contact admission incorrectly required grounded index1 even for the Drone
full3D path. Airborne movement therefore bypassed both contact directions.

The scoped fix admits player index3 in common identity/life admission,then keeps
non-Drone pairs grounded-only in each directional pair filter. No class,health,
registry,hidden/linked or original pair-rejection gates were removed. Drone uses
the same oriented sphere sweep and existing response; other NPCs retain lateral
contact. Xbox compile/link passed(build-drone-airborne-contact.log).

Identical post-fix replay20261015-220005-L3S4.rfl completed90 inputs without a guest
failure. It recorded59 player-first queries,two accepted contacts and last-live
player admission1 in mode3. Final eye X44.373631 versus pre-fix46.139339 at the same
Y8.196915,Z-26.971186. Three raw ray hits,two selected contacts; nearest proposed
segment gap-0.0572317 signals an intersecting proposal,not committed penetration.
The player ended1.765708m farther back. This is direct scoped contact evidence,
not full animated-mesh parity or a claim that every reported overlap is solved.
Melee still damages at57 and kills at58; melee reach remains distinct from body
contact. The white/yellow effect remains unresolved. The renderer source review
found ordinary source-alpha/inverse-source-alpha blending for the solid flash,
with explicit RGB/alpha submission; no speculative rendering patch was made.

Post-fix mixed-combat regression20261015-220211-L3S4.rfl completed600 inputs:
509 alive,91 dead,two scene starts and one death/reload recovery,with five player
shots and two enemy attacks as sampled per-scene maxima. At input480 the log
includes one Drone missile launch and one positive Smash contact. No reported
guest error. This exercises combat and reload after the airborne change but is
not simultaneous multi-enemy stress qualification or visual confirmation.

## Sustained guard/drone run and reported flashing

Run20261015-214653-L3S4.rfl completed1800 inputs on stock64MiB with1589 live
inputs,211 dead inputs,three scene starts and two death/reload recoveries.
Sampled per-scene maxima include12 player shots and3 enemy attacks; simultaneous
multi-enemy pressure and repeated-enemy-fire gates remain unmet. No reported
guest failure. This is bounded evidence, not overall stability.

The nearest-separation receipt records the closest point along the permitted
translation segment for each sphere pair, respecting the earlier world-hit limit.
There were1257 NPC-first and812 player-first queries,zero raw ray hits and zero
accepted contacts. Closest gap1.2606314: Drone sphere center(45.724945,11.641300,
-26.062033),radius2; player sphere center(46.174976,12.679989,-22.360859),radius0.6;
Drone delta(0.0110245,0.0407076,0.0906715),world limit0.0988842. This recipe does
not reach body contact. It cannot validate the response or negate the user's
observed overlap. Compact results and lifecycle-summary.json are retained.

The user additionally reports the screen turning white and flashing yellow
before death. Record this as observed behavior, cause unconfirmed. The authored
Drone secondary is a15-damage missile with blast radius2; its primary Smash has
base bash damage1600 before AI/difficulty/target scaling. There is no nuclear
weapon selection in this owner. Ordinary player damage flash requests red,
RGBA(255,0,0,128). New logging includes existing Smash/secondary counters and
submitted full-screen flash ARGB/alpha, to distinguish weapon damage from effect
presentation. No images, host input or visual-correctness claim are involved.

The follow-up20261015-215337-L3S4.rfl completed90 inputs(58 live,32 dead).
Drone Smash counters show one start,one armed strike,one due contact,one body
contact and one positive damage dispatch at input57,status0. Drone secondary
shows54 qualified attempts,all held by geometry/LOS,zero launches. Player death
is recorded at58. Thus this close approach reproduces a melee death,not a missile
or nuclear blast. Flash telemetry:90 draw calls,33 active draws,last ARGB0x40ff0000,
last alpha64,maximum alpha128,status0. These sampled/submitted values establish a
red damage overlay in this reproduction; they do not establish the rendered
white/yellow effect's cause or disprove the user's report.

Nearest permitted body gap is0.1874573m,between player sphere center(45.255215,
8.292233,-26.971186),radius0.15,and Drone center(46.349720,10.390756,-26.971186),
radius2,with forward delta0.0649872 and limit1. There were49 player-first queries
and zero hits.
The attack's reach can produce melee contact before physical body contact; these
are distinct paths. No contact-response validation is claimed. Both Xbox builds
passed; only diagnostics changed this turn. Next investigate effect presentation
and obtain an actual body-contact case without weakening authored collision gates.

## Live admission investigation

Runs20261015-213517-L3S4.rfl and20261015-214035-L3S4.rfl completed60 inputs
on stock64MiB:58 alive,2 dead,one player shot and one enemy shot,without a
reported guest failure. These are diagnostic probes, not sustained stress passes.
The preserved last-live player admission is accepted1,mode1,index1,health100,
object flags8,unlinked,allocated body396bytes. This disproves player admission
or initial height as the explanation for zero Drone pair queries in this recipe.
The second run adds Drone owner admission tracing, but its call count remains0:
the live query never reaches that owner before death. The combat phase is
stationary for its first60 live ticks; the player dies at58. A forward-walking
replay from the first input is the next focused test, without shooting or changing
health, enemy state, or host input. The owner trace records identity,flags,mode,
health and admission stage without changing eligibility. Xbox compile/link passed
(build-drone-owner-admission.log). The collision bug remains open.

The movement-first follow-up20261015-214242-L3S4.rfl completed60 inputs,
58 alive and2 dead,with zero player shots and one enemy shot. Player eye moved
from X42.34972 to45.38519 at Y8.19691,Z-26.97119. Drone owner trace is
[49,4,3,1159479296,33554448,33554448,0,2147483768,11,11,4294967295,348,18,0,31,1]:
all identity gates pass,live fly11 owner reaches accepted stage4. The sphere
path ran49 player-first queries and zero accepted contacts. NPC-first count0.
This demonstrates that the stationary probe's missing query is a coverage issue,
not evidence that Drone eligibility is broken. It does not resolve the user's
observed overlap. Next inspect committed sphere centers, relative separation and
movement proposals during a representative approach; retain world-hit precedence.
The harness adds --input approach using the existing RFI6 process-local replay,
ordinary forward movement .65 from the first frame,without firing or OS input.

## Pink floor material identified and invisible rendering corrected

Original L3S4 geometry at the reported player's eye(49.22456,13.69607,-22.43431)
places grate faces5030/5031 atY12.02999, texture mtl_grate01_A.vbm, flags0xc8.
Collision/detail surfaces6117/6118 occupy the same footprint atY11.875/12.125,
texture cem_invisible03.tga, flags0x2108. The original maps2.vpp texture is32x32,
24-bit TGA with bright red/pink pixels (common RGB223,0,74), SHA256
1d8a6176ecc1b52a1814631a76e565843d7871ab59599e415588bb01ab7a0db2.
This is authored texture data, not rf_image_missing or the renderer's white fallback.
The user's screenshot plus source geometry strongly identify the unwanted surface;
no new image was captured or generated. Compact source evidence is retained in
artifacts/crash-hunt/l3s4-invisible-surface-review.json and l3s4-floor-materials.json.

Existing original-code research identifies facebit0x2000 as invisible. Rendering
only checked show-skybit1 and missed it. Added the invisible bit to both retained
Xbox world preparation loops, shared preview face admission, and sky geometry.
Collision inputs remain unchanged. All116 authored invisible faces in L3S4 are
now excluded by those render filters; the grate's flags remain admitted.
Native build passed;20261015-212646 completed600 inputs,509 live/91 dead, five
sampled player shots and one normal recovery without fatal error. This establishes
integration, not rendered visual verification; the pink disappearance remains
USER visual confirmation while images are prohibited.

The former near-drone placement wasY11.01576 over a floor atY6.52999. Added bounded
--spawn-y, valid only with --distance, to stage only the private player-start
height. Original actors/assets are unchanged and the recipe/start receipt records
the value. Next contact probe uses7.53 to avoid the prior long initial fall.

The height-adjusted near-drone run20261015-213023 also timed out during reload
with58 live/62 dead inputs and no admitted pair query/contact. It ended at the
same eye position(42.34972,8.19691,-26.97119) as the prior near-drone run. Thus
initial height was not established as the coverage failure's cause. The earlier
height hypothesis is not a resolved diagnosis. Next record live admission state
(player mode/life/identity and Drone flags/pair eligibility) rather than repeat
placement variations. No fatal error logged; collision bug remains unverified.

## Drone collision implementation and incomplete validation

Implemented a bounded live Drone fly11 contact path for both movement directions.
It sweeps oriented source spheres against the other body's committed spheres in
full3D, preserves earlier world hits and uses ordinary contact/stop consumers.
Only exact authored/current Drone fly11 extends the old grounded owner filter.
Original pair rejection remains; admitted flags2/4 model-contact branches use
this explicit first-pass sphere adaptation rather than a full animated mesh.
Player admission remains grounded. Angular-only motion, preexisting overlaps,
dead-body contact and general flying actors are not covered.

The first native run20261015-210750 and traced repeat211054 both failed RF_FORMAT
at input27. New admission tracing identified slot3 Drone with mass1000 but zero
body spheres/radius. Class sphere declarations are name-based overrides; they
do not create absent compiled model CSPH geometry. The original Drone table
explicitly enables collide_player/collide_entity and authors Collision Radius2.

Added that optional authored radius to class physics parsing, with a Makefile
header dependency to invalidate all ABI consumers. When the exact Drone fly11
model produces no spheres, initialize one centered sphere using its positive
finite authored radius. Existing model spheres are preserved. This is a bounded
first-playable collision proxy, not recovered exact mesh geometry. The ten-word
rf_scene_drone_player_contact log records NPC queries/hits, player queries/hits,
last admission side/slot/count/material/mass/radius. Radius values use float bits.

Corrected native build passed. Run20261015-211602 (guard2049,distance8,shotgun,
aim,combat) completed600 inputs with509 live,91 dead and one normal recovery.
The log confirms count1/radius2 and active queries; no fatal error. Contact hits
remained zero, so this is initialization/integration evidence, not proof that
the observed overlap is fixed. Near-drone run211944 (Drone844,distance4) timed
out during reload after58 live/62 dead inputs; no admitted pair query or hit.
The closer placement therefore failed to provide contact coverage. Next use a
grounded approach with measured relative positions and accepted pair contacts.
Do not report the collision bug resolved or refresh tester delivery yet.
The pink surface's material remains unconfirmed. No images or host input used.

## User-observed L3S4 drone/player collision bug

Run `artifacts/crash-hunt/20261015-205144-L3S4.rfl` used actor2049 at distance8,
finite shotgun loadout4, bounded aim and game-owned combat with the HMP reader.
The user directly observed the floating robot move into the player and supplied
a screenshot of the resulting death view. Treat that behavior as observed, not
an inference from the screenshot. The screenshot also shows a bright magenta
patterned surface; the user suspects a default texture, but its identity remains
unconfirmed. Built-in rf_image_missing uses muted gray-purple, while the renderer
fallback is white; trace the actual material/decoding rather than assume either.
No new captures were taken.

Source finding: scene_player_npc_owner in scene_player_npc_contact.inc requires
movement index1; both player-first and NPC-first contact use it, excluding Drone
fly11. campaign_script_movement uses scene_npc_live_body_sweep even for xyz
pursuit, but the contact compose filter returns before considering the drone.
The existing contact shape is lateral-only and grounded; simply allowing mode11
would not provide correct full3D flying-body contact. Implement a scoped drone
contact path with actual body geometry and preserved world-hit precedence, then
validate relative positions/contact under this same encounter. No fix yet.

Parent stopped only verified owned XEMU PID30728 after this concrete report.
Result preserves PROCESS_EXITED as harness_terminal_status and explicitly labels
STOPPED_FOR_OBSERVED_COLLISION_BUG. Last captured activity:720 inputs,418 live,
302 dead, three starts/three deaths; shotgun slot3 observed with firing/hits/kill.
No fatal game error was logged, but this is not a completed or qualified combat
check. Rapid deaths and missing collision make repeated runs poor sustained-load
coverage until the contact defect is addressed. Existing local work is preserved.

## Terrain admission rejection localized

Added a ten-word application-log receipt for rejected owner UID, cut index,
status, detail-guard count and cutter bounds. It resets at each template-edit
attempt and is filled only when scene_authored_cuts_admit rejects. No geometry,
admission or gameplay rules changed. Native Xbox compile/link passed; runtime
confirmed the new receipt through the unchanged OpenJKDF2 HMP reader.

Run `artifacts/crash-hunt/20261015-203337-L1S2.rfl` completed600 live inputs,
six rocket launches and35 enemy shots with two simultaneously alerted enemies.
Recipe: actor9774 at distance8, loadout5, explosives, aim-point(124,-1,4).
No fatal error, terrain edit or debris was observed. The receipt identifies
owner8123, first cut, RF_NOT_FOUND from cavity admission, with no detail guards.
At sample120 its cutter bounds were(120.42709,-1.34916,0.83029) through
(126.41105,2.70753,7.53278).

Read-only parsing consumed all389 original L1S2 brush records. Comparing the
logged cutter bounds against authored brush bounds identifies overlap with8221,
10001 and10004 at that sample. cavity_admit checks all other brush bounds before
surface-window admission, so these overlaps explain rejection. Bounds analysis
uses Python double transforms rather than the runtime float rounding; overlaps
are substantial, not boundary-tolerance cases. Compact admission-review.json,
l1s2-brush-bounds.json and source-section hash are retained under crash-hunt.
This is a supported-geometry limitation, not evidence of a crash. Do not remove
the guard or claim this wall exercised destruction.

The next target(120,-1,-17) was selected from original brush bounds with a3.8-unit
heuristic envelope free of other brushes. This is a placement heuristic, not a
proof: actual randomized cutter bounds and publication must be verified at runtime.

The follow-up `20261015-203606-L1S2.rfl` completed1800 inputs,1589 live and
211 dead, with three scene starts and two deaths. Sampled per-scene maxima:
six rockets,48 enemy shots, two simultaneous alerts, one committed terrain edit
and16 debris pieces. A committed cut/debris was observed before and after the
first reload. Deeper cut index1 overlapped brush8229 and was rejected by cavity
admission; repeated cuts within one scene remain uncovered. No fatal game error
was logged. This confirms combined destruction/combat activity, not overall
stability or visual/audio correctness. The generic12-shot gate remains unmet.

The user reported another security interruption during this follow-up. Prior
statements that no tool-level rejection appeared do not contradict or supersede
that app-level report; the reason is unavailable. Rechecked the reference reader:
SHA256 is identical in both repos,092dc553be9eee98ee566e1189f6d3511e4b0e5cfc272876c4035ccbe76f4b95.
OpenJKDF2 tools/xbox_xemu_smoke.ps1 defaults to15-second polling, launches the
localhost HMP monitor and calls this reader with timeout3. Red Faction matches
those settings and disables host input. Its Python orchestration, game-owned
input sequence and JSON log contents are adaptations, not an identical complete
OpenJKDF2 harness. Do not claim this transport prevents security interruptions.

## Completed grenade encounter with HMP logging

Run `artifacts/crash-hunt/20261015-202316-L2S2a.rfl` completed the original
2400-input target within the600-second watchdog. Recipe: levels1.vpp/L2S2a.rfl,
actor8394 at distance8, finite grenade loadout6, bounded aim and game-owned
explosive phases. All57 original actors were retained. This uses the unchanged
OpenJKDF2 HMP reader and the same diagnostic XBE as the completed L17S1 run:
SHA256 de045fc298d6d67585f9fe4bfa6d13727de92b912ea1eac86ab48754008500cc.
The game reported67108864 bytes of memory.

Observed2069 live inputs,331 dead inputs, four scene starts and three deaths.
The lifecycle receipt confirms all three recoveries with unchanged live-phase
counters during sampled dead intervals. Per-scene sampled maxima: three grenade
releases, two detonations, twenty bounces, two blast kills,22 enemy shots and four
simultaneously alerted enemies. These are not cumulative totals across reloads.
No fatal game error or tool-level security rejection appeared. The harness
completed and stopped its owned XEMU normally.

Terminal completion and repeated explosive/enemy activity are established, but
`scenario_qualified` remains false: the generic repeated-player-fire gate requires
12 shots per scene and this finite-grenade run recorded three. Do not weaken that
gate or treat completion as overall stability. Zero terrain edits/debris were
observed; L2S2a still has no connected terrain cutter. Visual/audio correctness
remains unverified. Next investigate the preserved L1S2 publication rejection
and continue distinct weapon/environment loads rather than repeating this same
now-completed timeout recipe. HMP's faster completion is an observation, not a
controlled attribution of the earlier timeout's cause.

## L17S1 completed combat/reload follow-up

Run `artifacts/crash-hunt/20261015-201206-L17S1.rfl` completed1800 inputs
using the unchanged OpenJKDF2 HMP reader, game-owned combat phases and bounded
aim. Recipe: levels3.vpp/L17S1.rfl, actor20026, distance10, finite rifle loadout2,
600-second watchdog. All ten original actors were retained; only the private
player-start transform was staged. Runtime memory receipt confirms67108864 bytes.
XBE SHA256: de045fc298d6d67585f9fe4bfa6d13727de92b912ea1eac86ab48754008500cc.

Observed1229 live and571 dead input frames, six scene starts and five deaths.
The offline lifecycle receipt confirms all five deaths recovered, with no live
phase progression during sampled dead intervals. Per-scene sampled maxima were
12 player shots,10 enemy shots and two simultaneously alerted actors; these are
not cumulative shot totals. All bounded combat qualification gates passed.
Native terminal success was observed before the harness stopped its owned XEMU;
process exit1 reflects that teardown, not a guest crash. No fatal game error or
tool-level security rejection appeared. Visual/audio correctness is unverified.

This closes the interrupted L17S1 encounter check, not the overall crash hunt.
Next prioritize a completed grenade encounter and the concrete terrain-publication
limitation; do not count this rifle-only case as destruction/debris coverage.

## Reopened: representative action loads

The user explicitly rejected treating two fixes and short probes as completion.
The goal is active again. Force player placement into original authored action
areas, retain original enemy populations and measure actual enemy/player activity.
Recipe3 adds repeated strafing/fire/reload/weapon cycling and ordinary Use edges
for diagnostic death recovery. It does not alter health, damage or enemy state.
Frame completion alone is not a combat stress pass.

L3S2 (27 original actors, player staged near guard941) and L3S4 (25 original
actors, player staged near env_guard2049 beside riot_guard8038 and Drone844)
both fail before gameplay at NPC initialization. Evidence:
artifacts/crash-hunt/20261015-152345-L3S2.rfl and
artifacts/crash-hunt/20261015-152539-L3S4.rfl. Neither counts as combat coverage.

The traced L3S2 run artifacts/crash-hunt/20261015-152917-L3S2.rfl identifies
actor index0 patrol resource admission, scene_authored_patrol.inc line121.
The patrol initializer demands campaign_npc_motion_data/count at load stage24,
but campaign_npc_motion_residency creates those at stage30. Move initial patrol
activation after motion residency for registered actors; preserve all admission
checks and the original actors/routes. Runtime results follow below.

The ordering fix reaches gameplay in both environments:
20261015-153115-L3S2.rfl recorded two alerts, four enemy shots and player death;
20261015-153502-L3S4.rfl recorded five player shots, two enemy shots and player
death. Both hit the180-second watchdog during the subsequent ordinary reload.
They validate removal of the startup blocker, not sustained combat stability.

The diagnostic-only crash-loadout.bin supplies pistol/rifle/shotgun through
ordinary finite-ammo acquisition and selects one. Tester-profile code ignores
this selector. Damage, health and actor population are unchanged. The harness
can additionally stage the player3..20 units ahead of an authored actor by
altering only the48-byte player-start transform in a private scene-fixture.vpp;
original archives are read-only. The result records the changed level hash and
start transform, alongside an original actor census. A first L17S1 attempt used
an unsupported archive name and was rejected before loading; it is classified
as a harness staging error, not a game crash.

L17S1 ten-unit staging (20261015-153833-L17S1.rfl) reached10 live registered
actors and2 simultaneously alerted actors,7 player shots,9 enemy shots and
one observed death. It timed out after a normal reload resumed gameplay;
the1800-frame target was not reached. Recipe4 changes held rifle fire into
repeated trigger edges with a stationary opening, because the old held input
only produced a single burst. The harness now recognizes native terminal
success plus global replay consumption, since per-scene draw counts reset on
ordinary reload. No aggregate stress pass is established by these results.

The desktop later reported Goal stalled; get_goal confirmed status blocked.
This is not a repository/emulator blocker, and the next run was started manually.
The available goal tools cannot resume a blocked goal; automatic continuation
requires the user-controlled resume action. Do not mark this hunt complete.

Recipe4 L3S2 at8 units (20261015-155348-L3S2.rfl) observed22 live registered
actors and3 simultaneously alerted actors,6 player shots with2 kills,12 enemy
shots and one observed death. It again reached the180-second watchdog during
the death/reload cycle. No new fatal error was observed; sustained combat and
complete repeated reload coverage remain open. Next work must increase useful
live-action coverage rather than counting dead/loading samples as stress time.
The tester package now includes the patrol-order fix; its current hashes are in
artifacts/delivery-prep/crash-action-result.json. Earlier receipts are historical.

## First failure

The user-driven tester session stopped with RF_FORMAT (-2) at simulation frame
13837. Live QMP evidence identified enemy combat, not player firing; the player
was unarmed. Evidence: artifacts/xemu/manual-smoke-20261015-135713/failure-full.json
and failure-subsystems.json. The unchanged tester XBE hash is
802d54eb73a1a861c395823206a0126beb35cbe80e4b185bf4ad7fbf09faeb1c.
Its matching original map is preserved as build/xbox/tester-original.map; subsequent
diagnostic builds write build/xbox/main.map.

A bounded diagnostic spawn near authored guard UID8462 in L1S1 reproduces the
same error at frame172. First trace: artifacts/crash-hunt/20261015-141306-L1S1.rfl.
Fine trace: artifacts/crash-hunt/20261015-141603-L1S1.rfl. The enemy failure receipt
identifies slot1, UID8462, weapon3, scene.c line13000. Tactical receipt identifies
route publication line32 in the traced source.

Root cause: scene_tactical_route_publish reads campaign_navigation.references,
whose order keys are zero-based (src/core/level.c navigation_pass). It incorrectly
required the one-based keys used by rf_level_navigation_workspace_open. Valid
sampled movement destinations therefore failed publication and ended the whole
session. Correct the authored-reference checks; retain workspace checks and all
identity/geometry admission. Validation results are recorded below as they finish.

The fixed guard probe completed600 consumed frames, recorded24 enemy shots and
normal player death at475, with no enemy/tactical failure:
artifacts/crash-hunt/20261015-141846-L1S1.rfl/result.json.
L1S2 then completed900 consumed exercise frames,87 sampled distinct eye positions
and one shot: artifacts/crash-hunt/20261015-142407-L1S2.rfl/result.json.
These are bounded functional probes, not campaign completion.

## Empty-environment failure

The ctf06 developer environment failed with RF_RANGE before consuming input.
The traced run artifacts/crash-hunt/20261015-142733-ctf06.rfl/result.json identifies
campaign_npc_bodies_open at load stage24. Body/class charge3236 plus374640 scratch
fits the unchanged655360 budget, with zero actors. The noise metadata loader
unconditionally calls scene_blind_classes_read, which rejected a null class
array even when class_count is zero. Require a class pointer only for nonzero
count, preserving validation of populated metadata. The fixed run
artifacts/crash-hunt/20261015-142925-ctf06.rfl/result.json completed900 consumed
frames with41 sampled distinct eye positions and20 shots. Weapon cycling was
requested during reload but no successful switch was recorded; do not count
this as a successful weapon-switch test.

The second firearm profile also completed900 frames with42 sampled positions
and25 shots (artifacts/crash-hunt/20261015-143121-ctf06.rfl/result.json).
Input recipe2 moves the cycle edge to phase420 and reload to450, so the switch
can happen before the reload lock. The original recipe used the reverse order.
Recipe2 completed900 frames,41 sampled positions,25 shots and one successful
weapon switch with no fatal error:
artifacts/crash-hunt/20261015-143312-ctf06.rfl/result.json.

## Updated delivery

The fixed stock64 tester package is build/xbox/tester-disc, with matching
build/xbox/tester-main.map and build/xbox/redfaction-tester.iso. The original
XBE/map remain at build/xbox/tester-original.xbe and tester-original.map.
artifacts/delivery-prep/crash-fixes-result.json holds new hashes; the original
result.json remains historical. Runtime probes use the same compiled code with
diagnostic flags; the updated interactive frontend has not been rechecked.
Hardware has not been deployed or tested. Visuals/audio remain unverified.

The shell wrapper encountered existing CRLF line endings; packaging succeeded
through its equivalent stock64 make target with the existing validated asset
membership. No installed toolchain or original assets were edited.

## Harness

### User-requested OpenJKDF2ogx logging path

The user requested the OpenJKDF2ogx workflow directly after recurring Daybreak
interruptions. The default runner now uses `--log-protocol jkdf2-hmp`: the unchanged
`scripts/xbox/poll_xemu_ram_log.py` reader is vendored under
`tools/vendor/openjkdf2`, with its license and source hashes/commit in
`PROVENANCE.json`. The application mirrors its64KiB RAM buffer, header symbols,
magic words and circular writer. OpenRedFaction supplies its own JSON gameplay
records at frame boundaries (every60 consumed inputs, plus startup and return).
The reader uses an HMP TCP monitor, including the reference15-second initial wait
and polling interval. This mode does not connect QMP or read individual gameplay
structures from the host. Stock64 confirmation comes from the game's logged
MmQueryStatistics result. Raw RAM-log text and decoded records are retained.

Integration differences are explicit: LLD's redundant address padding is removed
in a separate poll-build.map for the unchanged reader; Red Faction has different
gameplay records; tester mode ignores the opt-in log flag; images and host input
are not copied from other upstream smoke-script branches. The adapter uses the
RAM mirror/debug-output path, not OpenJKDF2's hardware file-path fallback logic.
This is a protocol/workflow match, not a claim that the two games or full launch
scripts are identical. Map/XBE and reader hashes pin the run. Host poll timestamps
are collection times, not exact event times; partial JSON lines are discarded.

The first attempt stopped before launch because the upstream parser rejected
sixteen-digit LLD address formatting. After normalization, a first emulator attempt
exited before log collection (`20261015-195035-L2S2a.rfl`, exit0, cause unknown).
Its mistaken TIMEOUT label was corrected to PROCESS_EXITED. The harness now
distinguishes an early process exit from its watchdog. The next attempt includes
the upstream initial wait. None of these outcomes establishes a Daybreak cause.

The same build also compiles the previously prepared opt-in world aim-point
input and compact terrain-stage receipt. These are not selected as part of the
RAM-log transport check and have not yet received their targeted runtime test.

Validation: NXDK build passed (`build-jkdf2-ram-log.log`). Run
`20261015-195140-L2S2a.rfl` completed900 inputs using only the HMP RAM-log reader.
The game log confirmed67108864 physical bytes, sixteen complete log writes and
terminal boot_phase5. It recorded20 player shots and six enemy shots. The reader
used virtual addresses (no physical fallback), and its byte-identical SHA256 is
`092dc553be9eee98ee566e1189f6d3511e4b0e5cfc272876c4035ccbe76f4b95`.
Only one simultaneously alerted actor was sampled, so scenario_qualified remains
false even though the transport/input run completed. The optional OpenJKDF2 clock
probe is not implemented here; its missing-probe field is not a gameplay failure.
The host terminated only its owned emulator after completion (host exit1); no
project XEMU remained. No tool security rejection appeared during this run, but
that does not establish the cause or resolution of earlier Daybreak UI messages.
Visuals/audio and sustained stability remain unverified.

The prior enemy-directed L1S2 run (`20261015-193524-L1S2.rfl`) completed1800
inputs with1709 live and91 dead inputs, six rocket launches, twelve enemy shots,
two simultaneously alerted actors and one death/reload. Its loaded terrain owner
received an edit attempt but reported RF_NOT_FOUND (-3) without a committed cut. The new
terrain-stage receipt and aim-point path are prepared to investigate that result;
do not count it as destruction coverage or a fatal crash.

### L1S2 wall stress and publication rejection

`20261015-195551-L1S2.rfl` aimed ordinary virtual look/fire input at the recorded
wall contact124.314514,-2.12633848,-17 while retaining the39 original actors.
It completed1800 inputs (1709 live,91 dead), six rocket launches,54 enemy shots,
two simultaneously alerted actors, and one normal death/reload. No fatal error
was reported. The stage receipt showed initial terrain requests reaching stage9
(template edit) and returning -3; subsequent near-duplicate impacts were also
rejected at the existing history check. Correction: -3 is RF_NOT_FOUND, not
RF_RANGE (-4); the earlier description of this status was wrong.

An application-owned terrain-owner receipt now records affected source count,
last mutated UID/status, publication status, transaction status, active hardness
and any mismatching owner. It is serialized through the same HMP RAM log; no
host structure polling was added. Native rebuild validation also exposed a
missing Makefile dependency for ram_log.inc, now fixed so changes rebuild main.obj.

Focused600-input run `20261015-200229-L1S2.rfl` observed two affected owners,
successful mutation through UID8219, then RF_NOT_FOUND from publication and the
transaction. The transaction calls publication only after every mutation succeeds,
and rejection discards the private candidates. Compact terrain-review.json retains
the application records. This is a publication rejection, not a solver failure or
evidence of memory exhaustion. The exact inner publication branch is not traced.
Existing cavity-obstacle/surface admission and the special recorded second-Driller
cut restriction remain intact; no validation gate was removed to obtain a pass.

Run `20261015-200500-L1S2.rfl` targeted a different face at124,-1,4 and also
completed1800 inputs (1709 live,91 dead), with six rocket launches,55 enemy shots,
two simultaneously alerted actors and one death/reload. Its receipt showed only
source8123 affected: mutation succeeded, publication and transaction declined
with RF_NOT_FOUND. Thus paired-owner admission alone does not explain the failure.
Both full wall runs committed zero terrain edits and spawned zero terrain debris;
neither proves joint destruction/combat stress or overall stability. Visuals/audio
remain unverified. Continue crash coverage while retaining this concrete
publication limitation for a targeted integration fix, rather than changing guards
or repeatedly selecting arbitrary wall positions.

### Workflow revision requested after recurring security interruptions

Read-only comparison on October15:

- C:/Programming/GitHub/OpenJKDF2ogx/tools/xbox_xemu_always_on_soak.ps1
  prepares a declared MENU/LEVEL plan for one emulator session.
- C:/Programming/GitHub/OpenJKDF2ogx/src/Main/jkMain.c consumes that plan
  inside the game, with bounded scenario records and explicit transitions.
- C:/Programming/GitHub/OpenJKDF2ogx/src/Platform/Xbox/stdControl_xbox.c
  applies synthetic gamepad data before normal input gates and logs transitions.
- C:/Programming/GitHub/OpenJKDF2ogx/src/Platform/Xbox/xbox_debug.c writes
  application logs with important-event flushing and a bounded log mirror.
- D:/Programming/GitHub/OpenXML1xbox/docs/ISSUE-1-PERFORMANCE.md documents
  synthetic gameplay kept separate from optional physical input polling.

Adopt the architecture, not these scripts verbatim: their platform assumptions,
capture options and physical-input paths are outside this task's constraints.
OpenJKDF2ogx's XEMU log transport also reads guest memory, so copying it is not
evidence that a security rejection will disappear. No other repository was edited
or built. No emulator was launched during this workflow review.

Immediate implemented change: ordinary fatal results now save failure-report.json
from the already-collected application telemetry, with binary/map hashes. Routine
runs no longer invoke the broad CPU/register/instruction/stack snapshot. Existing
failure evidence and the separate diagnostic tool remain intact. This change has
Python syntax/source validation only; it has not been exercised by a new guest run.

Target architecture: one game-owned, opt-in scenario runner with explicit input
phases, gameplay-state transitions, finite loadouts and application-level event
reports. Start with the already-established authored encounters, then add the
remaining explosive/destruction cases. Require actual player/enemy activity and
scenario-specific events independently of elapsed frames. Count live, dead and
loading time separately; keep normal death/reload semantics. Host orchestration
owns only staging, one emulator process, a bounded watchdog, report collection
and cleanup. Application log transport remains an implementation decision; do not
claim live file logging or a complete multi-scenario runner is implemented yet.

First implementation slice: `--scenario combat` opts into a game-owned encounter
sequence in `scene_crash_scenario.inc`, disabled in the tester profile. A neutral
RFI6 replay supplies the bounded input budget; the game supplies engage, strafe,
reload, engage/jump and weapon-cycle inputs. Its live clock stops during death;
separate dead ticks issue released Use edges through normal restart handling.
No health, damage, ammunition or actor state is overwritten by this sequence.
The eight-word trace records version, phase, live/dead input ticks, transitions,
completed cycles, phase-local tick and restart requests. Loading never calls this
input hook and is not counted as live gameplay. The frame budget still includes
dead inputs; it is not a guarantee of a particular live duration.

Routine results now qualify this bounded encounter separately from completion:
at least two live phase cycles, twelve player shots, six enemy shots, two
simultaneously alerted actors, and repeated requested explosive use when applicable.
Shot counts are sampled per-scene maxima, so these gates can conservatively miss
activity spread across reloads. They do not prove multiple distinct enemies fired,
visual/audio correctness, sustained performance or overall stability. The previous
multiple-enemy flag incorrectly used accumulated alert events; it now uses the
simultaneously alerted population. Multi-encounter sequencing, app log transport,
and the remaining explosive/destruction coverage are still open.

Validation: stock64 NXDK cross-build passed (`build-scenario-phases.log`).
Run `20261015-190926-L2S2a.rfl` completed1200 inputs in149.188 seconds with
the rifle start, aim enabled and game-owned combat phases. It observed32 player
shots, six enemy shots, three simultaneously alerted actors and three completed
live phase cycles. The sequence counted1199 live ticks (frame zero is initialization),
zero dead ticks and twenty phase transitions. It passed the bounded encounter's
activity gates; no fatal result was reported. Death recovery was not exercised,
and this firearm run does not validate explosive coverage. Visuals/audio remain
unverified. XBE SHA256:
`6180a6c823fd52367929dd1d3d440510c21805872531034c53088ed5c08142ad`.
The process exited through owned teardown. This diagnostic-only build does not
replace the preserved hardware delivery payload.

Explosive sequence revision: run `20261015-191252-L2S2a.rfl` completed1800
inputs with34 player shots, fourteen enemy shots and three simultaneously alerted
actors, but only one grenade release. It correctly failed the repeated-explosive
coverage gate. There were no deaths or reported fatal errors. The combat sequence
does not keep a particular weapon selected after ordinary pickups/cycling.

New `--scenario explosives` mode uses ordinary cycle-input edges every30 live
ticks to return to the requested rocket/grenade while owned ammunition remains.
It suppresses firing during that selection search, disables the phase's voluntary
switch away while that explosive remains usable, and falls back to the combat
sequence after depletion. It does not directly select a weapon, alter pickup
policy, replenish ammo or modify health. Scenario trace version2 distinguishes
this implementation; the empty flag keeps combat mode, byte `2` selects explosive
focus, and other flag contents are rejected. Compact reports now also retain
sampled per-scene maxima for grenade bounces/detonations, terrain edits and debris.

Stock64 validation of explosive focus (`build-explosive-scenario.log`, run
`20261015-191630-L2S2a.rfl`): the360-second watchdog expired with1463 of2400
inputs consumed,1252 live and211 dead inputs. No fatal error was reported. Two
deaths each returned through normal reload into live gameplay (scene starts at
97.469,223.610 and351.891 seconds). The game-owned live clock remained532 across
the first death/loading interval and1115 across the second, then resumed. Its
final trace counted1249 live ticks,211 dead ticks, three cycles and four restart
requests; the three initialization frames explain its difference from live input
telemetry. Compact `lifecycle-summary.json` retains those transitions.

Sampled per-scene maxima: two grenade releases, two detonations, eleven bounces,
two blast kills, twenty enemy shots and four simultaneously alerted actors.
Terrain edits and debris were zero. The run is TIMEOUT, scenario_qualified false:
it did not complete the input budget and also failed the twelve-player-shot gate
(only two counted player shots). Repeated-explosive and recovery behavior is
observed, but this is not a completed sustained scenario or overall stability
result. Do not inflate per-scene maxima into cumulative totals. Visuals/audio
remain unverified. XBE SHA256:
`1c607baa3e8c7587c369e90252219506f030ae13d29ac8614e4cd4038892691c`.
Owned emulator teardown completed. Remaining work includes completing a bounded
explosive sequence, selecting supported destructive surfaces for combat stress,
and evaluating action thresholds appropriate to finite explosive loadouts.

Destruction coverage investigation: `scene_terrain_open` only connects terrain
for the explicit developer paths and the L1S2 campaign Driller path; L1S1 has a
separate bounded campaign cut. Other ordinary campaign levels, including L2S2a,
load region hardness but return without connecting a terrain cutter. Thus this
encounter cannot establish destruction/debris stress regardless of grenade count.
Do not interpret its zero edits as proof of a projectile/collision failure or
claim generic campaign destruction coverage. The L1S2 Driller path admits authored
sources8123/8219; the original actor census has39 actors, including Driller8122
and nearby guards9774/9767/9768. Candidate positions are retained in
`artifacts/crash-hunt/l1s2-destruction-candidates.json`; distance alone does not
establish hostility, sight lines, accessible firing position or coverage. Existing
PC Driller scripts were read as setup evidence only and were not executed.

`tools/summarize_crash_lifecycle.py` reads saved telemetry offline and reports
observed dead intervals, frozen live ticks, and resumed live ticks in a subsequent
scene. It does not connect to XEMU or issue a gameplay pass verdict. Checked against
the earlier rifle run (no death intervals) and the360-second explosive run (two
recovered intervals, zero sampled live-tick changes while in phase7). Reports retain
the non-atomic sampling limitation; frame counts and these transitions do not
verify visual/audio correctness.

Longer-budget continuation `20261015-192343-L2S2a.rfl` retained the same2400-input
target and unchanged XBE but reached the600-second watchdog after1328 inputs
(1117 live,211 dead). The death live-tick boundaries532 and1115 matched the earlier
run. The first sampled death interval lasted from140.266 to321.735 seconds before
live gameplay resumed; the second began359.156 seconds and was still loading when
the watchdog stopped the owned process. Load stages continued advancing, so this
is not evidence of a fixed load-stage hang. It is TIMEOUT and unqualified; no fatal
error was reported. Sampled per-scene maxima remained two releases/detonations,
eleven bounces, twenty enemy shots and four simultaneously alerted actors. No
destruction or debris was observed. Do not claim that the larger wall budget
completed the scenario or improved useful live stress.

Routine telemetry now defaults to a one-second interval rather than0.3 seconds,
with explicit `--poll-seconds` bounds0.3..5. This reduces polling frequency and
records the selected interval in the recipe. It may miss short-lived states, so
observations remain conservative and are not exhaustive coverage. Python syntax
validation passed; the changed polling interval has not yet been runtime-tested,
and no performance improvement or cause of the slower loading is established.

Daybreak's triggering decision is not exposed here. Repository observations do
not establish its cause. This redesign does not change access controls, use a
different identity/model to avoid a rejection, disguise requests, or promise that
security interruptions are resolved. Preserve the session evidence for product
feedback if the legitimate gameplay work is being misclassified.

Diagnostic loadout5 (rocket) and6 (grenade) extend the same ordinary finite-ammo
acquisition used by the firearm starts. Their selected resource bit enters the
existing optional weapon resource planner; normal stock64 admission and all
firing, collision and damage checks remain in force. The tester profile still
ignores the diagnostic selector. Native compile/link succeeded.

Rocket-start L2S2a run20261015-183817 completed1800 inputs in283 seconds with
1547 live/253 dead input frames and two scene starts. Per-scene maxima were31
player shots,12 enemy shots,4 weapon switches and4 simultaneous alerted actors.
The first life launched3 rockets with3 impacts; the second life used other
weapons after normal cycling. No fatal error occurred. Terrain-edit/debris
counters stayed zero, so this is not destruction coverage. Blast-specific
counters were added to the next run; do not infer blast kills from hitscan counts.

Initial grenade-start run20261015-184328 completed1800 inputs but released no
grenades: ordinary acquire(-1) owns a clipless weapon without reserve ammunition.
The game correctly switched away from the empty grenade. Its result explicitly
sets requested_weapon_observed=false; it is excluded from grenade coverage.
The diagnostic fixture now supplies4 reserve rounds through the capped ordinary
pickup API after acquisition. This is a fixture correction, not a game crash fix.
The harness records rocket launches, grenade releases and blast kills and emits
requested_weapon_observed for explosive starts independently of frame completion.

Corrected grenade-start run20261015-184828 completed1800 inputs with1472 live
and328 dead frames, two scene starts, and no fatal error. It released/detonated
one grenade, launched one rocket, and recorded2 blast kills, with up to4
simultaneously alerted actors and11 enemy shots in a scene. Selection changed
from grenade to rocket while3 grenades remained; the cause of that selection
change has not yet been traced. Do not count this as repeated grenade or bounce
coverage (bounce count0). Terrain edits and debris remained0. Next work must
exercise repeated explosives and destructible combat terrain while preserving
ordinary ammo, collision, damage and authored actor populations.

L2S2a recipe5 run20261015-160906 completed1800 inputs and native terminal
success after374 seconds. It retained all57 original actors, reached55 live
registered actors and4 simultaneously alerted actors, and recorded1346 live
input frames,454 dead input frames,3 scene starts and2 deaths. Both subsequent
reloads returned to gameplay. Per-scene maxima were18 player shots,11 enemy
shots and4 weapon switches; the first life recorded2 hits/1 kill and the second
5 hits/2 kills. This is a completed telemetry scenario, not overall stability,
visual/audio verification or proof that all actors were fighting simultaneously.

L20S1 run20261015-161555 completed1800 live input frames with54 player shots
and7 weapon switches, but no enemy alerts, enemy fire or player hits. Its staged
start was8 units ahead of actor12542. Exclude this from combat stress coverage;
the cause of the inactive encounter is not established. The next placement is
5 units ahead of actor11975, identified as a live hostile by the first run's aim
telemetry, retaining the same55 authored actors and their original states.

The closer L20S1 run20261015-161858 also completed1800 live inputs without a
fatal error. It recorded57 player shots,3 hits,7 switches and2 simultaneously
alerted actors but zero enemy shots. This remains a combat-coverage gap: inspect
ordinary AI admission/visibility and authored activation around these actors
before claiming this mercenary mix has been stressed. Do not force enemy firing,
disable AI admission checks or treat this as an established engine defect yet.

Admission trace run20261015-162430 (900 inputs, native terminal success) identifies
target11975 in ordinary waiting mode2, weapon15, hostile affiliation0, alerted,
with no alarm/medic/script-move ownership or AI suppression. Enemy aim was not
holding shots; the combat loop recorded284 world-obstructed firing attempts and
zero enemy shots. This explains the immediate coverage failure without proving
whether the obstruction itself is correct. Preserve ordinary collision checks.
The next sustained mercenary probe uses L17S1 actor20026 at10 units, where an
earlier run already observed enemy return fire.

L17S1 run20261015-162654 was interrupted after the user reported another
Daybreak block and get_goal confirmed blocked status. The emulator and harness
were still live and advancing. QMP quit through a second connection timed out;
only the verified owned XEMU process was terminated. Result status is explicitly
STOPPED_AFTER_GOAL_BLOCK, preserving the harness's original terminal label.
At interruption:1661 consumed inputs,957 live/704 dead,4 scene starts/4 deaths;
per-scene maxima11 player shots and9 enemy shots,2 simultaneous alerts. No guest
fatal error was observed. The3600-input target was not completed. Do not relaunch
to work around Daybreak; its internal reason is not available to this session.

Recipe5 adds an opt-in `--aim` controller inside the Xbox process. It computes
bounded look commands toward the nearest live hostile within30 units and permits
the replay's ordinary trigger edges once aligned. It does not force hits, change
actor placement, health or damage, or supply infinite ammunition. The hardware
tester profile ignores the aim flag. Native compile/link succeeded; L2S2a runtime
telemetry confirms normal shots, hits, kills and target changes. Aim telemetry records input frames, target UID,
target changes and aligned fire inputs.

Process-lifetime `rf_scene_crash_activity` counters distinguish live input frames,
dead input frames, scene starts and deaths across reloads. Loading does not call
the input callback. Earlier `alive_samples`/`dead_samples` are host polling counts
and must not be interpreted as gameplay duration. Shot maxima are per-scene
maxima, not cumulative shots across reloads.

The600-second L3S2 run20261015-155842 reached repeated ordinary death/reload
cycles without a new fatal error but timed out before3600 input frames. Its
per-scene maxima were6 player shots,12 enemy shots and3 simultaneously alerted
actors. It does not establish sustained live-action stability.

`tools/xemu_crash_hunt.py` owns build/xbox/crash-disc and redfaction-crash.iso.
It never edits tester-disc. Immutable assets are hardlinked into the diagnostic
stage. All scenarios use the existing standalone local/xemu-harness/pacing-base.qcow2
with XEMU -snapshot, a private EEPROM copy, disabled host controller bindings,
localhost QMP, and a default180-second watchdog, explicitly bounded to at most600
seconds when requested to cover ordinary death/reload cycles. It closes only its
own emulator. The180-second LEGO project limit does not apply to this repository.

RFI6 samples are consumed by the game's player_poll_paced callback. The exercise
profile directly supplies movement/look and crouch, jump, Use, fire, reload,
cycle-weapon and alternate-fire values. No keyboard/mouse/controller OS events
are generated. Neutral and exercise recipes, replay hashes, XBE/map hashes,
telemetry and outcomes are retained for each run. Runtime completion must include
consumed replay samples; rendering-only preview modes are not gameplay passes.

Reproduction command (after building the diagnostic XBE and matching main.map):

    python tools/xemu_crash_hunt.py --actor 8462 --frames 600 --seconds 180

Armed developer scenario:

    python tools/xemu_crash_hunt.py --level ctf06.rfl --archive levelsm.vpp --dev-room --firearms 1 --input exercise --frames 900 --seconds 180

Images remain prohibited. These probes establish functional/error telemetry only;
visual and audible correctness remain unverified.

## Build portability and cleanup

prepare_checked_stdio.py now authenticates LF-normalized PDCLib source, accepting
verified LF or CRLF checkouts without modifying the installed SDK. Focused Python
checks established identical generated output and rejection of changed source.

Use tools/clean-generated-artifacts.ps1 -Apply -PreserveDelivery after all owned
processes exit. The opt-in preserves tester-disc and redfaction-tester.iso while
removing regenerable run payloads. Compact evidence remains. The diagnostic
harness restores missing immutable stage assets when next run.


## L9S4 grenade contact diagnosis (225133)

Added a16-word application RAM-log trace without changing flight/collision
policy. Xbox cross-build passed. Identical L9S4 actor1197/distance8/loadout6,
aimed explosives1800-input recipe completed on stock64 with1709 live/91 dead
inputs,two scene starts,one death/recovery,three grenade releases/detonations,
six enemy attacks,four alerted actors and one blast kill. No guest failure;
no terrain/debris coverage or visual/audio verification.
Run: artifacts/crash-hunt/20261015-225133-L9S4.rfl.

Before reload:1696 sweeps,1446 matched contacts,962 zero fractions,zero
non-incoming contacts. Gameplay counter1445 excludes the separately consumed
object contact.354 ticks exhausted the four-contact limit. Last capped step1279:
room3,face151,fraction0,normal(0.342668,0.122442,0.931443),velocity
(0.0164275,-0.0775609,0.0286357),resting0. At1259:face155,
normal(-0.794496,0.0370486,-0.606138),velocity
(-0.0114624,-0.0767134,-0.0263833). These steep low-speed contacts fail the
existing settling gate normal.y>0.5. Evidence points to repeated incoming
contacts among surfaces, rather than outward-hit reacceptance or ordinary floor
settling. Multi-surface resolution remains open; no arbitrary sleep threshold or
weakened sweep was introduced. One bounce sound is requested per contacting tick,
which warrants review. The loop is bounded; this is a work/effect amplification
lead, not a demonstrated crash or measured performance regression.

Trace layout: sweep calls,matches,zero fractions,non-incoming matches;
last capped frame,slot,fraction bits,normal xyz bits,velocity xyz bits,
room,face,resting. Cleared at scene frame0, emitted through existing RAM logging.
Previous question response restated findings; this continuation adds runtime
evidence. Full crash hunt remains active.


## Extended L11S2 combat and grenade settlement correction

Run20261015-225654-L11S2.rfl completed3600 live inputs,97 player shots,
17 weapon switches,18 enemy attacks,two simultaneously alerted actors and no
death or guest error. All encounter coverage gates passed. The noise/navpoint
fix preserved three waits again. This extends the earlier1800-input check; enemy
attacks did not increase after the first half, so it does not establish sustained
new enemy pressure for every tick or general campaign stability.

Source review: collision_sweep_face uses rf_collision_sphere_plane, whose plane
admission rejects outward movement. The observed zero-fraction hits were all
incoming. More specific retained evidence in
research/secondary-re/weapons-grenade-settle.md identifies original49d4e3:
low incoming linear speed plus low angular speed settles, without a floor-normal
gate. The earlier response report explicitly excluded settling. The first-pass
flight has no angular state; it now applies its zero-spin approximation at
incoming speed<0.5 before bounce, on all admitted ordinary contacts. Previously
it checked post-bounce speed and required normal.y>0.5. Fuse ownership, object
contact precedence, alternate contact and four-sweep cap remain in place.
This is not full original physics parity: spin, elasticity decay, force wake,
rotating attachment and support removal remain outside this correction.

RAM contact trace extends16 to20 words: settled ticks,new settlements,
detonations while resting,last settle frame. Native Xbox build passed;
no PC build or host input used. Validation result follows below.

The identical recipe20261015-230214-L9S4.rfl completed1800 inputs:
1709 live/91 dead,two scene starts,one normal death/recovery,three releases,
three detonations,six enemy attacks,four alerted actors,one blast kill.
Contacts1445->41,capped ticks354->2,zero fractions962->0. Two grenades settled,
accumulating353 resting ticks,and both later detonated; the separately consumed
object contact remains present. No guest error; generic12-shot gate still false
for this finite grenade loadout. No terrain/debris or visual/audio claim.
Compact before/after: artifacts/crash-hunt/grenade-settle-comparison.json.
This is a validated bounded contact-work correction, not a global stability
claim. Original spin/material/wake fidelity remains deferred. Existing delivery
artifacts were preserved and were not refreshed with this private test build.


## L11S2 rocket combat with recovery (230633)

Run20261015-230633-L11S2.rfl used the current private Xbox build,authored
actor10362 at distance8,loadout5,normal aimed explosives scenario,1800 inputs,
360-second watchdog and unchanged OpenJKDF2 HMP log transport. All32 authored
actors were retained. No source/build change was needed for this batch.
COMPLETED1709 live/91 dead inputs,two scene starts,one normal death/recovery.
Sampled per-scene maxima:25 player shots,six rockets,19 enemy attacks,three
blast kills,three simultaneous alerted enemies,six weapon switches. These
maxima are not cumulative totals across reload. All encounter coverage gates
passed. Native guest error arrays stayed clear. The late recovery resumed
without restarting the host session or increasing the watchdog.

Zero terrain edits/debris were observed: this qualifies explosive combat and
resource reload only, not repeated destruction. Visual/audio correctness and
white/yellow flashing remain unverified. A read-only review of the current HUD
flash path confirms red input and the existing HUD draw callback; it adds no
rendered-output proof or cause for the user's report. No captures,host input,
PC builds or hardware deployment occurred. Compact lifecycle-summary.json and
result/telemetry remain in the run directory. Earlier local work and delivery
artifacts are preserved; the overall hunt remains active.


## L14S2 authored heavy-mercenary stress: incomplete rifle coverage

Read-only census:levels3.vpp/L14S2.rfl has23 actors(13 merc_heavy,six
merc_com,four merc_grunt). Selected heavy10437 near heavies10438/10440/10441
and commander10439. Selection positions retained in
artifacts/crash-hunt/l14s2-encounter-selection.json. No enemy transforms,
flags,health or ammunition were altered. Same current Xbox binary,loadout2,
aimed combat1800 inputs,360-second watchdog,unchanged HMP reader.

20261015-231237-L14S2.rfl,distance8:TIMEOUT with106 live/314 dead inputs,
three scene starts/three deaths,implying two completed reloads. Individual
live intervals32/37/37. Per-scene sampled maxima:three player shots,three
enemy attacks,three alerted actors. No guest error; requested duration and
sustained combat coverage not achieved.

20261015-231919-L14S2.rfl,distance20:TIMEOUT with402 live/318 dead inputs,
three starts/three deaths(two completed reloads),134 live inputs each cycle.
Per-scene sampled maxima:nine player shots,18 enemy attacks,one alerted actor,
one weapon switch. Player movement and exchanged fire were observed numerically;
no visual placement verification. Authored pitch makes staged height change with
distance; the farther start is about20-26 units from the five nearby actors.
This increases combat exposure but fails simultaneous-enemy and sustained-fire
coverage. Both cases have zero terrain edits/debris; neither is a passing
stability test. Timeouts bound repeated death/reload runs; they do not by
themselves prove a crash or a stuck reload. The last reload was not observed
complete before shutdown. Sparse telemetry can miss entire short live intervals,
so offline recovered_intervals may undercount the explicit scene-start changes.

Next meaningful variation:finite rockets against the close authored group,
keeping normal damage and existing enemy membership. Do not lengthen watchdogs
merely to turn these rifle probes green. Compact lifecycle reports and the
per-run coverage-index.json retain failures and build hashes. Index review finds
terrain+debris only in the earlier L1S2 run; repeated cuts within one scene remain
unproven. No images,host input,PC builds or deployment. Goal remains active.


## L14S2 rocket follow-up and rejected cannon hypothesis

20261015-232705-L14S2.rfl:actor10437,distance8,finite rocket loadout5,
aimed explosives1800-input/360-second recipe. TIMEOUT with106 live/314 dead
inputs,three starts/three deaths,two completed reloads. Per-scene maxima:
one rocket launch/impact,three enemy attacks,three alerted actors,no blast
kills. No guest error. This repeats the short rifle lifecycle and does not
qualify as sustained/repeated rocket use. No terrain edits/debris.

Existing source documentation identified shoulder_cannon on nearby10441 as a
possible lethal weapon. Added existing rf_scene_ai_fusion[6] and
rf_scene_fusion_projectiles[5] to application-owned RAM logging; no gameplay
policy changed. Xbox compile/link/XBE passed. Focused replay
20261015-233401-L14S2.rfl uses the same recipe with90 inputs/180-second
watchdog solely for attribution. COMPLETED32 live/58 dead inputs,one death,
one rocket,three enemy attacks/positive damage dispatches. Both new arrays
were entirely zero: no accepted NPC cannon shot and no shared Fusion launch.
The cannon hypothesis is rejected for this reproduction. This does not identify
the exact lethal ordinary shooter, or establish broader inactivity of10441.

Read-only entity records, using src/core/level.c's v180 field order:
10437/10438/10440 primary heavy_machine_gun,10439 Rail_Gun,10441
shoulder_cannon; all secondary none. Compact l14s2-group-weapons.json retained.
An initial local extraction read the editor strings as weapon fields; it was
corrected against the runtime parser before using the values above.
Next diagnosis should use existing campaign_combat_event records for actual
source/amount/remaining-health attribution, rather than infer from inventory.
The previous L3S4 Drone/white-yellow report remains separate and unresolved.
No capture,host input,PC build,hardware deployment or delivery replacement.


## L14S2 ordinary damage sequence (233853)

Application log now emits existing32x5 combat-event ring,event count and ten
damage-kind counters; no gameplay changes. Native Xbox build passed.
20261015-233853-L14S2.rfl repeats the90-input diagnostic:32 live/58 dead,
one player rocket,three enemy attacks,one death,no guest error. JSON lines
remained parseable(maximum observed3602 bytes,within8192-byte writer buffer).
Decoded compact evidence:run/combat-events-decoded.json.

Frame25:type0 damages handle26476947 by329.81723,leaving49.687725 health.
Frame31:type1 source26476947 amount19.6875,player health90.550003;
source26542484 amount19.6875,health81.100006;
source26608021 amount669.375,health-508.75. Damage-kind counters show three
armor-piercing-bullet requests and no unsupported fallback. Cannon/Fusion
counters remain zero. Amount is the damage service's returned amount,not
necessarily equal to health lost after armor handling.

Read-only installed weapons.tbl:heavy_machine_gun damage75,SP AI scale0.7;
Rail_Gun damage2550,SP AI scale0.7. Observed amounts equal those AI-scaled
values times an effective0.375 downstream multiplier. The actual difficulty
helper supplies .1/.25/.4/.55 and target factors are separate;0.375 must not be
misreported as the difficulty setting. Exact downstream-factor attribution was
not separately logged. Lethal source's identification as nearby railgun commander
10439 is an inference from the exact amount and authored local inventory;
the trace contains runtime handles,not an explicit UID mapping. This explains
the rapid death pattern without evidence for a new damage bug or cannon blast.

Candidate next recipe should prioritize commander10439 with ordinary aiming.
Nearest-hostile auto-aim from its staged starts can select nearby heavy10437
instead:at12 units,the heavy is6.13 units away versus commander12.07.
Use a deliberate bounded aim point or ordinary target-selection harness support;
do not weaken enemies,inflate health or count a short attribution replay as
sustained combat. Broader crash hunt and prior Drone/effect issues remain open.
