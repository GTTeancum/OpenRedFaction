# Distinct retained-prop projectile tags

Integrated from `8baec9559c2ab5718d0cdbc573a726aa1f27f1ee` on 2026-10-10
after independent source review. The 07:00 Xbox compilation is pending;
no builds, tests, emulator, fixtures or original-input changes. Runtime unverified.

## Two-line prerequisite

Clutter slot 0 and turrets both used `0x04000000u`. The driller helper consumes
recognized stale turret tags, so handled-gated chains skipped prop slot 0.
Move only the clutter macro to unused projectile namespace `0x02000000u`.
Explicitly admit `SCENE_TURRET_PROJECTILE_OWNER` in `scene_grenade_object_contact`
to preserve turret object consumption previously supplied accidentally by the
alias. All sweeps, tie order, owner generations and damage policies stay intact.

## Source compatibility review

- Clutter production/damage, grenade classification, Laser classification and
  existing tests use the macro. Other scene projectile namespaces are distinct;
  raw hex/decimal matches elsewhere are unrelated flags, data or sizes.
- Grenade contact is included by grenade gameplay under `scene.c`, where the
  turret macro is already defined. Existing Fusion/clutter tests include
  `scene.c`; no standalone contact stub needs editing.
- Vehicle body-collision consumers publish `contact.face` (full owner handle),
  not the synthetic projectile tag. Diagnostic tokens can naturally change;
  no new telemetry field or schema is added.
- RFAP saves flight state or grenade contact fraction/normal, not object/face;
  Flame canister rows do the same. Remote saves remap only actor/mover bound
  tags and reject unknown attached tags, including props. No saved contact-tag
  migration, ABI, save-layout, allocation or retained metadata change is needed.

Apply this prerequisite before [Fusion direct contacts](FUSION-DIRECT-CONTACT.md).
