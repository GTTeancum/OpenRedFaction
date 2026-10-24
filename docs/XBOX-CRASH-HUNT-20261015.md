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


## Finite railgun loadout and process-contained priority aiming

The private diagnostic profile now accepts loadout8(railgun) using existing
resource demand and rf_weapon_acquire_sp. It supplies the normal loaded round,
not infinite ammunition or extra health. Existing loadouts remain accepted;
unsupported7 and values above8 remain rejected. Tester-profile flag admission
remains disabled. No damage or authored encounter membership changes.

New --aim-actor UID validates membership in the authored level and excludes
--aim/--aim-point. Native mode3 tracks that living hostile's current eye position
within the existing30-unit range,then uses nearest-hostile fallback when it is
no longer eligible. It uses existing bounded look axes,with a0.02-radian fire
alignment threshold for this precision mode; existing modes retain0.12.
No orientation write,host input or guaranteed hit is introduced. RAM logs now
include the existing four-word aim trace. Invalid negative UID and conflicting
CLI aim modes were rejected before launch. Both native builds passed.

Fixed-point attempts from commander10439,distance12,aim point
(-25.949854,-2.365121,-453.611115):
- 20261015-234433-L14S2.rfl,rockets:33 live/87 dead inputs,one impact,no kill.
- 20261015-234804-L14S2.rfl,railgun:33 live/87 dead inputs,one shot,no hit.
Both were intentionally terminated by exact owned emulator PID after the
unqualified outcome; operator-stop.json explains PROCESS_EXITED. Neither is
classified as a guest crash or a completed stress pass.

20261015-235337-L14S2.rfl uses loadout8,--aim-actor10439,normal combat,
1800-input/360-second bounds. TIMEOUT223 live/317 dead,three scene starts/
three deaths(two completed reloads). Individual live intervals74/79/70.
Per-scene maxima:one player shot/kill,one weapon switch,13 enemy attacks,two
simultaneous alerted actors. All23 authored actors are retained; sampled live
population22 follows the first kill. No guest error,terrain edits or debris.

First cycle:frame13 rail hit killed handle26608021(amount3825,health-3175).
That exact handle delivered the prior lethal669.375 hit. The requested priority,
source selection code and target-change count correlate it with commander10439;
no separate runtime UID/handle mapping array was added. Aim trace subsequently
selected UID10437. HMG-valued19.6875 hits began at43 and killed the player at73.
The first rail round is consumed normally; fallback weapons and normal cadence
remain in force. All three cycles recorded the priority kill and fallback.
Decoded event evidence:run/priority-combat-events.json.

This validates a more capable input harness and actual target damage,not
sustained gameplay stability. Next vary ordinary movement/follow-up fire instead
of repeating stationary reloads. Existing visual/audio and Drone/effect limits
remain. No PC builds,captures,hardware deployment or delivery replacement.


## Mobile profile and L13S1 turret/reload validation (October16)

New diagnostic scenario mobile uses native mode3/trace version3. It retains the
six360-live-tick phases, normal death/Use recovery, weapon cadence and collision.
After20 initial aiming ticks it strafes at axis0.8, reverses during later phases
and moves backward0.35 during reload. The mobile railgun loadout additionally
uses an ordinary capped four-round pickup; older recipes retain their ammo.
No health,damage,enemy membership or weapon timing changes. Host coverage
recognizes version3 with unchanged activity thresholds. Added existing player
ammo,turret combat and turret-owner counters to RAM logs. Native builds passed;
a mistyped Makefile path failed before compilation and was corrected.

20261016-000336-L14S2.rfl:mobile railgun/priority10439,80 live/40 dead inputs,
one priority kill,13 enemy attacks,two alerted actors,rail reload pending at
death. Intentionally stopped exact owned PID32040; operator-stop.json records
that PROCESS_EXITED is not a guest crash. It does not establish sustained fire.

Read-only L13S1 selection retains39 authored actors,with guards/mercenaries and
two auto turrets. Positions:artifacts/crash-hunt/l13s1-encounter-selection.json.
20261016-000744-L13S1.rfl staged at guard8230,distance8,priority8230,mobile
railgun. First life killed the guard, then turret-path20-damage events killed
player at125. Ordinary NPC shot counter remained zero; this excludes static
turret fire and must not be reported as zero total enemy action. Aim already
switched to8253. Intentional stop PID30296 was based on an older sample;
final collection showed929 live/91 dead inputs,two starts,one death and four
player shots. It had resumed useful combat. Corrected operator-stop.json and
retain interrupted status; neither fail nor completed stress-pass classification.
Do not stop a future progressing run based on stale gameplay observations.

20261016-001302-L13S1.rfl:identical placement,priority8253 instead.
COMPLETED1800 live inputs,no deaths,no guest error,16 player shots,five weapon
switches,one turret death. Separate turret counter13 shots by sample120,then
unchanged; ordinary NPC attacks remained zero. Only one alerted NPC observed.
Thus repeated-player-fire/phases gates pass, but sustained/multiple-enemy
pressure remains unmet. Do not count the quiet tail as sustained combat.

Ammo evidence:weapon14(railgun) begins reserve4/loaded1;first rail shot consumes
loaded round;ordinary reload transfers one. Rail selected again by1440 with
reserve3/loaded1;second rail shot by1500,then reload yields reserve2/loaded1
by1680. Across all weapons three reloads transferred four rounds. End values
are selected-handgun ammunition,not rail reserve. The intervening weapon cycle
includes ordinary handgun,rifle and shotgun fire. Compact per-sample evidence:
run/mobile-ammo-summary.json. First successful L13S1 compile/load also exercises
the existing clutter-material budget path on a different authored environment.
No terrain edits/debris or visual/audio verification. Preserve all incomplete
coverage and prior Drone/effect issues. Updated AGENTS instructions explicitly
retain this targeted crash hunt over earlier smoke-only limits; no images,
host input,PC builds or deployment occurred. Delivery artifacts unchanged.

## October16: reject inactive-group stress coverage

20261016-064821-L13S1.rfl completed1800 live inputs on stock64 with no
reported guest failure, nine player shots, five weapon switches, no NPC or
turret attacks and no alerted actors. Recipe:actor8234,distance8,rifle2,
nearest-hostile aim,mobile profile. This is NOT a combat stress pass.
Read-only authored-activation.json records the v180 fields using level.c's
spawn layout: mercenaries8233/8234/8235/8410/8411/8412 have creation_flags2,
which entity.c maps to object0x4000; the aim and combat paths exclude this
state. Guard8230 and turret8253 have creation_flags0. The recipe retained
all39 authored actors but placement alone did not activate this group.
Next selection must check authored activation state or exercise the actual
activation event without silently removing those flags.

Extended the offline lifecycle summarizer with same-scene live intervals,
separate NPC attacks/turret shots, and quiet-input totals. Death/reload
boundaries are excluded. Synthetic checks passed for NPC action, turret action,
quiet intervals, death and reload boundaries. Prior001302 run has1680 live
inputs in quiet observed intervals and60 in intervals containing enemy action;
the initial60 inputs lack a preceding live sample and are not attributed.
An interval containing attacks does not prove continuous/simultaneous fire.
Both runs retain action-intervals.json. No new images, host input, PC build,
hardware deployment or delivery refresh. Crash-hunt goal remains active.

## October16: active L6S3 elite cluster

Added tools/inspect_crash_encounters.py: read-only v180 actor activation,
weapon and proximity selection across Installed_Game archives. Uses level.c
spawn field layout; fingerprints level bytes and retains nearby records.
Checks matched known active guard8230/turret8253 and inactive merc8234.
270 candidate clusters are hints only, not proof of line of sight, room sharing,
runtime activation or combat. No authored actor flags are changed.

20261016-065437-L6S3.rfl stages8 units from3840 with rifle2,mobile,nearest
hostile aim,2400 requested inputs/360-second watchdog,stock64. Nine initially
active elites within15 units carry machine pistols. Result TIMEOUT,1375 live
inputs,425 dead inputs,four scene starts/four deaths,three observed recoveries.
No reported guest failure. Maximum per-life counters:9 player shots,93 enemy
attacks,7 alerted actors; these are not cumulative across reloads. First life
lasted286 live inputs with9 shots/74 enemy attacks. Later life snapshots retained
loaded ammo but few aligned fire inputs; mobile aiming is limiting repeated
player fire. Do not reduce enemy count/damage or count this timeout as a pass.

