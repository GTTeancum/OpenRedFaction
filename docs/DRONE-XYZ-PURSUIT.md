# Drone robot-fly pursuit: bounded XYZ first pass

Status: source-integrated after independent review against `b12a062a`; no
source blocker was found. No build, syntax check, test,
emulator session, PC run, new gameplay fixture or campaign route was run for
this slice. Compilation and all movement/runtime claims await the parent's
scheduled Xbox batch.

## Actual owner and original evidence

Original `levels1.vpp/L3S4.rfl` UID 844 is the exact `Drone` class, already
supported by the separate Drone Smash and Drone Missile owners. The original
`tables.vpp/entity.tbl:4086–4138` class authors `robot fly`, `fly` and `sentient`,
maximum speed 6, acceleration 4, mass 1000 and two body spheres of radii 1.3
and 1.4. The movement parser's original sixteen-name table resolves
`robot fly` to descriptor index 11. These are source evidence, not new movement
constants or changes to an original asset.

Read-only original `RF.exe` evidence:

- `42a060` admits current descriptor 11 as free flight. The separate
  `42a0a0` swimming predicate admits descriptors 4/7; neither is a reason to
  admit arbitrary actors to this bounded exact-class adapter.
- `40ba04` supplies the actor eye at `actor + 0x7d4` to the route follower;
  `40bb2c..40bb44` subtracts it from the route point and passes the complete
  XYZ vector to `408e50`.
- `408e54..408e7b` normalizes, scales and accumulates all XYZ components into
  the movement command at AI `+0x500`, consumed as actor `+0x7a0` by `40a350`.
- `40bbc7` samples the eye for arrival. The free-flight/swimming calls at
  `40bbdc`/`40bbea` bypass the nonfree Y replacement at `40bbf6`, preserving
  full eye-to-route XYZ distance at `40bc17..40bc1d` for robot-fly 11.
- Steering is a separate decision: `40b8af..40b910` flattens movement-aim Y
  only for descriptors 9/7 or class flag `0x400000`. Original Drone's flags
  are `fly | sentient` (`0x12`), so robot-fly 11 retains full-target steering.
  Reusing the Sea Creature movement-steering flatten would be incorrect.

The patch keys identity to exact `Drone` plus authored and current descriptor
11. It does not depend on a particular held weapon: the original class also
permits another primary, and movement belongs to the actor, not its current
Smash/Missile selection. No UID-specific grant or L3S4-only runtime branch is
introduced.

## Existing body-space port policy

Before this change, ordinary NPC pursuit discarded target Y for translation,
target refresh and arrival, except for exact Sea Creature sub7. The new Drone
admission reuses that bounded XYZ policy while preserving the existing Drone
full-target steering path.

This is explicitly the port's existing body-space reconstruction. Pursuit
targets are already player/NPC body positions or the existing vehicle aim
point, route selection starts at the current body, and retained route points
are already consumed as body destinations. This adapter consistently measures
translation and waypoint/final arrival from current body XYZ and refreshes
stored targets by full XYZ change. It adds no guessed source-eye/target-feet
offset and does not reconstruct the original final pursued-target producer.

The original-backed principle is vertical pursuit retention. Body-space
coordinates, direct kinematic stepping, the existing greater-than-1 squared
target-refresh threshold and 0.25-unit arrival radius remain explicit
first-playable port policy. No exact retail eye-space trajectory, acceleration
integration or movement timing is claimed.

## Admission, collision and control ownership

1. `scene_drone_pursuit_owner` checks the retained campaign record and class,
   exact class name, authored descriptor 11 and current descriptor 11. Target
   refresh does not require follow2 because it also begins a new pursuit.
2. Only `script_move.follow == 2` enables XYZ movement. Existing ordinary
   player awareness, explicit Attack and acquired opposed-actor targets keep
   their existing producer and priority. The adapter selects no new target.
   Goto, Goto_Player/follow1, Follow_Waypoints, Look, standalone combat aim,
   non-Drone fly modes and altered current modes retain their existing paths.
3. The existing live-owner predicate is renamed `scene_xyz_pursuit_live` with
   an unchanged body. Both exact XYZ consumers require full registry/entity
   identity, a live actor view, positive health, no hidden/frozen object bits,
   no terminal entity bit and no linked owner. Existing seat and suspended
   scripted-physics gates remain first. A refused exact XYZ owner stays still;
   it cannot fall through to horizontal pursuit. No pain timer is reinterpreted.
4. Waypoint and final arrival include vertical separation. A waypoint directly
   above or below the actor cannot advance based only on matching XZ.
5. The proposed step remains `min(resolved movement.speed * elapsed, distance)`.
   Existing authored/slow/alternate speed resolution owns the speed; no new
   6-unit constant replaces it. The unchanged full-body `0x460` sweep owns
   acceptance. Routes, radius/height clearance, collision spheres, retry
   cadence and route acquisition remain unchanged.
6. XYZ pursuit skips the existing ground uphill projection and horizontal
   wall-slide retry. Any blocking full-body contact follows the ordinary
   blocked path. There is no partial-contact advance, shrunken sphere,
   clearance waiver, endpoint teleport or new flight slide solver.
7. Only the Sea Creature `sub_pursuit` branch creates the local flat steering
   target. Drone keeps the original `target` passed to `rf_scene_npc_steer`.
   Existing angular preparation, ordinary commit and publication retain
   ownership. Ground and Sea Creature behavior remain unchanged.

The existing ground and idle-ground functions already admit only descriptors
1/3, so robot-fly 11 does not receive gravity or floor snapping there. No new
hover, thrust, drag, banking, general free-flight physics or Make_Fly support
is added. Separate delayed Smash, secondary cooldown, missile flight and
audio owners are unchanged.

## Persistence and remaining limits

The change owns no new persistent data, timer, resource, allocation, save row
or wire format. Existing position, movement descriptor, resolved speed,
pursuit target and retained-route ownership stay in place. Capture/restore,
body-pitch/roll and velocity/command restrictions, settled-state requirements
and placement clearance are not changed or relaxed. This is not a claim that
pitched or otherwise unsupported fly11 saves now work.

Natural vertical pursuit, pitched movement, full-body obstacle response,
retained-route arrival, control interruption, action interaction and all
save/load behavior remain runtime-unverified. Exact original eye-route
semantics and general flight physics remain deferred. Parent alone owns
integration, builds and the scheduled stock-64-MiB Xbox validation batch;
this slice adds no runtime recipe.
