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

## Scene publication collection wiring

`scene_stream` now has a source-owner collection view (authored assets, terrain and registry per entry). The existing scene publication adapter accepts up to four entries, validates their common room, copies/merges up to256 immutable binding references, and registers the union of their replaced compiled face IDs in one collision composition. Conflicting shared reference records reject. The existing no-collection path retains single-source behavior.

Preparation uses the selected private candidate and the other owners' current terrain histories to build one grouped mesh with a shared room revision. Intact sources publish their original windows instead of taking the single-source whole-room reset shortcut. The bank carries total cut count and resolves all source charts from the merged reference table. All sources continue to share the existing bounded publication work, double banks, collision composition and overlay.

The actual scene adapter test opens real ctf06 sources93/94, initially publishes8 windows and confirms collision at both posts. Sequential cuts produce39 then70 publication faces. Rays through both cuts miss after the second commit; the full room tree has852 faces (790 original minus8 replaced plus70 published). A staged candidate followed by abort retains both openings. This test reports1757172 resident/1890936 peak publication bytes on PC. Generated lightmap image2 is explicitly test-owned metadata, not a tested GPU atlas.

PC build and121/121 tests pass; stock NXDK build passes. Existing live97 replay still emits the unchanged2758-byte checkpoint SHA256 d98a3c0e8b1c725e4924aaf82f7872e99ede438963d3b0e342f892a884f4aea0 (`artifacts/grouped-scene-live`).

The collection view is currently populated by this scene-level integration test. Ordinary scene startup still creates one selected source. A playable multi-source developer mode is not enabled yet: source collection allocation/lifetime, aggregate real lighting, hit dispatch, queries over every registry and multi-source saves remain required. This update establishes the actual scene publication/collision path, not live multi-object visual acceptance.

## Owned collection lifetime in scene startup

Ordinary authored scene startup now calls the source collection factory with one selected UID. The factory accepts up to four distinct supported UIDs, stages assets/cores/registries privately, and reserves decoder/core/registry ceilings plus staging context and prior resident owners before each source construction. Its current caller supplies a6MiB construction ceiling; this is not extra RAM or a claim that shared scene resources are free. Those remain separately owned and charged. Failure closes staged owners and restores source identity telemetry before returning.

Selection synchronizes the active terrain and registry aliases back into their collection entry before changing source. This matters because a committed cut or load replaces the core pointer: a stale collection entry must never be freed later or selected again. Cleanup closes overlay/publication borrowers first, synchronizes the current owner and then closes all registries, cores and assets. Repeated collection cleanup is harmless.

The two-real-post scene test now uses heap-owned source assets and collection entries, exercises source selection, replaces the active core with a decoded equivalent, frees the old core and switches away/back to verify the new pointer survives. It then closes the entire collection and checks all ownership fields clear. Normal one-source publication retains the original reset and save behavior.

PC build and121/121 tests pass; stock-profile NXDK build passes. The550-frame live97 run through collection startup and cleanup produces the identical2758-byte checkpoint (SHA256 d98a3c0e8b1c725e4924aaf82f7872e99ede438963d3b0e342f892a884f4aea0; `artifacts/source-lifetime-live`). Multi-source factory invocation beyond the scene fixture, live aggregate lighting, impact dispatch, all-registry contacts and multi-source saves remain unverified. The startup caller still requests one source; simultaneous gameplay has not been enabled.

## Source-qualified rubble weapon queries

The scene weapon sweep and fragment damage call sites now use source-collection adapters. Each source retains16 batch ID values, so source slots map to0..15,16..31,etc. This keeps a later source's transient contact ID stable when an earlier registry adds batches. These are scene-lifetime hit IDs, not a new save format. Selected registry aliases take precedence over stale collection entries after a committed load/edit.

Weapon sweeps retain the nearest current-pose polygon hit across registries with stable source-order ties. Damage decodes the source-qualified batch and affects only that registry. Invalid source IDs reject; misses preserve hit output and errors preserve both outputs. Existing source0 IDs are unchanged.

Rubble tick and draw loops now visit every registry, accumulate telemetry and collect retired payloads per source. Original fragment-fragment exclusion remains unchanged. Player and admitted vehicle queries still require separate multi-registry integration with their own shape/tie rules; generic weapon queries are not substituted for those paths.

The new scene test uses two real registry-owned cube chunks, verifies a nearer hit in the later source, equal-time first-source retention, selected alias handling and isolated retirement with the other registry's encoded state unchanged. It checks misses/errors and verifies the actual scene tick visits two sleeping bodies. This does not prove dynamic multi-source contact or multi-source visual quality.

