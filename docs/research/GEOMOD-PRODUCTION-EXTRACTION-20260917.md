# Production terrain extraction transaction

The public terrain owner now supports optional detached-solid extraction via
rf_geomod_terrain_set_extraction, configured before the first cut. Existing
terrain behavior remains the default. Cavity extraction is rejected because its
classification/retention policy is not established.

With extraction enabled, chronological replay clips new caps against the actual
retained solid, classifies disconnected components, emits accepted pieces, and
compacts retained geometry plus support provenance before the next prefix.
Emission occurs before any live terrain/tree/history publication. Callback
failure discards the private replay, preserving the live terrain.

## Ownership contract

Every rebuild emits all accepted historical pieces in prefix order, with a
one-based cut prefix and per-prefix ordinal. The caller must stage owned copies,
discard staging on any outer failure/read-only check, and publish only after
successful mutation. Callback inputs expire on return. No callback may reenter
the terrain owner. Scene integration must avoid respawning old pieces on each
blast: replay prefix/ordinal are available for that decision. RNG scheduling and
active-piece persistence remain scene responsibilities.

The callback/context must outlive the terrain policy. Checkpoint consumers must
configure the same policy before decode; RGCH cutter history does not encode
this policy or active body motion. This is currently an opt-in API, not enabled
for the live scene.

## Budget and verification

Extraction scratch is charged to the existing terrain budget during replay and
freed before tree binding. Callback-owned batches require a separate caller
budget that also accounts for concurrent terrain scratch. Tested production
terrain peaks are1343132-1344476 PC bytes; these exclude callback allocations,
allocator metadata and scene rendering resources.

rf_geomod_terrain_extraction_tests uses the public linked core, not a private
source include. Four cuts retain volumes3600,3600,3568,1568 from an8000-volume
cube. Pieces are copied into staged subdivision/body batches. A blast in the
already removed half does not regenerate terrain. Callback rejection preserves
live mesh and cut count, including failure at prefix4 after prefix1 has already
allocated a batch. The caller clears that partial staging. Checkpoint decode
rebuilds byte-identical retained vertices/faces and matching batch counts/RNG.

Seven relevant Release tests pass: terrain_extraction, disconnected,
current_solid_replay, extracted_replay, history_check, publication, history_cuts.
Stock-profile NXDK build passes in
artifacts/geomod-postedit-re/terrain-extraction-xbox.log.

## Remaining

Scene staging/publication, live collision registration, material/lightmap
ownership, rendering, motion/response and active-body saves remain incomplete.
Repeated arbitrary star blasts and authored topology need broader coverage.
No native runtime or visual acceptance is claimed by this API test/build.

## Scratch sharing and star-path coverage

Extraction now borrows the terrain owner's unpublished collision faces/positions
and transient filter array. These are rebuilt before collision publication;
the active collision tree remains untouched. Clip arrays and component graph
scratch now occupy a union because clipping ends before extraction and each
phase initializes its working contents before use. No capacity was reduced.

This removes237568 bytes of concurrent scratch. The four-cut PC peaks are now
1105564-1106908 bytes for quad box cutters and1105564-1107748 for triangulated
box cutters submitted through the star path. Both variants pass rejection
rollback (including one already staged historical batch) and reload with a
1152KiB terrain budget. Callback piece budgets remain separate. These are
triangulated box fixtures, not the shipped irregular rocket template.

Five rebuilt regression targets pass: terrain_extraction, history_check,
publication, history_cuts and extracted_replay. Stock NXDK build passes in
artifacts/geomod-postedit-re/terrain-extraction-scratch-xbox.log.

Live scene inspection: the ordinary DEV room core uses cavity=1; extraction is
currently supported only for outward solids. The authored-post core uses
cavity=0, so it is the first eligible integration path. Its current1MiB core
budget is still below the tested extraction peak. No live budget was increased
and no callback was enabled in the scene during this step.
