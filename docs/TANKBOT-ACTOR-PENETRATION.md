# Authored Tankbot Chaingun actor penetration

Status: 2026-10-10 02:00 Xbox compilation passed at source 5c506c1a.
Build-only evidence: changed attack/audio/parser execution and save restoration
remain runtime-unverified. No fixture, grant, route, forced event or original
asset change was used. See HOURLY-20261010-0200.md for exact build proof.

## Evidence and scope

- Read-only installed `tables.vpp/weapons.tbl:1771–1833` defines the exact
  `Tankbot Chaingun` primary with `continuous_fire` / `from_eye`, Flags2
  `undeviating`, bullet damage, finite `turret_ammo`, and no magazine/reload pair.
- Its primary authors AI range 50, AI spread 1.5 degrees, a 16-shot burst at
  0.1-second intervals, and damage 100 with AI scale 0.2. The existing shared
  damage consumer therefore supplies a fresh base of 20 before target-specific
  armor, class and damage rules.
- Lines 1829–1830 author `$Piercing: true` and `$Piercing Power: 0.1`. The adapter
  consumes these retained parsed values; it does not hardcode a replacement
  power or change the table. The authored projectile collision radius 0.02 is
  not the actor penetration debit.
- Original RF.exe `4c9b48..4c9b73` scales each contact from the fresh base using
  remaining/initial power. `4c9b75..4c9bde` spends `0.2f * body.bounds.radius`,
  continues only while power remains strictly positive, and advances from the
  body entry point by that radius along the normalized incoming direction.
- `scene_sniper_penetration.inc` already supplies that bounded shot-local body
  policy for sniper, authored handheld firearms and mounted Vauss. This slice
  reuses the policy, not those adapters' target enumeration.

The nominal authored power is small: an actual body radius of 0.5 or greater
exhausts power 0.1 on the first body under this policy. That first contact still
receives its pre-spend damage. Enabling authored penetration does not promise
that an ordinary human-sized actor will permit a second contact.

Only the exact admitted Tankbot Chaingun with authored piercing enabled enters
the new adapter. Its existing finite-reserve admission, loadout, readiness,
cadence, one accepted-shot debit, single spread evaluation and fire/burst audio
remain owned by the ordinary NPC caller. A disabled piercing definition stays
on its existing ordinary path. Tankbot Missile and Smash, other NPC guns,
player weapons and turret adapters are unchanged. The current synchronous NPC
spread adaptation is retained; this does not reconstruct the original
continuous projectile scheduler or reinterpret `undeviating`.

## Accepted shot and integration

`src/diagnostic/scene_ai_tankbot_penetration.inc` is included immediately after
`scene_ai_actor_hit_probe.inc`, before the ordinary enemy tick. It locally
includes `rf/weapon_precision.h` and the guarded shared penetration helper.

`scene_ai_tankbot_penetration_shot` retains the accepted full source handle,
intended target handle, weapon, damage kind, authored enable/power and fresh
AI-scaled base. The caller snapshots it and the original eye after its single
ammo debit and before fire presentation, admitting the exact source once
through its registration view and full registry/entity identity. That initial
check preserves ordinary source admission without rereading a mutable shooter
between later contacts. After its existing single spread evaluation, it calls:

```c
scene_ai_tankbot_penetration_fire(stream, frame, owner, owner_slot,
    &shot, original_eye, spread_ray, physical_player_eye, clock_bits,
    &effects, &feedback, &result);
```

The adapter copies the shot and complete original ray locally. No contact
reselects the held weapon, recalculates the base from mutable inventory, debits
ammo, samples spread, changes AI targeting or advances a persistent projectile.
The accepted owner pointer/slot are used only for the bounded existing actor
hit probe and only while that slot still has the accepted source fullhandle.

`scene_ai_tankbot_penetration_result` separates:

