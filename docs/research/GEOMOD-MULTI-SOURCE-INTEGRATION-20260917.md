# Multiple live authored sources: integration boundaries

Current runtime supports opt-in paired ctf06 sources93/94 or96/97, shared room publication and per-source terrain/rubble ownership. Two separate real rockets damage both retained posts with matching PC/stock64MiB Xbox state and two settled fragments. Safe paired reset and later unselected-source recut also pass stock Xbox validation. Source-indexed saves, broader reset cases, simultaneous-blast budget acceptance and broader interacting geometry remain unfinished. The sections below preserve the implementation/evidence history; later acceptance sections supersede earlier boundaries.

## Required shared ownership

The initial single-source implementation owned one terrain, authored asset, detached registry, history, publication, lighting atlas, draw mesh and collision overlay. Its `scene_terrain_publication_open` created a composition from the original room tree and one source replacement list. Independently binding one overlay per post would overwrite the previous room replacement rather than compose both edits. Independently rebuilding each post from the original room would restore earlier holes in collision.

Use one room-level composition and retain the unchanged room faces exactly. Its replacement identity set must cover the union of participating source windows. Unedited sources must contribute their original windows until edited; otherwise registration itself creates invisible collision holes. Aggregate the current published geometry for all sources before binding one candidate room tree. Common atlas, draw/publication capacity and temporary work must have a shared ceiling; do not multiply the current whole-scene allocations by four.

Each source needs its own immutable UID, digest, source planes/neighborhood, cut history, active terrain and piece identity namespace. Each blast must identify all affected sources through actual geometry/eligibility. Retain original fragment-fragment exclusion; new sources do not justify enabling fragment stacking. Source extraction must account for current support geometry where two edited sources share a beam/floor; static neighbor snapshots alone cannot establish general interacting-source correctness.

## Transactions and saves

Prepare every changed source plus aggregate room publication privately. Allocation, geometry, lighting or binding failure must retain all active source histories, piece registries and the old room tree. Publish only after the complete candidate is valid. Abort frees all newly staged owners. Retired-piece collection must preserve stable identities and cannot free borrowed geometry beneath the active draw/collision views.

The existing RFCP single-source envelope binds one source digest and replay history. Multiple-source saves need explicit source count/UID/digest records, independent histories and unambiguous piece references; validate the entire candidate before restoring the player or publishing any geometry. Keep each legacy one-source save tied to its recorded source identity. Merely relaxing identity checks would load geometry against the wrong immutable source.

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


## Grouped transaction connected to scene binding

The authored edit adapter now accepts up to four indexed source requests. Each clone factory receives its explicit authored source instead of implicitly copying the selected one. A grouped publication callback temporarily exposes every candidate to the existing production atlas/draw/collision bind, restores the old aliases on both success and failure, and leaves final pointer replacement to the transaction commit. The selected alias is refreshed from its collection entry after commit, avoiding synchronization of a freed old pointer.

The memory reservation includes shared publication/lighting ceilings, every authored owner, a full detached-registry ceiling for each existing registry, collection entries, and resident unedited cores; the transaction separately charges all edited old cores and clone ceilings. The13MiB destruction ceiling remains unchanged. This is a conservative subsystem reservation, not proof of whole-game multi-source stock64MiB headroom.

Single-source runtime edits retain their established path. Selected edits in an existing multi-source collection now use the grouped adapter with one request. Ordinary startup still creates one source, and the rocket call site does not yet enumerate affected sources. The convenience DEV reset rejects a multi-source collection until whole-collection clearance and shared-atlas cleanup are implemented; applying its former single-source cleanup would erase surviving sources' atlas state.

The real93/94 scene fixture now calls the actual grouped adapter with distinct cut centers. An invalid second scale rejects after first-candidate mutation; a zero lightmap image count rejects during publication after both candidates mutate. Both controls retain original core pointers, selected-core snapshot, room collision/publication and atlas bytes. Success replaces both cores, retains the selected alias, advances room revision1 to2, and publishes four local cuts in total. The two-source atlas/collision fixture has no detached registries: grouped registry commit/abort is covered separately by the existing transaction tests, not claimed here as live fragment validation. The test sources and cloned histories now consistently use128x128 mapping, matching the factory's required mapping identity.

