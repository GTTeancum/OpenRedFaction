# Ordinary handheld actor penetration

Source staged October 10, 2026 against exact commit
`b343bdfe35c1d7859ae10f4571de18a71dbdbbec`. Parent integrated the independently source-reviewed patch before the 01:00
Xbox batch. Compilation and runtime verification remain pending. No helper build, syntax
check, test, emulator run, fixture, grant, route, generated gameplay input or
original-data edit was performed.

## Evidence and admitted scope

This adapter consumes the original object policy already reconstructed in
[SNIPER-PENETRATION.md](SNIPER-PENETRATION.md); it does not introduce a new
material-resistance model or claim a fresh disassembly pass:

- `4c45df..4c4627`: optional `$Piercing: true` enables descriptor `+264`
  bit `0x80000000` and retains required `+Piercing Power` at `+27c`.
- `4c7c2e..4c7c40`: each fresh projectile receives descriptor power at `+2f0`.
- `4c5cfc..4c5d38`, `4c600f`, `4c6132`: fresh base damage is resolved for each
  contact; object policy precedes direct damage.
- `4c9b48..4c9b87`: this contact receives
  `base * (remaining / initial + 1) * 0.5`, then power spends
  `target_extent_180 * 0.2f`. The exhausting contact still receives damage;
  only strictly positive remaining power continues.
- `4c9b9a..4c9bde`: advance from the actual contact by normalized incoming
  direction times target `+180`. The body-bound producer `4a0cb0` stores this
  enclosing radius at body `+f8`, retained as `body.state.bounds.radius`.
- `4c5b54`: nano interception precedes ordinary object-power attenuation.

The installed-table evidence recorded in the sniper note authors power `0.25`
for Assault Rifle, Machine Pistol Special, heavy_machine_gun and
scope_assault_rifle. The new dispatch requires the selected primary metadata's
actual enable flag, plus the explicit ordinary handheld scope:

- Assault Rifle primary burst rounds and alternate continuous shots
- Heavy Machine Gun primary and alternate shots, sharing descriptor power
- Scoped rifle primary shots, whether zoomed or unzoomed; alternate is the
  existing sight control, not an additional attack
- Machine Pistol Special's retained primary mode, using
  `campaign_selected_weapon()` rather than the ordinary MP slot identity

Normal Machine Pistol, Riot Stick, shotgun, pistol, undercover pistol, other
nonpiercing paths, sniper and rail routing remain unchanged. Mounted Vauss,
Tankbot/NPC fire, world-metal exit traversal and ricochet are outside this slice.

## Integration and contact lifetime

`scene.c` includes `scene_handheld_penetration.inc` after the existing precision
adapter, which provides the already-bounded `scene_sniper_penetration` helper.
After the existing successful ammo debit, the ordinary tick snapshots accepted
weapon identity, source handle, slot/mode, fresh base damage, kind and authored
power. The post-spread dispatch handles only the admitted single-pellet path.
It does not change launch audio, hearing, spread, trigger stepping, burst timing,
reload or ammo consumption. Existing applicable spread draws occur once; AR
primary still had no spread draw in this historical penetration slice. The
separate [PLAYER-PRIMARY-SPREAD.md](PLAYER-PRIMARY-SPREAD.md) correction now
integrates one authored primary draw before this same continuation adapter, with
no contact resampling and with compilation/runtime still pending.

The helper keeps private copies of the original start and delta. Damage/death
callbacks cannot retarget that segment or the accepted source/weapon. Every
contact recomputes its damage from the same fresh mode-specific base and current
pre-contact power, never from the preceding contact's attenuated amount.

Actor selection retains the ordinary `combat_body` sphere query, enumeration
order, strict-nearer ties and posed-body-versus-shield check. It is deliberately
not replaced with the precision selection service. Later actor queries use the
radius-advanced segment and skip previously consumed full generation-bearing
handles. The shield token retains its original local query limit and the same
advanced start/full endpoint for commit. Absolute fractions are used only for
original-ray competing cover and the shared power cursor.

Prop, vehicle/turret, detached-fragment, world and rubble checks are rerun from
the original complete ray, limited by the next absolute actor contact. This
conservative policy retains cover inside a radius advance and behind prior
actors. Props reuse the existing ordinary selector through its precision
wrapper, adding generation-qualified commit; prop/vehicle/fragment contacts
remain terminal and receive current pre-contact object scaling. Existing
fragment priority and equal-fraction behavior are retained.

For a qualified NPC body:

1. Revalidate its full registry identity and live/visible allocated body.
2. Run ordinary selected-shield commit or body-selected receive fallback with
   unattenuated base damage. Accepted physical interception, including a broken
   shield and its existing weapon fallback, consumes the shot.
3. Run nano interception with the actual accepted weapon and unattenuated base;
   accepted nano interception also consumes the shot.
4. Snapshot actual body radius, real advanced-ray body point and collision
   material before ordinary damage/death callbacks. Spend radius and prepare
   continuation before those callbacks; zero applied damage still spends power.
5. Apply the ordinary shared NPC damage/feedback/death route once. Retain source,
   damage kind, hit/death counters, rifle-alternate counters, combat-alert burst
   reset and player-form compromise behavior. Audio uses the retained actual
   contact point/material and accepted weapon.

At most 32 distinct full handles are consumed, using the existing precision
hit-capacity bound. There is no allocation, actor-count-sized list, persistent
projectile, new timer, save field, catalog identity change or cleanup hook.
The shared power helper's implementation is unchanged; its header comment now
identifies both consumers.

## Rejected shield safety boundary

The ordinary commit-versus-receive routing and broken-shield fallback callbacks
are retained. A selected shield that returns `accepted=0` is not itself a body
contact. This adapter separately retains that owner's real body-sphere fraction.
Body fallback is permitted only if it was already selected within the original
actor query limit and is no farther than the checked shield surface. Existing
competing-actor and full-ray cover decisions therefore still qualify it.

If no such body was retained, or its contact lies beyond the rejected shield,
this bounded adapter stops the pellet. It does not fabricate damage at the
shield surface, skip to an unchecked farther body, or start a second speculative
arbitration pass. This conservative stale/contact-disagreement boundary applies
only to the new piercing adapter; the original nonpiercing code is untouched.
Existing explicit shield-query/commit errors still propagate normally.

## Remaining limits and verification

The helper deliberately duplicates the bounded ordinary contact/damage adapter
instead of refactoring all ordinary and precision weapons during a frozen
build. Future ordinary-counter, body/shield-order or feedback changes must keep
this helper aligned with the nonpiercing block in `campaign_combat_tick`.

The existing synchronous 100-unit handheld ray and ordinary sphere body proxy
remain port adaptations. Radius advance may skip another actor wholly inside
that advance, as in the already-written sniper policy. Full projectile flight,
location-specific body damage, same-frame overlapping-body parity and x87 bit
parity are not claimed. World and movers remain blocking; there is no guessed
thin-metal exit, ricochet, terrain edit, new exit sound or object-extent estimate.

Compilation, actual multi-body damage, power exhaustion, secondary blockers,
shield/nano interception, MP Special identity, both HMG modes and native audio
remain unverified. The 01:00 compilation/original neutral startup, if successful,
will establish build and mandatory table admission only. It cannot establish
these gameplay outcomes; no fixture or inventory modification is bundled here.

## 01:00 UTC compilation status

The October10 Xbox build and original L1S1 neutral120-frame stock64MiB startup
passed at7cdfd7a4. This establishes compilation and table admission only;
action-specific gameplay, physical controller pause, audio output and save
restoration remain unverified. See HOURLY-20261010-0100.md.