Offline intervals exclude deaths/reloads:720 live inputs in intervals with
enemy action,360 without; remaining live inputs cross excluded or unsampled
boundaries. Five intervals contain both player shots and enemy attacks. Reports
now include player shot deltas and live-input endpoints because buffered samples
may share a host collection timestamp. Checks confirm interval input arithmetic
and combined action on this real log. Action in an interval does not prove
continuous fire. Native logs/result/selection/action-intervals retained; next
improve normal input aim/fire opportunities against this active group. No new
Xbox source build required for these offline tools. Goal remains active.

## October16: mobile aiming stops under elite pressure

Mobile profile now stops lateral input for the first24 of each60 live ticks
in fire-capable phases, then resumes ordinary strafing. Reload movement,
bounded look input, fire pulses, cadence, ammo and authored enemies/damage are
unchanged. Trace version4 distinguishes this recipe; host phase/cycle coverage
accepts4 with existing thresholds. Xbox native build passed in
build-mobile-aim-stops.log; no PC build.

20261016-070258-L6S3.rfl repeats3840/distance8/rifle2/mobile/nearest aim,
2400 requested inputs/360-second watchdog. TIMEOUT:1513 live/407 dead inputs,
four starts/four deaths,three recoveries, no reported guest failure.
Per-life player shots9/18/1/7 versus baseline9/1/1/2; enemy attacks73/115/39/70
versus74/48/69/93. Max simultaneous alerted population7; nine alert events is
not nine simultaneous enemies. Same-scene sampled intervals with enemy action
cover780 live inputs versus720 before; quiet intervals420 versus360. Seven
intervals contain both player shots and enemy action versus five before.
These interval counts do not prove continuous combat. Third-life shooting is
still sparse and the requested input budget was not reached. Generic activity
gates cannot turn this watchdog result into a completed stress pass.

Saved mobile-stop-comparison.json and action-intervals.json beside result/logs.
Next examine phase continuation across recovery and aiming under movement;
do not weaken the encounter to obtain a green result. No visual/audio claims,
images, host input, deployment or delivery refresh. Crash hunt remains active.

## October16: L19S2A mixed-weapon probe and priority tolerance

Authored candidates identify six initially active nearby rifle/railgun/grenade/
flamethrower mercenaries around11812; all27 authored actors retained.
Recipe:levels3/L19S2A,anchor11812,distance8,rocket5,priority11890,mobile,
1800 requested inputs/360-second watchdog.071036 run had45 live/75 dead inputs,
one death,two enemy attacks and no rocket; intentionally stopped, operator-stop
record retained. No guest error or completed stress pass.

Priority aim formerly required0.02-radian alignment for every weapon, inherited
from the railgun probe. Diagnostic-only aiming now uses0.02 only for equipped
rail slot7 and ordinary0.12 for other weapons. No aim snap, weapon timing,
damage or actor changes. Added existing rf_scene_ai_grenades[5] and
rf_scene_ai_flame[8] to RAM logs to distinguish actual mixed-weapon activity.
Xbox build passed (build-priority-rocket-telemetry.log).

071342 identical probe with rebuilt XBE launched a rocket, but two45-live-input
cycles ended before sustained combat. Final90 live/210 dead,2 starts/2 deaths,
max per-life1 player shot/2 enemy attacks,5 simultaneous alerts observed.
Recorded damage events in the second life:frame33 amount23.4375 then frame44
amount669.375. Dedicated enemy grenade and flame arrays remained zero in
collected samples; authored weapon membership is not actual attack coverage.
Operator stopped exact owned XEMU after this repeated pattern; PROCESS_EXITED
is intentionally interrupted, not a guest crash or pass. Both operator-stop
records and compact logs/results retained. Next use finite railgun priority
against11890 so the mixed group has a chance to engage; preserve authored
stress and distinguish subsequent observed weapon paths. No images, host input,
PC build, hardware deployment or delivery refresh. Goal remains active.

## October16 handoff: user-requested break, commit and push

071816 L19S2A finite railgun at prior11812 anchor could not align before death:
45 live/75 dead,zero player shots; intentionally stopped with operator-stop
record. No guest crash or stress pass.

072100 stages player eight units facing railgun carrier11890, retaining all27
authored actors,finite railgun8,priority11890,mobile profile. Frame13 hit for3825
killed runtime handle26870169 (the prior lethal source); normal fallback aim
then selected other enemies. First life reached253 live inputs. Dedicated AI
counters establish actual mixed weapon activity: per-scene maxima2 grenade
launches,16 contacts,1 detonation;7 flame pulses,35 fuel,7 player hits,zero
reported flame status or grenade pool refusals. These maxima are not cumulative
across reloads. Final watchdog result TIMEOUT:613 live/287 dead inputs,three
starts/three deaths,max1 player shot and14 enemy attacks per scene,three
simultaneously alerted actors observed. Repeated player-fire/phase gates fail;
no guest error reported, no completed stress pass. The run had already exited
when the user requested a break. No emulator was left running.

User requested commit/push and pause. Goal marked paused, not complete.
Remaining: sustained return fire after the initial priority kill, broader crash
coverage and previously documented Drone/effect issues. No new image, PC build,
hardware deployment or ready-delivery refresh. Compact run evidence stays local;
only tracked source/tools/docs are included in the push.

## October17: resumed hunt, fallback aim comparison

Goal verified active; checkout at pushed630b8a5e, no project processes. Prior
ammo evidence disproved a delayed-reload hypothesis: automatic rail reload
had154 ticks remaining at60,34 at180 and had transferred one round by240.
No reload rule or weapon timing was changed. Diagnostic priority precision now
applies only to the named railgun target; nearest-hostile fallback uses ordinary
0.12-radian tolerance. Bounded look remains unchanged. Xbox build passed in
build-priority-fallback-aim.log.

20261017-203848-L19S2A.rfl repeats carrier11890/distance8/finite rail8/mobile,
1800 requested inputs/360-second watchdog. TIMEOUT with613 live/287 dead inputs,
three starts/deaths,max1 player shot/14 enemy attacks/3 simultaneous alerts,
no reported guest error. Same activity outcome as072100 baseline despite more
aligned input. First life again253 inputs; second life observed grenade
detonation and seven flame hits. This change does NOT establish improved return
fire or stability. Next use ordinary weapon switching after the opening rail
kill rather than shorten authored reload or reduce enemy pressure.

User reported frozen near watchdog completion. Process and current logs were
checked: last sample at327.281 host seconds showed third death,613 live/287 dead;
the watchdog then returned TIMEOUT. This does not prove or disprove a visual
freeze. No images or desktop input used. Retain user observation as unresolved;
no new run launched in response. Compact evidence retained and payload cleanup
performed. Goal remains active; no push or delivery refresh in this batch.

## October17: ordinary weapon cycle after priority kill

Diagnostic-only mobile rail opener now requests one cycle edge after the named
priority actor's health reaches zero. The latch resets at scene start; range or
visibility loss cannot trigger it. No direct selection, ammunition or gameplay
mutation: normal holster/draw/selection handles the request. Xbox build passed
(build-priority-followup-cycle.log).

20261017-204726-L19S2A.rfl repeats carrier11890/distance8/finite rail8/priority
11890/mobile,1800 requested inputs/360 seconds. TIMEOUT:613 live/287 dead,three
starts/deaths,two observed recoveries,no reported guest error. Per-life shots
3/9/2 versus1/1/1 before; switches1/2/1. First life shows handgun follow-up and
normal reload (reserve125 to123,loaded16). Per-life enemy attacks13/14/6;
flame pulses3/7/0; grenade detonations0/1/1. Max simultaneous alerts3.
420 live inputs in same-scene sampled intervals contain enemy action; remaining
live inputs cross excluded/unsampled boundaries. No quiet same-scene intervals
observed; this is not proof of uninterrupted action. Per-life maximum9 player
shots remains below the unchanged repeated-fire gate, and watchdog status is
not a completed stress pass. Evidence:per-life-summary.json/action-intervals.json
beside native logs/result. Retain the useful input improvement and broaden
other authored stress rather than treating this small batch as goal completion.
No images,host input,PC build,hardware deployment,delivery refresh or push.

