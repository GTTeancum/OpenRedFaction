# Authored NPC seat binding integration

`src/diagnostic/scene_npc_seat_bind.inc` implements the postspawn assignment
identified in [original/data evidence](research/NPC-VEHICLE-SEAT-ASSIGNMENT.md).
It is not yet integrated or compiled by this helper. Parent owns shared wiring
and the single Xbox build/check. Only the helper include and this note were
added in this slice.

## Ownership and supported placements

- Jeep: L12S1 miner7646 binds host7629 at ordered seat0 (`interface_1`). The
  helper reuses `entry.occupant`, `entry.view.occupants`, and `entry.host.driver`.
  It does **not** start the player session or put the NPC in the player token.
  Existing player possession rejects the occupied driver through host.driver.
- Turret: guards936/3023 and miner9882 bind their authored static turret hosts.
  Each binding owns one actual occupant word, exposed by host.view.occupants
  and occupant_count1. The existing operator helper owns AI action13 and host
  control; this file owns the physical links/pose.
- Actor.view.linked_handle identifies its parent. Host.view.linked_handle is
  left intact: it is not an occupant back-pointer. Reciprocal ownership is the
  host's occupant slot, checked through both full-generation registrations.
- Unknown host references are counted, preserved in parsed spawn data and
  skipped. No nearest-vehicle fallback. Limit128 bindings costs a fixed4.5KiB
  array; no per-frame allocation. The installed corpus needs at most one
  skeletal assignment per level.

The current Jeep adapter has one published driver slot. Player-in-gunner-seat
coexistence with an NPC driver remains separate; do not advertise full Jeep
passenger support from this change. Auto Turret generated child heads are also
separate. No hypothetical seatowner factory or dynamic boarding is introduced.

## Exact shared hooks

1. Include `scene_turret_operator.inc` after `scene_turret_combat.inc`, then
   include `scene_npc_seat_bind.inc` **after `scene_driller_runtime.inc`**.
   The operator file forward-declares `scene_npc_seat_pair`; earlier users of
   attached/related/unbind need ordinary static forward declarations.
2. Reset operator state at scene startup. Call
   `scene_npc_seats_open(stream, now)` after every authored NPC, turret and
   current vehicle is registered and their model/eye/room resources are ready,
   before ordinary AI/physics work. Initial now0 is valid. It reads the named
   `campaign_seeds.items[i].spawn.seat_host_uid` and resolves actual host UIDs.
   Avoid executing authored behavior before initial occupancy becomes visible.
3. Suppress **free-body** NPC integration, grounded mover carry, idle gravity,
   navigation and independent handheld aiming/fire when
   `scene_npc_seat_attached(owner->registration.handle)` is true. Preserve pose
   animation, health/pain/death and scripted affiliation changes. The operator
   helper supplies the finer turret firing gate; Jeep drivers must also be
   excluded from normal handheld combat. Do not simply skip all NPC updates.
4. Call `scene_npc_seats_tick(frame,now)` once after host vehicle motion and
   turret aim update, before actor collision/render/eye consumers. It places
   the NPC at the transformed model tag, preserves basis when object flag100
   locks it, copies physics/published/model pose, refreshes eye and room, zeros
   free velocity, and removes stale ground/fragment support. A per-binding frame
   stamp prevents applying the same update twice. Do not call it before host
   motion and then again with the same frame expecting a new pose.
5. Exclude a parent/occupant pair from mutual solid/contact/ray selection with
   `scene_npc_seat_related(first_handle,second_handle)`. The operator helper
   independently excludes its own NPC from turret targeting. Unrelated actors
   retain their ordinary collision/damage admission.
6. Call `scene_npc_seat_unbind(actor_or_host_handle,now)` before either endpoint
   unregisters or transfers ownership. It tells the turret control adapter to
   release first, then clears only matching slots/links. Tick additionally
   detaches dead or vanished endpoints. A living actor in action13 returns to
   waiting2 on release (explicit first-pass policy); dead actors keep death AI.
7. Call `scene_npc_seats_close(now)` before NPC/vehicle/turret teardown, then
   reset operator control rows. This removes borrowed occupant pointers before
   owners can be freed. A host destruction release leaves the actor at its
   existing seat pose; it does not claim a collision-safe living ejection path.

## Operator and existing-vehicle joins

The physical helper calls the agreed
`scene_turret_operator_bind(host,actor,tag,now,0)` only after publishing a valid
pair, and `scene_turret_operator_unbind(host,actor,now)` before removal. The
operator implementation never owns duplicate physical seat state.
`scene_npc_seat_pair(host,actor,tag,&attached)` returns RF_OK with0/1 for
validated matching ownership; stale endpoints return an error.

The Jeep branch uses the existing core `rf_entity_ai_set_action` and sets both
actor.ai_mode.action_280 and view.action_520 to13. It stops its standalone
pursuit/fire/route state; host autonomous route motion remains owned by the
parent vehicle adapter. It does not convert occupant affiliation into host
affiliation or invent new driver input. Existing Set_AI_Mode propagation needs
to account for the newly visible occupant without rejecting all linked actors.

There is no generic exported core427240 seat mutation API in the current tree;
this helper composes existing registration lookups, `rf_static_model_tags_find`,
`rf_static_model_tag_place`, `rf_physics_publish_position`, NPC eye/room refresh
and the real mutable host slots. Original attachment evidence supplies the
ownership/pose contract rather than an unimplemented API call.

## Save and validation boundary

Ordinary save ownership for seated NPCs is **not closed by this include**.
RFNC currently rejects linked actors and RFTU rejects occupants. Keep those
guards until stable host UID/occupant UID/tag state is staged and published
together. A fresh world load must not accidentally bind authored seats and
then restore unrelated actor poses without restoring this relationship.
Whole-world restore needs an explicit seat stage or a tested post-restore
rebind policy; the operator helper offers restoring1 for that later join.

Telemetry `rf_scene_npc_seats[10]` is: requested, bound, unresolved, rejected,
pose updates, detached, active, last actor UID, last host UID, last error.
Meaningful bounded validation should establish one authored Jeep attachment
and one stationary-turret operator, transformed pose following, handheld
suppression, and reciprocal cleanup; rendering counters alone are insufficient.
No tests, builds, XEMU, PC runtime, images or desktop input were run here.

## Integration status

Runtime and RFNS save hooks are integrated. See NPC-TURRET-SEAT-CHECK.md and
NPC-TURRET-SEAT-SAVE-CHECK.md for bounded Xbox evidence. The earlier handoff
save warning above is superseded by that explicit staged implementation. Jeep
runtime and detached-seat save paths remain unverified. Authored empty turret
seats now inhibit autonomous fire; lethal detach clears stale seat action13
without calling the living waiting transition. Driver routes pause when their
authored driver is absent/catatonic or the player owns the driving session.
