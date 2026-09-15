# Original crater-history ownership and capacity (2026-09-15)

The duplicate ledger records **admitted/queued requests**, not successful terrain edits. Do not evict old entries arbitrarily or add entries only after successful CSG while claiming original behavior.

## Lifetime and ordering

Inspected original467020 sequence:46727a calls437230 to queue an event;467290 onward writes the32-byte record at648600[count], including template16-bit, flags16-bit, room32-bit, compressed position, orientation/normal and scale;46736f increments647c9c. Boolean/CSG runs later through the queued state machine. There is no check of a CSG result before this increment. Thus a later unsuccessful/no-op boolean request can already have entered history.

Linear executable instruction inventory found direct count mutations at466af5(reset),46736f(increment),4b4aba(restore). No timed eviction/decrement was found. This is a scoped direct-reference audit, not a proof against all indirect memory writes.

`tools/verify_geomod_terrain_history.py` executes original466aa3..466afb initializer after texture-loading, using the original list-initialization loop: seeded counts1/8/128 all become0. The containing initializer466a90 is called at435b3e among level subsystem initializations. Exact enclosing lifecycle naming was not dynamically established.

Original save-state restoration4b4aa5..4b4ad0 copies a byte count from state+1871b into647c9c and count*32 bytes from state+17388 into648600. Four executed cases0/1/8/128 restore all bytes exactly. It then calls4674b0 to reconstruct/replay state (not executed here). Script/results: tools/verify_geomod_terrain_history.py and terrain-history.json.

## Capacity

At46726a, count>=128 branches to467375, skipping queue creation, history writing, and count increment. The subsequent debris/audio path can still run, and normal function exit sets AL=1. The history does not evict an old record to make space. Consequently the master function returning true is not sufficient evidence that terrain work was queued. The port may reasonably expose a clearer capacity status, but should retain a bounded128-entry no-eviction history if matching this admission behavior.

## Scope of previous gates

Within467020, requested-radius<1 is checked before hardness or driller scaling and has no template/flag exemption. Duplicate scanning is also unconditional after the shape/room/hardness placement setup and keyed to same template and room; radius, orientation and request flags are not compared. This applies to requests passing through467020, not necessarily editor operations, save reconstruction or direct CSG entrypoints. Existing record centers are decoded using current solid bounds through4b5900; they are not stored as float XYZ.

4b5820 packs XYZ into three uint16 values using the solid bounds, and4b5900 reconstructs them. The prior18-case duplicate harness supplied already decoded world positions at that boundary; quantization itself is not verified by it. A precise integration must either retain verified compressed representation or explicitly label a float-coordinate approximation, particularly at the0.2-distance threshold.

All evidence applies to RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No source build, emulator run, or shared implementation changes were performed.

Primary review: retained the executable probes under tools/ and reran them successfully. These prove the scoped original CPU behavior, not live port or visual parity.