## October17: L17S3 void failure, user visual correction

205501 L17S3/levels3 stages8 from rail carrier18723,loadout8/priority18723/mobile;
205911 stages4 from Meca Turret14435,rifle2/nearest aim/mobile. All23 authored
actors retained. Both reached1800 inputs with no guest error. First had6 shots/
zero enemy attacks; second9 shots/two enemy attacks. These terminal COMPLETED
statuses are NOT valid gameplay coverage: user-provided screenshot shows HUD
and weapon against empty space. No new image was captured or uploaded.

Decoded eye positions confirm falling outside the world. First run Y falls from
-5.487 at60 to-28.687 at240 and-1737.728 at1800. Second falls from-5.672 at60 to
-35.472 at240 and-1801.710 at1800. Static geometry Y minimum is-8. Earlier
commentary misclassified this as a quiet tail/possible cover; corrected here.
Both runs carry outside-world-review.json retaining original terminal status
alongside INVALID_GAMEPLAY_OUTSIDE_WORLD classification. Treat the input count
as harness progress only, not sustained gameplay. Do not add pursuit input
until placement/support/collision has been investigated.

Host harness now derives conservative static-vertex bounds from the read-only
level and stops on OUTSIDE_WORLD if the sampled player eye is nonfinite or lies
more than16 units outside those bounds. This is an exterior guard, not proof
of room/floor validity, and not a gameplay fall fix. Offline replay catches both
traces at240; pre-game zero sample is ignored; previous L19S2A mixed-combat trace
stays inside its bounds. No new emulator was launched after the user's report.
No Xbox source changes/build or PC build in this batch. Goal remains active;
void cause is unresolved. Compact logs and user observation preserved locally.

## October17: unsupported staged starts diagnosed and screened

Read-only vertical triangle intersections against authored L17S3 geometry show
205501 start has no static floor in its vertical column;205911 startY-4.523 is
below floor4038 atY-1.5,room7. They were unsupported/under-floor starts created
by an unchecked actor-relative offset across stacked rooms. No evidence here
justifies changing collision filtering or restoring invisible render faces.
Compact source evidence:l17s3-floor-support-review.json and supported-starts.json.

Harness preflight now rejects staged starts without an upward static floor
0.05..3 units below the marker. Portal/flag-bit1 faces are excluded;
render-invisible faces remain eligible. It records face/room/height/clearance.
This conservative point support check is NOT a full collision-body fit, room
containment or mover-support validator. It rejects both known bad starts and
accepts the previous valid L19S2A start. No game assets are mutated.

210608-L17S3 repeats rail8/priority18723/mobile at distance4 rather than8:
face4815,room89,floorY-6.5,marker clearance1.394. Runtime maintained standing
height instead of falling (see telemetry). TIMEOUT at360 seconds:1254 live/
306 dead,three starts/deaths,max16 player shots/22 enemy attacks per life,
three weapon switches,five simultaneously alerted actors,no guest error or
outside-world event. The first two lives reached374 and486 live inputs.
This supports the placement diagnosis; it does not prove collision correctness
for every surface or broad stability. No dedicated laser counter is logged,
so do not claim laser-specific coverage from the map's weapon membership.
No Xbox source change/build,PC build,images,host input,deployment or push.
Goal remains active; valid action evidence replaces the prior void runs.

## October17: L8S2 supported explosive encounter

Run20261017-211355-L8S2.rfl used levels2, actor9378,distance8,finite rocket loadout5,nearest-hostile aim,explosives profile1800/360. All13 authored actors retained. Static support face4791,room1,Y-2.75,marker clearance1.651. Live telemetry eyeY remained-1.083..0.194; no exterior escape or guest fatal error was recorded. This corroborates safe placement here, not general collision correctness.

TIMEOUT:374 live/286 dead inputs,three starts/deaths,two observed recoveries. Per-scene maxima:two rockets/player shots,three enemy attacks,four alerted actors. No terrain edits/debris or blast kills. Same-scene sampled intervals contain120 live inputs with enemy action and60 without; boundaries excluded. Early deaths and reload time prevent sustained stress qualification. No visual/audio validation. The earlier L17S3 void runs remain invalid gameplay evidence.

No source build,host input,image capture,hardware deployment or push. Goal remains active. Compact result,telemetry and action-intervals.json retained; temporary payloads pruned after the run.

## October17: L8S2 rifle/mobile follow-up and damage attribution

Prior211355 rocket run shows two rocket self-hits in each sampled life and no blast kills (damage-review.json). Source scene_explosion_blast_source_policy logs the victim handle for player blast events; direct enemy-fire events log the attacker. Do not infer attacker identity uniformly from event type1. Nearest-target test aiming has no visibility gate; cross-room targeting is possible but intervening geometry was not directly proven for these shots.

212134-L8S2 kept actor9378/distance8,supported room1 start and all13 authored actors. Changed finite loadout to rifle2 and input profile to mobile,nearest aim,1800-input/360-second bound. TIMEOUT at1109 live/211 dead inputs,two deaths/two observed recoveries. Per-life player shots6/9/22,direct hits2/1/4,direct kills1/0/4,NPC attacks4/6/8. Five actors simultaneously alerted at peak; two switches observed. EyeY-1.104..0.205,no recorded exterior escape or guest fatal. No terrain edits/debris.

Sampled same-scene intervals contain420 live inputs with enemy action and540 without. Quiet intervals do not establish sustained combat. This improved live action over the rocket probe but changed both loadout and movement, so it is not an isolated causal comparison. TIMEOUT remains unqualified. No new visual/audio evidence.

Offline lifecycle summarizer now records per-scene maxima for shots,direct hits/kills,NPC/turret attacks,rocket self-hits and alerted population. Direct counters explicitly exclude explosion paths. Replayed prior rocket telemetry against the independent damage review: all three lives match for launches,self-hits and enemy attacks. No native source changes or builds. Cleanup retained compact logs/reports and delivery; no host input,images,hardware deployment or push. Crash hunt remains active.

## October17: world-visible diagnostic target selection

Diagnostic actor aiming now rejects targets occluded by the existing combat_obstructed world/mover ray, matching NPC-awareness geometry queries. Priority target selection uses the same gate; explicit terrain aim points retain their prior behavior. The gate does not cover every clutter/dynamic-object obstruction. No changes to enemy AI,health,damage,authored population or ammunition. Added application-owned visibility counters (rays,blocked candidates,errors,last status); host stops INPUT_QUERY_ERROR on a sampled query failure.

Xbox build-visible-aim.log completed compile/link/CXBE with the existing linker merge warning. Python harness syntax and diff checks passed. Stock64 run213038-L8S2 repeated212134 recipe:actor9378/distance8,rifle2,mobile,nearest aim,1800/360. TIMEOUT at1169 live/91 dead inputs,two starts,one death/recovery. Per-life shots9/14,direct hits1/7,direct kills1/4,NPC attacks4/5. Final-life visibility3260 rays,2947 blocked candidates,zero query errors/status. No guest fatal or exterior escape recorded.

This is NOT sustained-pressure improvement: sampled intervals with enemy action180 inputs versus420 previously,quiet900 versus540 previously. The quiet tail after local kills is consistent with stationary/strafe input failing to reach remaining opponents; route reachability is not yet verified. Longer survival is not broader coverage. The filter establishes that cross-geometry candidates were being considered, not that every prior rocket impact was caused by this. Next pursue reachable actors using process-contained movement or use a connected action-heavy encounter; keep all authored opponents. World/mover filtering alone does not prove complete visibility or navigation.

No new images/audio evidence,PC build,host input,hardware deployment or push. Compact telemetry and lifecycle report retained; generated payload cleanup performed after terminal process exit. Goal remains active.

## October17: L20S2 generated-boss patrol startup fatal

