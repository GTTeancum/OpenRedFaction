# Original Spit startup admission correction

Source-only diagnosis of the single completed 09:00 Xbox attempt at
`5d55baeda849e221fa1278aa5b04b26c262317bb`. The build passed, but original
L10S4 neutral 120 startup failed before frame 0 at phase 0x80000102 (RF_FORMAT)
and campaign stage 11. This correction is uncompiled and runtime-unverified;
no repeat, test, syntax check, generated gameplay fixture or emulator execution
was performed. Parent owns the next consolidated 10:00 batch.

## Exact rejection

`campaign_clutter_open` zeroes `rf_scene_weapon_supply`, then loads the weapon
supply, preferences and exact special-NPC profiles. It only publishes the four
supply diagnostic words after every early profile loader succeeds. The observed
four zeros and stage 11 therefore locate this failure before clutter tables or
placed prop admission. No later cleanup clears those four words. RF_FORMAT is
-2; the zero free-pages field is not evidence of allocation failure.

The newly added `campaign_enemy_spit_open` validates Small then Big against the
already loaded reset catalog before it reads their detailed profiles. Both
original rows declare empty `$Flags` and `$Flags2: ("no_fire_through")`.
`rf_weapon_flags_read` assigns the secondary vocabulary by 1 << index, and
`rf_weapon_reset_catalog_read` assigns that result directly to `flags_268`.
The exact Small guard wrongly required 0x20; actual 0x08 deterministically rejects
it. Big contains the same latent mismatch. These loaders run unconditionally,
so the defect is not limited to levels containing Snakes.

Independent read-only PE parsing of original RF.exe confirms the same vocabulary
at VA 0x5a2534: index 3 is `no_fire_through` (0x08), index 4 is `no_world_collide`
(0x10), and index 5 is `undeviating` (0x20). The preceding new Cane gate correctly
requires 0x10 and its original scalar/string row matches its contract. The older
loaders were unchanged since the successful 07:00 source; Sonar still correctly
requires `undeviating | no_fire_through` = 0x28. This is not an original table bug.

## Bounded original-input evidence

- RF.exe SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
- tables.vpp weapons.tbl entry: archive offset 1013760, bytes 108890,
  SHA256 `5f93d97b44342df4d0734cbbd67953569a8abe49967874d1854b11f1b26cb8db`.
  Small begins at entry-relative 85564 (line 2052); Big at 87592 (line 2114).
- Original levels2.vpp L10S4.rfl: archive offset 7331840, bytes 2742205,
  SHA256 `91c25e73b62cc11ca610428954af8a10fb0d7ee63d2987865c74636cba07bf87`.
  Read-only authored decoding retains Rock Snake UID 5255 at RFL offset 2062573,
  explicitly selecting Rock Snake Spit/none, and Big Snake UID 7007 at 2062790,
  explicitly selecting Big Rock Snake Spit/none. Entity section consumption was
  exactly 3641/3641 bytes. No loadout, visibility, event or inventory was changed.
- The same level's clutter section begins at 2066344, with 77 records and exactly
  6923/6923 bytes consumed: two 2PartSwitch, 42 ShopLight01 and 33 Mine Light 5.
  tables.vpp clutter.tbl is at 8192, bytes 164638,
  SHA256 `671a98c34694f29f39b81e646bcfeafa86471b976811afe93c5bad9c0383a2de`.
  No prop or clutter-table rejection is implicated; these later engine stages
  were not reached, and their success is not newly claimed.

## Minimal correction and preserved scope

Change only the exact Spit `flags_268` expectation from 0x20 to 0x08 and correct
the mistaken flag numbers in the two existing Spit documents. A comment records
the original weapon vocabulary and distinguishes the unrelated entity drools-
slime bit 0x20, which remains unchanged. All strict scalar, name, supply, owner,
resource, finite-ammo, save/lifecycle and unsupported-field guards remain intact.
No fallback, skipped loader, suppressed attack, extra grant, altered original
asset, different startup level, memory-limit change or retry is introduced.

The preserved raw result and restoration predicates remain authoritative under
`/workspace/shared/rf-startup-admission-20261010-0900`. This diagnosis explains
that failed admission; it does not turn the failed attempt into a pass or prove
that later startup/resource/gameplay stages will succeed after the correction.