PC build and122/122 tests pass; stock NXDK compilation passes. `tools/check_detached_rocket.py` passes through the updated scene: two real rockets, one terrain extraction, second hit retires the chunk,0 live/drawn pieces,1852 retained owner bytes and no motion errors. No new Xbox runtime or multi-source visual acceptance is claimed.

## Collection player and vehicle queries

Player movement and ground call sites now query all source registries using their established size-dependent adapters. Each registry receives the original query limit; results are compared globally. Nearest contact wins, polygon contacts win exact ties with sphere-route contacts (matching the within-registry selection), and other equal-time ties retain source order. Source-qualified batch IDs use the same16-entry ranges as weapon queries. Outputs publish only after every query succeeds.

Admitted vehicle contacts also compare all registries, retaining earliest contacts and source-qualified identities. Use-kind admission remains in the existing original-derived path: kind9 humanoid AI response stays excluded. The scene's early no-rubble gates now count all source registries instead of only the selected one.

Tests use two real registry-owned intermediate-radius cubes, confirm later-source movement/ground contacts, first-source retention for identical contacts and the radius<=.5 exclusion. Vehicle use-kind1 reaches the later source; kind9 misses without overwriting contact or identity outputs. Invalid player query limits preserve output. The first fixture used a.1 actor sphere, whose combined sphere radius was too small to satisfy the original raw-normal-y<-.5 ground condition. The corrected fixture uses a.6 sphere; no runtime threshold was relaxed.

PC build and122/122 tests pass, and stock NXDK compilation succeeds. `tools/check_intermediate_rubble_standing.py` passes after the integration: natural jump onto the intermediate fragment, exact standing and walk-away save continuations, and rejection of missing saved support. These remain single-source live controls. Multi-source checkpoint support providers and restore ownership are still unimplemented, and no new native or multi-source visual claim is made.


## Real aggregate atlas preparation

The real93/94 scene fixture now runs the production lighting-stage prepare, draw, commit and discard sequence instead of assigning synthetic image2 bindings. It allocates the normal bounded atlas/noise/draw storage, publishes the first cut and then the second source cut, and reacquires the active publication view after committing each stage.

This exposed an aggregate cache invalidation defect: both independently cut source cores can have local generation1. The collection publication entry point now uses the room publication serial when more than one source exists, preserving local generation behavior for the single-source path. Without this change the second source's new generated faces can retain unbound image metadata because the lighting stage sees an unchanged generation. The fixture explicitly verifies room generations1 and2.

The first cut creates6 atlas maps; the second expands to12 maps and prepares344 draw vertices across70 published faces. Every generated face binds the atlas image. The first35 face bindings, existing map descriptors and all pixel rows inside the first six maps remain byte-identical after the second cut. Both holes retain their previously verified room-collision behavior. This is actual CPU atlas/noise baking and draw preparation, with no enabled level-light fixture; it does not establish full lighting parity or GPU output.

PC build and122/122 tests pass; stock-profile NXDK compilation succeeds. Logs: artifacts/grouped-lighting-build.log, artifacts/grouped-lighting-tests.log and artifacts/grouped-lighting-xbox.log. No new disk-image clone or native emulator session was created. Ordinary startup still selects one source: grouped live impact dispatch, source-indexed checkpoint restoration and native multi-object memory/visual acceptance remain open.


## Multiple cuts per room commit

Preparation previously required the sum of all source cut histories to be no greater than the room revision. This rejects a legitimate grouped edit: two newly cut sources both have one local cut, while the room has committed only once. Validation now checks each non-selected source history against the room revision independently, as already done for the selected candidate, and leaves the aggregate cut count as telemetry. Source count and per-source history caps still bound the sum.

The installed-asset scene fixture republishes both one-cut source histories at room revision1 through real atlas preparation and collision commit. Both openings remain passable and the70-face publication reports two cuts with generation1. A separate private two-cut history in the non-selected source rejects at revision1 without pending publication or changes to the active room; the test then restores its original owner. This tests publication of a grouped result, not live blast dispatch or simultaneous core mutation.

All122 PC tests pass and a forced scene recompilation completes under the stock NXDK profile. Logs are artifacts/grouped-revision-build.log, artifacts/grouped-revision-tests.log and artifacts/grouped-revision-xbox.log. Live dispatch still routes through the selected source, and post-edit changed-box/wake handling remains selected-registry-only. Those call sites must be connected to the existing grouped transaction before enabling multiple sources in gameplay.