L8S2 authored-floor review shows nearby actors across several rooms and elevations; it does not establish runtime survivor identities or route connectivity. Expanded coverage to L20S2 instead of blindly steering toward cross-floor actors. Supported start actor18260/distance12,room129,face7023,floorY-2.375,clearance1.635. All45 authored actors retained,finite rail8 priority18260,mobile1800/360.

213907-L20S2 failed before gameplay: GUEST_ERROR,RF_RANGE(-4),load stage18,patrol line145. scene_waypoint_open traverses all seed descriptors. L20S2 appends the existing generated hidden masako_endgame actor (source464696..464744),slot45,with intentionally no raw RFL span. Patrol qualification attempted rf_level_entity_ai_read on that descriptor; entity_spawn_read rejects null raw. The old actor-index0 diagnostic was stale,not the failing actor identity. Navigation size calculation fits the existing budgets; no memory cap increase was needed.

Fix recognizes only scene_masako_seed_is and validates null raw,zero bytes and the explicit generated UID before returning no authored patrol. Arbitrary malformed authored records still fail. Boss lifecycle/activation and all original actors remain intact. Patrol source lookup now updates its failure index. Xbox build-masako-patrol.log compiled/linked/produced XBE; existing linker merge warning only.

Identical214339 replay passed the blocker and loaded46 runtime descriptors (45 authored plus generated dependency). TIMEOUT after830 live/190 dead inputs,two starts/deaths and one observed recovery. Per-life player shots7/16,direct hits4/14,direct kills1/2,NPC attacks11/14. Shoulder-cannon telemetry accepted3/6 launches and3/6 impacts,zero pool refusals/status. Sampled live intervals:600 with enemy action,120 without. Two simultaneously alerted actors,eyeY-0.708..0.826,no guest fatal,visibility query error or exterior escape. No terrain edits/debris. Fighter-specific firing is unverified because those counters are absent from this log format. No full stress-pass or boss-handoff completion claim.

Reporting correction: rf_scene_rocket_blast is a historical symbol; its implementation counts any explosion with a non-sentinel source handle. Index4 counts player hits,not necessarily self-inflicted rocket damage. Lifecycle summaries now label sourced_blast_player_hits and sourced_blast_npc_kills; source comment corrected without runtime behavior change. Earlier L8S2 self-hit attribution was an inference from its player rockets and encounter weapons,not something index4 alone proves.

Compact failure diagnosis,authored floor/start reports,telemetry and lifecycle evidence retained. Cleanup after terminal runs; no images,host input,PC build,hardware deployment or push. Active hunt remains incomplete.

## October17: L19S2B pain-audio voice-pressure fatal

Expanded to all52 authored L19S2B actors. Supported actor12044/distance8 start in room8,face6923,floorY2.15937,clearance1.55555; finite rifle2,nearest visibility-checked aim,mobile1800/360. No enemy edits or host input.

215201 and diagnostic replay215604 both GUEST_ERROR at43 live inputs: RF_IO(-1),enemy call line13537,frame42,actor slot42/UID12181,weapon16. This is the NPC-to-NPC damage call. Added existing combat-pain,pain-audio and live-audio arrays to RFLOG; second run isolated the error to pain audio: combat-pain status-1,pain-audio two selections/one play/two sample loads/one error,global41 successful voice starts/11 failed. Baseline had eight alerted actors and one live enemy grenade.

Source rf_audio_select_ordinary (original522470) uses30 slots with no active-voice stealing; occupied active or looping slots may legitimately refuse optional playback. The pain wrapper converted any absent voice into RF_IO. Added an exact mixer-capacity preflight matching that allocation policy: recognized full pool declines the optional sound without setting a gameplay error. No active voice is stolen; validation,PCM loading and device errors on attempted playback retain their existing handling. New cumulative pressure counter records declines,last sample,occupied slots and looping slots. Damage/RNG/weapon rules remain unchanged. This helper also serves existing impact/contact sound callers; audible output is unverified.

Xbox diagnostic and fix builds completed compile/link/CXBE (existing linker merge warning). Fixed replay215940 completed1800 live inputs,no deaths/reloads,no fatal or exterior escape. Pressure counter4 declines,last sample665,30 occupied/11 looping; later pain-audio10 successful plays,zero errors. Per-scene player shots6,hits3,direct kill1,NPC attacks27,seven simultaneous alerts. Enemy grenade1 launch/12 contacts/1 detonation; zero flamethrower pulses. No terrain edits/debris. Exact crash path resolved under recorded capacity pressure.

Coverage remains unqualified: repeated-player-fire gate false,420 sampled live inputs with enemy action,1320 quiet. This is a validated crash fix,not sustained mixed-weapon or overall stability completion. Lifecycle summary now retains NPC grenade,flame and fusion maxima; verified prior L20S2 launch/impact pairs3/3 and6/6 against reviewed evidence. Compact diagnosis,telemetry and reports retained; generated payload cleanup after terminal runs. No PC builds,images,hardware deployment or push. Goal remains active.

## October17: optional forward-input stress profile

Prior L19S2B quiet samples retain loaded ammo while every candidate world sight line becomes blocked; ammo exhaustion is not supported as the explanation. Added opt-in advance scenario4/traceversion5,retaining old profiles. It uses mobile strafing/aim pauses plus forward0.65 input outside reload,half lateral input,normal reload/backpedal,cycle,jump and death recovery. No direct position writes,pathfinder,actor edits,damage changes or infinite ammo. Rail pickup/priority-cycle behavior matches mobile when explicitly selected. Xbox flag parsing and host trace recognition accept the new mode; Python syntax,CLI help,diff checks and Xbox build-advance-input passed.

220614-L19S2B repeats the same52-actor start,rifle2 and nearest-visible aim with advance. TIMEOUT156 live/204 dead inputs,two deaths,one recovery. Lives82/74 inputs versus previous1800 surviving live inputs. Per-life shots3/5,hits0/2,no direct kills;NPC attacks10/15;grenade launches2/2,detonations1/2. Seven simultaneously alerted actors;zero flame pulses. Sourced blast NPC kills1/2 are not player kills. No guest fatal,query error or exterior escape. Pressure refusal path again exercised; optional sound handling remains nonfatal.

Only60 sampled same-scene live inputs include enemy action,zero quiet; most short-life boundaries are excluded,so zero quiet does not mean sustained coverage. Eye samples show little travel in first life and roughly2 units of Z extent in second. Forward input alone did not establish escape from the starting pocket or prove route reachability. More aggressive exposure shortened useful survival; retain as an optional short pressure probe,not the preferred sustained profile or a passing batch. Comparison saved in l19s2b-advance-comparison.json.

No PC builds,images,host input,hardware deployment or push. Compact evidence retained and generated payloads pruned after terminal run. Crash hunt remains active.

## October17: L19S1 repeatable flame/grenade combat

Read authored support before launching: several8/12-unit offsets lack nearby floor and were not used. All26 actors retained,finite rifle2,mobile profile,nearest-visible aim,1800-input/360-second bound. No build or native source changes in this batch.

221432-L19S1 start actor9917/distance8 was deliberately stopped after observed first-life64 inputs,zero shots and zero aligned fire inputs. Terminal buffered logs show127 live inputs,two deaths and one recovery,still zero player shots. Per-life flame pulses4/8 and grenade detonations1/1 confirmed enemy action; zero flame-player hits. Confirmed owned XEMU PID29764 was terminated by terminal process management,operator-stop.json records why. PROCESS_EXITED is operator termination,not a crash,freeze diagnosis or pass.

221750 instead starts facing flame actor9911 at distance4:room5,face2969,floorY approximately-1,clearance1.556. Same loadout/profile and unchanged authored population. TIMEOUT1085 live/415 dead inputs,four deaths,three observed recoveries. Per-life player shots15/6/6/6,direct-hit counts8/10/15/5,direct kills2/0/1/1;NPC attacks18/18/14/19. Hit counts are damage contacts,not necessarily unique shots/targets. Flame pulses and player hits8/8/4/10,grenade launches2 each life,detonations1/1/1/2. Up to four simultaneously alerted actors. No guest fatal,query error,exterior escape or pain-audio error; no full-voice refusal in this run. EyeY0.665..1.898.

