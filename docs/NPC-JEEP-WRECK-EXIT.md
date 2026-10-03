# NPC Jeep wreck exit

Integrated first-pass implementation. Parent owns serial Xbox validation; native results are recorded below when available.

The installed L12S1 miner7646/Jeep7629 is the sole resolved authored moving NPC
driver pair. Existing seat teardown released a living driver at the interior
seat position when the Jeep died. The implementation finds a safe, grounded exit
before releasing ownership. It does not change player exits, corpse detachment,
teleportation, Remove_Object or normal level teardown.

## Integration

`scene_npc_seats_tick` calls the wreck-exit helper after the final accepted Jeep pose and before generic dead-host release. A blocked attempt retains the binding and refreshes its seat pose for the next tick. Generic object removal, actor death, teleport and teardown keep their ordinary immediate unbind path. Scene startup resets telemetry. The helper is included after `scene_driller_checkpoint_adapter.inc` so it can derive world angular velocity from body-local inverse inertia and actual angular momentum rather than treating momentum as angular velocity.

`scene_vehicle_script_slay.inc` adds active/passive vehicles to the existing Slay_Object callback. Generation-valid owners receive the same forced direct lethal damage policy as NPC Slay through normal damage/destruction services. This preserves wreck registration and ejection scheduling without inventing radius damage to occupants. Repeated completed destruction is idempotent. Active runtime feedback quantizes event milliseconds to the existing frame API; passive feedback uses exact event milliseconds. Passive vehicles are integrated but not covered by the Jeep fixture.

## Placement and ownership

Eight directions use current chassis sphere support bounds and the actual NPC
sphere shape: the established four cardinal exits plus diagonals at the same
shape-derived clearance. The existing .15m lateral margin, eye-height standing
proposal, radius-times-two-plus-four floor search and .01m floor separation are
retained port policies, not new emergency teleport distances.

Transit uses the existing real seated-head sphere convention; destination and
descent use the complete NPC shape. Queries compose world/movers, other vehicles,
player/NPC bodies, props and detached pieces. Final fit checks static geometry,
movers, props, pieces and all actor/host sphere unions. The host is excluded only
from interior transit, never the final pose. Floor must be world or mover; an
actor or loose fragment is not accepted as an ejection platform in this slice.

On success, the ordinary seat release clears matching ownership and converts
action13 to Waiting2. The helper publishes upright body/model/eye pose, resets
stale support/contact caches and assigns `host.velocity + host.angular_velocity
cross (seat.position - host.position)` once to body velocity. Additive support
velocity is zero, preventing a second inheritance.

No candidate returns `RF_OK` with `released=0`: the original binding remains,
seat pose updates and next frame retries. Propulsion/firing already obey the
dead-host gate. This deliberately does not promise escape from a fully enclosed
wreck. Invalid geometry/ownership returns an explicit error, not a fallback
placement. Wider wreck/death effects remain outside this slice.

## Native evidence contract

`rf_scene_vehicle_wreck_exit[12]`: attempts, exits, blocked, errors, last frame,
actor handle, host handle, chosen direction, status, stage, actor UID, host UID.
Stages: ownership1, transit2, floor3, final fit4, publication5.

First-success `rf_scene_vehicle_wreck_exit_apply[32]`: frame0, actor1, host2,
parent3, driver4, occupant5, AI6, health7, seat XYZ8..10, exit XYZ11..13,
inherited velocity14..16, host linear17..19, host angular20..22, host origin23..25,
direction26, active seat27;28..31 reserved. Floats are bitcasts. Existing
`rf_scene_npc_jeep_detached_probe` supplies later detached position/health facts.
Stationary destruction proves safe release; nonzero moving inheritance needs
separate evidence and must not be claimed from a stationary fixture.

## Stock64MiB Xbox result (2026-10-03)

`artifacts/xemu/npc-jeep-wreck-exit-20261003-154739/report.json`: PASS,180 frames, 2866 free physical pages. The original L12S1 Jeep7629/miner7646 pair is staged grounded in empty CTF06. Ordinary Set_AI_Mode parks the host; a delayed Slay_Object request at60 destroys it at120 through one real forced damage request. The pre-destruction probe confirms a live seated miner and health400 Jeep. The live miner exits at frame120, retaining health1, clearing parent/driver/occupant/active-seat ownership and switching to Waiting2. Exit position [2.2413883209228516, -0.40847861766815186, 2.491253137588501] differs materially from the interior seat. Final position [2.2413883209228516, -0.40847861766815186, 2.491253137588501] confirms subsequent ordinary physics did not snap the actor back. Exactly one exit occurred, with 0 blocked attempts.

The immutable release probe matches independently calculated host linear velocity plus angular velocity cross seat offset. This stationary fixture does not establish behavior at nonzero driving speed. No player possession, NPC death, blast, weapon fire, images, host input or campaign walkthrough was used. The harness restored ordinary disc inputs and rebuilt successfully. Passive-vehicle Slay, crowded exit retry, moving-host inheritance, player wreck exits, save continuation and audiovisual output remain unverified.