- `total_applied`: damage applied to actual actors plus any terminal vehicle
- `player_applied`: only damage applied to the actual physical player
- `vehicle_applied`: only damage applied to the terminal vehicle/turret
- `vehicle_handle`: that terminal fullhandle when vehicle damage is positive,
  otherwise `UINT32_MAX`

Shield durability, clutter and detached-piece damage do not masquerade as
actor health damage in this result. The caller preserves intended-target
health and Attack bookkeeping, consumes a queued single shot once, and counts
one accepted shot. It bypasses the legacy `!victim` inference of player damage
for this adapter and emits player/vehicle events using their separate amounts
and the retained source. Terminal vehicle health is resolved from the returned
actual vehicle handle, not from the AI's intended target.

## Contact and continuation policy

1. Query live actors on the current advanced segment, excluding the accepted
   source, existing seat/operator/generated relationships, and all previously
   visited generation-bearing actor handles. Mirror ordinary
   `scene_ai_actor_ray_select`: physical player first, then NPC order with
   strict-nearer replacement, retaining an endpoint contact when no earlier
   actor exists. Each candidate combines its real body sphere admission and
   independently posed held shield before competing with another actor.
   Affiliation and the intended AI target do not filter incidental hits.
2. Convert the selected local contact fraction to the original absolute ray.
   On every pass, run existing vehicle, clutter, detached fragment and world
   arbitration against the original complete ray up to that fraction. A body
   radius advance cannot leap over intervening cover. These terminal objects
   receive the fresh pre-contact attenuated damage; none enables continuation.
3. Revalidate the selected actor's full handle, registry/entity identity,
   body allocation, visible/live flags and health after cover callbacks.
   Physical shield commit uses fresh unattenuated AI base. Accepted protection,
   including the hit that breaks it, ends the shot. A rejected shield admits
   only its genuinely retained body contact at or before the already-checked
   actor limit, with another original-ray cover check. A shield-only contact or
   a farther retained body conservatively stops instead of fabricating a hit
   or bypassing another actor. NPC nano protection likewise consumes fresh
   unattenuated base and terminates.
4. Revalidate immediately before body damage. Retain the actual body radius,
   entry point and material; prepare shared power spending/continuation before
   damage or death callbacks can change the target. Damage is freshly computed
   as `base * (remaining_power / initial_power + 1) / 2` for each contact, never
   multiplied from the previously attenuated amount. Clear-air travel spends
   no power. Zero applied damage still spends body power, and the exhausting
   body still receives its current pre-spend damage.
5. Apply normal NPC/player damage with the caller's effects, feedback and
   clock. Handle each actual NPC death immediately through the existing death
   entry, script-move cancellation and combat death start, retaining fullhandle
   checks across callbacks. Invoke the existing material impact presentation
   hook and then the bounded actor-hit probe. Repeat live selection only while
   power/range and the shared 32-distinct-fullhandle cap permit it. An empty continuation
   after a body contact does not add a second miss.

World metal, exit surfaces, ricochet, material resistance guesses, actor radius
guesses, new damage queues and repeated-hit damage are deliberately absent.
No save layout, persistent field, allocation, asset grant or startup traversal
is introduced.

The adapter preserves the existing optional impact-audio call sites and their
accepted contact points/materials. Tankbot Chaingun is currently absent from
`scene_weapon_impact_audio_supported`'s allowlist, so these hooks remain silent
for this weapon. This slice does not expand that allowlist or claim new audible
impacts. The separately admitted Tankbot burst-onset audio is unchanged.

## Remaining verification

The parent owns compilation and any authorized bounded stock-64-MiB Xbox
validation. This source change alone is not runtime evidence of penetration,
audio, finite exhaustion or save/load continuity. See
[primary fire](TANKBOT-PRIMARY-FIRE.md) for the existing natural L7S4 startup
limitations: the player begins about 48.69 units behind the Tankbot while
unalerted acquisition uses its 20-unit forward gate, and all 20 linked
Shoot_Once events request secondary mode 1. No synthetic primary order or
campaign traversal was used to bypass those authored conditions.
