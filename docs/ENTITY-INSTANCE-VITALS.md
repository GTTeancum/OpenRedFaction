# Authored entity health and armor

`rf_level_entity_vitals_read` projects two finite float32 values from a retained
v180 entity record. Within the 29-byte block after the two relationship-adjacent
strings, health begins at byte 17 and armor at byte 21. Both are little-endian
IEEE floats, in the same units as runtime health/armor. Byte 25 starts FOV.
The eight-byte result contains only `health` and `armor`; existing entity,
spawn, probe-wire and owned-record layouts are unchanged by this addition.

Original executable evidence uses RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- Loader `00464010` reads `local_dc` and `local_e4` through float reader
  `0052c9b0`, each with default `0xbf800000` (-1.0).
- After factory `00422360`, before relationship and weapon instance overrides,
  it compares each value against `00589510`. Direct PE data confirms that
  constant is exactly -1.0f. Exactly -1 inherits the factory value; this is not
  an all-negative sentinel.
- Other values write entity `+34` (health) or `+38` (armor), then call
  `0040a4c0` to clamp against zero and class `+44`/`+48` respectively.
  Thus zero is explicit zero; other negatives clamp to zero; excessive values
  clamp to the corresponding class maximum. The class maximum stays unchanged.

Local decompilation evidence is in
`artifacts/analysis/rf_b8fb9ab4c9bf/464010.c.txt` and `40a4c0.c.txt`.
The reader returns authored values without clamping: scene construction owns
application after factory setup, before publishing damage/vital state.
Nonfinite values are rejected as a port input-validation policy.

A direct read-only scan of all 1,610 installed entity records found:

| Field | Exactly -1 (inherit) | Explicit override | Explicit zero |
| --- | ---: | ---: | ---: |
| Health | 1 | 1609 | 0 |
| Armor | 254 | 1356 | 282 |

All values were finite, with no other negative values. Concrete L1S1 examples:

| UID | Class | Health | Armor |
| ---: | --- | ---: | ---: |
| 8456 | env_guard | 50 | 50 |
| 8462 | env_guard | 100 | 100 |
| 8625 | env_guard | 60 | 70 |
| 8431 | miner1 | 20 | 10 |
| 8367 | Grabber | 30 | 0 |

The reader reuses complete retained-record span validation (including optional
flag and exact exhaustion) without requiring the cached runtime UID to equal
the authored raw UID. DEV seed construction intentionally remaps runtime UIDs
while borrowing original raw records. The public spawn reader retains its
existing UID-equality check. No heap allocation, raw mutation or partial output
publication occurs. Focused `level_tests.c` cases cover shifted string offsets,
inheritance values, zero, fractional/negative/unclamped values, remapped UIDs,
truncation, extra bytes, invalid optional flags and nonfinite input.

## Integration and focused verification

NPC construction now applies retained instance health/armor after class factory setup and before persistent state restoration, preserving class maxima and flags. The lower-first clamp retains the original early-return ordering, including negative class maxima. Player startup is separate and unchanged.

Focused reader and adapter checks pass. A controlled L1S1 fixture uses the same original guard, class, geometry and one real handgun hit with only authored vitals varied: 20/0 yields -20 health and death, 80/0 yields 40 health and survival, and 80/40 yields approximately 60.8 health and survival. All three PC captures were inspected: the fragile guard enters its death pose, while the other two remain standing. These are controlled encounters, not full campaign acceptance. Audio is unverified.

Existing `tests/npc_residency_tests.c::actor_retirement_check` exercises actual capture/restore: damaged 37.5/12.25 overrides fresh values on revisit, survives a cross-level round trip, and dead actors remain retired. This source audit avoids duplicate coverage; it is not a new full-factory live save test.

Native verification: `artifacts/xemu/render-20260922-183849/report.json` passes 90 frames of the fragile guard encounter. PC/Xbox COMBAT, COMBAT_DEATH and WEAPON_DROPS words match, including one fatal player hit and one emitted drop. Native framebuffer inspected for the death pose; 5,244 available pages (20.48 MiB), and the original disc was restored. This verifies emission, not subsequent Riot Stick collection in that encounter.
