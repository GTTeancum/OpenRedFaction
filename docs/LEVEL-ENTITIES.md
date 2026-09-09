# Level entity records

`rf_level_entities_begin` and `rf_level_entity_next` traverse the installed
v180 entity section (0x30000) without allocating its payload. Each result
contains UID, class/script names, position, reordered orientation, state
animation and skin, plus the section-relative raw record span. Other behavior
fields are traversed to locate the next record but are not interpreted or
applied to gameplay. The class name is not assumed to be a mesh filename.

Strings retained in the public result are bounded to 255 bytes plus terminator;
unretained strings are skipped with section bounds checks. Nonfinite transforms,
invalid optional-range tags, impossible counts and trailing bytes fail. The
iterator and caller result remain unchanged when a record fails. NOT_FOUND
means all declared records consumed exactly the section. The caller keeps the
level/archive open throughout iteration. There is no entity spawning, AI,
inventory, model/skin resolution or game-state initialization here.

The layout lead is Rafal Harabien / wardd64's
[rf-reversed RFL specification](https://github.com/rafalh/rf-reversed/blob/master/rfl.ksy),
published as GPL-3.0-or-later. The downloaded reference on 2026-09-08 has SHA256
`78d4cdf16a66a1fd991405786f553a86e539663c011d8b0eaae43169ab0b9e2b`.
This project adds its own bounded C reader and independent byte-inspection
tool; no generated Kaitai parser is used. Field names are format-reference
annotations, not evidence of original executable loader semantics.

`tools/verify_level_entities.py` reads all installed entity-bearing level
sections and compares every exported field and record span against the compiled
C probe. All 1,610 records in 66 levels pass, with exact section exhaustion.
The report at `artifacts/level-entities-verification.json` includes class counts
and Live Mines placement candidates. In L1S1, miner1 UID 9858 is at approximately
(-92.402, -3.191, 49.761), near the stored player start; the nearest entity is a
Grabber, so proximity alone must not choose the actor's model.

The level tests add orientation-order validation, 155 truncation boundaries,
nonfinite transform, overlong retained string and invalid optional-range-tag
checks. PC/NXDK builds and all four CTest tests pass. Verification covers file
decoding and bounds, not execution of the original entity loader or gameplay.
No new visual output was produced.
