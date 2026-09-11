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
Force application and alternate airborne speed-cap ownership remain to be
connected and verified. The reader
comparison is not execution of the original parser or a gameplay fidelity test.

`rf_physics_force_region_build` now converts an authored record to the 108-byte
runtime layout. It rotates the disk matrix rows into runtime order, computes
bounds and squared radius, preserves flags/strength and sets activation to one.
The apparent vector operation at `40a3f0` is negation, confirmed by disassembly;
the oriented-box path passes negative and positive half extents to `539a40`.

`verify_force_build.py` executes original prepared construction blocks
`46306a`, `4630d8` and `463167` through `4631b1`, retaining their vector and
bounds callees. All 108 output bytes match PC and NXDK for all 142 authored
records (1 sphere, 2 axis boxes, 139 oriented boxes). File reads and prior field
writes are supplied at the boundary; this is not a complete loader execution.
World registration and force application remain open.
