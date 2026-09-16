# Partial-room collision composition — 2026-09-16

Implemented `include/rf/collision_composition.h`, `src/core/collision_composition.c`, focused `tests/collision_composition_tests.c`, and real-asset `tools/geomod_post_collision_composition_probe.c`. This is reconstruction ownership infrastructure using existing collision primitives, not a newly recovered original executable routine.

## Scene protocol

Open with the immutable original room tree, one compiled ID per **base tree-order face** (the room tree's existing source_indices), exact replaced IDs149..152, merged capacity1024 and a separate budget. Owner validates IDs/unique replacement membership, retains a borrowed reset tree, and copies maps. Base faces/filters/vertices remain unchanged. Retained descriptors are restored to ascending compiled ID order before appending publication faces.

Prepare with publication collision faces and explicit compiled metadata references (149..152 for retained post,171 for exposed floor, an intentional reference for generated crater surfaces). It copies descriptors and IDs, builds a complete pending tree including all unselected original faces, and preserves the active tree on capacity, allocation, budget or malformed-bound failure. Vertex positions remain borrowed and must outlive whichever active/pending tree uses them.

Call pending_get, then `rf_geometry_collision_overlay_bind(pending.tree,pending.face_ids,pending.count)`, then commit. Overlay_bind consumes **source-order metadata IDs** and applies the candidate tree's permutation. On bind failure, abort the candidate. Commit frees the old tree only after external users have rebound; it allocates nothing. Reset uses the same prepare_reset/pending/bind/commit protocol, borrowing the original geometry with an identity source-index permutation. Close the overlay before closing its currently bound composition owner, and close the owner before original room geometry.

Optional allocator callbacks apply only to owner and temporary composition workspace allocations; default core tree allocation remains unchanged. Callback/context lifetime must cover the owner. This supports deterministic local failure testing without global allocation mutation.

## Budget and validation

Budget conservatively includes owner/maps, original borrowed base tree allocated_bytes, current owned tree, pending tree and temporary face/build scratch. It excludes separately owned geometry/vertex arrays, room descriptors, render resources and allocator overhead, which scene accounting must include against stock64MiB. The base-tree charge may conservatively duplicate a global accounting entry; do not accidentally subtract the actual allocation from both owners.

Real installed ctf06 room3, using `post-published-core.rgm` from the actual source publication module:

- Original790 faces, remove exactly4, add35 ->821faces; all82 liquid faces retained.
- Centerline through post becomes clear; upper post still reports149.
- Floor revealed under the hole reports171 atY-1.25.
- Existing floor171 and water6656 collision hit structures and metadata IDs are bit-identical before/after; filters and original vertex pointers remain owned by the immutable base.
- Reset restores790 faces and the original post contact.
- First candidate resident288524 bytes, peak417421; while another replacement is pending over a committed replacement, resident426448, peak555345 bytes.1MiB composition budget passes; publication/core/vertex/atlas allocations remain additional.

Focused self-contained tests also pass real overlay binding and liquid-view updates, unchanged floor/water filters, ordinary liquid exclusion, abort, capacity/budget rejection, malformed-face failure, deterministic owner/work allocation failure and reset. Failed prepares preserve generation/current tree and leak no custom-owned allocations. The core's internal malloc-failure branch is propagated but was not independently fault-injected; the deterministic failure case occurs at the composition workspace allocation.

Results: `artifacts/future-vehicles-re/collision-composition-tests.txt`, `post-collision-composition.txt`. Builds were isolated objects/executables against the existing Release library; no full build or emulator launch. Primary owns CMake/NXDK registration and scene integration.
