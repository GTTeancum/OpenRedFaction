# Authored Jeep driver and player gunner

2026-10-03: entry/runtime/seat/damage integration is complete; parent-owned NPC host admission, weapon ownership/exclusion and load guards remain below. Xbox build and native validation are pending. The moving APC exit scheduling remains unchanged.

`scene_jeep_npc_gunner.inc` closes a concrete ownership gap: the existing generic player possession refuses any nonempty `host.driver`, while Jeep role switching assumes that the player occupies that same single driver word. L12S1 already binds miner7646 to Jeep7629's original driver tag. The new adapter keeps that real NPC driver and admits the player at `interface_2`, using the existing use radius, seated-head transit, authored tag/camera and full-body safe-exit queries. It does not create an NPC or attach nearby actors.

The two real physical slots are driver0 and gunner1. The existing player `session.driver` remains the player control-session owner; the real `host.driver` remains the NPC. A local host-control copy adapts the generic player transaction and is never published over physical ownership. No second seat bank is introduced. Driver death can leave an unpiloted player gunner. After the host settles, ordinary cycle input transfers the player into the empty driver seat without dismounting or reboarding; occupied seats remain protected.

## Integrated gameplay path

The entry owner exposes two real Jeep occupant words while other vehicles retain one. The NPC seat resolver still writes only slot0. Runtime Use dispatch tries the separate gunner adapter before generic possession; Jeep cycle input consumes and rejects a switch into the occupied driver role. The NPC route continues while the player session owns gunner role1, with existing death/catatonic driver gating intact.

Player camera/body publication uses the existing authored interface_2 tag. Gun rounds and aim queries retain the session player's handle separately from the physical NPC driver; sweeps and direct damage exclude the attached NPC too. In-flight rounds retain their launch-time player token. Use/death/wreck release clears the gunner slot and player's parent without overwriting NPC ownership. Whole-level teardown unregisters the host normally; no extra live unpossess is attempted.

## Save boundary

Ordinary fresh-boot shared-seat persistence now uses explicit RFNS2 ownership and passes the focused Xbox continuation documented in `JEEP-NPC-GUNNER-SAVE.md`. RFNS1 retains its old single-seat semantics. Loading while currently a separate gunner remains rejected before mutation; mounted-flight/cooldown saves remain outside this supported slice. RFNS3 now also restores a parked player gunner after the authored driver dies; see the unpiloted continuation in `JEEP-NPC-GUNNER-SAVE.md`.

## Focused Xbox harness

`python tools/xemu_npc_jeep_gunner.py` owns its temporary disc flags and restores them in `finally`; parent runs it serially. `--prepare-only artifacts/fixtures/npc-jeep-gunner` only produces local fixture inputs. AST parsing and prepare-only round-trip decoding passed; no emulator/build was run by the helper.

The fixture keeps the complete L12S1 miner/Jeep records and installed resource values. Their transforms move4m closer than the earlier6m fixture distance, inside the real5m use radius. Original player spawn and collision geometry remain unchanged. Set_AI_Mode parks the Jeep at0; Use90 boards after settling; Follow_Waypoints is requested at60 with an ordinary1s event delay, so120 starts the one distant fixture node; cycle140 attempts the occupied driver seat; fire145..169 uses real Jeep Gun rounds while backwards movement is deliberately supplied; Use210 exits; run stops240. A live memory probe180..209 must prove separate driver/gunner slots, no player steering, retained NPC driver health/action, mounted shots and ongoing NPC route commands. Final state must prove a real safe exit, no remaining player parent, preserved NPC driver, and continuing route commands.

This is a bounded functional input fixture, not a campaign walkthrough. A failed native clearance or missed live-probe window is a failed check, never inferred success. No ordinary coexistence save, destruction ejection, visual, or broad route coverage is claimed.

Initial native admission evidence (`npc-jeep-gunner-20261003-124221`): first Use reached the real seated-path query and returned clear0/status0; squared body-to-host distance21.1064 was within radius5. The fixture host was still elevated at y2.21608. The next check delays boarding until90 and route start until120; no runtime collision/range rule was relaxed.

The delayed-board run (`npc-jeep-gunner-20261003-124946`) instead failed range admission: squared distance33.9974 exceeded25 because the chassis rolled while settling. Fixture spacing was reduced to2m initially; normal5m Use radius and collision queries remain unchanged.

## Initial blocked integration evidence (resolved below)

