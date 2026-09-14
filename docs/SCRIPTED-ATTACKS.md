# Scripted attacks: first playable implementation

Attack event 38 now calls a scene-owned combat backend on activation and cancellation, including delayed activation. This is a practical shared PC/Xbox implementation, not a reconstruction of the complete original AI state machine.

## Source evidence

For RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836:

- Loader 462150 calls 4b86d0 for type 38; 4b86d0 stores its numeric argument at event +2b8.
- Factory 4b69d0 constructs the event through 4beae0, whose vtable is 589b1c.
- On action 4bcac0 resolves the +2b8 UID via 48a4a0, obtains the actor via 426fc0, and selects the first resolvable linked target. It resets AI through 408ac0 and assigns the target through 409050. A matching player name overrides that target with the player.
- Off action 4bcba0 clears the target through 409050(-1), then calls 407ee0.
- Installed L1S1 event 9394 has words[0]=8638 and target link UID 9391. This is the attacker/target direction used by the implementation.

These are static decompile/disassembly observations. Original AI consumers have not been execution-equivalence tested. The exact case sensitivity of the original name comparison remains unverified; the current backend accepts literal `player`.

## Runtime behavior

The scene resolves the attacker from its authored UID, chooses a live supported NPC/player target, and retains an explicit target handle. Cancellation, allegiance changes and respawn clear that state. The existing basic enemy firing loop checks geometry obstruction, range and cooldown, then routes damage to that target. NPC lethal damage enters the existing death path. A stale or dead explicit NPC target cannot silently become the player. Commanded movement stops when an explicit attack starts.

No attack command allocates memory; ownership adds two 32-bit fields per NPC. Other target families, pursuit, aiming/firing animation, weapon-specific NPC damage/rates and NPC-source retaliation remain unfinished. Existing basic firing uses ten damage and a sixty-tick interval. The world combat loop still pauses when the player is dead.

## Evidence and limits

All 37 CTest tests pass. Added coverage includes actual L1S1 event9394 dispatch, delayed on/off, missing-backend retention, ordered target selection, player override, cancellation, nonfatal NPC-to-NPC damage, cooldown and stale target handles. These tests do not demonstrate a full natural-trigger encounter or lethal scripted-shot presentation.

The NXDK build passes. XEMU replay `replay-20260914-105213` passes the stock64MiB 180-frame L1S2-to-L1S3 crossing and state checks. It is a compatibility check for the changed build, not proof that an authored Attack sequence played correctly. No capture was requested. Local evidence lives in `artifacts/scripted-attack/`; original analysis exports remain untracked.

## Pursuit first pass

Scripted armed attackers now request movement when their target is farther than twenty world units or their sight line is obstructed. They keep pursuing until a clear sight line is available inside sixteen units; the gap prevents rapid start/stop changes near one threshold. Decisions refresh every fifteen simulation ticks, independently of the shooting cooldown. These thresholds are practical first-pass choices, not recovered retail constants.

Pursuit feeds the existing navigation, ground support, steering and body-sweep path. Its destination follows the selected NPC body or the player body. Movement beyond one horizontal unit updates that destination and invalidates the retained route; smaller movement keeps the route and retry state. No new navigation allocation is introduced. Two additional words per NPC retain the decision deadline and a deferred stand request.

Combat pursuit owns follow mode2; authored Goto_Player keeps mode1. Cancelling or invalidating a pursuit clears its route and requests standing on the next live actor update. A new authored Goto command supersedes explicit combat targeting. Allegiance changes and respawn cancel combat-owned movement.

PC tests cover distant and wall-obstructed targets, destination/retry updates, clear-sight stopping, stale handles, walking-to-standing cancellation, and preservation of unrelated Goto_Player movement. They verify decision-to-movement handoff, not a complete character navigating an authored encounter. Full-route pursuit, doors/obstacles during pursuit, aiming/firing presentation and natural encounter validation remain open.

The NXDK build and stock64MiB XEMU replay `replay-20260914-105927` pass the 180-frame L1S2-to-L1S3 transition/state checks with pursuit enabled. No capture was requested. This is build compatibility coverage, not full authored-pursuit validation. All37 PC tests pass; evidence is in `artifacts/scripted-pursuit/`.

## Authored encounter replay

`tools/replay_scripted_attack.py` runs three process-local checkpoints for L1S1 event9802. The existing NPC-event replay entry now accepts Attack38 as well as Goto types; `RF_REPLAY_GOTO_UID=9802` is the legacy harness variable used for this controlled request at frame30. It retains the authored13.75-second delay, attacker8324 and target8322. The player/camera is staged beside8324. It does not activate the natural trigger chain.

At840 frames the command has not activated. At1000 the attacker has moved3.60 world units; at3000 it has moved35.62 units, completed1418 movement steps and encountered726 blocked steps. All36 route searches failed. No shots were fired and target health remains100. The report is deliberately `PURSUIT_VERIFIED_ENCOUNTER_INCOMPLETE`, not a completed-encounter pass. The navigation workspace is loaded with333 nodes.

