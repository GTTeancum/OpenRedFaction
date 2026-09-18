# Gameplay integration, 2026-09-18

Integrated enemy shotgun shells through the existing damage and cover paths, with one shell consumed by the caller. Focused actual-scene tests establish multiple pellet damage, blocking cover and death cutoff. Existing authored cadence/reload checks pass.

Teleport_Player type63 now dispatches immediate/delayed events into a queued frame-boundary scene relocation. Position, orientation, room and controller publication update together; support/contact state is invalidated while velocity and inventory remain. Last destination wins within a frame. Attached-vehicle teleport is unavailable. Focused dispatch and actual-scene pose tests pass.

Grenade gravity/bounce/rest and timed/impact lifecycle motion are in the shared PC/Xbox source lists. Focused tests pass; this does not yet make grenade throws playable. Selection, animation release and explosion integration remain.

Validation: PC scene builds; scene_ai_gameplay, scene_event_gameplay, grenade_flight and scene_ai_shotgun pass. NXDK produces default.xbe and ISO. No emulator run or visual acceptance performed for this batch. Logs: artifacts/ai-gameplay/integration-*.log.

Overall estimate remains approximately60%; GeoMod100% first playable acceptance. Current systems: weapons, AI and scripted interactions.

## Switch movers and throw controller

Switch controller-family lookup/dispatch now reaches registered door/platform controllers. Activation reuses existing link activation; deactivation uses existing motion stop, preserving the rotational ramp alias. Actual helper tests cover start/stop/restart, rotating stop policies and stale identities. Shared scene PC and NXDK builds pass. No live authored switch/emulator acceptance yet.

Grenade throw controller is in both builds with focused checks passing:102/96 tick primary/alternate release, cancellation on selection loss, one release request and successful-spawn acknowledgement debiting one reserve grenade. Three-second cooldown begins after release as an explicit first-pass policy.

Rocket radial damage and terrain editing were extracted into parameterized scene helpers for grenade reuse; rocket call order/values remain the same. Compilation passes; live projectile regression will be checked with grenade integration. Logs: artifacts/ai-gameplay/switch-*.log.

## Playable grenade integration

DEV slot5 now loads Grenade first-person resources and its existing world weapon model, displays/debits reserve ammunition, uses primary/alternate throw clips, simulates eight bounded projectiles, and invokes radial damage plus terrain editing. The static world model currently keeps an identity orientation; spin, liquids and lost resting support remain refinement. Explosion presentation reuses rocket effects for now.

Live PC ordinary input: artifacts/grenade-live/throw.bin selects five times, starts primary at70, releases at172 (ammo8->7), detonates at472. Landing location rejects a terrain edit as ineligible. impact.bin starts alternate70, releases166, contacts/detonates179 and produces GEOMOD success1 with zero error. Captured endpoints were inspected: grenade view/ammo visible, impact smoke/debris visible; smoke obscures the final cut surface. This is not broad visual-parity acceptance. No Xbox runtime check yet; NXDK build succeeds (xbox-build.log).

AI firearm hearing and Switch NPC visibility also integrated; focused scene_ai_hearing and scene_switch_objects pass. No visual/live encounter claim for these two helpers.

Overall rough implementation61%; current weapons work (grenade first pass) approximately75%. Remaining grenade work includes native memory/runtime validation, world-flight visual confirmation, water/support handling and separate effects. Report current-system percentage instead of GeoMod going forward.

## Native grenade acceptance and NPC reload

artifacts/xemu/render-20260918-080706 completes240 frames on stock64MiB,78 checks PASS. GRENADES exactly matches PC [1,1,1,1,0,0,0,0]: start/release/contact/detonation, no live projectile or failures at end. GeoMod has one successful edit and zero error; budget12,138,664 bytes <=13,631,488. Endpoint3704 free pages (14.47MiB), not a peak-memory measurement. Native framebuffer inspected: grenade first-person view, smoke/debris and ammo7 match expected endpoint. Smoke obscures cut shape; no whole-flight visual claim. Harness exited its owned emulator and restored the disc.

Added native grenade diagnostics comparison to the retained harness. Authored NPC reload action39 now starts on finite-ammo reload begin, using actor weapon-specific mapping/sound; missing clips remain optional. Focused scene_ai_reload passes and both platforms build; this reload change postdates the native grenade run and has no new live acceptance.

Grenade first-pass estimate85%; overall remains61%. Prioritize missing weapons/systems over remaining grenade polish.