Sampled same-scene intervals:660 live inputs with enemy action,120 without; death/load boundaries excluded. This establishes repeatable mixed-weapon action and ordinary recovery,not sustained full-budget completion. No terrain edits/debris or visual/audio verification. All compact logs and support/lifecycle reports retained; generated payload cleanup after both terminal sessions. No host input,images,PC build,hardware deployment or push. Goal remains active.

## October17: L18S3 turret/cannon pressure and mobile retry

All13 authored actors retained. Start12 units from shoulder-cannon actor11347, facing the active group including stationary Vauss turrets11329/11334; finite rocket loadout5,nearest-visible aim,1800-input/360-second bound. Static support is face79,room0,height4.874532,clearance1.613769. Source support alternatives are saved in l18s3-start-support.json. No native source changes or new build in this batch.

222600 explosives-profile probe was deliberately stopped after59 live/61 dead inputs,one death,one player rocket,sixteen aggregate turret shots and one cannon launch/impact. The first life ended before that profile starts strafing at60. operator-stop.json records the exact owned-process termination; PROCESS_EXITED is not a crash,freeze diagnosis or pass.

223227 repeats the same start/loadout/population with existing mobile profile. TIMEOUT at360 seconds,118 live/182 dead inputs,two59-input lives and one observed normal reload recovery. Each life records one player rocket,no direct hits/kills,sixteen aggregate turret shots and one NPC cannon launch/impact. Two turret owners exist; aggregate shots do not prove both fired. Generic NPC-alert maximum is one and does not count turret owners as NPC alerts. No sourced blast player hits or NPC kills,terrain edits or debris. No guest fatal,query error or exterior escape logged.

First-life eye moved from staged X/Z[-99.844353,304.096222] to[-99.948036,306.371033]; stationary probe remained at staged X/Z. Mobile input therefore produced lateral displacement but no survival gain. Sampled eyeY remained6.541461..6.541560. The lifecycle summarizer has no eligible same-scene live intervals because sparse rows straddle death/reload boundaries; zero eligible intervals is not zero enemy action. The actual per-life counters prove short mixed turret/cannon action,not sustained combat. Visual/audio correctness remains unverified.

Do not repeat merely to obtain a passing status. Next inspect finite weapon/target selection against this lethal group or choose a supported connected combat start that permits repeated return fire; retain all authored enemies and normal damage. Earlier strafing alone did not solve exposure. Compact result/telemetry/lifecycle/operator-stop reports retained; generated payloads pruned after terminal processes. No host input,images,PC build,hardware deployment or push. Goal remains active.

## October17: turret target-selection gap and startup-only timeout

Source inspection found scene_crash_aim_input searched only campaign_npc_bodies. Static turret owners are separate registrations, so the previous driver could not intentionally return fire against those enemies. Added registered, living hostile turret candidates using existing scene_turret_target (eye tag/model bound fallback), ordinary world/mover visibility and the same30-unit range. Nearest-visible and explicit UID priority both work through bounded look/fire inputs; named turret death participates in the existing one-time rail weapon-cycle request. No health,damage,AI,ammo,actor population or host input changes. NPC priority still takes precedence when selected.

Xbox build-turret-input.log confirms compilation/link/XBE generation; existing linker section-merge warning only. Diff review checked NPC fallback,priority retention,dead/inactive/friendly turret exclusion and query error reporting. No PC build.

224155-L18S3 retains actor11347/distance12 and all13 actors; finite rail8,mobile,priority11329,1800-input/360-second bound. TIMEOUT with zero scene starts,zero replay inputs and only the initial game RFLOG record: diagnostic phase1,load stages0. Reader poll files continued to appear and the owned XEMU process consumed CPU time, but no scene/gameplay telemetry arrived. This is an unresolved startup stall,not evidence of turret aiming/damage,combat stability or a new gameplay crash. New target-selection code is compiled but runtime-unverified; its gameplay callback was not observed executing. Preserve this evidence and diagnose startup before interpreting another combat result. No causal attribution to the targeting change is established.

Harness handle reached terminal exit0 after watchdog shutdown. Compact logs/result retained; cleanup after terminal processes,ready delivery preserved. No images,host input,hardware deployment or push. Goal remains active.

## October17: startup observability and turret priority owner correction

Correction to prior startup interpretation: an initial RFLOG record with load stages0 is the last sampled state,not proof execution remained there. Previously no forced samples occurred between that record and gameplay/final return. Added rf_xbox_startup_trace[4] (sequence,stage,GetTickCount,status),bounded forced checkpoints and HMP deduplication keyed by this trace as well as frame/phase. Stages1/2 bracket particle-resource check (2 means returned,not an independent resource-pass assertion),3 follows table validation,4 animation validation,5 diagnostic campaign entry,6 level selection/staging,7 pre-scene initialization,8 campaign return. Existing errors and execution paths remain intact; output is gated by crash-ram-log.flag. Python syntax and Xbox build-startup-trace passed.

225145-L18S3 repeats the exact prior rail8/priority11329/mobile recipe and supported start with13 authored actors. Checkpoints advanced through stage7 by game tick5318 and repeated on reload at147488 and281666. Earlier startup timeout did not reproduce; its cause remains unresolved. TIMEOUT at360 seconds,118 live/211 dead inputs,two59-input lives and one observed recovery. Each life selected turret11329,fired twice,had zero direct or turret query hits/kills,received sixteen aggregate turret shots and one cannon launch/impact. No fatal/query error or outside-world event. No eligible same-scene live action intervals due sampled death/reload boundaries; no sustained-coverage claim. Checkpoint sequence retained within same frame/phase demonstrates the deduplication change preserves startup evidence.

Weapon selection exposed an unintended early cycle: slot0,one cycle in each life despite live priority turret and zero turret deaths. Source explains it: campaign_npc_bodies is calloc'd per pose; static/dependent turret classes skip skeletal body admission. The old NPC priority-death branch checked UID/zero health before testing body admission,so it interpreted the unused skeletal slot as a dead priority target. Added registration/body admission before NPC death recognition; turret-owned death remains checked by the turret loop. This is a diagnostic driver correction,not a change to player weapon rules or enemy damage. Target selection executes, but effective turret hits and corrected rail retention require the next runtime probe.

Compact logs/lifecycle retained. Build-turret-priority-owner records the corrected Xbox build; no PC builds,images,host input,hardware deployment or push. Cleanup after terminal build/session preserves ready delivery. Goal remains active.

## October17: priority turret kills expose a new player-combat fatal

230015-L18S3 repeats rail8/priority11329/mobile,actor11347/distance12,all13 actors and stock64 after the empty-skeletal-slot correction. First life now records one turret query hit/one turret death,then ordinary cycling and nearest-visible aim toward cannon11347. Four player shots/one direct NPC hit,no direct NPC kills; sixteen aggregate turret shots and two cannon launches/impacts. Life lasts100 inputs versus59 before the correction. These observations exercise actual turret contact/death and remove the previous premature-cycle behavior; they do not establish sustained combat.

After one normal recovery,second life reaches59 live inputs,one turret hit/death,three player shots,no direct NPC hits,kills;seven turret shots and one cannon launch with zero logged impacts. At cumulative159 live/91 dead inputs the run terminates GUEST_ERROR: diagnostic phase0x80000104,rf_scene_profile_stage[58,109],rf_scene_combat[7]=0xfffffffc (RF_RANGE). Enemy-failure tuple is all zero. This is a new player-combat fatal,not a watchdog timeout. The grouped109 site covers campaign_combat_tick and two empty-weapon follow-ups; existing logs do not prove the nested failing call or cause. No correction to gameplay failure admission is justified yet.

Added first-error rf_scene_player_failure[4] (status,source line,frame,equipped slot): records returned failures within campaign_combat_input_tick and falls back to the outer three combat calls when needed. It resets at scene frame0 and preserves the first nonzero status; it does not swallow errors,retry operations or change damage. RAM log includes the tuple. build-player-failure-site captures diagnostic Xbox compilation. Next action is the identical replay with this instrumentation; crash remains unresolved.

Offline lifecycle reports now retain turret damage-call,query-hit and death maxima per scene. Replayed old225145 data has zero hits/deaths and corrected230015 data has one per observed life,matching raw arrays. Damage calls alone are not successful hits. No visual/audio verification,PC builds,images,host input,hardware deployment or push. Compact evidence retained; generated payloads cleaned after terminal build/session; goal remains active.

