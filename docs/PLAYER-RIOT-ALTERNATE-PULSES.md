# Player Riot alternate accepted contacts

Independently source-reviewed and parent-integrated against clean
`b4c4aabfdf77f35c0662975b1d8fa7c04a80f517`, awaiting the scheduled
10:00 UTC Xbox batch. No build, syntax check, test, fixture, XEMU run or
contact/audio runtime check has been performed for this slice.

## Gap and original evidence

The previous held slot2 path drained finite charge each active simulation tick
and also dispatched its generic ray every such tick, even when the shared
trigger did not admit a shot. Rubble, retained props, vehicles and NPCs received
alternate120/60 damage, with an electrical-kind6 substitution; Nano was excluded
because applying that per-tick amount or60 per held tick would be incorrect.

Read-only RF.exe inspection uses SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The timing and current-lifecycle investigations establish these bounded facts:

- Alternate admission requires positive loaded charge at `0x4a5555..0x4a556d`.
  Empty handling stops continuous fire and returns before scheduling at
  `0x4a55fa..0x4a5659`. Cooldown rejection queues nothing; later held input is
  polled again. Riot authors a0.5-second alternate wait.
- Weapon descriptor `+0x158/+0x15c` contains alternate delay metadata. Riot has
  one positive alternate delay,0.01 seconds. These fields are not another pair
  of player-owned timers. Primary retains its0.15/0.6-second descriptor slots.
- Continuous start `0x41ab05..0x41ab48` and player admission
  `0x4a579f..0x4a57f8` both write the same actor `+0x4c0/+0x4c4` slots. Those
  writes replace deadlines; they do not create a second contact owner.
- Player service clears every expired slot at `0x4a281b..0x4a2848` and then
  calls the factory once at `0x4a28fa`. It does not rearm there. NPC service
  `0x409340` excludes player-associated actors at `0x409368..0x409375` through
  `0x42a8e0`, so it cannot duplicate this player service.
- The melee factory collision retires its projectile at `0x4c8006`. Ten
  milliseconds is an accepted contact delay, not a repeat period. The alternate
  mode value0x20 must remain associated with that accepted action after release.
- Ordinary entity contact calls `0x4c8b10(projectile,0)` at
  `0x4c5cfc..0x4c5cff`, obtaining primary60 before later ordinary modifiers.
  Damage kind comes from the shared descriptor `+0x52c` at `0x4c610c`, authored
  bash. Alternate120 and electrical-kind6 are not substitutes for this getter.
- Nano independently obtains primary60 at `0x4c5a90..0x4c5a93`, debits armor,
  and consumes the contact, including a break. Its active predicate and break
  ownership remain those documented in `MELEE-NANO-SHIELD.md`.
- Release calls continuous stop at `0x430ef2..0x430f2e`. Stop `0x41ae70` clears
  continuous state while preserving already-admitted contact timers and mode.
  Selection explicitly clears delayed slots at `0x428f89..0x428f99`. Delayed
  service does not make a fresh loaded-charge check.

These addresses establish timing, mode and base-damage ownership. They do not
establish exact original swept collision, model/sphere ordering, stun behavior,
material-specific impact sound, bolt/spark effects or controller-frame parity.

## Bounded owner and admission

`scene_player_riot_alternate.inc` adds one allocation-free alternate ticket and
one10ms deadline beside the unchanged primary owner. The existing primary
metadata loader already validates the exact alternate delay and sole slot;
alternate admission additionally verifies the authored500ms wait and primary
60/bash profile, then captures that profile in its immutable definition.

The existing `combat_trigger` remains the only input/cadence owner. Only a real
`fire==1` with positive charge before held drain can begin an alternate pulse.
Its shared cooldown remains intact, including when changing requested mode;
held cooldown ticks do not queue damage, and a tap rejected during cooldown
cannot appear later after release. Simultaneous buttons still choose primary.

Accepted source, weapon, mode0x20, amount and kind are copied into the strike.
One attack counter/Taser onset is emitted during accepted presentation. The
contact is attempted once at the first actual service opportunity at or after
10ms, using current eye and forward orientation. At the port's60Hz simulation,
this is normally the following combat tick. There is no catch-up loop or target
captured at input time. A miss, cover, protected target, error or consumed shield
break retires the contact rather than retrying it.

Existing accepted alternate work services before this tick's new held-empty or
explicit reload admission. That ordering prevents the final-cell pulse from
being erased by the reserve-reload branch on the next tick. The live query
ignores held input and loaded charge. Release stops new admission and the
existing held drain/animation; it does not revoke the accepted contact. An
already active reload or another explicit lifecycle invalidation still cancels.
This is the bounded port ordering for these owners, not a claim that every
original presentation/update phase has been reconstructed.

Charged alternate takeover still cancels pending primary even during cooldown,
as established existing port policy. It now calls primary-only cancellation;
it cannot erase its own pending alternate record on each held tick. Primary
150/600ms deadlines, overdue coalescing, onset, no-ammo policy and release
survival remain unchanged.

