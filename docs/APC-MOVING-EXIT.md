# Moving APC exit timing

2026-10-03: integrated in `scene_driller_runtime.inc`; NXDK build and bounded stock-64-MiB Xbox check passed. This changes exit scheduling while preserving the existing authored seat, Use radius/locks and safe-exit geometry.

Before this change, the vehicle tick unpossessed before `rf_vehicle_rigid_step`, validated full-body exit/world/floor/hull clearance at that old chassis pose, then published the player after the vehicle moved. Its hull-exclusion result therefore did not cover the final accepted chassis transform. This is a source-proven ordering gap; no particular observed penetration is claimed. The current rigid resolver uses world/movers, so this report does not claim it directly collides with the newly unlinked player.

`scene_apc_exit_transition.inc` defers control release only for an APC occupied at tick start. It retains ownership through the rigid step, then invokes the same damage-ejection and Use services against the accepted final pose immediately before player publication. Entry remains before physics. Existing blocked exits retain occupancy, living wreck exits require full safe placement, player death uses existing release policy, and Use locks stay enforced. The port's existing zero player velocity on exit is unchanged.

## Integrated hooks

1. In `scene_driller_runtime.inc`, include `scene_apc_exit_transition.inc` after `scene_driller_damage_runtime.inc`. Its types only require entry/player/damage adapters, not the completed runtime struct.
2. Add local `scene_apc_exit_sample apc_exit;` in `scene_driller_runtime_tick`. Immediately after computing `was` and `use_edge`, call `scene_apc_exit_begin(&apc_exit,&r->entry,frame)`.
3. Run the current early damage tick and Use dispatch block only inside `if(!apc_exit.deferred){...}`. This includes the prepared Jeep gunner Use adapter if already integrated. Other vehicles and unoccupied APC boarding keep their existing path; do not call either control service twice for a deferred APC.
4. Immediately after the accepted physical step and before `scene_driller_player_publish`, resolve the deferred sample:

   ```c
   if(apc_exit.deferred){
       status=scene_apc_exit_finish(&apc_exit,&r->damage,player_input.use,
           campaign_player_damage.state.effects.health>0,&changed);
       if(status)goto fail;
   }
   ```

   Leave drive command selection and rigid physics between begin and finish. An exit-request sample can still drive the occupied APC during that sample; ownership releases at its accepted end. Damage receive already sets `entry.host.alive=0` when destroyed, so propulsion/fire remains disabled before deferred wreck release. The deferred APC command also checks living player health, preventing a dead player's input from driving during its last occupied sample.
5. For a deferred APC, publish the existing `campaign_triggers.vehicle_pulses` and changed entry/exit counters after the deferred finish, before common player publication. Other vehicles and initial APC entry retain their original early counter/pulse timing inside the nondeferred branch. Both paths preserve the same expressions: entered pulse when changed/active, attempted exit pulse when `was && use_edge && alive...`, and exactly one counter increment for changed. Saved `was/use_edge` precede input consumption. Jeep seat switching stays in its original location.
6. Near the end of the vehicle tick, after common player publication, call `scene_apc_exit_observe(&r->entry,frame)`. Reset both `rf_scene_apc_exit_transition` and `rf_scene_apc_exit_continuation` in runtime open alongside existing vehicle counters. These arrays observe real state only, with no fixture-specific gameplay mutations. Exact48/11-word layouts are beside their definitions.

There is no new persistent owner, allocation, wire format or timing hook in `scene.c`. No ordinary save/load change is needed for this scheduling fix. All code for entry/exit candidate geometry, collision and body publication remains shared.

## Bounded Xbox harness

Parent command: `python tools/xemu_apc_moving_exit.py`. `--prepare-only artifacts/fixtures/apc-moving-exit` only generates ignored fixture inputs. The harness retains the complete installed L1S3 APC9627 record except its transform, uses empty CTF06 geometry under the existing L1S3 selector, and sets a known player-start orientation without changing its position. No NPC passenger is synthesized.

Process-local replay boards30, drives45..140, turns100..140, exits140 and walks150..174; stop180. A pass requires an actually moving exit frame, one completed ownership release, final-pose full-body hull exclusion, no transient error, and subsequent alive on-foot movement. Merely reaching the frame count is insufficient. Native clearance rejection or lack of motion remains a failed fixture, not inferred success. No screenshots, host input, campaign traversal, save/load or destroyed-host exit coverage is claimed.

## Installed seat scope

A fresh independent scan of all68 installed campaign RFLs/1,610 entities found the same seven explicit seat-host references: one Jeep driver, three supported static turret operators and three unresolved references. No APC/NPC pairing exists. APC instances are L1S3 UIDs26/9627, L15S1 UID3303, L15S2 UID9680 and L15S4 UID9707. Installed `APC.v3m` has one `interface_1` tag (index0, model offset698, local position[-.863999009,.818611741,2.511374712]); it does not provide a second gunner/passenger tag.

L7S3 env_guard3959's unresolved3958 reference appears nowhere in the other runtime object sections; raw matches exist in static/editor brush geometry, which does not establish an entity host for the original resolver. The implementation therefore targets the real moving-player exit gap instead of inventing an APC passenger.

## Xbox result

`artifacts/xemu/apc-moving-exit-20261003-123049/report.json`:180 frames; ordinary Use exit at140 while moving1.19782m/s, with a0.0209431m chassis step that frame. The final-pose full-body hull exclusion passed, all driver/occupant/player links were released, and the living player subsequently moved2.227m on foot. No route, weapon launch or destruction fixture ran. Free physical pages: 2946. Wreck/death exits, crowded exits, saved continuation and visual/audio output remain unverified; no images were captured.
