# Authored weapon impact-delay metadata

Status: 2026-10-10 02:00 Xbox compilation passed at source 5c506c1a.
Build-only evidence: changed attack/audio/parser execution and save restoration
remain runtime-unverified. No fixture, grant, route, forced event or original
asset change was used. See HOURLY-20261010-0200.md for exact build proof.

## Original evidence

The installed, read-only `RF.exe` establishes two independent delay pairs:

- `004c3d98–004c3dae` clears descriptor offsets `+150`, `+154`, `+158`, and
  `+15c` to zero.
- `004c3db0–004c3de9` repeatedly matches `$Impact Delay:` and stores at most
  two floats at `+150/+154`, retaining authored order. Its literal is at
  `005a2ef4`; its scalar reader is `00512920`.
- `004c3deb–004c3e24` separately matches `$Alt Impact Delay:` and stores at
  most two floats at `+158/+15c`, using its own counter. Its literal is at
  `005a2f04`. Alternate fields are not primary replacements or additional
  primary impacts.

`Installed_Game/tables.vpp` contains `weapons.tbl` at offset 1013760, size
108890 bytes. Direct source inspection, without rewriting/extracting an asset,
confirms Drone Smash at lines 2175–2201: bash damage 1600, Fire Wait 1.5 seconds,
and exactly one `$Impact Delay: 0.4` at line 2194, with no alternate delay.
The table's introductory comments describe these delays in seconds before
applying melee damage or creating a weapon. This is not Fire Wait cadence.

Other original declarations demonstrate why two independent slots matter:
Reeper Claw has primary delays 0.4 and 0.8; Baby Reeper Claw has 0.3 and 0.6;
Mutant Attack 1 has 0.15 and 0.5; Riot Stick has primary 0.15 and 0.6 plus
alternate 0.01. Those observations establish metadata, not new support for
these attacks or proof of damage timing in the reconstructed runtime.

## Isolated API and bounds

`rf_weapon_impact_delays_read` reads one named declaration into a separate
`rf_weapon_impact_delays` record:

- `primary_seconds[2]` and `primary_count` describe repeated `$Impact Delay:`.
- `alt_seconds[2]` and `alt_count` describe repeated `$Alt Impact Delay:`.
- Unauthored slots and counts remain zero. Explicit zero values remain zero
  but increment their authored count; the count is not a pending-hit count.
- Both modes retain declaration order. There is no sorting, conversion to
  milliseconds, primary/alternate fallback, or relationship imposed between a
  delay and Fire Wait.

The reader reuses the existing bounded lexer, ASCII-insensitive matching,
quoted `$Name:` convention, `//` comment handling, and `sphere_number` decimal
parser. Like the existing named weapon readers, it selects the first matching
name and stops at its next `$Name:`, `#End`, or end of input. It does not require
the complete table catalog or validate unrelated weapon fields. The exact
delay labels above are recognized; other Impact/Alt fields are left outside
this subset. No existing lexer, numeric helper, or primary reader is changed.

Explicit bounded-port input policy rejects a third delay in either mode and
negative delay values with `RF_RANGE`. Malformed, quoted, nonfinite, or
float-overflow numeric values reject with `RF_FORMAT` through the existing
decimal parser. This validates representable nonnegative metadata without
inventing a gameplay-specific time maximum. The original's two-slot storage is
the evidence for capacity; stricter rejection is port input policy, not a claim
that the original performed these safety checks. The scalar field syntax is
the existing token-based one, not a new line-sensitive grammar.

A missing named declaration returns `RF_NOT_FOUND`; invalid required pointers
or an empty requested name return `RF_RANGE`. Every failure preserves the
caller's whole output record. Read allocates nothing and retains no input
pointers. `rf_weapon_impact_delays_load` locates `weapons.tbl`, rejects an empty
or over-budget entry, owns one temporary archive-sized allocation, and frees it
after either success or failure. Allocation failure is `RF_IO`, consistent
with neighboring named weapon loaders.

`rf_weapon_primary_definition`, `rf_weapon_descriptor`, existing catalogs, and
all save layouts are unchanged. There are no callers in this slice. A later
Drone consumer must independently own delayed attack identity, cancellation,
three-dimensional contact checks, and save/load publication semantics; see
`NPC-WEAPON-ADMISSION-OPEN.md` for the existing runtime boundary.