`artifacts/xemu/npc-jeep-gunner-20261003-125206/report.json` completed240 frames on stock64MiB. The first admission reached stage2 (seated-head path), clear0/status0, with squared range22.62085 inside the25 limit. Player body was[.09230,-.41205,.49175], host origin[.09230,1.68397,4.76112]. No player gunner session or gun rounds were created. The NPC remained registered/seated and its route ran; this is not a gunner pass.

The diagnostic next action was: retain the actual head-query start/end, radius and rejected world/mover contact, then resolve the geometric cause without weakening range, clearance or ownership invariants. No save/coexistence, firing, switch or exit success is claimed. One intermediate scheduling attempt used an unsupported12-byte setup interpretation and stopped before frames; the harness now uses the supported two-UID setup with an ordinary1s event delay instead.

Admission telemetry `rf_scene_jeep_entry_probe[12]` retains the first Jeep admission: attempts, stage(range1/path2), allowed, status, distance-squared float, radius float, playerXYZ, hostXYZ. Runtime resets it with the vehicle owner. The final NPC-seat aggregate legitimately records one teardown detach; live probe assertions still require active ownership, and final retained gunner/driver identity assertions remain strict.

## Geometric boarding diagnosis (2026-10-03)

`npc-jeep-gunner-20261003-125655` identifies static face92, a ceiling strip with plane(0,-1,0,3), footprint x[-6,7],z[3,4]. The radius.15 head sweep from[.09229878,.46867108,.49174702] to[.09229881,3.19255137,4.22381687] intersects at fraction.874241398, matching native.874241412. Contact[.09229881,3,3.75447699] is inside the polygon. The collision rejection is correct; no own-host exclusion or smaller radius is appropriate.

Floor face171 is y=-1.25. Installed Jeep wheel spheres have minimum localY=.040839687-.75=-.709160313 (table values do not override that radius), giving grounded originY=-.540839687. The fixture now uses-.530839687 for1cm clearance and applies the same vertical translation to the authored driver. This replaces the airborne spawn-eye+.8 placement; entity records/resources, collision and range policy are otherwise retained. Native functional check passed as recorded below.

## Passing Xbox result

`artifacts/xemu/npc-jeep-gunner-20261003-130215/report.json`:240 frames, live probe182, one player gunner entry/exit, one rejected driver-seat switch, three real Jeep Gun projectile launches, zero new launches/ammo spending after release, preserved NPC driver UID7646/host7629 and distinct player ownership. NPC route commands advance62 to120 between probe/final; retained chassis positions move from[.09231,-.62784,4.93260] to[.46048,-1.24691,10.71078] across that interval. Final telemetry verifies player link and gunner slot released while the NPC driver remains. Native free pages2878 (11.24MiB).

This proves bounded boarding, shared ownership, firing dispatch and safe exit. Current-session gunner loads, projectile damage/source attribution in a target encounter, driver/host death ejection and visual/audio presentation remain open or unverified. The failed high-spawn attempts above are retained as diagnosis, not remaining runtime failures. No source collision policy was loosened.


## Empty driver handoff (2026-10-03)

`scene_jeep_gunner_driver_transfer.inc` runs before the old single-owner seat switch. It requires the real NPC driver to have detached, physical slot0 to be empty, the existing player gunner session and saved exit look to be valid, and linear/angular speed no greater than0.1. The authored interface_1 placement must pass the existing seated-head sweep. Publication finishes all fallible placement work before assigning host.driver and slot0 to the player and clearing slot1; failure retains the gunner seat. The same possession session and original exit snapshot survive. A successful handoff clears gunner aim, enables normal steering, and leaves the detached NPC and suspended route alone.

`tools/xemu_jeep_driver_transfer.py --exit` uses the grounded original Jeep/miner pair. STOP parks the host; Use90 boards, cycle100 rejects the occupied driver, ordinary delayed Slay120 kills/detaches the driver, cycle150 requests transfer, throttle160–190 drives, a live memory probe200 verifies actual ownership and displacement, and Use220 requests exit. No ownership injection, campaign traversal, screenshots or host input. Native PASS: `artifacts/xemu/jeep-driver-transfer-20261003-140009/report.json` completed240 frames on stock64MiB. Live probe202 records exactly one occupied-seat rejection and one successful handoff, actual player ownership of host.driver/slot0, empty slot1, dead NPC detached once, and0.9698m horizontal motion from the transfer pose. Ordinary Use exit releases both slots and the player parent without reboarding. Minimum reported free pages: 2874. Fixture and restoration builds passed; disc flags restored. Visual/audio presentation, save/load after transfer and moving-seat rejection were not separately exercised.
