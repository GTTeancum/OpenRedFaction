# Fusion direct object contacts

Integrated from `8baec9559c2ab5718d0cdbc573a726aa1f27f1ee` on 2026-10-10
after independent source review. The 07:00 Xbox compilation is pending;
no builds, tests, emulator, fixtures, routes, grants or original-input changes.
Action runtime remains unverified.

## Original evidence

`RF.exe` SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Object contact `0x4c4c35` enters `0x4c59f0`; fresh damage at
`0x4c5cfc`–`0x4c5d38` reaches direct dispatch `0x4c6132` using descriptor
`+0x52c` kind and retained source/target. Independent radial preparation/call is
`0x4c62b5`–`0x4c62f5`. Kind-4 object dispatch `0x4895bc`→`0x489532` calls typed
clutter `0x410270` at `0x489547`, applying factor/health at `0x410288`–`0x41029c`.
Authored `shoulder_cannon`: Damage 1000, explosive kind 3, radius 25, nonsticky,
nonpiercing/nonremote. Runtime continues using its existing loaded shot values.

## Bounded wiring and ordering

The shared sweep already selects vehicle/turret and retained-prop contacts.
Fusion now calls `scene_driller_projectile_damage` with retained contact,
source/damage, kind 3 and frame; only unhandled tags fall through to
`scene_clutter_projectile_damage`. Existing helpers retain generation,
protection, typed-factor and retirement semantics.

The block follows Nano consumption, visual and existing actor/detached damage,
then leaves the independent radial blast and terrain request unchanged. Handled
never skips the blast; disjoint tags prevent another actor/detached direct hit.
The [clutter namespace prerequisite](CLUTTER-PROJECTILE-NAMESPACE.md) was
integrated first at `9fd64748`, including explicit grenade turret admission.

Contact storage is call-local; existing retirement callbacks cannot invalidate
it. Prop break ownership stays deferred, and radial damage uses current owners.
No new callback implementation, allocation, metadata, save row, flight/ammo/cadence change.