## October17: pain-audio bank pressure fix; paused at user request

230742-L18S3 exactly reproduced230015: GUEST_ERROR at cumulative159 live inputs,second-life frame58,pistol slot0. New first-failure tuple[-4,17980,58,0] points to NPC damage/feedback. Pain counters identify the sound callback: sample338 selected,name hash128425424,zero loads/plays,one error,statusRF_RANGE. First life loaded the same27672-byte sample successfully. Source campaign_pain_audio_play_voice propagates campaign_ambient_reload,whose bounded bank loader returns RF_RANGE when idle eviction cannot make room. This is separate from the earlier full30-voice-pool fix.

Implemented a narrow optional-playback refusal after the existing reload/idle-eviction attempt: only RF_RANGE plus a valid archive entry with nonzero size and resident bytes+entry size above the unchanged bank budget clears the optional callback status and declines the voice. Malformed/missing samples,invalid indices,in-budget allocation failures and other device/load errors retain existing handling. No active PCM eviction or bank enlargement. Shared helper also serves existing contact/impact audio; pressure counts must not all be labelled NPC pain. New cumulative rf_scene_pain_bank_pressure[5] records refusals,last sample,resident bytes,requested bytes,budget. Xbox build-pain-bank-pressure passed.

231405 repeats the same13-actor L18S3 recipe,stock64,actor11347/distance12,rail8/priority11329/mobile,1800-input/360-second bound. TIMEOUT,not GUEST_ERROR:200 live/211 dead inputs,two100-input lives,one observed recovery. Former second-life frame58 failure now yields a normal NPC hit and later ordinary player death. Two optional bank refusals; last sample885,1301784 resident bytes+50808 requested against1310720 budget. First-life pain sample338 loads/plays; second-life pain selection has zero error. Player-failure tuple remains zero. Per-life player shots4/3,direct NPC hits1/1,no direct NPC kills; turret hits/deaths1each,aggregate turret shots16/15,cannon launches2each,impacts2/1. Second-life sourced blast player hit1 is not proof of self-damage. No eligible same-scene live action intervals in sparse sampled rows; this establishes the targeted fatal-path fix,not sustained combat completion or visual/audio correctness.

### Pause / resume note

User requested wrap-up and a note; active goal explicitly set to PAUSED. No more emulator runs or builds until resumed. Both current sessions are terminal and the corrected Xbox binary is built. Latest source change is the bounded pain-bank refusal above; diagnostic/error-site and turret-targeting work remains committed from earlier turns. Compact evidence folders:230015 fatal baseline,230742 exact call-site reproduction,231405 corrected replay (all under artifacts/crash-hunt/20261017-<time>-L18S3.rfl). Generated payload cleanup preserves their reports/logs and ready delivery.

On resume, inspect current processes/Git first. Continue sustained authored combat beyond these short lives; L18S3 still dies quickly after switching from the killed priority turret to pistol. Improve ordinary weapon choice/engagement or use another supported connected authored combat space,retaining enemies and normal damage. Do not declare the broader crash hunt complete. Repeated destruction cuts,Drone NPC-driven contact and reported white/yellow effect remain open. No new image capture,host input,PC build or hardware deployment. Ready tester delivery is not refreshed by private crash builds. No push authorized in this wrap-up; preserve unrelated untracked work. No percentages unless directly asked.

## October22: resumed with repository-confined runtime; L20S2 combat replay

Revalidated HEAD34b2c829,clean tracked worktree,no live project emulator; goal is active again. New confinement rule supersedes earlier external working paths. Human explicitly authorized reading/copying C:/Games/Emulators/Xemu runtime/firmware into this repository and separately authorized copying C:/nxdk/tools/extract-xiso/build/extract-xiso.exe. Copied only xemu.exe,LICENSE.txt,MCPX/mcpx_1.0.bin,BIOS/xbox-4627_debug.bin,EEPROM/eeprom.bin and the packager into ignored local/runtime; source/destination hashes recorded there. No modifications to outside installations. No Xbox/PC build this turn; reused the compiled pain-bank fix.

Crash harness now requires repository-local runtime inputs before acquiring its session lock. New repository_runtime.py resolves paths before containment checks (including junctions),redirects child TEMP/TMP/TMPDIR,HOME/USERPROFILE,APPDATA/LOCALAPPDATA,XDG config/data/cache,shader-cache and Python-cache destinations beneath each run. Packager and emulator both receive that environment and explicit repository-local working directories. Missing runtime files fail closed; no external executable fallback. Focused checks accepted local destinations and rejected both direct outside paths and an in-repository junction resolving outside; temporary test junction removed. Python syntax/diff checks passed. During live execution,XEMU reported its base path under artifacts/crash-hunt/20261022-153355-L20S2.rfl/profile/AppData/Roaming/xemu/xemu. This verifies that reported base path,not every possible OS/driver write.

153340 preparation incorrectly requested L20S2 from levels3.vpp and stopped HARNESS_ERROR before launching XEMU. Saved historical recipe identified levels2.vpp; corrected153355 retained all45 authored actors,supported actor18260/distance12,finite rail8 priority18260,mobile profile,stock64,1800-input/360-second bound. The existing generated hidden actor remains part of normal scene setup; no roster reduction.

153355 ended TIMEOUT after1094 live/331 dead inputs,three lives314/516/264 and two observed normal recoveries. Per-life shots7/16/6,direct hit contacts4/14/3,direct kills1/2/1,NPC attacks13/14/13,cannon launches4/6/5 and impacts3/6/5; up to two simultaneous NPC alerts. Sampled same-scene intervals include720 live inputs with enemy action and180 without; boundary intervals excluded. No guest fatal,player failure,query error or outside-world escape logged; no optional bank-capacity refusal in this run. No turret shots,grenades,flame,terrain edits/debris; fighter-specific attacks are not separately established. Visual/audio correctness unverified. This expands repeated combat/reload evidence after the audio fixes; it does not complete the sustained crash hunt or qualify the full1800-input scenario.

After terminal session,preserve compact logs/result/lifecycle and runtime copies; prune regenerable payloads and check repository size without junctions. No host input,images,hardware deployment or push. Next continue authored active-enemy/weapon mixes with verified floor support and useful return fire; runtime already available locally. Before any future cross-build,audit toolchain output/temp/cache destinations under the new confinement rule.


## October22: wrap-up and restart point

User requested a break and a recorded stopping point. Stopped the active terminal session6540 for artifacts/crash-hunt/20261022-154150-L19S1.rfl; subsequent process inspection found no project Python/XEMU processes. The run has raw RAM polls but an empty telemetry.jsonl and no finalized result.json. Added a separate wrap-up.json rather than fabricating a harness result. This is an operator-stopped, unqualified attempt, not a demonstrated game crash, pass, or sustained action sample. No source implementation changed during this wrap-up.

Recipe was L19S1.rfl / levels3.vpp, actor9911 distance4, finite grenade loadout6, aim, explosives scenario,1800 inputs/360 seconds, jkdf2-hmp. On explicit resume, inspect retained raw logs and collector behavior before retrying; establish actual telemetry collection and useful player/enemy action. Latest completed evidence remains the153355 L20S2 combat run documented above. Broader crash hunt remains unfinished; repeated destruction cuts, Drone NPC-driven contact and white/yellow effect diagnosis remain open.

External read/copy approvals for XEMU runtime/firmware and extract-xiso are recorded and already implemented in ignored local/runtime with provenance manifests. No further outside work is implied. Before future compilation, audit confinement of output/temp/cache paths. Preserve unrelated untracked files and existing delivery; private crash fixes are not automatically incorporated in the tester package. No new push, deployment, image capture or host input. No percentages unless directly requested.


## October22: resume and correction of premature wrap-up

