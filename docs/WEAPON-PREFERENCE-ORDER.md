# Authored weapon preference order

Source-written on 2026-10-09. The independent reader and archive loader are
implemented; compilation and runtime verification remain for the parent's
scheduled Xbox batch. No helper build, test, fixture or emulator run was made.

## Original-game evidence

The installed read-only `Installed_Game/RF.exe` and its `tables.vpp` are the
authority for this reconstruction.

- `4c6801..4c6814` initializes all 32 entries at `7c710c` to `-1` before
  reading weapon declarations. Empty positions remain empty; there is no
  sorting, compaction or fallback insertion.
- `4c6835..4c6881` reads primary declarations in stable weapon-ID order through
  `4c2b60`; `4c6881..4c68d9` reads secondaries through `4c4850` and records the
  primary/secondary boundary at `87211c`.
- `4c2dce..4c2de6` reads the original Flags vocabulary. Its `player_wep` bit is
  `0x80`; `4c2e1d` gates the required player-only fields. `4c2e35..4c2e55`
  reads `$Pref Position:` via integer parser `512750`, placing the value at
  descriptor `+534`. The authored comment says position zero is highest.
- `4c2ba2..4c2bad` gates insertion on primary descriptor `+264 & 0x80`.
  `4c2c06..4c2c15` requires a signed position in `[0,31]`, despite the older
  table comment saying less than 16. `4c2c15..4c2c35` asserts that the target
  is still `-1`; duplicate positions are errors, not first/last-write wins.
  `4c2c37` stores the stable weapon ID at that exact position.
- `4c4850..4c489f` parses secondary descriptors using the same parser but
  does not insert them into the preference map, even if player-only fields
  are authored there.
- `4a23a0..4a23be` copies exactly 32 integers to player `+1154`.
  Existing `rf_weapon_choose_available` reconstructs `4a6e50` and scans these
  positions from zero upwards; `rf_weapon_decide_empty` owns depletion policy.

## Reader and admission contract

`rf_weapon_preference_read(text, bytes, names, preference)` takes the existing
immutable `rf_weapon_names` catalog and writes 32 `int32_t` IDs only after
the entire bounded pass succeeds. It allocates nothing. The retained output
is 128 bytes, with fixed-size stack workspace; there is no duplicate full
catalog or descriptor allocation.

`rf_weapon_preference_load(tables, scratch_budget, names, preference)` reads
`weapons.tbl` using one temporary allocation no larger than the caller's
budget, releases it on every read/parser result, and preserves output on
failure. Allocation failure is `RF_IO`; null/invalid arguments or an
over-budget table return `RF_RANGE`.

Both delimited sections, their order, every declaration name and the primary
boundary must match the supplied catalog. Names are compared with the existing
ASCII-insensitive convention at their exact ordinal; lookup cannot accidentally
map a later same-named declaration to an earlier ID. The catalog itself is
unchanged. Counts are bounded to 64 and supplied names must be terminated
within their 64-byte fields.

Every declaration requires one `$Flags:` field using the existing original
vocabulary reader. `$Pref Position:` must occur exactly once if and only if
`player_wep` is present. Only primary player weapons enter the result. A
secondary's syntactically valid integer preference is discarded, just as the
original secondary wrapper does not insert it. Secondary values do not claim
slots or produce collisions. Primary positions outside `[0,31]` return
`RF_RANGE`; duplicate claimed positions return `RF_FORMAT`.

Missing/duplicate selected fields, malformed integers/flag lists, catalog or
section mismatches, truncated input and overlong bounded tokens fail without
publishing any partial output. The existing integer reader rejects overflow
and malformed numeric suffixes rather than reproducing unsafe arithmetic.
Unknown unrelated fields are skipped: this is a bounded preference subset,
not a new full weapons-table validator. Duplicate names remain separate stable
declarations, and repeated flag names retain the existing bitwise-OR behavior.

This API does not alter `rf_weapon_supply_catalog`, its hashing, weapon/save
layouts, inventory, ownership, ammo or automatic-switch policy. Callers retain
this map separately and pass it to the existing depletion selector; they must
not manufacture an order if authored loading fails.

## Installed authored map

The installed table contains 40 primaries and four secondaries. Its 15 primary
player weapons produce the following map, directly from `$Pref Position:`.

| Position | Weapon ID | Authored name |
| --- | --- | --- |
| 0 | 17 | shoulder_cannon |
| 1 | 7 | Rocket Launcher |
| 2 | 15 | heavy_machine_gun |
| 3 | 14 | rail_gun |
| 4 | 16 | scope_assault_rifle |
| 5 | 5 | Shotgun |
| 6 | 6 | Sniper Rifle |
| 7 | 8 | Assault Rifle |
| 8 | 12 | Flamethrower |
| 9 | 9 | Machine Pistol |
| 10 | 11 | Grenade |
| 11 | 3 | 12mm handgun |
| 12 | 2 | Riot Stick |
| 13 | 0 | Remote Charge |
| 14 | 13 | riot shield |
| 15–31 | -1 | Empty |

The Remote Charge Detonator, Undercover handgun and Machine Pistol Special
lack `player_wep` and have no authored preference position. They are not
silently added to the order.
