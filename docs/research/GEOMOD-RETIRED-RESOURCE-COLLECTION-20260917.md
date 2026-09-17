# Retired rubble resource collection

`rf_geomod_piece_registry_collect_retired` releases collision-sphere allocations
for retired chunks and shared geometry once every chunk in a batch is retired.
It allocates nothing and rejects calls during an edit transaction. Scene collection
runs between uses of borrowed piece views at the start of detached-body ticking.
Healthy pieces are not expired or collected.

Batch/ordinal/piece slots, body state, life state and immutable birth health remain
in the small owner allocation. RFPB encoding is unchanged, including retired
records. Decoding a matching retired snapshot still works after collection.
Revival into a collected owner rejects before any state mutation; loading older
live saves uses the normal history-rebuilt owner path. Get returns RF_NOT_FOUND
for collected slots. Zero-sphere live bodies remain valid and are distinguished
from collected bodies through ownership state, not the sphere pointer alone.

Tests cover healthy retention, released-byte accounting, idempotent collection,
unchanged encoded snapshot bytes, collected-state decoding, atomic rejection of
legacy revival into a collected owner, and transaction exclusion. All121 tests
pass, including the real authored extraction/digest test with zero-sphere bodies.

`tools/check_detached_rocket.py` exercises actual rocket extraction and destruction.
`tools/check_retired_resources.py` compares healthy retention with retirement and
reload, then compares the latter against uninterrupted two-rocket continuation.
The measured registry residency is133780bytes healthy and1852bytes retired;
131928bytes released, with byte-identical final RFCP. Evidence lives in
`artifacts/geomod-postedit-re/retired-resources/` and `detached-rocket/`.

The stock-profile NXDK build passes. Native collection/continued destruction is
not yet validated. Shared geometry in partly live batches remains allocated;
per-piece geometry compaction and the historical sixteen-batch limit remain open.
This collection changes storage lifetime only, not damage, healthy expiry, physics
shape, or save identity semantics.

## Stock64MiB native validation

`artifacts/xemu/render-20260917-102743` runs the550-frame two-rocket destruction
sequence. All74 checks pass. Native retained rubble residency is1852bytes, with
zero live/drawn chunks and a2760-byte checkpoint identical to PC. Its SHA256 is
`4733d79800840a340087cb474604ae5caefcf449b03e21e0b51bca8a224fbb7b`.

`artifacts/xemu/render-20260917-102925` loads that Xbox-created checkpoint in a
fresh owned process and continues for201 rendered frames. All74 checks pass;
the retired chunk remains absent and retained residency stays1852bytes. The
final checkpoint also matches uninterrupted PC continuation byte-for-byte:
`020de8f1f631b4d514cd708a43979efcd9b0b622de361f433f86e7158cb4e5f1`.

Both memory reports show67108864 base bytes and zero added memory. Native endpoint
framebuffers were inspected: the room, weapon and HUD render, and the retired
chunk is absent in both. This does not establish complete animation/audio fidelity.
Owned emulator processes closed and the build disc was restored. The harness used
the reusable base and did not retain another HDD or ISO copy.

Repeated new destruction after collection, partly live shared geometry and larger
histories remain open; this validation covers one naturally extracted retired batch.

## Later edits and mixed ownership

`tools/check_post_retirement_cut.py` fires a third ordinary rocket after the
previous two rockets have extracted and destroyed the chunk. Both aim heights1.25
and.25 produce a second terrain edit. Each uninterrupted800-frame RFCP matches
the251-frame continuation from the retired550-frame save exactly. The retired
chunk remains absent and registry residency stays1852bytes. No new detached chunk
is extracted in these two live cases; they prove later terrain editing, not live
new-chunk allocation. Outputs are `post-retirement-cut/` and
`post-retirement-cut-0.25/` under `artifacts/geomod-postedit-re/`. The default PC
endpoint image was inspected for the room, edited post, weapon and HUD.

The `geomod_disconnected` core test now exercises an11-piece subdivision batch:
collect one retired member while retaining a live neighbor's geometry/body;
replay its extraction identity without resurrection; append another batch;
retire both batches, retaining8936bytes of history owners; stage/abort a third
batch with exact accounting rollback; then create and commit the third batch.
The focused test passes. This is synthetic geometry through the production
registry/subdivision path, not an additional live gameplay or native acceptance.


## Mixed-batch geometry compaction

Retired collection now also attempts to repack live geometry when a batch contains both live and retired pieces. It retains the original piece-slot array and body/life/birth records, but copies only live vertex, face, filter, provenance and collision arrays into a smaller bank. Collision planes and coordinates are copied verbatim; only owned pointers are rebound. Borrowed geometry views must be reacquired after collection. This extends the previous no-allocation collector contract, now documented in the public header.

Allocation is bounded by the registry's remaining budget while old geometry remains resident. Allocation/capacity failure simply retains the original bank. A replacement is published only when smaller, with no subsequent fallible work; old geometry is then freed. Healthy bodies never expire, historical IDs are not reused, and encoded state is unchanged. The sixteen historical batch slots remain a separate limitation; this change reclaims storage rather than erasing identities.

The existing eleven-piece synthetic extraction case retires one piece and now reduces registry payload from142588 to30108bytes, releasing112480bytes (including retired physics and spare geometry capacity). The test verifies exact serialized snapshots across collection, surviving body state, vertex and face bytes, rebound collision coordinates, idempotent subsequent collection, historical replay without resurrection, later extraction, all-retired collection, and transaction rollback. All123 PC tests pass, and the stock-profile NXDK build/link/XBE/ISO passes. Logs: artifacts/rubble-compact-build.log, rubble-compact-tests.log and rubble-compact-xbox.log. This is core validation and an Xbox build, not native mixed-batch gameplay acceptance; low-headroom allocation failure is handled conservatively but was not fault-injected in this test.