The previous pause incorrectly applied an older wrap-up request after work had resumed. User continuation restores the full crash-hunt objective. Current inspection also corrects the prior empty-telemetry statement:154150 telemetry.jsonl now contains81109 bytes and valid samples through266 live/211 dead inputs, two deaths and one observed recovery. No finalized result exists, so the interrupted run remains unqualified. Per-life player grenade releases0/1, detonations0/1, enemy flame hits17/8, NPC attacks23/14 and up to four alerted actors;120 eligible sampled live inputs contain enemy action. No logged player-failure tuple in the retained last sample. A separate wrap-up.json records the interruption without inventing a normal harness result.

Expanded the offline lifecycle report with player grenade release/contact/detonation/limited-tick maxima, checked against scene_grenade_gameplay.inc counter ownership. Throw starts remain counted in ordinary player shots; a start is not a release. grenade_throw.c currently requires102 ticks for a primary release (96 alternate), with180 cooldown after successful spawn; no timing or enemy behavior changed. Repeating the exact supported grenade recipe in154720 to obtain a terminal bounded outcome.

154720 completed its360-second watchdog as TIMEOUT,593 live/451 dead inputs,four deaths/three observed recoveries. All26 authored actors retained. Per-life player grenade releases0/1/1/1,detonations0/1/1/0,contacts0/0/0/13,limited ticks zero; one throw start each life. NPC attacks23/14/18/26,flame hits17/8/13/17,NPC grenade launches1/0/0/2 and detonations0/0/0/1,up to four alerted actors each life. Sourced blast NPC kills0/0/2/2 and player hits0/1/1/0 cannot alone identify the blast owner.360 sampled same-scene live inputs contain enemy action; zero eligible quiet inputs,with death/reload boundaries excluded. No logged guest fatal,player failure,visibility-query error or outside-world stop; optional audio-bank declines zero. No terrain edits/debris or visual/audio validation.

This extends explosive/combat recovery evidence without qualifying the full1800-input encounter or sustained stability. Next compare ordinary mobile input with this grenade loadout to improve survival and repeated releases while retaining authored actors,finite ammunition and normal damage; do not merely repeat short stationary lives as broad coverage. Source implementation was unchanged; offline report fields added and exercised on both existing and new telemetry. Session76284 exited normally after watchdog cleanup; no restart from observation timeout. Clean generated payloads,preserve compact evidence/delivery and unrelated work. Goal remains active.


## October22: mobile grenade comparison

155453-L19S1.rfl repeats154720 with only scenario mobile instead of explosives: levels3,actor9911/distance4,grenade6,aim,all26 authored actors,stock64,1800 inputs/360 seconds. Existing binary reused; no engine changes or build. TIMEOUT at667 live/451 dead inputs,four lives150/128/162/227 and three observed recoveries. Previous explosives profile had593 live inputs; neither completed1800.

Per-life player grenade releases1/1/0/1,detonations1/1/0/0,contacts0/0/0/14 and no limited ticks; player shot/start counts1/1/3/3,direct hit contacts0/0/2/2,direct kills0/0/0/1. NPC attacks16/20/26/31,flame hits8/16/20/13,NPC grenade launches1/1/1/3,detonations0/0/0/3,up to four alerted actors.360 sampled same-scene live inputs contain enemy action and zero eligible quiet inputs; boundary intervals excluded. No reported guest fatal; normal watchdog termination. No destruction/debris coverage or visual/audio validation.

Third-life telemetry shows grenade start without release at cumulative329 live inputs,selected slot5; by389 it is slot0 with one normal weapon cycle and still no release. Diagnostic scenario phase6 issues cycle at tick330. Core grenade_throw.c cancels phase1 on deselection and requires102 ticks before primary release. This supports scheduled-cycle cancellation of the pending third-life throw,not a projectile failure. First-life release improved with movement,but aggregate releases/detonations match previous run; do not call sustained stress solved.

User correction: switching during a pending throw is legitimate player behavior and valuable crash-hunt coverage. Do not suppress it or treat cancellation as a defect merely because it reduces grenade releases. No source change was made. Preserve this sequence as evidence of interrupted-throw handling under combat stress. Continue broader authored action and state-transition coverage; investigate actual fatal errors, hangs or invalid world state rather than optimizing the driver for successful throws.

Session18569 terminated normally at the watchdog; cleanup preserves compact results,logs,lifecycle and ready delivery. Unrelated local work preserved. No push,host input,images,PC builds or hardware deployment.


## October22: broad gameplay crash hunt, L11S2

User clarified to keep this practical: spawn,move,fire,switch weapons and use vehicles where available; do not optimize health loss or successful attacks. Recorded this in AGENTS.md. No gameplay or input-driver changes.

160309-L11S2.rfl uses levels2,actor10362/distance8,finite grenade6,aim,mobile,2400-input/360-second cap,stock64 and the complete authored roster. TIMEOUT after1128 live inputs,three combat lives and two normal recoveries. Movement,grenade throwing,weapon switching and enemy fire occurred. No reported guest fatal or invalid-world stop; the watchdog is not a crash or completion verdict. Detailed telemetry/result/lifecycle retained; no visual/audio verification.

Next use existing broad exercise inputs (forward/side movement,primary/alternate fire,switch,reload,Use,jump,crouch) across another suitable authored space. Existing check_jeep_replay.py provides process-local driver/gunner input examples,not proof of current vehicle coverage. Do not replace this crash hunt with isolated control-performance tuning or invent vehicle success. Session56955 finished; cleanup preserves compact evidence and ready delivery. No PC build,images,host input,deployment or push. Goal remains active.


## October22: ordinary movement and interactions near an authored Jeep

161126-L18S1.rfl: levels3,authored Jeep10517/distance3,finite rifle2,existing exercise input,1800-input/300-second cap,stock64. Floor preflight accepted the nearby spawn; roster retained. Completed all1800 live inputs with movement,20 player shots,three weapon switches and scheduled alternate-fire,reload,Use,jump,crouch inputs. No reported guest error or outside-world stop. Enemy attacks/alerts were zero,so this is basic gameplay/state-transition coverage rather than active combat stress. Current RAM log lacks vehicle boarding/occupancy fields; driving is not established. Do not infer interaction success merely from input delivery. No source change,images or host input.

Read the repository's authored vehicle inventory to locate Jeep10517; compact inventory retained at artifacts/crash-hunt/authored-vehicles.json. Since the nearby encounter stayed quiet,follow with the same broad input actions in the known active L6S3 group,finite shotgun4,actor3840/distance8,with bounded aim. No combat outcome tuning.

161440-L6S3.rfl: levels1,actor3840/distance8,finite shotgun4,bounded aim,exercise input,1800-input/240-second cap. TIMEOUT after886 live/255 dead inputs,two combat lives/two deaths and one observed recovery; current logs advanced after a long loading interval,so no freeze conclusion from the earlier stale sample. Per-scene maxima include two player shots,74 enemy attacks and four alerted actors. No reported guest fatal or outside-world stop. The input budget was not completed. Existing exercise input does not publish the scenario-phase trace; do not apply the scenario-only lifecycle summarizer as if zero output meant zero gameplay. Raw activity and result retain the evidence.

Both sessions terminal; preserve compact logs/results and ready delivery,clean generated payloads. Goal stays active. Vehicle driving remains unverified; retain nearby Jeep attempt as interaction coverage only. No code tuning or new build.


## October22: required vehicle lifecycle coverage

User requires entry,movement,firing,death and exit for every vehicle type. Added existing vehicle state/damage,Jeep seats,APC/Jeep gun,submarine/fighter weapons and drill counters to the opt-in RAM log; unchanged OpenJKDF2 HMP reader. Added ordinary vehicle replay inputs based on check_jeep_replay: Use,throttle/steering,primary/alternate fire,seat cycle,exit. No direct occupancy,health or gameplay-state writes.

Human explicitly authorized reading/executing C:/nxdk and C:/msys64. Dry-run build commands inspected; actual compilation used repository-local temp/profile/cache environment via repository_runtime.child_environment,explicit build/xbox cwd and private crash-disc output. build-vehicle-log.log records successful Xbox compile/link; no PC build or external installation modification.

