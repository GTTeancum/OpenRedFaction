# Authored Jeep driver and player gunner

2026-10-03: entry/runtime/seat/damage integration is complete; parent-owned NPC host admission, weapon ownership/exclusion and load guards remain below. Xbox build and native validation are pending. The moving APC exit scheduling remains unchanged.

`scene_jeep_npc_gunner.inc` closes a concrete ownership gap: the existing generic player possession refuses any nonempty `host.driver`, while Jeep role switching assumes that the player occupies that same single driver word. L12S1 already binds miner7646 to Jeep7629's original driver tag. The new adapter keeps that real NPC driver and admits the player at `interface_2`, using the existing use radius, seated-head transit, authored tag/camera and full-body safe-exit queries. It does not create an NPC or attach nearby actors.

The two real physical slots are driver0 and gunner1. The existing player `session.driver` remains the player control-session owner; the real `host.driver` remains the NPC. A local host-control copy adapts the generic player transaction and is never published over physical ownership. No second seat bank is introduced. Driver death can leave an unpiloted player gunner; the player can exit and re-enter to drive the vacant Jeep. Mid-session seat promotion is deferred.

## Integrated gameplay path

The entry owner exposes two real Jeep occupant words while other vehicles retain one. The NPC seat resolver still writes only slot0. Runtime Use dispatch tries the separate gunner adapter before generic possession; Jeep cycle input consumes and rejects a switch into the occupied driver role. The NPC route continues while the player session owns gunner role1, with existing death/catatonic driver gating intact.

Player camera/body publication uses the existing authored interface_2 tag. Gun rounds and aim queries retain the session player's handle separately from the physical NPC driver; sweeps and direct damage exclude the attached NPC too. In-flight rounds retain their launch-time player token. Use/death/wreck release clears the gunner slot and player's parent without overwriting NPC ownership. Whole-level teardown unregisters the host normally; no extra live unpossess is attempted.

## Save boundary

Ordinary saves while NPC driver and player gunner coexist remain unsupported. Existing `scene_vehicle_combat_checkpoint_common` rejects the mismatch between physical `host.driver` and player `session.driver`, and RFNS rejects a saved player-occupied host with an active NPC seat. Preserve both checks. Do not reinterpret the new second slot as an old single-driver save.

World load, quickload admission and direct vehicle checkpoint reads explicitly reject a currently active separate gunner before staging or publication. Supported single-slot restore clears the new gunner word. These guards prevent an unsupported session from being partially overwritten; they do not add coexistence persistence.


## Focused Xbox harness

`python tools/xemu_npc_jeep_gunner.py` owns its temporary disc flags and restores them in `finally`; parent runs it serially. `--prepare-only artifacts/fixtures/npc-jeep-gunner` only produces local fixture inputs. AST parsing and prepare-only round-trip decoding passed; no emulator/build was run by the helper.

The fixture keeps the complete L12S1 miner/Jeep records and installed resource values. Their transforms move4m closer than the earlier6m fixture distance, inside the real5m use radius. Original player spawn and collision geometry remain unchanged. Set_AI_Mode parks the Jeep at0; Use90 boards after settling; Follow_Waypoints is requested at60 with an ordinary1s event delay, so120 starts the one distant fixture node; cycle140 attempts the occupied driver seat; fire145..169 uses real Jeep Gun rounds while backwards movement is deliberately supplied; Use210 exits; run stops240. A live memory probe180..209 must prove separate driver/gunner slots, no player steering, retained NPC driver health/action, mounted shots and ongoing NPC route commands. Final state must prove a real safe exit, no remaining player parent, preserved NPC driver, and continuing route commands.

This is a bounded functional input fixture, not a campaign walkthrough. A failed native clearance or missed live-probe window is a failed check, never inferred success. No ordinary coexistence save, destruction ejection, visual, or broad route coverage is claimed.

Initial native admission evidence (`npc-jeep-gunner-20261003-124221`): first Use reached the real seated-path query and returned clear0/status0; squared body-to-host distance21.1064 was within radius5. The fixture host was still elevated at y2.21608. The next check delays boarding until90 and route start until120; no runtime collision/range rule was relaxed.

The delayed-board run (`npc-jeep-gunner-20261003-124946`) instead failed range admission: squared distance33.9974 exceeded25 because the chassis rolled while settling. Fixture spacing was reduced to2m initially; normal5m Use radius and collision queries remain unchanged.

## Current integration evidence and next blocker

`artifacts/xemu/npc-jeep-gunner-20261003-125206/report.json` completed240 frames on stock64MiB. The first admission reached stage2 (seated-head path), clear0/status0, with squared range22.62085 inside the25 limit. Player body was[.09230,-.41205,.49175], host origin[.09230,1.68397,4.76112]. No player gunner session or gun rounds were created. The NPC remained registered/seated and its route ran; this is not a gunner pass.

Stop repositioning by trial. Next action: retain the actual head-query start/end, radius and rejected world/mover contact, then resolve the geometric cause without weakening range, clearance or ownership invariants. No save/coexistence, firing, switch or exit success is claimed. One intermediate scheduling attempt used an unsupported12-byte setup interpretation and stopped before frames; the harness now uses the supported two-UID setup with an ordinary1s event delay instead.

Admission telemetry `rf_scene_jeep_entry_probe[12]` retains the first Jeep admission: attempts, stage(range1/path2), allowed, status, distance-squared float, radius float, playerXYZ, hostXYZ. Runtime resets it with the vehicle owner. The final NPC-seat aggregate legitimately records one teardown detach; live probe assertions still require active ownership, and final retained gunner/driver identity assertions remain strict.
