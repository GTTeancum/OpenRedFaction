# Future vehicles: seat release ownership

Priority P1: implement together with exit clearance and possession handoff. This is original-code evidence, not current playable vehicle support.

`python tools/future_re/vehicle_seat_release.py` executes complete427380 for64 cases, with object lookup40a0e0 and audio selection/playback434d00/5056a0 supplied. Actual seat-array methods40a480/40a490, player ownership predicate48acf0 and movement selector4339d0 execute. Checked RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. Results:seat-release.json; disassembly:427380.disasm.txt.

## ABI and retained state

Thiscall: ECX=host, stack arguments occupant handle and play-detach-sound flag. Callee pops8 bytes. AL reports success. Host+8cc is an array owner: count at+0, pointer to seat pointers at+8. Each seat record is8 bytes (tag handle+0, occupant handle+4); empty occupant is-1. This matches existing reference EntityInterfacePoint and prior occupant predicates.

- Handle-1 or no matching seat returnsfalse without effects.
- Iterate in seat order, selecting only the first matching handle. Duplicates later in the list remain untouched; array count/storage are not compacted.
- Set that seat's occupant to-1 **before** resolving the object through40a0e0. A stale handle therefore returnsfalse after clearing the seat. This is an original partial mutation, not atomic failure. Do not silently claim original rollback semantics.
- A resolved object's host handle+200 becomes-1.
- If sound flag is nonzero,434d00(host class+118,0) selects the detach sound, then5056a0(selected,host published position+3c,1.0,global173c378,0) plays it. The fixture records arguments/order; actual audio services and audibility are excluded.
- Host byte+720 becomes0, after the optional sound operations.
-48acf0 succeeds when resolved object type+24 is0 and player pointer+1430 is nonzero. Only then the routine requests movement descriptor1 through4339d0 and stores its pointer at actor+858. Existing movement selector semantics apply: disabled descriptor1 falls back to descriptor0. No player object must be dereferenced here.
- Returntrue after this sequence.

Fixtures include invalid/missing handles, first/second and duplicate seat matches, stale object, sound flag off/on, nonplayer/player and enabled/disabled requested movement. Actor host link and mode pointer, seat words, host720 and ordered calls are checked.

## Campaign and implementation handoff

Authored L1S2 Driller01 UID8122 and L1S3 APC UID9627 have use-kind1 and movement index5 (archive probe evidence in EXIT-ADMISSION report). Their seat records must be reconstructed from actual interface points before a usable board/exit loop exists; use-kind alone is not occupancy.

The existing compact entity view's occupant handle list is sufficient for read-only predicates, but a live vehicle owner needs stable mutable seat records and actor host linkage. Shared helper should take caller-owned seat storage plus lookup/audio/movement services; scene owns player/control publication. Do not use a raw RF.exe struct dump. The collision-admitted exit caller4279d0 calls this function with sound1 but ignores its boolean return before continuing other cleanup; that orchestration is the next boundary.

Remaining uncertainties: precise gameplay meaning of host720 beyond this observed reset; live audio policy; full board assignment, enter/exit event veto, player camera/control and vehicle weapon release. No source implementation edits, builds or emulator runs performed.
