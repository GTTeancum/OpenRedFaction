# Shared radial lethal death direction

Source-written and staged against clean `dd9001fec90833b64032a75999e13df280aab25c`.
No build, syntax check, test, fixture, original-game execution or XEMU run was
performed. Compilation and action/save behavior remain unverified pending the
parent's scheduled 15:00 UTC batch. Original inputs remain unchanged.

## Concrete gap, not blast knockback

Read-only disassembly used original RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The ordinary shared blast `0x488dc0 -> 0x489010` does not directly add actor
velocity, force or momentum. Its directional calls `0x4a5a20/0x4a5af0` mark
player damage-direction bits; they are not physical impulse. Generic entity
damage `0x4892c0 -> 0x41a350` handles vitals and existing reactions without an
outward blast-force request. The ordinary Rocket impact path `0x4c8a10 ->
0x4c16e0 -> 0x48e640` initializes the named particle recipe, not actor motion.
The separate `0x491f50 -> 0x4920f0` path does relaunch effect-mesh debris using
its own velocity at record+0x40. That record is not an actor physics owner and
is not evidence for adding arbitrary radial knockback to actors or extracted
GeoMod solids. No impulse was added.

A concrete missing actor consumer is the radial death direction written by
`0x4891bf..0x48921f`. The current shared damage loop reached generic damage and
existing death selection without supplying this original hint. Neither
`rf_scene_npc_damage` nor `rf_entity_damage_sp` reconstructs the radial vector
or writes `death.action_824`; their interfaces have no blast epicenter.

Player damage-direction marking is also absent from the shared blast loop,
but the current retained `campaign_player_contact_flags` has no rendered
consumer beyond diagnostic uses. This slice does not add telemetry-only
feedback or reopen presentation work.

## Original source branch

- `0x489034..0x48904a` requires the existing clear CF5 ray to physics position.
- `0x489050..0x489058`, through `0x409fa0`, stores the float vector
  `victim.physics_position - epicenter`.
- Active Nano handling `0x48905d..0x48910b` returns before generic damage and
  before the death-direction branch, including the shield-breaking pulse.
- `0x48910c..0x48913d` requires positive body-center falloff and stores the
  attenuated damage amount.
- `0x4891bf..0x4891ec` resolves the victim's full handle, requires positive
  health, then requires `0x40a1e0 == 1`. The predicate requires object+0x80
  model presence and class+0x94 model kind2.
- `0x4891ee..0x48921f` dots the retained physics-position delta against the
  actor's physics-forward vector at object+0x114. This is orientation elements
  6..8, not the separate published/model basis used by Riot contact hints.
- `0x40a0b0` sums products Z, Y, X without storing the projection to float.
  Positive projection writes action6; zero or negative writes action7.
- The original write precedes `0x48929a -> 0x4892c0`, overwrites a prior hint,
  and is not conditional on actual applied damage or lethality. Persistent
  nonlethal behavior remains explicitly outside this bounded implementation.

## Bounded implementation and ordering

`scene_radial_death_hint.inc` adds a stack-only receipt and read-only actor
qualification. The shared loop includes it and captures a receipt only after
its unchanged positive falloff and non-consuming Nano result, before generic
damage callbacks. Nano's only mutating break path always consumes the pulse;
therefore it never reaches this capture or publication.

Qualification requires matching full handle, registry wrapper, entity view,
damage owner and allocated physics body; type0; valid class model kind2; and
the actor's currently loaded, untransferred model pose. It does not acquire,
load, evaluate or step a model. Finite position/orientation and float-storable
component deltas are required. Projection uses Z/Y/X double accumulation and
retains its sign without a final float store. This is the existing bounded
numeric convention, not a claim of universal bit-exact x87 cancellation.
Nonfinite geometry simply leaves the new hint absent and does not alter the
established damage outcome.

The receipt carries slot, full handle, class, computed action and the existing
`scene_robot_death_epoch` captured at shared-blast entry, before any other
population callback. That epoch already advances on NPC teardown, successful
standalone NPC restoration and successful world publication. Capture and
publication both reject a different timeline. The existing caller-supplied
flight/robot pool epoch remains independent and unchanged. Source attribution
and blast epicenter remain those of the accepted blast; no mutable shooter,
held weapon or post-callback direction is consulted.

After generic damage succeeds, the existing code handles its normal combat
accounting. Immediately before the existing NPC death-entry call, the receipt
re-resolves the full target, class and live model join. It publishes only for
finite positive applied damage, finite nonpositive health, no removal/hidden
flag and no already-entered death. There is no callback between this final
lookup/write and death entry. A qualifying lethal hit overwrites an earlier
`action_824`, following original radial priority, rather than inventing a
previous-hint veto. A surviving, immune, zero-applied, shield-consumed, failed,
removed, transferred or replaced target receives no new retained hint.

Existing death entry, `combat_death_start`, `rf_scene_npc_death_select` and
`rf_entity_death_select` remain authoritative. Actions6/7 still undergo
forward/backward clearance and existing crouch/controller/missing-motion
fallback. The current `combat_death_start` already clears `requested_83c`
before selection; this slice leaves that existing behavior unchanged and does
not claim to restore full original requested-action priority.

The older ordinary NULL-pool blast callers' broader callback-continuation
semantics are unchanged. The new hint has its own full-pulse timeline guard;
this patch does not claim to harden every preexisting damage-loop dereference.

## Persistence and delivery boundary

Living RFNC does not retain `death.action_824`, so original nonlethal hint
persistence would require separate continuation ownership. This slice writes
only at accepted lethal entry, where existing dead-actor/corpse persistence
already owns the selected death action. No retained allocation, timer, new
save field, wire version, save guard, source population, cover rule, damage
factor, armor debit or physical movement state is added or changed.

The deliverable is one new include, five small shared-loop additions and this
document. The active repository and grounded player/NPC contact work were not
edited. Independent source review and the parent's scheduled compilation are
separate from action/runtime verification; no new runtime result is claimed.
