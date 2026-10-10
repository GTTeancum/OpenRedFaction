# Sniper actor penetration

Source written October 10, 2026, after the 00:00 batch. Compilation and runtime
verification are pending the parent's consolidated Xbox batch. No helper build,
syntax check, test, emulator run, gameplay fixture, grant or original-data edit
was performed. This closes a bounded player-sniper actor-continuation gap, not
complete material penetration or retail projectile equivalence.

## Original evidence

Read-only disassembly of owned `Installed_Game/RF.exe`, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `4c45df..4c4627` recognizes optional `$Piercing: true`, sets descriptor
  `+264` bit `0x80000000`, and reads required `+Piercing Power` into `+27c`.
  Absent/false bypasses this block. `4c9b28..4c9b47` stops a nonpiercing object
  contact without reading its power. The port initializes disabled power to
  zero; that is safe initialization, not an original explicit-reset claim.
- `4c7c2e..4c7c40` copies authored descriptor `+27c` to fresh projectile `+2f0`.
- `4c5cfc..4c5d38` resolves fresh base damage for each contact. At
  `4c600f`, object policy `4c9b20` runs before direct damage `4c6132`.
  `4c9b48..4c9b73` writes contact damage as
  `base_damage * (remaining_power / initial_power + 1) * 0.5`.
  The ratio is evaluated before this target spends power. Damage is therefore
  not repeatedly multiplied by earlier contacts' attenuated amounts.
- `4c9b75..4c9b87` stores
  `remaining_power - target_extent_180 * 0.2f`, using the binary32 constant at
  `58959c`. The contact still receives damage when this subtraction exhausts
  power. Only strictly positive post-contact power continues.
- `4c9b9a..4c9bde` normalizes incoming velocity through `4fa930`, then advances
  from the actual contact by that direction times target `+180` before the
  next collision. There is no clear-air power debit in this policy.
- Target `+180` is the enclosing physics-body radius: body starts at object
  `+88`, and sphere-bound routine `4a0cb0` stores radius at body `+f8`.
  The retained mapping is `owner->body.state.bounds.radius`; it is not an
  individual sphere radius, health, material multiplier or guessed thickness.
- Nano interception returns at `4c5b54`, before ordinary body penetration.
  The existing physical shield consumer also remains an earlier independent
  stop in this port. Neither receives a newly invented body-attenuation rule.

The installed `tables.vpp/weapons.tbl` has 44 weapon declarations. Eight enable
piercing and all eight declare explicit power: Sniper Rifle 1.0, Assault Rifle
0.25, Machine Pistol Special 0.25, rail_gun 100.0, heavy_machine_gun 0.25,
scope_assault_rifle 0.25, Vauss 0.1 and Tankbot Chaingun 0.1. The other 36 omit
the field; none explicitly authors false. Primary metadata now retains the
optional enable/power pair, but the other seven firing consumers are unchanged
by this slice, including the existing rail path.

## Integrated player path

`scene_precision_gameplay.inc` retains its one shared NPC damage, feedback and
death block. `scene_sniper_penetration.inc` supplies only bounded shot-local
power, full-handle visitation and next-ray preparation. There is no persistent
projectile, allocation, archive access per shot, save field or cleanup hook.

For a sniper shot:

1. Capture the accepted weapon's identity, base damage, damage kind and authored
   power after the existing single ammo consumption.
2. Query the existing live NPC body and physical-shield geometry, retaining its
   original near-to-far and tie rules. The first query uses the original ray;
   later queries start at the original-derived radius advance. Previously hit
   full generation-bearing handles cannot be damaged again.
3. Requery prop, vehicle/turret and detached-piece candidates from the original
   complete ray, limited by the next actor's absolute contact fraction. World
   and rubble cover also keep the original complete-ray visibility checks.
   This deliberately conservative adaptation prevents the radius advance from
   jumping over real competing cover, including cover behind an earlier actor.
   It does not synthesize a contact at the new ray origin.
4. Revalidate the winning actor. A blocked/stale contact terminates the sniper
   path. Physical shield and nano consumers run first with their existing
   unattenuated base request and terminate the shot; no body fallthrough occurs.
5. Scale the qualified body's damage using pre-contact power, spend its actual
   retained radius, and prepare the next origin before damage/death callbacks
   can retire or rebuild that owner. Dispatch the existing damage/death path
   once, preserving source attribution, damage-kind factors and impact audio.
   A zero-applied-damage contact still spends power, as the original ordering
   requires.
6. Continue only while power and range remain. At most 32 distinct actor-body
   contacts are consumed. The existing hit-capacity constant and full-handle
   visitation provide a bounded fallback even for zero-radius bodies; no
   fabricated forward epsilon is required.

Props, vehicles/turrets and detached pieces remain terminal contacts. When one
follows earlier actors, its fresh base damage receives the same pre-contact
object-power scaling. No new object-extent estimate is needed because that
first-stage terminal policy never continues past it. Existing world checks,
typed damage and live-owner audio qualifications remain authoritative.

Rail keeps one original full-ray pass, its existing 32 retained NPC hits,
per-prop visitation, detached-piece and vehicle handling, shield continuation,
nano termination and original damage amounts. The new power policy is admitted
only in the sniper branch. Other weapon categories and NPC-fired weapons are
outside this integration.

## Explicit remaining boundaries

World policy `4c9bf0` is not a blanket material pass-through. Without
`pierces_all`, only material 2 (Metal) retains power. Metal first checks the
authored ricochet cosine through `4c9d30`; otherwise exit traversal `4987b0`
must find a real finite back-facing surface within remaining power and spend
the traversed distance. Other materials clear power. Installed sniper authors
45-degree ricochet; rail authors 180 degrees and `pierces_all`.

That exit traversal is distinct from current `498e80`-style visibility rays:
`498740` admits back-facing exit crossings, and `498bd0`/`498d70` visit actual
finite world/mover faces. Reusing the ordinary ordered retained-fraction ray
as a thickness oracle would be incorrect. This slice therefore retains all
world/mover blocking and does not approximate thin metal, ricochet, exit audio
or terrain changes. Those remain a separate missing gameplay feature.

The sniper remains the port's synchronous 100-unit ray with existing sphere
body selection and authored-model shield/prop queries. Exact retail projectile
flight, location-specific body damage, moving-target timing, same-frame
overlapping-body order and x87 bit parity are not claimed. Radius advance can
skip another actor lying wholly inside that advance, as implied by the original
position update; world/fragment/prop/vehicle cover is conservatively retained.

The scheduled original-level startup, if successful, establishes compilation
and mandatory table admission only. It cannot establish multi-actor damage,
radius advance, blocker handling, shield/nano interception or native audio.
Those outcomes remain unverified until an authorized focused gameplay check or
tester feedback exercises them.

## Save compatibility

The appended primary metadata is not part of Remote Charge catalog identity.
RFRM1/2 retain their exact historical 76-byte primary-definition hash prefix,
with a compile-time boundary assertion. No save layout or validation guard is
changed; actual legacy load remains unverified for this build.

## 01:00 UTC compilation status

The October10 Xbox build and original L1S1 neutral120-frame stock64MiB startup
passed at7cdfd7a4. This establishes compilation and table admission only;
action-specific gameplay, physical controller pause, audio output and save
restoration remain unverified. See HOURLY-20261010-0100.md.
