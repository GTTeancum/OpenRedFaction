# Future vehicles: common seat attachment

Priority P1, paired with seat release. This helper establishes ownership; it is not the complete gameplay enter/use policy and must not be exposed as unconditional player boarding.

`python tools/future_re/vehicle_seat_attach.py` executes complete427240 in80 cases. Only generic object lookup40a0e0 and audio selection/playback434d00/5056a0 are supplied. Actual seat methods,48a840 player-handle predicate,40a240 turret predicate, vector zeroing4fad00,48acf0 player ownership and movement selector4339d0 execute. Checked RF.exe SHA256 is unchanged (`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`). Results:artifacts/future-vehicles-re/seat-attach.json.

## ABI and behavior

Thiscall: ECX=host, stack arguments occupant handle, seat tag handle; callee pops8 bytes, AL success.

- Scan all host+8cc seat pointers and remember the **last** seat with matching tag+0. This differs from release, which stops at the first matching occupant handle.
- Missing tag, selected seat occupied (occupant+4 !=-1), or unresolved object rejects before ownership mutation. A repeated request for an already seated handle also rejects. A last duplicate tag occupied rejects even when an earlier duplicate tag is empty.
- It does not inspect or release the incoming object's existing host. The fixture begins with host99 and successful attach overwrites it with new host66. The higher-level caller must own legal transfer/eligibility.
- On success, actor+204 receives the selected tag; actor+200 receives host handle+2c; selected seat+4 receives actor handle.
- Always select attach sound via434d00(host class+114,0), then5056a0(selected,host published position+3c,1.0,global173c378,0). Actual audio output is not tested.
- If48a840(occupant handle) is true and40a240(host) identifies turret class flag0x2000, set host byte720=1. Other cases leave it unchanged; fixture initializes it0.
- Zero occupant linear velocity+144 and angular velocity+150 through actual4fad00.
- If48acf0 finds a type0 object with nonnull player pointer1430, request movement descriptor10 and assign actor+858. Disabled descriptor10 falls back0. This shared attachment helper uses mode10 even for non-turret hosts; the outer vehicle boarding routine is responsible for any subsequent vehicle-specific mode/control policy.
- Returntrue. No camera or position publication occurs directly in this helper.

Fixtures cover tag absence, ordinary/duplicate tags, occupied/duplicate self seats, stale object, player/nonplayer, turret/non-turret, enabled/disabled descriptor10, existing old host overwrite, exact host/tag/seat words and velocity zeroing.

## Campaign and implementation entry points

Authored Driller01 UID8122 in L1S2 and APC UID9627 in L1S3 use-kind1 examples are established in EXIT-ADMISSION report. Their actual seat tags/seat counts still need extraction. Attach sound is class field114; detach sound118, already parsed in class ownership. Do not invent a single seat for all vehicle types: Jeep driver/gunner predicates already distinguish occupancy slots.

A live owner can expose this bounded seat mutation behind scene use dispatch, using stable handles and a caller-owned seat list. It must compose higher-level eligibility, position/orientation placement, weapon/camera/control handoff and vehicle-specific movement, not merely call the helper. Existing entity predicates can consume a synchronized compact view of this owner. Original failure/effect order should remain explicit; no fake completion of missing services.

Next RE: higher-level427240 callers, control/camera and enter/exit events; full4279d0 commit tail. No shared source edits/builds/emulator runs.
