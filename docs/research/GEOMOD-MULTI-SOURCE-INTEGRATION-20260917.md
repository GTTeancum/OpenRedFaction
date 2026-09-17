# Multiple live authored sources: integration boundaries

Current verified runtime supports one selected ctf06 source93/94/96/97. Multiple concurrent source ownership is still unimplemented. This document records source inspection, not acceptance of a new architecture.

## Required shared ownership

`scene_stream` currently owns one terrain, authored asset, detached registry, history, publication, lighting atlas, draw mesh and collision overlay. `scene_terrain_publication_open` creates a composition from the original room tree and one source replacement list. Independently binding one overlay per post would overwrite the previous room replacement rather than compose both edits. Independently rebuilding each post from the original room would restore earlier holes in collision.

Use one room-level composition and retain the unchanged room faces exactly. Its replacement identity set must cover the union of participating source windows. Unedited sources must contribute their original windows until edited; otherwise registration itself creates invisible collision holes. Aggregate the current published geometry for all sources before binding one candidate room tree. Common atlas, draw/publication capacity and temporary work must have a shared ceiling; do not multiply the current whole-scene allocations by four.

Each source needs its own immutable UID, digest, source planes/neighborhood, cut history, active terrain and piece identity namespace. Each blast must identify all affected sources through actual geometry/eligibility. Retain original fragment-fragment exclusion; new sources do not justify enabling fragment stacking. Source extraction must account for current support geometry where two edited sources share a beam/floor; static neighbor snapshots alone cannot establish general interacting-source correctness.

## Transactions and saves

Prepare every changed source plus aggregate room publication privately. Allocation, geometry, lighting or binding failure must retain all active source histories, piece registries and the old room tree. Publish only after the complete candidate is valid. Abort frees all newly staged owners. Retired-piece collection must preserve stable identities and cannot free borrowed geometry beneath the active draw/collision views.

The existing RFCP single-source envelope binds one source digest and replay history. Multiple-source saves need explicit source count/UID/digest records, independent histories and unambiguous piece references; validate the entire candidate before restoring the player or publishing any geometry. Keep legacy one-source saves tied to their original94 identity. Merely relaxing identity checks would load geometry against the wrong immutable source.

## Memory evidence

Native selected97 run `artifacts/xemu/render-20260917-105421` used stock67108864 bytes with no added RAM. At the endpoint it reported3998 available pages. Publication reported1596336 resident/1721151 peak bytes. The existing GeoMod aggregate reported12138184 peak against13631488 ceiling; these telemetry categories are not additive heap totals. This is one small cut, not worst-case multi-source capacity evidence.

## Acceptance sequence

Cut two different posts in one live session, revisit the first, and confirm both visible holes and collision remain. Save after both cuts; compare resumed state with uninterrupted control, including source-indexed piece ownership. Retire rubble from one source, then extract new rubble from another without losing the first source's tombstones. Exercise budget/allocator failure while a second source stages and confirm all prior geometry and saves remain unchanged. Verify on stock64MiB XEMU and inspect native output before claiming multi-source gameplay acceptance.

## Implemented grouped collision candidate

`rf_collision_composition_prepare_groups` now accepts a complete set of source publications for one room. Each group declares its subset of the owner's replaced compiled IDs and its current collision faces/metadata. The subsets must partition the registered IDs exactly once. Missing, duplicate or unknown ownership rejects before allocation/publication. An unedited source supplies original windows; a removed source supplies zero faces. All groups are current state, not incremental patches.

The implementation concatenates groups directly into the existing single bounded tree-build scratch allocation; it adds no resident owner arrays or extra tree per source. Old single-source prepare calls delegate through one group, preserving that contract. Vertex borrow lifetime and bind-before-commit ordering remain unchanged. The owner still retains original unregistered room faces with their exact descriptors and filters.

The core fixture first cuts an east-facing source while preserving the west-facing source. It then removes the west source and checks both openings, the east upper remnant, floor height, liquid height and solid-query water exclusion. Omitted/duplicate/unknown ownership, injected allocation failure and an aborted candidate leave the active generation and queried geometry unchanged. The grouped fixture reports4126 peak bytes; this small synthetic fixture does not predict full-room memory usage.

PC build and121/121 tests pass; stock NXDK build passes with the existing .edata merge warning. These are core collision tests, not simultaneous live-post acceptance. Scene source ownership, grouped rendering/lightmaps, cross-source support, multiple-source checkpoints and stock Xbox runtime remain the next integration requirements.

## Implemented grouped authored publication

`rf_geomod_publication_build_groups` aggregates1..32 independently evaluated source jobs using the existing single work buffer and aggregate vertex/face ceilings. Every source has a unique crater owner; the room revision is explicit. The existing single-source builder shares the same append/copy implementation. No additional owner allocation or per-source work buffer is introduced.

All jobs finish inside the work buffer before output publication. A malformed later job, duplicate source owner or aggregate capacity failure preserves all caller output arrays and the output view. Source interactions remain caller-owned: this API does not solve cross-source Boolean operations or mutable neighboring support by concatenation.

The installed ctf06 integration test opens real sources93 and94 together. Uncut publication has8 faces/32 vertices. After cutting93, aggregate output has39 faces/166 vertices and94 retains its four original windows. After cutting94, output has70 faces/300 vertices. The first source's150 vertices,35 faces and35 provenance records remain byte-identical after the second cut. The test checks output rollback for insufficient face capacity, duplicate source UID and invalid planes in the second job.

PC build and121/121 tests pass; stock-profile NXDK compilation passes. These are production publication calls exercised with real assets, not live scene acceptance. The scene still needs multiple retained source owners, aggregate drawing/lightmap binding, impact dispatch and source-indexed saves. No new visual or native gameplay claim is made for grouped publication.

## Grouped terrain and rubble transaction

The scene edit transaction now stages up to four independently owned terrain/registry pairs under one room publication callback. Existing single-source edits call this same implementation through a one-source adapter. Source callbacks only create and mutate; their individual publication callbacks are not called by a grouped transaction.

The reservation charges every old core and clone budget, one reused history scratch buffer and the caller's total external reservation. All clones and staged registries must succeed before room publication. The one successful room callback is the commit point: source pointers, registries and serial then commit without fallible work. Duplicate terrain or registry ownership is rejected before mutation. Abort discards every begun registry and candidate. The output result array and shared serial change only on success.

Grouped transaction tests exercise failure creating the second source, failure after mutating it and failure at aggregate publication. Both original cores and empty registry ownership/memory remain unchanged. Successful two-source commits create independent rubble owners; a later clone/edit retains moved body positions123 and124 in their respective batches. Duplicate source/registry and insufficient aggregate budget controls reject. These are synthetic real-core transaction fixtures, not concurrent live-post rendering.

PC build and121/121 tests pass; stock NXDK build passes. A fresh550-frame PC97 rocket replay through the new shared transaction produces the unchanged2758-byte checkpoint, SHA256 d98a3c0e8b1c725e4924aaf82f7872e99ede438963d3b0e342f892a884f4aea0 (`artifacts/grouped-transaction-live`). This establishes existing live-state compatibility; no new native or multi-source visual acceptance is claimed.

Remaining scene work is source collection ownership, aggregate drawing/lightmap references, all-affected-source impact dispatch, queries over every detached registry and source-indexed save restoration. Do not report grouped live gameplay as complete until those paths use this transaction and pass the acceptance sequence above.
