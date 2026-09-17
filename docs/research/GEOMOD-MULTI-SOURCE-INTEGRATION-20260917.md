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