All122 PC tests pass, stock NXDK scene compilation succeeds, and the existing two-rocket live PC regression passes through the ordinary path: the first extracts a chunk and the second retires it. Logs: artifacts/grouped-edit-build.log, artifacts/grouped-edit-tests.log, artifacts/grouped-edit-xbox.log and artifacts/grouped-edit-rocket.log. No new Xbox runtime, audio or GPU acceptance is claimed. Blast-source enumeration/admission, combined changed-box notifications, multi-source save/reset and native memory validation remain required before enabling the mode.


## Cross-source post-edit wake

The rocket edit path now snapshots each source registry's batch count before the edit. After successful collision/render publication it gathers newly extracted changed boxes across all sources and broadcasts the change to every source registry. Selected live aliases retain precedence over stale collection entries. The changed-box list remains capped at the original32 entries; source order supplies deterministic append order, with excess entries ignored rather than enlarging or merging bounds. Exact original multi-source append ordering and saturation coverage remain unverified.

A new read-only registry notification preflight shares the original-derived validation/object-decision implementation with the existing write path. The scene validates all registries before applying any notification, without intervening callbacks, simulation or ownership changes. A malformed body in a later registry therefore cannot leave earlier bodies partially woken. This adds no impulse and does not change original radius/box wake criteria.

The source-collection test uses two real registry-owned cube bodies. Only source0 contributes a new changed box, yet both overlapping bodies wake; a repeated notification reports zero newly woken bodies. With no new boxes, radius fallback wakes both sources. Invalid bounds in source1 and an invalid later prior-count preserve the output and leave both bodies asleep. These are scene adapter fixtures, not live multi-object fragment acceptance.

All122 PC tests pass and stock-profile NXDK compilation succeeds. The existing live PC support-loss replay passes: a second actual rocket removes the remaining support without directly hitting the chunk, wakes it once, and it falls over0.2 units before settling. Logs: artifacts/source-wake-build.log, artifacts/source-wake-tests.log, artifacts/source-wake-xbox.log and artifacts/source-wake-support.log. No additional retained disk clone or native run was created. Blast source enumeration/admission, source-indexed save/reset and stock64MiB multi-source validation remain open.


## Template-based source dispatch

The authored template edit entry point now enumerates candidate sources when a collection contains more than one owner. It computes cutter bounds from the exact transformed and shallow-deformed vertices shared with the core cut path, compares them with each retained authored source's bounds, and sends all overlapping candidates through the grouped transaction. This is conservative broadphase, not an exact intersection test: CSG still evaluates the actual mesh, and a bounds-only false positive can retain a no-op cut history. No distant source is cloned solely because it is selected. A complete miss returns RF_NOT_FOUND without a room commit.

The shared template preparation retains the existing double-expression transform, scale, basis validation and shallow deformation. The new bounds API additionally validates the star mesh before publishing its bounds. The fixture compares returned bounds byte-for-byte with the actual retained cutter vertices' extrema. The four supported sources share default hardness; a candidate whose default differs from the already admitted selected-source hardness rejects until independent per-source admission exists. This does not claim general mixed-material/region behavior.

The real93/94 scene fixture now performs its first two edits through template dispatch while the other post is selected. Each cut changes only the intended source. A later common cutter centered between the posts dispatches to both and commits one room revision, taking both local histories from two to three cuts. Distant bounds yield no candidates; invalid scale preserves selection outputs. Initial radius3 did not reach either post: actual transformed z bounds were approximately[-2.01704,2.01704], short of the posts' nearer edges at +/-2.25. The shared-cut fixture uses radius4, rather than assuming nominal radius implies equal reach on every axis.

