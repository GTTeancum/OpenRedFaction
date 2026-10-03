# Submarine and ground vehicle ownership

L10S4 contains submarine7010 and Jeep7009. Previously selecting the submarine at startup left the Jeep permanently passive: the ownership handoff accepted only ground profiles1..3. Profile4 now participates in the same preloaded, quiescent ownership exchange. The focused native handoff result and remaining save blocker are recorded below.

The submarine profile pack owns cockpit and torpedo resources while borrowing its chassis. Startup merges all textures before renderer submission and retains existing4MiB auxiliary budget and16MiB admission reserve. On promotion, chassis ownership and material bases transfer once, runtime collision/liquid pointers are rebound, and authored submerged movement parameters replace ground suspension control. Returning to ground clears water-only entry callbacks. Target submarine boarding must pass the existing complete player-sphere wet path and normal world/mover/actor clearance before registry mutation. The whole target hull must also fit the static world and remain fully wet, preventing promotion into an immobile partly dry submarine.

Each parked submarine retains its torpedo reserve/cooldown. A handoff rejects live torpedoes, positive cooldown and existing motion/attachment/route dependencies. Ground weapon pools and submarine pools are cleared before class defaults and owner-specific state are installed; ammunition cannot leak between vehicles. Existing stable UID/generation ownership and ground switching semantics remain.

RFSW3 retains the24-byte header and320-byte rows. Submarine rows use profile4 at+60, torpedo reserve at+16 and cooldown at+160; ordinary ground fields keep their existing meaning. RFSW1/2 remain supported. Boot resolves authored profile4 identities before loading resources, and fresh-load ownership uses existing RFVC4 plus RFVA records.

NXDK build and one-direction runtime handoff pass. Fresh-load continuation and presentation remain unverified; the ordinary save blocker is recorded below. Fighter switching, grouped/moving transfers and passive autonomous vehicle AI remain separate open work.

## Native integration evidence

Initial fixture startup rejected an inconsistent testbed: navigation nodes were stripped while old waypoint routes remained. Both sections now empty together; no engine reader was relaxed. The shared native harness now persists terminal diagnostics before reporting a missed probe or missing save, so startup errors remain inspectable.

`vehicle-sub-switch-20261003-184517` runs380 frames on stock64MiB. Ordinary submarine Use12 boards, firing40 launches one torpedo (reserve20→19), and Use160 exits. The later Jeep approach stops at player[100.1353,61.8124,7] beside the submarine at[104,58.7,7], leaving Jeep[110.5,61.71916,7] out of range. No ownership exchange occurred. Final available pages6457; a subsequent unintended on-foot shot prevents the requested save. This is partial vehicle evidence, not a switching/save pass. The movement sequence is being corrected; world collision is unchanged.

`vehicle-sub-switch-20261003-185031` exposed a resource-adoption defect: the extra Jeep pack merged, but the active submarine adoption returned RF_FORMAT because both submarine pack guards used `sub.v3m` instead of the installed `Sub_Mini01.v3m`. The original `entity.tbl` filename and existing submarine resource loader confirm the correct chassis. Both guards are corrected without weakening identity checks. The same run reaches the dock but attempts Use while airborne; the fixture now waits for the observed settled dock position before boarding.

## Working handoff, open save blocker

`vehicle-sub-switch-20261003-185347/runtime-summary.json` verifies the runtime portion of620 frames on stock64MiB: normal sub boarding/fire/exit, one sub7010→Jeep7009 ownership exchange, ordinary Jeep boarding, gunner selection and one mounted shot. Original handles6946921/4063293 stay with their owners; parked torpedo reserve19 and new Jeep reserve999 are distinct, and Jeep firing reduces it to998. Resource preload/adoption status0, no vehicle runtime error, and available memory6455 pages. No visual/audio claim.

The complete harness remains FAIL: ordinary save returns RF_NOT_FOUND at player-capture stage1, before RFSW3 capture at stage11. No payload or fresh-load result exists. Source review narrows this to live Jeep checkpoint admission or seated-player placement; it does not establish a submarine codec fault. Added `rf_scene_vehicle_player_capture[8]` records exact admission stage/status and static-placement sphere/reason for the next focused diagnosis. Do not count fresh-load/return support as verified yet.