162210-L18S1 completed1200 inputs but vehicle profile/state remained zero,so no boarding/driving claim. Selected supported authored L12S1 Jeep7629 next,without changing its roster.162447-L12S1 completed1200 inputs with one entry,one exit,15 mounted rounds,528 gunner frames and zero player driver frames. Vehicle moved while occupied,received10 damage calls and remained alive; enemy fire occurred. Neither player nor vehicle death observed. No guest fatal. Compact vehicle-coverage.json distinguishes occupied movement from player driving; raw counters retained.

Current coverage:

| Vehicle | Entry | Player driving | Firing | Death | Exit |
| --- | --- | --- | --- | --- | --- |
| Jeep | observed | open (gunner only; host moved) | observed | open | observed |
| Driller | open | open | open | open | open |
| APC | open | open | open | open | open |
| Submarine | open | open | open | open | open |
| Fighter | open | open | open | open | open |

Next choose an unoccupied authored Jeep for driver control and retain combat exposure for death; continue the other vehicle types through ordinary gameplay actions. Do not count sent inputs as outcomes or optimize successful combat. No visuals/audio verified. Cleanup after terminal sessions retains compact evidence/private build and ready delivery. No push/deployment; goal active.


## October22: Jeep player driving and APC admission

163022-L10S1.rfl: visible unoccupied authored Jeep2485,levels2,vehicle replay1800/300,stock64. Completed with three entries/three exits,1074 driver frames,510 gunner frames,six seat switches and24 mounted rounds. Occupied driver samples moved25.04 units from the first driver sample. No vehicle or player death,damage calls,enemy shots or reported fatal error. This establishes player driving alongside entry/fire/exit,not combat/death coverage. All authored actors retained. Jeep death remains required.

163315-L1S3.rfl: APC9627,vehicle replay1800/300 completed without a reported fatal,but entries/firing/exits stayed zero despite enemy fire. Inspection of original record confirms creation_flags2 (hidden); existing source rejects use on hidden hosts and prior fixture activates original UnHide9628. This is expected admission behavior,not a boarding defect. Do not claim successful APC lifecycle coverage. Visible unoccupied APC3303 in L15S1 chosen next; no hidden flags changed and no event bypass added.

163556-L15S1.rfl: visible authored APC3303,levels3,vehicle replay1800/300,stock64. Completed1800 total inputs (1514 live/286 dead) with one player death and normal recovery. First life records entry,occupied movement and11 mounted rounds. Player dies at frame255 while seated; vehicle state transitions from occupied to one exit by sample300,while vehicle destroyed remains zero. This is occupied-player death/seat release coverage,not voluntary exit or destroyed-APC coverage. No guest fatal reported; later on-foot firing/enemy attacks continue after recovery. Vehicle-coverage.json retained for each run.

Updated lifecycle status: Jeep entry/player driving/fire/voluntary exit observed; player/vehicle death still open. APC entry/movement/fire and occupied-player death/recovery observed; voluntary exit and chassis destruction still open. Driller,submarine and fighter remain pending for all requested actions. Continue those types next without declaring lifecycle coverage complete from these partial results. No source changes this turn; no host input,images,PC build,hardware deployment or push. Sessions terminal; cleanup preserves compact logs/reports and ready delivery. Goal active.


## October22: Driller admission and submarine occupied death

164149-L1S2.rfl: authored visible Driller8122,vehicle input1800/300,stock64. Completed1646 live/154 dead inputs with on-foot combat,one death and normal recovery,but zero vehicle entries/exits and drill contacts. Do not count this as Driller driving/weapon/death coverage. Added the existing16-word rf_scene_vehicle_entry_probe to RAM logging (observational only); approved toolchain rebuild passed with repository-local temp/cache/output (build-vehicle-entry-log.log).

164653-L5S3.rfl: authored submarine3963,vehicle input1800/300. Completed1800 total inputs (1641 live/159 dead),one entry,two torpedo launches/detonations and4.50 units of sampled occupied movement. Player died while seated at frame382; seat released by sample420,submarine destroyed counter remained zero. Normal recovery completed and on-foot inputs continued; no reported guest fatal or invalid-world stop. This establishes entry/movement/firing and occupied-player death/recovery,not voluntary underwater exit or destroyed-submarine coverage.

165045-L1S2.rfl short120-input diagnostic replay completed with vehicle runtime active but all entry-probe words zero. The entry-allowed callback was never reached; this does not demonstrate a collision/path rejection. Next trace ordinary Use selection/ownership and scene_vehicle_physics_allows_control before changing gameplay or repeatedly rerunning a full no-entry sequence. Source locations: scene_use_edge.inc,scene_vehicle_switch.inc,scene_driller_runtime.inc around use_input/can_use. No control gates bypassed.

Fighter lifecycle remains pending. Jeep/APC/submarine chassis destruction and unobserved voluntary exits remain open. Do not mark broad crash hunt complete. All sessions terminal; cleanup preserves compact evidence and ready delivery. No images,host input,PC build,deployment or push.


## October23: full retest pass after the 5.6 Sol move

Every previously exercised recipe was replayed on stock64 from the current tree:
108 unique recipes (latest non-preflight-rejected run per recipe, original frame
and watchdog budgets). Compact per-recipe results, prior statuses and floor-retry
distances are retained in artifacts/crash-hunt/retest-20261023.json. Older reports
stay historical evidence only.

Outcome: 70 COMPLETED, 36 TIMEOUT, 2 unrunnable, zero GUEST_ERROR, zero
PROCESS_EXITED, every enemy-failure word zero. Each TIMEOUT kept advancing live
or dead inputs to the watchdog (largest gap one 15-second poll during a death
reload), so none is a freeze; they are lethal encounters (L14S2, L18S3, L19S1/2A,
L17S3, L20S2) cycling ordinary death/restart, or early180-second recipes whose
level load consumes most of the budget. Earlier PROCESS_EXITED rows (L8S2, L14S2,
L13S1, L19S2A, L18S3, L19S1) were deliberately short burst instances, not crashes;
their recipes now run to TIMEOUT/COMPLETED. Improvements over earlier results:
L17S1 goal-block stop, L2S2a explosives, L8S2 rifle/rocket, L19S3 and L11S2 now
complete their full budgets.

Seven early recipes predate the static floor preflight and are now rejected at
their original distance. Only the staging distance was changed (nearest supported
of 3..14): L20S1 3, L9S4 6 (both recipes), L13S1 5, L17S3 18723 6 ran; L3S4 844
distance4 (its spawn-y variants still run) and L17S3 14435 have no supported
distance and remain unrunnable. Retry folders collided when two preflight
rejections fell in the same second; the retry driver now spaces attempts.

Vehicle recipes (17) reproduce prior counters exactly: Jeep entries/driving/seat
switches/24 mounted rounds (L10S1), L12S1 gunner/damage and stress chassis
destruction with exit, APC L15S1 entry/11 rounds/occupied death, submarine L5S3
torpedoes/occupied death, Fighter L18S2 35 shots, Driller L1S2 220 drill contacts
with bullet health unchanged. Replay is deterministic across the renderer change.

### One-frame dark flash (user-reported)

User observed an intermittent one-frame black flash at roughly one-second cadence
during runs. Cause in source: the streaming preview and every menu draw called
pb_show_front_screen, which writes PCRTC_START=pb_FBAddr[pb_front_index]; nxdk
pbkit sets pb_front_index=0 at init and never advances it, while its VBlank ISR
owns scanout of completed back buffers. Each frame therefore re-pointed scanout at
framebuffer0; whenever no flip was pending at the next VBlank (every frame here is
late, see below), the display latched framebuffer0, which in the triple-buffer
rotation is periodically the buffer being cleared to 0xff101018 and drawn. Fix:
switch off the debug screen once per pbkit lifetime (reset on pb_kill) and leave
presentation to the ISR. Built into crash and tester discs; USER visual
confirmation pending.

### Frame rate

Added rf_xbox_frame_rate to the opt-in RAM log (guest ms, frame-start VBlank,
frame-clock ticks/presentations, late frame starts). The crash replay advances one
simulation tick per drawn frame without the player frame clock, so the game runs
slowed rather than dropping frames. XEMU measurements: L13S1 mercenary group median
22.8 fps (20.7..29.2), L17S3 laser/rail/turret group median 24.9 fps; every frame
start found its VBlank already crossed. Emulator throughput on the host, not a
hardware measurement.
