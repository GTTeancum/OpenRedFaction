# Trigger layout and authored door links

Evidence targets the installed RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Original level dispatch 460e1e calls 462150 for event section 0x600;
4610cc calls 465510 for trigger section 0x60000. Both functions are now included
in the Ghidra baseline export. This is reader/layout evidence; runtime trigger
eligibility and event dispatch have not been reconstructed.

`tools/inspect_triggers.py` follows 465510's v180 read order with bounded reads,
finite-float checks, shape validation and exact section exhaustion. It parses
2,367 triggers and 4,471 ordered links across 93 levels. Raw field names remain
provisional. In particular, `enabled_byte` is a preserved byte with unverified
semantics, not a usable runtime enabled flag. It is zero on the Live Mines door
triggers. Version-dependent pre-v180 branches are not supported.

The serialized record contains UID, name, a byte, shape, float timing (v134+
loader converts this to int), a word, flag/script/byte fields, shape geometry,
trailing configuration fields, and an ordered UID list. Shape zero reads a
position and radius; shape one reads position, nine matrix floats, three
dimensions and another flag. Matrix and dimension order remain disk order.
Original 52d640 stores only AL at 52d664, confirming that its field is one byte,
despite the decompiler's local uint declaration. No inference is made from the
uninitialized box-only local flag in the original sphere branch.

Live Mines has 61 triggers. Cross-referencing their link UIDs against the
already inventoried moving-group key UIDs gives these authored door pairs:

| Trigger UID | Ordered links | Matched controller keys / attached movers | Other links |
| --- | --- | --- | --- |
| 8542 | 8593, 8591, 9826 | Door Out 01b key 0 → mover 8543; Door Out 01a key 0 → mover 8544 | 9826 |
| 8522 | 8694, 8603, 8605, 8695 | Door Out 02a key 0 → mover 8524; Door Out 02b key 0 → mover 8523 | 8694, 8695 |

Both are records named `Trigger Door`, shape one, with dimensions [7,18,10]
in disk order and raw timing 1.0. Trigger 8542 is centered near the inspected
door at [-49.640572,-7.125,-18.602852]. Trigger 8522 is centered at
[-40.494873,-0.125,47.600555]. Trigger 8994 has no links, and trigger 20 points
to key 19 of the crate-lid rotation group. These are authored references, not
proof of runtime activation conditions or a reason to discard the other UIDs.

The cross-reference retains link order and all candidate matches, including
non-first keys or duplicate UID matches in other levels. `other_links` means
only that the UID did not match a moving-group key; event/entity resolution
remains open. Reports live in ignored `artifacts/triggers.json`.

Next: implement and verify the C trigger reader, recover UID-to-runtime-target
resolution and trigger activation, then parse the other event links. Preserve
the complete ordered list when dispatching; do not activate all door groups
globally or silently drop unresolved links. The current visible door test still
uses explicit simultaneous activation of all four translation controllers.
