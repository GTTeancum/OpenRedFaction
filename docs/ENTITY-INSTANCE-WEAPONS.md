# Authored entity weapon overrides

The v180 entity record's seven-string block (after the 29-byte life/armor/FOV
block) starts with primary and secondary weapon names. These are now retained
as `rf_level_entity.primary_weapon` and `secondary_weapon`. Parsing preserves
the strings without resolving IDs or changing inventory. Empty names preserve
class defaults; `none` is an explicit category disable, not a missing value.

Evidence is the local original RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`,
loader `00464010` (local decompilation
`artifacts/analysis/rf_b8fb9ab4c9bf/464010.c.txt`). Its first two strings
`local_f8`/`local_c8` resolve through `004c81f0`, grant through `004030d0`,
and assign entity offsets `+2a4`/`+2a8`. Literal strings at `0059c9ac` and
`0059c9cc` are both `none`, confirmed directly from PE data.

The loader retains defaults for unresolved names, aliases primary `glock` to
`12mm handgun`, and grants valid overrides on top of existing inventory.
Primary overrides fill the corresponding ammo reserve to the weapon's authored
capacity; Rocket Launcher (global `00872458`) receives reserve 3 instead.
Initialization at `004c6570` resolves that global from `Rocket Launcher`
(`artifacts/analysis/rf_b8fb9ab4c9bf/4c659f.c.txt`). Grenade uses normal capacity.
Secondary overrides do not perform that extra reserve fill. Explicit `none`
clears ownership in the corresponding category and sets its selection to -1.
The class melee grant precedes the overrides. The scene now applies these
semantics after class startup and rebinds animation when the selected primary
changes. Model-less classes preserve the existing startup binding policy.

A read-only direct scan of all 1,610 installed entity records found 23 primary
`Rocket Launcher` and 15 primary `Grenade` strings. Concrete authored examples:

| Level | UID | Class | Primary | Secondary |
| --- | ---: | --- | --- | --- |
| L1S1.rfl | 9428 | miner1 | Rocket Launcher | none |
| L1S2.rfl | 9712 | env_guard | Rocket Launcher | none |
| L10S2.rfl | 6800 | env_guard | Grenade | none |
| L10S2.rfl | 6801 | env_guard | Rocket Launcher | none |
| L19S3.rfl | 12078 | merc_grunt | Grenade | none |

The two fixed strings add 512 bytes per retained entity, without extra
allocations. Existing offsets through the raw span remain unchanged. The
diagnostic entity probe retains its historical 1,084-byte output prefix;
the in-memory record is now 1,596 bytes. `level_tests.c` covers primary,
secondary, empty and explicit-none strings, archive-independent ownership,
and failure atomicity on truncation. No original-game execution or screenshot
reference was used for this change.
