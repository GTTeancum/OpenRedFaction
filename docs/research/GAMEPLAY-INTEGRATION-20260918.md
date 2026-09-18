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
