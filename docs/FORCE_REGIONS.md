# Authored force regions

The v180 section `0x1100` reader follows original PC function `462f60`
(RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`).
Each record contains UID, name, position, disk-order matrix, label, one header
byte, shape, extent, strength and flags. Shape 1 stores one radius; shapes 2
and 3 store three size components. Names and the header byte are retained
without assigning runtime behavior. Original construction discards them.

`rf_level_force_next` commits the cursor and output only after a complete
record, and requires exact section exhaustion. Unknown shapes, nonfinite
floats and truncated data fail. Strings are limited to 255 non-NUL bytes.
`rf_level_owned_forces_open` copies records in authored order into one
budgeted allocation, independent of the source archive after return.

`inspect_force_regions.py` inventories 142 records across 27 levels.
`verify_force_reader.py` compares every C record byte with that disk inventory,
checks each record truncated by one byte, exact budgets and archive-independent
lifetime. Maximum retained allocation on the tested PC build is 10,812 bytes,
including the owner and excluding allocator overhead. NXDK compiles and links
the reader; this does not establish native Xbox execution of its ownership API.

Runtime selection was independently verified by `verify_force_region_select.py`.
Authored-to-runtime bounds construction, force application and alternate
airborne speed-cap ownership remain to be connected and verified. The reader
comparison is not execution of the original parser or a gameplay fidelity test.
