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
checks and the original actors/routes. Runtime validation pending.

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

`tools/xemu_crash_hunt.py` owns build/xbox/crash-disc and redfaction-crash.iso.
It never edits tester-disc. Immutable assets are hardlinked into the diagnostic
stage. All scenarios use the existing standalone local/xemu-harness/pacing-base.qcow2
with XEMU -snapshot, a private EEPROM copy, disabled host controller bindings,
localhost QMP, and at most180 seconds per run. It closes only its own emulator.

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
