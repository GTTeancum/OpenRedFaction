# NPC melee contact with retained props

Source-written after the October 10, 2026 02:00 Xbox batch. Uncompiled and
runtime-unverified until parent-coordinated validation. No build, test, syntax
check, emulator run, route, fixture, capture or original-input change was made
for this slice.

## Original evidence

Read-only installed RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `00409340` consumes NPC primary impact timers independently. The loop begins
  at actor-relative `+220`, corresponding to object `+4c0`, checks expiry at
  `0040939b`, disables that timer at `004093aa`, calls projectile factory
  `004c77a0` at `0040956b`, and advances to the second timer at `0040959b`.
- Factory collision registration is `004c7c9b -> 0048c9a0`. Weapon flag `20`
  selects immediate melee collision at `004c7ed1–004c7edd`. The factory checks
  candidate object pairs and selects model/general responses at
  `004c7fc8 / 004c7fd6`; accepted contact reaches `004c4b50` at `004c8000`.
- Projectile versus kind-4 clutter admission is present at
  `0048c2c7–0048c334`. After owner exclusions specific to Riot Shield, it runs
  projectile eligibility and selects a model response for clutter radius
  greater than `0.2`. Ordinary stationary props are not omitted merely because
  they are separate from room or mover geometry.
- Object-contact dispatch `004c59f0` reaches generic damage `004892c0` at
  `004c6132`. Kind-4 damage uses `00410270`, whose typed amount is multiplied
  by the authored clutter damage factor before reducing health.
- Installed `tables.vpp/weapons.tbl` entries for Reeper Claw, Baby Reeper Claw,
  Drone Smash and Mutant Attack 1/2 declare melee, from_eye, underwater,
  no_fire_through, bash damage, collision radius `0.5` and zero blast radius
  (lines 1942–2018, 2175–2201 and 2236–2294).

These instructions establish that original melee can contact and damage
physical props. They do not establish equivalence between the original
projectile collision volume/order and this port's existing intended-target
segment. Original melee displacement uses velocity multiplied by lifetime;
the port retains its existing authored AI-range segment.

## Existing consumer gap

`combat_obstructed` calls `rf_geometry_collision_ray`, whose input consists
of world room geometry and `campaign_movers`. Retained clutter is held
separately in `campaign_clutter_bodies`. The old
`campaign_enemy_melee_contact` queried the intended body and then
`combat_enemy_fragment_shot`, adding detached rubble and world cover but no
retained prop candidate. A victim behind an eligible prop could therefore
receive a strike that neither stopped on nor damaged the prop.

## Bounded integration

The shared melee helper now takes `int32_t kind` immediately before its
contact output. Both ordinary and delayed melee callers pass their existing
resolved primary damage kind, preserving bash-specific clutter factors.

Contact processing retains the existing normalized AI-range ray and body
fraction, or the full ray when the body misses:

1. Use `campaign_clutter_firearm_select` to find a strictly nearer eligible
   retained prop, no farther than that retained body limit.
2. Snapshot the selected slot, owner and generation-bearing handle.
3. Call the existing `combat_enemy_fragment_shot` once, clipped to the prop
   fraction when present. Nearer world stops all damage; nearer unobstructed
   rubble receives its existing direct damage and consumes this contact.
4. If unblocked, revalidate the selected slot, owner pointer, registry handle
   and visible/nonretired state, then call `campaign_clutter_firearm_damage`.
   A stale candidate consumes the contact without damaging a replacement.
5. Only an unblocked strike with no prop candidate can report actor contact.

The firearm wrapper is deliberately not reused: its preliminary
`combat_shot_obstructed` also detects detached rubble and can return before
the damaging rubble helper. This melee integration preserves the old
nearer-rubble damage path rather than introducing that separate behavior.

The existing clutter damage service retains authored protection/factors,
damaged state, one-time retirement and deferred break presentation. A
protected prop still consumes contact. Breaking a prop never permits
same-impact continuation into the intended actor. Later independently
scheduled contacts perform a fresh query. No piercing or blast is added.

Prop eligibility stays with the existing selector. In addition to stationary
props, supported moved or group-controlled owners are queried at their current
retained pose. No new stationary-only filter or swept-motion reconstruction
is introduced.

The delayed owner continues to consume its due bit before callbacks and
requalify source/target handles and geometry before actor damage. No new
state, allocation, timer, save field, ammo rule or animation owner is added.

## Limits

Physical player/NPC shield interception, incidental actors, swept limbs,
original `0.5`-radius volume, exact original reach/contact ordering, swept prop
motion and vehicle melee remain outside this change. Existing melee contact
audio remains body-contact-only. The selector retains existing prop eligibility and strict
nearer tie policy: an exact prop/body tie preserves body priority, and a prop
contact exactly at the full-ray `t=1` endpoint is not newly admitted. Endpoint
behavior remains unverified. Runtime contact, protection, break and
later-impact behavior have not been validated by this source-only slice.
