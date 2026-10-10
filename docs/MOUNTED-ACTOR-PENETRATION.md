# Mounted Vauss actor penetration

Status: 2026-10-10 02:00 Xbox compilation passed at source 5c506c1a.
Build-only evidence: changed attack/audio/parser execution and save restoration
remain runtime-unverified. No fixture, grant, route, forced event or original
asset change was used. See HOURLY-20261010-0200.md for exact build proof.

## Original evidence and scope

This adapter consumes the object policy already reconstructed in
[SNIPER-PENETRATION.md](SNIPER-PENETRATION.md), with the conservative contact
lifetime boundary documented in
[HANDHELD-ACTOR-PENETRATION.md](HANDHELD-ACTOR-PENETRATION.md). It does not claim
a new disassembly pass or a new material-penetration model.

- `4c45df..4c4627` retains optional `$Piercing: true` and required power in
  descriptor `+27c`; `4c7c2e..4c7c40` copies it to fresh projectile `+2f0`.
- `4c5cfc..4c5d38`, `4c600f` and `4c6132` resolve fresh base damage for each
  object contact and apply object policy before direct damage.
- `4c9b48..4c9b87` supplies
  `base * (remaining_power / initial_power + 1) * 0.5`, then debits
  `target_extent_180 * 0.2f`. The exhausting contact still receives damage;
  continuation requires strictly positive post-contact power.
- `4c9b9a..4c9bde` advances from the actual contact by normalized incoming
  direction times target `+180`. The enclosing body radius producer
  `4a0cb0` maps this to `body.state.bounds.radius`, not individual sphere
  radius, a guessed material thickness, health or surface-to-surface chord.
- `4c5b54` terminates nano interception before ordinary object attenuation.

The installed-table evidence recorded in the sniper note authors Vauss power
`0.1`. The existing turret metadata loader already retains this field. The
new dispatch requires both the accepted Vauss identity and its authored enable
flag; it copies the actual descriptor power rather than hardcoding `0.1`.

Both the player-mounted and autonomous/NPC-operated Vauss paths already use
`scene_turret_scene_shot`, so they share this adapter. HEAP returns through its
unchanged projectile branch. Disabled-piercing Vauss keeps the prior fallback.
Ordinary NPC-held weapons, Tankbot Chaingun and other weapon consumers are not
expanded by this slice.

## Integration and accepted-shot identity

`scene_turret_scene_combat.inc` now locally includes `rf/weapon_precision.h`
and the guarded `scene_sniper_penetration.inc`, because turret combat precedes
precision gameplay in `scene.c`. The existing later includes are harmless
under their guards. No `scene.c` edit is needed. The shared helper's algorithm
is unchanged; only its consumer comment is updated.

After the existing HEAP branch and player-versus-AI base-damage choice, the
callback snapshots Vauss identity, damage kind, fresh base, authored power,
turret collision source, damage source, accepted mounted-player full handle
and accepted NPC-operator full handle. The existing single spread draw, shot
count and fire-audio request happen once before dispatch. Acquisition, turning,
burst/rate stepping, player hearing, ammunition policy and loop-audio lifetime
remain owned by their existing callers.

Collision selection always receives the turret host identity, preserving
host/generated-part exclusion. A player-mounted shot's damage requests retain
the accepted player handle; autonomous/NPC-operated shots retain their turret
source and existing AI damage scale. The operator exclusion is shot-local, so
an earlier damage/death callback clearing a seat binding cannot make the same
shot hit its accepted operator. Full generation-bearing IDs are retained,
rather than slot indices or masked IDs.

## Contact ordering and lifetime

The adapter copies the original eye and post-spread displacement before its
contact loop. Every damage amount starts from the same accepted fresh base,
using current pre-contact power. It never attenuates an already-attenuated
previous amount and never debits power for clear-air travel.

1. Select live bodies and physical shields on the current advanced actor
   segment, skipping visited full handles and the accepted firing identities.
   Preserve player-first enumeration, strict-nearer NPC ties, ordinary body
   sphere selection and the existing posed-body-versus-NPC-shield decision.
   Player shield geometry continues to use the autonomous context's physical
   player eye and existing orientation; the mounted firing player's own body
   and shield are excluded. Shield tokens retain their original query limit
   and advanced start/full endpoint for commit.
2. Convert the selected actor fraction to the original ray's absolute
   fraction. Re-run the existing vehicle selector, clutter interception and
   fragment/world checks in the same order on the original complete ray.
   A radius advance cannot skip cover inside that advance or behind an earlier
   actor. Clutter, vehicle/turret and detached fragments are terminal and use
   current pre-contact object-power scaling. Existing equal-fraction behavior
   is retained by the same selectors.
3. Revalidate the exact winning player/NPC handle and live allocated body.
   Commit selected physical shields with unattenuated accepted base damage.
   Accepted shields, including their breaking hit and existing weapon-fallback
   callbacks, consume the shot. An explicit commit error still propagates.
4. A rejected shield is not a body contact. Body fallback requires a real
   retained sphere hit at or before the selected shield fraction, already
   qualified by actor and full-ray cover selection. A shield-only contact or
   farther body conservatively stops the shot. No shield-surface body damage
   or second speculative arbitration is manufactured. Revalidate the owner
   again before using its body.
5. For a qualified NPC body, the existing nano consumer receives unattenuated
   accepted base and weapon identity. Accepted nano interception consumes the
   shot without body fallthrough or a new penetration debit.
6. Snapshot the actual body contact point and enclosing body radius. Apply
   the shared fresh-base attenuation, debit radius, mark the full handle and
   prepare the next segment before ordinary damage/death callbacks can retire
   or rebuild the target. A zero-applied-damage body still spends power.
7. Dispatch existing NPC/player damage and feedback once per admitted body.
   NPC death entry retains script-move cancellation and `combat_death_start`;
   player damage retains the host-attributed combat event. Existing health,
   death HUD and contact counters publish per admitted body. The shot counter
   stays one; an empty continuation after a body does not create a new miss.

The existing shared helper caps consumption at 32 distinct full handles,
including the player if hit. Its visited list supplies a bounded fallback for
zero/tiny radii without a fabricated epsilon. There is no persistent projectile,
allocation, archive read per shot, new timer, save field or cleanup hook.

## Remaining boundaries

This is the existing synchronous turret ray with ordinary sphere-body proxies.
It does not add retail projectile flight, location-specific body damage,
same-frame overlapping-body parity or x87 bit parity. Radius advance may skip
another actor wholly inside that advance, as implied by the original position
update; competing world and object cover is conservatively retained.

World/mover policy remains blocking. The original `4c9bf0` material policy,
metal ricochet and finite back-facing exit traversal are separate work; no
visibility ray is repurposed as a thickness oracle. There is no material pass-
through, guessed thin-metal exit, terrain edit or invented exit/impact sound.
Existing Vauss firing audio remains unchanged; this adapter adds no new impact
audio path.

The bounded adapter intentionally parallels the current single-contact turret
block, preserving that block for disabled piercing. Future turret selection,
damage callbacks and telemetry changes need to keep both adapters aligned.

Parent compilation and original neutral-level startup, if successful, can
establish build and mandatory metadata admission only. They cannot establish
multi-actor damage, actual radius advance or exhaustion, secondary blockers,
operator exclusion through callbacks, shield/nano behavior, native audio or
save-load gameplay. Those remain unverified until an authorized focused check
or tester feedback exercises them. No such check or gameplay fixture is
bundled here.