All122 PC tests pass, stock-profile NXDK compilation succeeds, and the existing live PC support-loss replay still passes after the shared core-transform refactor. Logs: artifacts/source-dispatch-build.log, artifacts/source-dispatch-tests.log, artifacts/source-dispatch-xbox.log and artifacts/source-dispatch-support.log. Ordinary startup still requests one source, so this is the actual scene edit adapter exercised with real assets, not an enabled multi-source gameplay session. Source-indexed checkpoint/reset, budgeted multi-owner startup, mixed-hardness admission and native rendering/memory acceptance remain open.


## Opt-in paired-source startup

PC developer replay accepts RF_REPLAY_AUTHORED_SOURCES=2 (default1). Xbox setup accepts an optional D:\authored-count.bin containing exactly one little-endian uint32 with value1 or2. The selected UID remains controlled by the existing source setting; its paired post is the other member of93/94 or96/97. Source order is selected first. Four-source startup is not enabled. The new placement API validates count before changing level placement and the old selected-source API restores count1.

The first real paired launch exposed two setup assumptions. An entirely uncut collection must publish the original room and empty draw bank, avoiding a premature lighting stage before scene lighting initialization. After a cut, the unchanged source still contributes its original windows to aggregate publication. The static render exclusion now uses the union of replaced IDs, and the draw gate reads committed aggregate publication cuts rather than the selected core's count. These prevent untouched-face duplication and hiding unselected-source damage.

The first rocket was correctly rejected by the13MiB subsystem ceiling because unedited registries were charged their full2MiB construction capacity. Such registries cannot allocate during this synchronous edit. The reservation now charges their actual resident bytes, while edited registries retain their full construction ceiling. The measured one-source edit with two live owners reserves12931972 bytes (about12.33MiB), under the unchanged13631488-byte cap. This does not prove a simultaneous two-registry cut fits, or establish whole-game Xbox headroom. The mixed-hardness guard compares candidate authored settings with selected authored settings; it must not compare brush defaults with the separately prepared region/level hardness.

Legacy authored save writing and staged restore explicitly reject multi-source scenes. The convenience reset already rejects them. This prevents apparently successful saves from silently dropping an owner while source-indexed formats remain unfinished.

The new tools/check_paired_authored_startup.py replays one real rocket with both94 and93 retained. Startup prints both source owners; the rocket commits one cut, publication14 faces/78 vertices, and one12-triangle detached piece. Publication resident/peak telemetry is1609840/1734812 bytes; the selected piece registry retains135268 bytes. The second source stays uncut in this run. The harness also requests a save, verifies the writer rejects with RF_RANGE and confirms no checkpoint file is written. Output is under artifacts/paired-startup; no disk-image clone is created.

The actual final PC framebuffer was inspected: room, weapon/HUD and the damaged post with its detached chunk are visible. This endpoint does not verify movement/audio or cuts to both live objects. All122 tests pass and the stock NXDK build succeeds. No paired XEMU session has yet been run. Logs: artifacts/paired-startup-build.log, artifacts/paired-startup-tests.log, artifacts/paired-startup-xbox.log and artifacts/paired-startup-harness.log. Paired save/reset, two-object live sequences, simultaneous-edit memory reservation and native acceptance remain open.


## Paired-source stock Xbox acceptance

The native harness now accepts --authored-sources2, mirrors it to the PC environment, stages authored-count.bin and restores/removes it with the other disc flags afterward. Argument validation rejects paired checkpoint requests before building or launching. New retained AUTHORED_COLLECTION telemetry reports the actual constructed owner count, up to four UIDs and source-asset resident bytes, rather than treating a configuration flag as proof of allocation. Native comparisons use count/UIDs; the size word remains informational.

The550-frame native run artifacts/xemu/render-20260917-123126 passes74/74 comparisons. Both platforms report collection[2,94,93,0,0,30120], one fired/impacted/accepted rocket, publication14 faces/78 vertices at one cut/revision1, and one12-triangle fragment with135268 registry bytes. Detached motion words match exactly and show the fragment settled without errors. Source93 remains intact; this is not a two-object damage sequence or paired save continuation.