## Shared contact and consuming shields

The former primary walk is now a shared guarded Riot contact adapter. Each
caller supplies a stack-local immutable contact record and its owner-specific
current-ticket/live callback. Primary supplies exactly its former immutable
primary profile; alternate supplies its captured primary60/bash profile. Neither
contact reads current input or a newly selected damage profile to reinterpret
an accepted action. All previous primary revalidation points remain in order.

The existing2.6-unit ray and body/shield candidate policy are retained. The
existing world/mover obstruction, rubble, retained-prop, vehicle/turret and NPC
populations and ordering are unchanged. No target, actor, route or grant is
manufactured. Whole target handles are retained and reacquired around callbacks;
stale selected contacts remain consumed rather than hitting replacements.

Physical Riot Shield processing precedes Nano, and an accepted physical-shield
contact, including its breaking contact, returns before Nano or health. A
rejected physical shield reacquires the full NPC handle before the Nano call.
Nano receives60 once per admitted contact through the existing sole consumer.
Invulnerable consumption, armor debit, Capek break hook and zero-clamping remain
unchanged. A consumed Nano contact returns before generic health, flesh sound,
aggression/disguise changes and death handling, with no added hit flash. The
next independently accepted contact may reach health after armor is exhausted.

On an ordinary unshielded NPC, the same base60/bash request reaches the existing
generic damage/effect/death path. The shared adapter preserves primary effects
and contact sounds. The alternate's existing damage-contact diagnostic advances
only for applied health damage under the captured mode. Exact alternate
presentation and the existing held-charge HUD coloring are not reworked here.

Slot2 no longer reaches the generic active-driven ray. The provisional
alternate120/60 and electrical-kind6 substitutions are removed from that path.
Other weapon rays, NPC melee owners and radial Nano processing are unchanged.

## Lifecycle and save boundary

Each alternate record has a full player handle and monotonic ticket. Presentation
and contact reservations remain held while their callbacks unwind, even if a
callback cancels or resets the record. The due deadline is retired before the
contact callback; copied state cannot resurrect it afterward. Reentrant begin
or service calls cannot acquire another pulse while a reservation is active.

The existing `scene_player_riot_cancel`, `reset` and `save_pending` wrappers now
cover both owners at their existing explicit boundaries: successful live weapon
selection, form changes, inventory strip, death/restart, vehicle/turret boarding,
actual teleport, scene teardown, frame-zero initialization and successful load.
The pure live query also retires an invalid/removed source or lost ownership.
No cancellation was inserted in raw `campaign_select_primary` or shared load
staging/relocation helpers.

The existing on-foot save guard remains after the `!live` return, so pending
outgoing work cannot veto load preflight. The seated guard inherits both owners
through the same wrapper. A pending strike or callback reservation is rejected
by live save capture; no save version or persistence format is added. RFCP
success retires both owners after publication. Ordinary world load resets them
only after world publication and successful storage close. Failed load staging
and close do not gain a new cancellation. Existing charge-remainder save
limitations remain unchanged.

## Explicit battery approximation

This slice deliberately preserves the current finite fractional accounting:
`rf_weapon_charge_step` drains the100-unit cell over150 active held simulation
ticks and retains fractional debt across release. It remains independent of
contact admission, so holding through cooldown or into empty space still costs
charge. No delayed loaded-charge recheck, charge refund or second debit is added.

The original is more specific: service also debits one loaded unit per
alternate attempt via `0x4a296f -> 0x42c310 -> 0x42c3b3`; animation-state2 has a
separate drain at `0x4aafa8` with clipDrain/clipSize =25ms. Therefore authored
2.5 seconds is not proof of exact total battery duration when both original
consumers run. Reconstructing both owners requires a once-only, saturating,
reentrancy-safe attempt debit and the complete independent drain lifecycle.
Adding the extra unit blindly to this retained approximation would double-charge
without establishing that lifecycle. Full battery reconstruction remains
separate; this patch claims accepted contact timing/damage/Nano ownership only.

## Integration and verification limits

Parent integration must apply the bounded patch to the clean baseline and merge
only its top-level milestones/doc changes if other work has since landed. The
primary owner source is unchanged. No active-checkout mutation, build, syntax
invocation, test, new fixture, emulator run, campaign route, grant, image,
cleanup or original-input modification occurred during this implementation.

The scheduled10:00 Xbox batch remains the first compilation opportunity. Actual
alternate admission/contact timing, current-aim misses/hits, release and
last-cell survival, partial/breaking Nano armor, physical-shield priority,
ordinary props/rubble/vehicles/NPCs, interruption/reentrancy and save/load
behavior remain runtime-unverified. Historical held-taser results in
`RIOT-STICK.md` do not cover this changed damage owner and its old per-tick
expectations are superseded. No unrelated validation or route work is added.
