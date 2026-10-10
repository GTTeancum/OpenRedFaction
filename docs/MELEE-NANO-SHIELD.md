# Primary melee Nano shield damage

Independently source-reviewed and parent-integrated after the actual 09:00 UTC
batch against `5d55baeda849e221fa1278aa5b04b26c262317bb`. Awaiting scheduled
10:00 Xbox compilation. No build, syntax check, test, fixture or runtime check
has been performed for this slice.

## Gap and original evidence

The three current primary melee NPC consumers reached generic NPC damage after
qualifying their contact. Its active Nano predicate consumed damage without
debiting armor. Direct firearms already had a separate prepass, but the original
melee chain now independently establishes that primary melee reaches that same
Nano consumer rather than being exempt from it.

Original RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Read-only inspection supplied by the original-consumer investigation established:

- Projectile flag `0x20` selects melee at `0x4c7ed7`; its collision path reaches
  `0x4c8000`, `0x4c4b50`, the object/model adapter at `0x4c4c35`, and contact
  dispatch at `0x4c59f0`.
- Model collision `0x49b07d..0x49b1a4` explicitly admits the Nano sphere and
  contains no melee exemption. This establishes the shield as a melee candidate,
  without proving the port's existing ray/candidate policy equals that collider.
- Contact processing calls the active Nano predicate at `0x4c5a80`. At
  `0x4c5a90..0x4c5a93`, it calls `0x4c8b10(projectile, 0)` for the primary
  amount before the later power/location modifiers. Player primary uses the
  descriptor's `+0x108` damage; NPC primary additionally multiplies by the
  resolved AI scale at `+0x120`.
- The named special adjustments are Remote, Grenade and Rail. Melee does not
  introduce another amount override. Armor debit is at `0x4c5ae0..0x4c5ae7`;
  exhaustion calls the break consumer at `0x4c5afa`, then the contact is consumed
  at `0x4c5b54..0x4c5b5d` before generic health/effects and later impact handling.
- The shared predicate at `0x42cca0` still requires positive armor, class
  physics flag `0x02000000` and actor `+0x814` bit `0x20` clear. Object flag4
  consumes the contact without a durability debit. Event76 remains a toggle,
  not an armor refill.

These addresses support the bounded damage consumer, not new collision,
shader, audio or timing fidelity claims. Original inputs were not modified.

## Three existing contact owners

1. `scene_player_riot_primary_contact` calls `scene_nano_weapon_contact` only
   after its existing world/mover, rubble, retained-prop, vehicle and physical
   shield handling. A shield-accepted contact, including a physical-shield
   breaking contact, already returns. A rejected physical shield reacquires the
   full NPC handle before Nano processing. The call uses the accepted strike's
   weapon and `request.amount`, the immutable primary profile's authored60.
   It returns on consumption before generic health, impact-flesh sound,
   aggression/disguise changes and death handling. It adds no new hit flash.
2. The actual guarded `if(melee)` NPC-target branch in `campaign_enemy_tick`
   reacquires the retained target by slot and full handle, then supplies the
   captured guard weapon and `request.amount`. Its amount was captured from
   `combat_enemy_primary_damage` before fire presentation and already contains
   exactly one AI scale. On consumption it jumps to the existing
   `enemy_shot_done`, preserving retained-owner reacquisition, order completion
   and zero-applied scripted accounting. The later `if(!melee)` Nano branch is
   unrelated and unchanged.
3. `scene_ai_drone_smash_impact`, which also owns the admitted delayed creature
   primary contacts, calls Nano only after the existing ticket/phase, pair,
   selection/order, geometry and unchanged-transform requalification. It uses
   `strike->weapon` and the current contact's already-once-scaled
   `request.amount`. A consumed contact skips the generic NPC/death block and
   reaches the existing result-only epilogue with zero applied health damage.
   The service continues to retire its preconsumed due bit and phase normally.

No consumer consults a newly selected weapon, adds another AI scale, splits a
primary amount between deadlines, re-queries a damaging contact, or reaches a
replacement target through a stale handle. The player-target paths are unchanged.
The shared direct helper is already declared before all three consumers; no
new forward declaration, state, pool, timer, resource or save version is needed.

## Debit and break ownership

The existing `scene_nano_weapon_contact` remains the sole owner of admission,
invulnerable consumption, diagnostics, armor debit and the Capek break hook.
Positive shield armor is debited by the captured primary amount once per
admitted contact. Exhaustion remains clamped to zero under the port's existing
nonnegative vitals/save contract. The same contact is consumed, including on
break, so it cannot spill into health or trigger the generic damage effects.
The next independently admitted contact may reach normal health damage.

For a qualified Capek, the existing break hook applies its fall transition,
explicit break history, authored base restoration and delayed live-speed
refresh before zero armor is published. This slice adds no second break path
or movement policy. Existing break-admission errors propagate before armor
publication. Other Nano actors retain the existing hook's no-op movement policy.
Shield-OFF, exhausted-armor and non-Nano targets retain generic damage handling.

## Separately integrated accepted Riot alternate

The bounded alternate slice in `PLAYER-RIOT-ALTERNATE-PULSES.md` supersedes the
previous deliberate exclusion. It replaces provisional120/60 held-tick damage
with one contact per accepted500ms action, due10ms later at current aim. A
separate ticket carries immutable alternate mode and the qualified primary
amount60/bash through the same guarded Riot contact walk. Thus it reaches Nano
only after physical-shield handling and uses60 once per accepted contact,
including a consumed break. It does not invent60 damage per held tick or use
alternate120/electrical-kind6 as a substitute for the original primary getter.

The primary wrapper preserves its original60 profile, guard order and effects;
its150/600ms owner is unchanged. The ordinary and delayed NPC consumers above
are unchanged. Alternate release/final-cell survival, transient save guards,
source evidence and pending runtime coverage are detailed in the separate doc.
Original serviced-pulse ammo plus continuous drain reconstruction remains
separate; the existing finite fractional drain is explicitly approximate.

## Verification and integration boundary

The parent owns shared scene integration after the frozen batch and the next
authorized consolidated Xbox build/check. The pending radial Nano patch has
separate consumers and must be preserved: apply only this patch's scene hunk,
not a full frozen-base scene copy, and merge both top-level milestones and the
shared Nano document paragraph. Direct and radial processing remain independent.

No active-checkout edits, build, syntax/test invocation, XEMU run, campaign
route, new fixture, grant, image, cleanup or original-input change occurred.
Historical Nano firearm/rocket results do not cover these melee hooks. All new
melee partial debit, breaking-contact consumption, later health damage,
invulnerability, shield-OFF/zero fallback, physical-shield precedence,
cancellation and authored delayed-contact behavior remain runtime-unverified.
Exact original swept radius, sphere/pose ordering, other melee populations,
shader/hit/break audio and full alternate battery reconstruction remain deferred.