The diagnostic reports16384 physical pages, with3802 available at the final endpoint (about14.85MiB). The retained xemu.toml has no expanded-memory setting and uses the existing pacing-base.qcow2; no HDD clone was created. This is endpoint headroom, not a measured minimum across the whole edit.

The native framebuffer was inspected and shows the room, weapon/HUD, cut post and tilted detached chunk. It matches the expected PC endpoint content. Audio quality, both-source damage and simultaneous-cut memory remain unverified. The Red Faction test session closed and authored-count.bin was removed by restoration; other-project emulator sessions were not controlled. All122 PC tests pass after the telemetry addition. The paired source setting remains opt-in, with saves/reset still explicitly unsupported.


## Two separate live source cuts on PC and Xbox

The reproducible tools/check_paired_authored_cuts.py extends the measured single-shot input to850 frames, turns using ordinary look commands, fires a second rocket at source93, then returns the camera toward the middle. The selected source remains94 throughout. Aim uses the recorded settled eye and the original-derived pitch/yaw update math; there is no teleport, source switch, injected cut or host input. Impacts occur at frames268 and472, at z2.5 and approximately-2.501936. Both edits are accepted.

Retained AUTHORED_SOURCE_CUTS telemetry reports UID/local-cut pairs after successful edits. It shows[94,1,93,0] followed by[94,1,93,1], establishing one cut per owner rather than two edits to the selected owner. The harness compares the final native array with the last PC report. The final shared publication contains18 faces/100 vertices, two total cuts and room revision2. Two fragments remain alive and settled, contributing30 triangles and266504 aggregate registry bytes. The PC edit reservation peaks at13065104 bytes, still below13MiB. This is two successive single-source transactions, not one simultaneous two-source blast.

Native run artifacts/xemu/render-20260917-123854 passes75/75 comparisons, including per-source cut counts, aggregate publication, rocket counters and the full detached-motion words/hash. Physical pages remain16384;3653 pages are available at the endpoint (about14.27MiB). No worst-case minimum-headroom or simultaneous-edit claim follows from that endpoint.

Both PC and native final framebuffers were inspected. The right damaged post/chunk is clearly visible; the left fragment is partly occluded by the retained red prop, so this endpoint is not complete visibility coverage of every fragment surface. Both views retain the room, weapon and HUD. No audible-quality acceptance is claimed. All122 PC tests pass, the harness builds the stock NXDK image, and no extra GitHub images or retained HDD clones were created. Source-indexed save/restore, paired reset/clearance, simultaneous-cut memory and broader topology remain unfinished.


## Paired reset and recut

The DEV reset now checks the actual standing/crouched body against every restored source's planes and the original full-room collision before staging any change. It then resets all source owners in one grouped transaction and performs shared admission/atlas/draw/debris cleanup once, after commit. The single-source reset path remains unchanged.

Grouped transactions support an explicit fresh-source request for reset. Such requests recreate the immutable source with its mapping, skip old history encode/replay, and stage an empty replacement registry. They cannot retain an existing registry without replacement. If every source is fresh, no history scratch buffer is allocated or reserved. Reset reservations charge existing registry bytes because no fragment reconstruction occurs; ordinary edits still reserve the full edited-registry construction ceiling. The13MiB subsystem budget is unchanged.

Tests inject second-source creation/mutation failures and final publication failure during fresh reset. Both old cores, two-cut histories and moved body owners survive; successful reset clears both registries and reports zero replay-history bytes. Installed clearance coverage now checks safe floor admission and restored-post interior rejection for all four supported source UIDs. These are mathematical/transaction controls; a live unsafe paired-reset replay remains separate work.

The reproducible tools/check_paired_authored_reset.py starts with both rocket cuts, issues ordinary crouch/use/alt input at frame750, and checks both source histories return to zero at room revision3. A1150-frame continuation then fires at unselected93: final pairs are[94,0,93,1], publication16 faces/80 vertices, one cut at revision4, and one66-triangle fragment. The existing cut RNG is intentionally not reset. PC reset-only and reset/recut frames were inspected: both posts restore, then only the left/unselected one is damaged again; its fragment is partly occluded by the retained prop.

