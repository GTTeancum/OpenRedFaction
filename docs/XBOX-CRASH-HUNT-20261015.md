# Xbox crash hunt — October 15, 2026

The user authorized targeted runtime crash hunting and virtual gameplay input,
including forcing environment/encounter placement. Xbox stock 64 MiB remains the
runtime target. No PC binaries, host input or image capture are used.

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
