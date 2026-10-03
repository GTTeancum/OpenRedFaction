# Submarine and ground vehicle ownership

L10S4 contains submarine7010 and Jeep7009. Previously selecting the submarine at startup left the Jeep permanently passive: the ownership handoff accepted only ground profiles1..3. Profile4 now participates in the same preloaded, quiescent ownership exchange. The focused native handoff result and remaining save blocker are recorded below.

The submarine profile pack owns cockpit and torpedo resources while borrowing its chassis. Startup merges all textures before renderer submission and retains existing4MiB auxiliary budget and16MiB admission reserve. On promotion, chassis ownership and material bases transfer once, runtime collision/liquid pointers are rebound, and authored submerged movement parameters replace ground suspension control. Returning to ground clears water-only entry callbacks. Target submarine boarding must pass the existing complete player-sphere wet path and normal world/mover/actor clearance before registry mutation. The whole target hull must also fit the static world and remain fully wet, preventing promotion into an immobile partly dry submarine.

Each parked submarine retains its torpedo reserve/cooldown. A handoff rejects live torpedoes, positive cooldown and existing motion/attachment/route dependencies. Ground weapon pools and submarine pools are cleared before class defaults and owner-specific state are installed; ammunition cannot leak between vehicles. Existing stable UID/generation ownership and ground switching semantics remain.

RFSW3 retains the24-byte header and320-byte rows. Submarine rows use profile4 at+60, torpedo reserve at+16 and cooldown at+160; ordinary ground fields keep their existing meaning. RFSW1/2 remain supported. Boot resolves authored profile4 identities before loading resources, and fresh-load ownership uses existing RFVC4 plus RFVA records.

NXDK build, one-direction runtime handoff and occupied Jeep save/fresh-load pass. Returning to the parked submarine and presentation remain unverified; native evidence is recorded below. Fighter switching, grouped/moving transfers and passive autonomous vehicle AI remain separate open work.

## Native integration evidence

Initial fixture startup rejected an inconsistent testbed: navigation nodes were stripped while old waypoint routes remained. Both sections now empty together; no engine reader was relaxed. The shared native harness now persists terminal diagnostics before reporting a missed probe or missing save, so startup errors remain inspectable.

`vehicle-sub-switch-20261003-184517` runs380 frames on stock64MiB. Ordinary submarine Use12 boards, firing40 launches one torpedo (reserve20Ã¢â€ â€™19), and Use160 exits. The later Jeep approach stops at player[100.1353,61.8124,7] beside the submarine at[104,58.7,7], leaving Jeep[110.5,61.71916,7] out of range. No ownership exchange occurred. Final available pages6457; a subsequent unintended on-foot shot prevents the requested save. This is partial vehicle evidence, not a switching/save pass. The movement sequence is being corrected; world collision is unchanged.

`vehicle-sub-switch-20261003-185031` exposed a resource-adoption defect: the extra Jeep pack merged, but the active submarine adoption returned RF_FORMAT because both submarine pack guards used `sub.v3m` instead of the installed `Sub_Mini01.v3m`. The original `entity.tbl` filename and existing submarine resource loader confirm the correct chassis. Both guards are corrected without weakening identity checks. The same run reaches the dock but attempts Use while airborne; the fixture now waits for the observed settled dock position before boarding.

## Working handoff, open save blocker

`vehicle-sub-switch-20261003-185347/runtime-summary.json` verifies the runtime portion of620 frames on stock64MiB: normal sub boarding/fire/exit, one sub7010Ã¢â€ â€™Jeep7009 ownership exchange, ordinary Jeep boarding, gunner selection and one mounted shot. Original handles6946921/4063293 stay with their owners; parked torpedo reserve19 and new Jeep reserve999 are distinct, and Jeep firing reduces it to998. Resource preload/adoption status0, no vehicle runtime error, and available memory6455 pages. No visual/audio claim.

The complete harness remains FAIL: ordinary save returns RF_RANGE (-4) at player-capture stage1, before RFSW3 capture at stage11. No payload or fresh-load result exists. The follow-up190113 run leaves the original late probe untouched, narrowing rejection to player scope or the early live-state guards; it does not establish a submarine codec fault. Added `rf_scene_vehicle_player_capture[8]` records exact admission stage/status and static-placement sphere/reason for the next focused diagnosis. Do not count fresh-load/return support as verified yet.


The190825 guard diagnostic isolates the ordinary save rejection to bit6 (retained climbing region), with scope admission successful and no other live guard set. The dock ladder reference survives normal Jeep boarding because seated updates skip climbing. Vehicle publication must detach this obsolete on-foot state; save admission remains unchanged.


## Ladder detachment and working save/load

`vehicle-sub-switch-20261003-191330` verifies the boarding fix on stock64MiB Xbox. On first successful vehicle publication, a staged climb state follows the existing climb-exit lifecycle, then releases its scene-owned region/contact. Publication leaves actual stance, seat pose, camera and ownership intact. Failed entry does not detach the player, and save guards remain unchanged.

The620-frame source produces a4664-byte ordinary save with player capture status0 and guardmask0; available memory6455 pages. Fresh-load restores occupied Jeep7009 without reboarding, with998 rounds and RNG3357800067; the parked submarine7010 retains19 torpedoes. RFVC/RFSW restore and world rejection diagnostics are successful. The240-frame continuation has6312 available pages and no replayed mounted weapon launch.

The complete return harness remains FAIL: its path stays near the Jeep and the final Use reboards that Jeep rather than approaching the submarine. No second owner exchange occurs. Its prior assertion incorrectly compared the restored parked handle against this unchanged final active handle; that is not evidence of restore corruption. Return wet boarding is still unverified. No further path-tuning batch is needed to establish the ladder/save fix, and no visual/audio correctness is claimed.


The192942 equipment-handoff run also passes ordinary save/fresh-load after a real on-foot pistol shot and interrupted reload (see OCCUPIED-VEHICLE-COMBAT.md). Source620/load380 retain998 Jeep rounds,19 torpedoes and successful occupied restoration; the report explicitly records SAVE_AND_FRESH_LOAD_PASS_RETURN_FAILED. The single revised return attempt walks around the rear but remains on the dock at[108.9090,62.3665,4.69505] when Use340 reboards the Jeep. It has not reached the required rear clearance Z<4.0477 or water. Return-submarine ownership code is therefore still unverified. This bounded check stops here rather than spending another batch on path tuning; movement/swim telemetry is retained for later diagnosis.