Native artifacts/xemu/render-20260917-124824 passes75/75 checks after the entire1150-frame sequence. Per-source histories, three accepted rockets, publication and settled-fragment state/hash match PC. It reports16384 physical pages and3687 available at the endpoint (about14.40MiB). The final native framebuffer was inspected and agrees with the expected recut scene. All122 PC tests pass, including the additional fresh-reset controls; the expanded all-four-source clearance test also passes. The harness builds the stock NXDK profile, restores disc staging and does not create a retained HDD clone.

Paired save writing/restoration still reject until source-indexed persistence is implemented. Unsafe live reset, simultaneous multi-registry cuts and broader shared-neighbor topology are not established by the safe reset run.


## Source-indexed persistence directory foundation

RFAS1 now provides a bounded, allocation-free directory for one to four authored source histories and optional detached-body payloads. It retains source order because scene hit/support batch IDs depend on the source slot. Each entry carries its UID, a32-byte identity and exact RGCH/RFPB lengths; payloads follow the complete directory without padding. This is a new reconstruction format, not an original-game save format.

The codec rejects duplicate/sentinel UIDs, zero identities, unsupported envelopes, malformed lengths/counts, reserved words, truncated spans and trailing data. Packing validates all entries and capacity before writing, and reading publishes its output only after complete validation. Input and output buffers must be disjoint. These are envelope checks, not authentication or deep body/history validation: callers must compare actual source identities and decode each payload privately before committing.

Tests cover two/four entries, optional body state, every truncated prefix, later-entry corruption and unchanged outputs on rejection. The real93/94 scene fixture packs its two three-cut histories into7608 bytes, recreates both cores and compares every reconstructed mesh vertex and face byte with the originals. That fixture intentionally uses synthetic identity bytes and no detached-body registries; it does not establish scene restore, identity authentication or rubble continuation.

All122 PC tests pass and the stock-profile NXDK build produces default.xbe and the diagnostic ISO. Logs are artifacts/source-save-directory-build.log, artifacts/source-save-directory-tests.log and artifacts/source-save-directory-xbox.log. No native session or retained HDD clone was needed for this codec-only addition.

Live paired save writing/restoration remain explicitly disabled. Next integration must version the enclosing save, bind authentic per-source identities, persist the shared atlas/admission journal once, stage every core and registry under the Xbox memory budget, validate player placement/support against the staged collection, and commit the entire collection atomically. Legacy single-source formats must retain their source identity semantics. Unsafe live reset, simultaneous-cut memory and mixed-hardness coverage also remain open.


## Collection save framing

The shared codec now accepts an explicit RFCP profile3 carrying RFDS3. Its416-byte header retains the128-byte owner-extension slot and version marker2 for that extension; the old core span instead contains the complete RFAS1 directory. Admission records, atlas maps and face-map indices follow once. Header word12 must be zero: detached-body state belongs to each RFAS source entry, never a global trailer. This defines structural framing only; the collection-specific owner digest and live scene adapter still need implementation.

A separate collection-layout reader checks every outer span before reading the nested directory. Legacy RFDS2 readers and mismatched RFCP profiles reject collection data. The existing file capacity, table limits and player encoding are unchanged. The collection directory itself consumes part of that same capacity, so a valid standalone RFAS packet can still be too large for a complete save and must reject.

The composed-save fixture exercises two source entries, optional per-source RFPB, shared table offsets, every truncated prefix, crossed versions/profiles, corrupt nested directory fields, duplicate later source UID and output preservation. It intentionally supplies synthetic histories/body envelopes and does not establish semantic restore. All122 PC tests pass and stock NXDK produces the XBE/ISO; logs: artifacts/collection-envelope-build.log, artifacts/collection-envelope-tests.log and artifacts/collection-envelope-xbox.log. No native session or HDD clone was created.

Live multi-source writing and restoration remain disabled. Authentic identities, aggregate material/collision digests, all-source private reconstruction, rubble/player support validation and atomic live publication are the next required integration work.


