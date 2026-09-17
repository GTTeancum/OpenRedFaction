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