### Compaction failure and retry

The mixed-batch test now compiles the production piece-bank implementation into its own test translation unit with a locally wrapped calloc. No production fault switch or global allocator override was introduced. After retiring one piece, the registry budget is capped at current residency: collection may release retired spheres, but replacement geometry cannot allocate, the original geometry owner remains, accounting stays exact, and serialized bytes remain unchanged. Restoring the normal budget and forcing its next bank allocation to fail also retains that owner, releases zero additional bytes, preserves the surviving body and borrowed geometry, and preserves the snapshot. Disabling the fault permits successful compaction and the existing exact geometry/collision/save comparisons plus later extraction/rollback tests.

All123 PC tests pass (`artifacts/rubble-compact-failure-build.log`, `artifacts/rubble-compact-failure-tests.log`). This closes the bounded-allocation and allocator-failure test gap documented above. It does not qualify a live native mixed-batch scene, allocator fragmentation over long gameplay, or historical batch-slot exhaustion. Production source and NXDK output are unchanged from the preceding compaction implementation.


### Real three-piece beam retirement and compaction

`tools/check_mixed_piece_collection.py` loads the previously verified Xbox beam/post recut save (render-20260917-185838), aims an ordinary rocket using the measured eye position and piece0 center, and advances351 input records. No health or pose is injected. Piece0 goes from62.67321 health to-337.32678 and retires; pieces1/2 retain their positions, health and flags exactly. The live scene reports two remaining pieces,57 faces and12148 retained registry bytes, versus the earlier three-piece136804-byte PC endpoint. It performs no additional terrain cut.

The script requires one fragment rocket contact, one retired serialized member, two unchanged survivors, retained payload below32KiB, and exact5096-byte checkpoint equality between saved121-record continuation and uninterrupted playback. PC acceptance passes in artifacts/mixed-piece-collection-pc.log; inputs, saves and report are under artifacts/mixed-piece-collection. Native acceptance is recorded separately below.


Stock64MiB native run render-20260917-193532 loads the prior Xbox settled save and executes the351-record rocket input. All76 checks pass; its5096-byte Xbox checkpoint equals the PC shot checkpoint exactly. Native DETACHED_PIECES reports two live pieces,57 faces and12148bytes, matching PC. Run render-20260917-193705 loads that compacted Xbox save and executes121 neutral records;76 checks pass, residency remains12148bytes, and the5096-byte checkpoint exactly equals uninterrupted PC control. Both endpoints have3623 available pages (14.152MiB); these are endpoint measurements, not minimum whole-run headroom.

Both native framebuffers were inspected: the targeted long chunk is absent and two remaining wood pieces are visible with room/weapon/HUD intact. Weapon animation differs between endpoint frames as expected from elapsed time. Audio quality and every intermediate visual frame were not reviewed. Logs are artifacts/mixed-piece-collection-native.log and mixed-piece-collection-reload-native.log; full native evidence is in their render directories. No extra HDD clone or GitHub screenshot was created. Historical slot exhaustion and broader mixed ownership remain open.


## Bounded historical capacity extended to32

The retained/staged registry arrays now admit32 batches rather than16, without increasing their byte budget or discarding retired identities. The fixed owner overhead increases by1408bytes per registry on the32-bit build. This is a bounded extension, not unlimited destruction: the32-batch bound, terrain cut limits, geometry limits and memory admission still apply independently. Changed-box output remains within its existing32-entry contract.

Scene contact tags preserve every previous source/batch mapping for batches0..15. Batches16..31 use the next four-source page: tag=(batch/16)*64+source*16+batch%16. Decoding recovers source=(tag/16)%4 and local batch=(tag/64)*16+tag%16. Weapon damage, player collision, NPC contact and checkpoint support resolution use the same mapping. Checkpoint piece records remain local batch identities and their format is unchanged.

The scene test creates32 historical batches under the unchanged2MiB registry cap by committing, retiring and collecting old batches, retaining batches16/31 for targeting controls. Queries return source1 tags80/95 and damage retires the correct extended-range pieces. All128 source/batch combinations round-trip; old tags remain identical. A staged33rd batch rejects while the32 preceding batches commit;32-box output does not overwrite its sentinel. The full32-batch state encodes and decodes. An initial fixture that kept all32 allocated simultaneously correctly exhausted the byte budget; the accepted fixture models reclaimed historical growth rather than raising that budget. All123 PC tests pass (artifacts/rubble-batch32-build.log and rubble-batch32-tests.log). Full native32-batch gameplay remains untested; older-save native compatibility follows separately.


Stock64MiB NXDK/XEMU run render-20260917-194239 loads the pre-extension compacted Xbox save from193532 and advances121 records. All76 checks pass; the5096-byte checkpoint remains byte-identical to the pre-extension uninterrupted PC control. The two-source registry payload is14964bytes (12148+2*1408), with two live chunks and57 faces. Endpoint3639 available pages is14.215MiB; allocator/run variation means this is not a claim that increasing registry capacity improves headroom. Native frame inspected: both surviving chunks, room, weapon and HUD remain visible. Audio and a live32-batch history remain unverified. Log: artifacts/rubble-batch32-native.log.
