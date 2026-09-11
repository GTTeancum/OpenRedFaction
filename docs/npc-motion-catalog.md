# NPC motion catalog

The campaign now builds `rf_entity_motion_catalog` after base/weapon binding load.
Each shared skeleton has one motion registry; class base and weapon maps refer to
that registry. Per-actor playback remains inactive. This does not add visible NPC
animation, selection, AI or damage behavior.

## Evidence and scope

Original RF.exe SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
The recovered entity factory at 0x422360 visits weapon groups before base mappings.
Cache helpers 0x539be0/0x539d00 compare case-insensitive last-dot stems and preserve
the first acquired name. Registry block 0x51cc42..0x51cc93 keys on resolved motion
identity and the exact loop byte. The catalog composes the existing reconstructed
helpers, processing seed classes in order and their retained weapon groups before
base mappings. Classes sharing the decoded skeleton share registry entries.

This is port ownership and stable IDs, not exact original global IDs or reference
counts: original global cache ordering, descriptor loading, alternate state clips
and the actor selector remain incomplete. Temporary caches are per model. Do not
infer motion identity from the compiled filename, whose conversion uses the first
dot. Identical resources with different loop bytes remain different entries.

## Lifetime and budget

The catalog owns mappings and exact-count motion metadata arrays. Temporary cache,
registry and resource arrays are freed per model. The budget includes the owner,
retained heap allocations and peak temporary allocations, excluding stack and
allocator overhead. Campaign cap is 512 KiB, in addition to existing binding owners.
Sound labels remain in those binding owners; the new catalog does not resolve IDs.
Catalog maps and file metadata survive closing source owners, but the motions
archive and skeleton index order must remain valid. Motion keyframes are still
archive-backed, not resident animation data. Cleanup is repeatable and failures
preserve the empty destination.

## Validation

`tools/verify_base_action_sets.py` independently derives canonical declarations,
registration keys and weapon-before-base maps. L1S1/L1S2/L1S3 produce 248/237/243
shared resources and retain 63004/59948/62160 bytes on PC; peak construction totals
are 123727/123135/103843 bytes. These are PC ABI counts, not Xbox memory readings.
The probe also checks exact and one-byte-short budgets and file access after source
owners close. A synthetic two-class/shared-model fixture verifies case/suffix
aliasing, distinct last-dot identities using the same compiled file, distinct loop
flags and transactional rejection of an invalid mapping. All nine CTest tests pass.
Both PC and NXDK builds pass. Native replay separately checks loading and existing
door/audio behavior; it does not establish NPC animation or exact catalog bytes.

Stock 64 MiB XEMU replay: `artifacts/xemu/replay-20260911-072311/report.json` PASS (180 frames, door/audio fixture).