The final attacker position is(-78.136,-5.629,45.468), versus target(-78.650,-4.554,48.784). Inspect the nearby geometry, door activation and omitted setup-chain prerequisites before deciding whether the route algorithm itself requires repair. This test proves movement through the real scene and exposes an unresolved engagement; it does not justify bypassing a closed door.

The earlier L1S1 event9394 dispatch fixture references attacker8638, which the installed actor reader could not locate for staging. Its unit test proves event dispatch only.

The new diagnostics retain the last successful attack owner, target, health, shot counts, pursuit ticks and sampled positions. PC tests37/37 and NXDK build pass. Evidence: `artifacts/attack-encounter/verified/report.json` and per-checkpoint logs. No new Xbox encounter proof or image comparison is claimed.

## Actor-sized route fallback fixes the controlled encounter

The blocked sample reports static world face5974 in room53, with no mover/object owner. It has188 eligible navigation nodes, a valid goal node32 and no start node. The scene adapter was interpreting the shared selector's fixed2.5 callback argument as a sweep radius even though this actor's body radius is about0.43. The shared original selector is unchanged; the scene adapter now supplies the actual actor radius for its fallback clearance check. The graph and normal body/ground collision checks remain active. This is a practical scene integration choice, not a new retail-equivalence claim.

In the matched3000-frame replay, the old adapter produced36 failed routes,726 blocked movement steps and no shots. The actor-sized adapter selects a four-node route, records no blocked movement steps and fires14 shots; the target reaches32.8 health. At4000 frames it has fired20 shots, the target health is effectively zero (small negative floating-point residue), and COMBAT_DEATH records entry into the existing death presentation. The attacker stops at(-78.200,-6.913,41.228), with the target at(-78.650,-4.554,48.784).

The replay now checks all four checkpoints840/1000/3000/4000 and fails unless the target is defeated and death presentation is entered. It still deliberately requests the authored event through the process-local harness. Natural trigger traversal, visible firing animation/aiming, and an Xbox execution of this entire encounter remain unproven.

Additional route diagnostics report actor clearance, eligible-node count, selected start/goal, containment and graph result; obstruction diagnostics retain the last body-hit solid, room, face, normal, fraction and owner. Evidence is under `artifacts/attack-obstruction/`, with the current full replay report in `artifacts/attack-encounter/verified/`.

Validation:37 PC tests pass and the controlled encounter regression passes. Stock64MiB XEMU replay `replay-20260914-111450` passes the180-frame L1S2-to-L1S3 transition/state checks; NXDK build/restoration succeeds. No native image was requested. This checks Xbox compatibility, not the complete Attack9802 encounter on Xbox.

## Stationary aiming and firing direction

Alert, armed, living NPCs that are not currently walking now steer toward their selected live target through the same retained steering/angular prediction/ordinary commit path used by scripted movement. The stationary path copies current position to proposed position first, so turning does not intentionally translate the actor. The committed body orientation is published to the model owner. Retired, hidden, dead and unsupported actors are excluded.

The basic firing loop now requires the target within a30-degree horizontal cone around the current facing direction. A rejected aim does not consume the shot cooldown. This is a practical first-pass threshold; vertical aiming, upper-body pose blends and weapon-specific firing animations remain open. Walking attackers keep their route steering and obey the same firing cone.

Tests cover front/side/back alignment and a real damage dispatch held while facing backward, followed by a shot after alignment. The controlled4000-frame encounter still defeats the target with20 shots and records1171 stationary aiming updates. This sample already has suitable facing at shot time; its aim-hold count is zero. It does not demonstrate a large stationary turn or visually validate body/weapon presentation. The replay regression now requires stationary aiming updates as well as target defeat and death entry.

Validation:37 PC tests and the controlled encounter regression pass. NXDK build/restoration and stock64MiB XEMU replay `replay-20260914-112202` pass180-frame L1S2-to-L1S3 transition/state checks. No capture was requested; full Xbox encounter execution and visual aiming remain separate. Evidence: `artifacts/enemy-aim/`.

## Authored firing clips and sound

Actual basic enemy shots now load and activate their selected `fire_stand` action2 through the existing motion cache and action player. The implementation uses the action mapping directly and does not alter the death-action owner. It confirms the clip is active after the request. Missing authored clips are counted and leave gameplay damage available; other playback errors propagate. Existing motion residency budgets remain in force.

An authored action sound label is requested at the attacker eye position. Where that label is absent, the known pistol and rifle use the already-supported Glock Launch and Assault Loop groups. This is a bounded first-pass fallback; remaining weapons, crouched fire, precise sound/muzzle timing and upper-body blending still need implementation. Audio errors remain in the existing diagnostic counters and do not stop gameplay.

The controlled Attack9802 regression still reaches target death and records20 shots,20 active firing clips (motion60),20 sound plays and zero audio errors. It now requires firing animation activation and audio success in addition to target defeat/death entry. All37 PC tests pass. NXDK and stock64MiB XEMU replay `replay-20260914-112944` pass the180-frame L1S2-to-L1S3 transition/state checks, which are compatibility coverage rather than full Xbox combat validation.

