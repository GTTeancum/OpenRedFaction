# Ordinary NPC lava and acid exposure

Status: source-written on 2026-10-09 for the parent 21:00 UTC batch. No
compilation, test, fixture, emulator run or authored encounter was performed
for this slice. Parent owns `scene.c` wiring and Xbox validation; its include, ordinary-step
hook and lifecycle reset were source-reviewed in place. This is a
bounded ordinary-NPC consumer, not completed swimming/drowning reconstruction.

## Missing live behavior and original evidence

`rf_liquid_damage_prepare` already reconstructs the selection and request
arithmetic of original `421240`, but the existing scene caller
`campaign_liquid_damage_tick` serves only the player. The general entity-update
routine `41e4b0` calls `421240` at `41e549` without a player-only condition.
The update-prefix evidence is also recorded in `docs/NPC-MARKERS.md`, under
“Entity-update scheduling boundary”; `tools/verify_footstep_schedule.py`
retains the corresponding pre-existing original-execution contract. That
script was read, not run, for this change.

The original executable source is the read-only `Installed_Game/RF.exe`,
previously recorded SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The following original contracts were reconfirmed by read-only disassembly
in the parent investigation:

- `4290d0` resolves actor `+200` through the type-0 entity lookup `426fc0`,
  then tests the linked actor class/use kind from `486c90` against 1.
  It is not a generic “any link means immune” condition. A stale generation
  or a different linked entity kind is not this rejection.
- `421240` requires either actor `+810` wet bit `0x1000` or `0x2000` and a
  room returned by `40a490`. The latter consumes retained actor word 0,
  whose room ownership and position cache are documented in
  `docs/DEATH-LIFECYCLE.md`, “NPC retained room identity and linked actor facts.”
- Original room liquid type 2 (lava) requests damage kind 4 only when actor
  `+1fc` equals 3. Liquid type 3 (acid) requests kind 7 without that material
  restriction. This is the actor's retained collision material, not the
  ground-contact/support material at `+1380`.
- The amount is the authored lava/acid rate times this frame's seconds.
  Source and auxiliary UID are `UINT32_MAX`, hit region is -1, argument 6
  and force are zero. The existing shared helper retains these arguments,
  including eligible zero-delta dispatch. There is no hazard deadline or
  oxygen accumulation in this function.
- Original `429100` establishes body liquid membership before the deeper
  eye-wet distinction; `4ce080` requires liquid presence and compares actor
  position Y with the room's authored minimum Y plus liquid depth. In
  `4ce08a..4ce097`, FLD depth / FADD minimum Y / FCOMP position Y has no
  binary32 sum store. The adapter compares a double sum, rounding only the
  diagnostic surface value; it does not borrow the player's float-rounded
  surface comparison.

The source declarations and implementations are `include/rf/liquid_damage.h`
and `src/core/liquid_damage.c`. Rates remain loaded by the existing
`rf_game_liquid_damage_load` path; this change adds no constants or table edits.

## Scene consumer

`src/diagnostic/scene_npc_liquid_damage.inc` owns one allocation-free loop over
the existing ordinary NPC body array. It excludes absent bodies/registrations,
removed owners, the separate player, the original object-flag `0x4000`
exclusion, and dead owners. Every candidate must agree across the registration,
full generation-qualified handle, entity view, damage owner and object registry.
Invulnerability, armor, class damage factors and ordinary damage side effects
remain the shared damage service's responsibility.

AI mode, hostile status, selected weapon, scripted animation freezing and
physics suspension are not admission conditions. Living stationary or
catatonic actors therefore still receive environmental exposure once per
ordinary simulation step. Only the actual linked-kind-1 predicate rejects a
linked actor; no new blanket seating or attachment exemption is invented.

Membership uses the target NPC's own current `body.state.position`:

1. Reuse its existing one-based `owner->room.room` only if it is in range for
   both retained tables and the room query position exactly matches the body.
2. Otherwise call `rf_geometry_collision_world_locate` for that body. A
   missing room means no exposure. A previous cached room is not used merely
   because the last room refresh failed to locate the current position.
3. Read the shared immutable `rf_liquid_room` snapshot. Dry rows have
   nonfinite depth. The surface uses original retained room minimum Y plus
   depth, never CSG-expanded collision bounds.