## Real collection snapshots and identity preflight

scene_authored_sources_checkpoint.inc now snapshots each actual source's RGCH history and RFPB registry into RFAS, using its immutable captured32-byte source identity. The selected source uses the active core/registry aliases, so a replaced owner is not read through a stale collection entry. It does not sync or otherwise mutate those entries. Every registry receives an explicit versioned payload, including empty state. Pending publication rejects.

The writer bounds temporary storage to two transport-sized buffers, fully encodes and checks a private packet, then copies it to the caller only on success. The enclosing writer must charge this temporary storage in its reservation. The separate identity preflight verifies count, source order, UID and captured identity against the current scene before publishing any layout offsets. It does not authenticate a save against a malicious author, decode history/body semantics or establish material/publication correctness.

The installed-asset test opens actual sources94/93 through the production factory, cuts both with extraction enabled, and creates a3336-byte snapshot. Both entries contain nonempty fragment registries. Independently recreated cores and extraction registries decode their corresponding histories and body states; mesh vertices/faces and re-encoded registry bytes match exactly. Changed packet/current-source identities reject atomically, insufficient output capacity preserves the whole buffer/count, and a deliberately absent selected collection core entry proves the read-only snapshot uses the current alias.

All122 PC tests pass; an additional focused test confirms both registry payloads contain fragments. Stock NXDK compilation succeeds. Logs: artifacts/source-snapshot-build.log, artifacts/source-snapshot-tests.log, artifacts/source-snapshot-target-tests.log and artifacts/source-snapshot-xbox.log. No new emulator session or HDD clone was created. This fixture does not verify evolved moving-body continuation or shared atlas reconstruction.

This is a source snapshot adapter, not an enabled gameplay save. The enclosing RFDS3 writer still needs runtime-material canonicalization and collection-aware publication/collision/material digests. Restore needs all-source private staging with the shared journal, player placement/support validation, complete memory reservation and one atomic publication. Existing paired save gates remain closed.


## Private collection reconstruction and material portability

Source snapshot writing now converts each cut history's runtime substrate material slot to canonical token0, matching the established single-source checkpoint convention. The collection stage preflights every source identity and canonical material token, checks each local cut count against the room serial, and reconstructs each source/core and extraction registry privately. It remaps canonical history materials to the current scene slot, replays extraction, then restores that source's body state. The shared publication and live aliases are never changed.

The new stage owns all reconstructed cores/registries and has one discard path for complete or partial construction. Outputs are assigned only after every source succeeds. A caller-supplied budget covers the stage/factory context, maximum history scratch and full per-source core/registry ceilings; the tested pair reserves6569948 bytes. This is incremental reconstruction reservation, not whole-scene peak memory. The eventual enclosing restore must charge existing owners and shared publication/atlas work separately before calling it.

The real paired fixture now uses this production stage, comparing restored mesh and body bytes for both nonempty registries. It also restores under a different runtime material slot, re-encodes both histories to canonical tokens and verifies unchanged history/body payloads. Insufficient reservation and a serial below the saved local cut count reject. A corrupt second-source body health value rejects after the first private owner has reconstructed, preserves the output pointer and leaves the live snapshot unchanged; cleanup discards partial owners.

All122 PC tests pass and the stock-profile NXDK build succeeds. Logs: artifacts/source-stage-build.log, artifacts/source-stage-tests.log and artifacts/source-stage-xbox.log. No emulator session or HDD clone was created. The paired stage is not wired into live save/load: collection-aware digests, shared atlas/admission reconstruction, player support/placement validation, complete scene memory accounting and atomic publication remain open.


## Shared retained-material collection digest

The pure retained-material codec now exposes RFRM v2 for a collection of one to four ordered source UID/identity/local-cut tuples. It hashes that collection before the shared substrate identity and the existing complete noise journal, including historical unused maps, regenerated base pixels, RNG/packing continuation and face-to-map bindings. RFRM v1 and its independent408-byte golden vector remain unchanged.