A PC process-local follow-camera capture at2200 frames was inspected: the robot attacker is visible in the mine, with the diagnostic first-person overlay still present. The log records one shot, active firing clip and sound play. One still frame does not verify the complete firing motion or exact alignment. Local output: `artifacts/enemy-fire/view.png`; no GitHub image upload. Muzzle/impact effects and visual sequence verification remain open.

## NPC retaliation

The gameplay damage callback now consumes AI-reaction notifications instead of discarding them. A living, visible, armed victim can acquire the recognized NPC or player that hit it. Reactive NPC targeting uses combat mode2 and shares the existing pursuit, aiming and firing paths. Explicit authored Attack mode1 keeps priority. Repeated hits from the same current target do not restart the30-tick reaction delay; a new valid source can replace a reactive target. Self hits, stale handles, zero incoming damage and unarmed/dead/hidden victims do not start retaliation. The source resolver now provides retained NPC/player affiliation instead of treating every NPC source as absent.

This is a practical gameplay reaction policy. The original407fb0 AI state machine, group assistance, unarmed reactions and faction-specific escalation rules are not implemented by this change. The separate original-behavior diagnostic entry retains its existing limited contract.

Tests cover direct source recognition, reaction/cooldown preservation, authored-order priority and rejection cases, plus retaliation reached through actual NPC damage dispatch. The controlled Attack9802 encounter now has39 scene-wide shots:20 from the original attacker and19 additional shots after the victim acquires that attacker. The victim is eventually defeated; all39 shots activate firing clips and sound playback with no audio errors. Telemetry records victim handle28705205 acquiring attacker28377520 and19 preserved authored-order reactions.

The previous replay assertion compared scene-wide effects against only the original attacker's20 shots. It correctly failed once the victim started shooting; the regression now compares effects to total scene shots and requires retaliation plus additional fire. This does not prove natural trigger traversal, full Xbox encounter execution or visual reaction fidelity. Evidence: `artifacts/npc-retaliation/` and `artifacts/attack-encounter/verified/`.

Validation:37 PC tests and the updated encounter regression pass. NXDK build/restoration and stock64MiB XEMU replay `replay-20260914-113808` pass180-frame L1S2-to-L1S3 transition/state checks. No capture requested. This remains compatibility coverage rather than full native duel validation.

## Full Xbox encounter check

The native replay harness now compares SCRIPT_ATTACK, ENEMY_AIM, ENEMY_FIRE and ENEMY_RETALIATION directly against the matching PC replay, in addition to its existing combat, audio, movement, animation and memory checks. Guest snapshots retain those counters for inspection. The existing `--goto-uid` option can request an authored Goto or Attack event; it does not bypass its delay.

Reproduce the controlled full encounter with:

```powershell
python tools/replay_scripted_attack.py
python tools/xemu_replay_check.py artifacts/attack-encounter/verified/inputs-4000.bin --campaign-spawn --level L1S1.rfl --actor-uid 8324 --goto-uid 9802 --seconds 900
```

This remains a process-local controlled event request and staged player position, not natural trigger traversal. The native run must reach4000 frames and pass its final comparison before it is treated as Xbox encounter evidence.

Result: `replay-20260914-114230` **PASS**,4000 frames,67108864 bytes guest RAM and no expansion. The native counters exactly match PC:39 shots,39 active firing clips,39 sound plays, one retaliation acquisition and one target death, with zero combat/audio errors. The delayed attacker accounts for20 shots and1290 pursuit ticks. NXDK restoration completed successfully. This closes controlled full-duel Xbox execution coverage; natural triggers and visual sequence verification remain open. Evidence: `artifacts/xemu/replay-20260914-114230/report.json`.

## Player-target pursuit

Player-target pursuit now uses the same movement path as scripted/reactive NPC
targets. Previously normal sight acquisition and retaliation against the player
set combat_alert with mode0, so the scripted-only navigation gate left these
enemies stationary when the player withdrew or broke line of sight. Alert actors
now request pursuit beyond20 units or behind an obstruction, retain it until
clear within16 units, and update the player's body position through the existing
navigation/ground/collision path. Unalerted neutral actors remain passive.
Tests cover actual sight acquisition without Attack, withdrawal, pursuit target,
and stopping hysteresis. All37 PC tests and NXDK build pass; the live PC hostile
and neutral replays retain8 shots/61.6 health and0 shots/100 health respectively.
This is practical continuous target tracking; search/lost-target AI remains open.
A naturally traversed campaign encounter is still needed; actor staging is used
by the live awareness replay. Evidence:artifacts/xbox-duel/player-pursuit-*.
Stock64MiB XEMU replay `replay-20260914-115452` passes240 frames of the
staged hostile awareness encounter with exact PC combat/action comparisons;
NXDK restoration succeeds. This validates acquisition/fire compatibility on
Xbox; retreat/chase behavior is covered by the focused tests, not this idle replay.