4. Derive only a call-local `0x1000` wet input when the body origin is at or
   below that surface. Existing NPC wet bits currently lack a general live
   producer; passing them unchanged would leave this consumer inactive.
   No NPC wet flags, eye state, room cache, model-room owner, movement modes,
   buoyancy, support state or physics coefficients are modified here.
5. Feed `rf_liquid_damage_prepare`; submit emitted requests to
   `rf_scene_npc_damage` with the existing combat effect backend and clock.
   On a newly lethal result, use `rf_scene_npc_death_entry`, clear the existing
   scripted movement activation, and call `combat_death_start` exactly as
   other ordinary NPC lethal-damage adapters do. This retains shared pain,
   death motion, burning closure, corpse, weapon-drop and marked-death event
   ownership. There is no bespoke health subtraction or synthetic event.

The ordering is an explicit first-playable adaptation: the original prefix
consumes previously owned room/wet state, whereas this adapter samples the
current body after ordinary NPC movement/playback and room refresh. Exact
cached-state phase parity is not claimed. The existing player exposure path
is unchanged. The adapter does not use the player's body, camera or room.

No persistent state is added beyond diagnostic counters. Existing vitals,
flags, pain, burning and death owners retain damage consequences through their
existing save/restore paths; there is no new hazard record or checkpoint format.
Existing checkpoint restrictions still apply: `scene_npc_physics_checkpoint.inc`
rejects dead/health-zero actors while `scripted_physics` remains set. A frozen
living NPC can correctly die from this exposure and reach that pre-existing
unsupported frozen-death save state. This change does not bypass that guard or
alter damage/death merely to avoid it; complete frozen-death save coverage is
not claimed.

## Parent integration

Include `scene_npc_liquid_damage.inc` immediately after the existing
`campaign_liquid_damage_tick` definition and before
`scene_turret_generated_death.inc`. At that point the NPC owner type, shared
rates, damage service, combat effect callbacks and `combat_death_start` have
all been defined. No forward declarations are needed with this placement.

In the existing `campaign_spawn` ordinary NPC simulation block, immediately
after successful `campaign_npc_rooms_pass(frame)` and before its phase-2
profile mark, call:

```c
status=scene_npc_liquid_damage_tick(stream,frame,particle_now);
if(status)return status;
```

Do not call from rendering, per-contact physics substeps or weapon/AI attack
paths. The once-per-step outer scheduler already excludes modal steps.
`scene_npc_liquid_damage_reset` is invoked on frame 0; it clears only telemetry.
A parent lifecycle reset can also call it without affecting gameplay.

Optional existing parent diagnostic emission can expose:

```c
extern uint32_t rf_scene_npc_liquid_damage[24];
```

The 24-word record is 96 retained bytes:

- 0: ordinary passes
- 1: qualified living body candidates
- 2: exact own-body room-cache uses
- 3: fallback shared room queries
- 4: body-origin wet memberships, including ordinary water
- 5: linked-kind-1 rejections
- 6/7: emitted lava/acid requests
- 8: positive shared-service damage results
- 9: newly entered shared death transitions
- 10: stale/mismatched owners skipped
- 11: returned errors
- 12/13: last emitted request's authored UID and full actor handle
- 14/15/16: its zero-based room, original liquid type and actor material
- 17/18: requested and applied amount, float bits
- 19/20: health before and after shared damage, float bits
- 21: request frame
- 22: liquid surface height, float bits
- 23: most recent tick status

Words 12 through 22 retain the last emitted request across dry steps; they
are not the last inspected actor. Error counters/status may describe a later
candidate than that retained request. Counters observe the normal path and
perform no injections or grants. No serial-output owner is added in this file.

## Remaining verification

Compilation and runtime behavior remain unverified until the parent batch.
A dry ordinary level can establish noninterference/counters but cannot prove
hazard damage. No particular naturally exposed NPC encounter is asserted,
and no new per-slice gameplay fixture, campaign route or setup action was
created. Broader NPC swimming, drowning and exact original phase ordering
remain outside this bounded consumer.

## Subsequent checkpoint source integration

RFNC17 now separately admits qualified retained source-body physics on the existing terminal death-pose profile, preserving freeze/wake state and every clearance guard. See [the checkpoint change](FROZEN-DEATH-CHECKPOINT.md). That source is awaiting compilation and actual save/load; the original limitation described above was the state when this liquid consumer was introduced, not a runtime result for RFNC17.