A collection validates each local cut count against the configured per-core limit and room serial, and requires the aggregate count to equal their sum. It deliberately does not compare that sum with the room serial: a simultaneous two-source blast produces two local cuts in one revision. Crater faces must reference a known source with at least one cut. Duplicate/sentinel owners, missing/zero identities, wrong aggregate counts, impossible local counts and malformed journal state reject without altering the output digest. Source order participates in the hash to preserve the scene support-ID contract.

The fixture uses two crater owners sharing one two-map journal at serial1 with one cut each, verifies changed owner/identity changes the digest, exercises malformed later-source state and a reset collection, and retains the original single-source golden hash. All122 PC tests pass and stock NXDK compilation succeeds. Logs: artifacts/collection-material-build.log, artifacts/collection-material-tests.log and artifacts/collection-material-xbox.log. This is a semantic journal codec, not yet the scene adapter or a live collection save; face geometry containment and authentic material capture remain separate gates.


## Collection-aware scene digest capture

The scene digest adapter now gathers verified material manifests across the collection, requiring a common substrate and consistent content for shared runtime material slots. Retained faces resolve their immutable chart/reference through the matching source manifest. Generated charts are keyed by both source UID and retained-map ordinal, allowing a shared atlas map without incorrectly assigning another owner's chart.

Collection material capture feeds actual local cut counts and immutable identities into RFRM v2. Collision capture uses the publication's complete replaced-face union and binds a domain-separated RFSC1 SHA256 of the ordered UID/source-identity list. The first collection source anchors metadata independently of the currently selected alias. Metadata source/neighbor counts aggregate the collection; these do not by themselves make the legacy owner-extension validator suitable for a collection save. Single-source save/read tests remain passing.

The real94/93 fixture prepares and bakes a pending combined publication with36 faces and4 generated maps, then captures publication, collision and material digests. Switching the selected source leaves all three digests and face-map bindings identical. Changing the second source identity changes material/collision digests; corrupting the shared RNG rejects while preserving all outputs. Source identity comparison against the saved directory remains a separate required preflight.

All122 PC tests pass and stock NXDK compilation succeeds. Logs: artifacts/collection-digest-build.log, artifacts/collection-digest-focused.log, artifacts/collection-digest-tests.log and artifacts/collection-digest-xbox.log. This is installed-asset CPU validation of the pending scene, not a native paired save/reload or visual acceptance run. Enclosing writer/extension integration, shared journal restore, player support and atomic live publication remain open.


## Complete paired RFDS3 writer

The explicit collection writer assembles the canonical RFAS source histories/body state, shared admission/maps/face bindings and all three scene digests into RFDS3. It anchors the header's source identity to collection slot0, writes per-source bodies only inside the directory, and preserves the existing1MiB writer scratch ceiling. Snapshot preparation reserves up to three transport buffers; subsequent directory/writer/payload/digest reservations are checked together. Every error preserves the caller's buffer and written count.

Collection owner extensions use mode1 in the same128-byte layout. Their independently verified directory count2..4 bounds aggregate source/neighbor counts and total cuts; local cut counts remain separately validated. Legacy mode0 encode/decode stays separate and rejects collection extensions. The collection gate permits simultaneous cuts whose sum exceeds the room serial, while bounding that sum by source count times serial.

The real paired scene fixture finishes its pending CPU publication and writes a4176-byte payload containing both cut histories/nonempty registries, four shared maps and36 face bindings. It reads the layout, validates the collection extension against independently captured scene expectations and reconstructs both source entries through the private restore helper. Capacity failure and corrupted live RNG preserve the whole output/count. The legacy scene writer still rejects the paired scene, explicitly preventing gameplay use before complete restoration exists.

All122 PC tests pass, including legacy owner-extension vectors, collection mode/serial/reset/truncation rejection and the installed paired writer. Stock NXDK compilation succeeds. Logs: artifacts/collection-writer-build.log, artifacts/collection-writer-tests.log and artifacts/collection-writer-xbox.log. No emulator session or disk clone was created. Shared journal restoration, player support/placement, whole-scene memory accounting and atomic live reload remain unfinished.
