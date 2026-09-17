# Staged detached-piece geometry and bodies

The scene currently commits rocket terrain edits before spawning ordinary
fragments. Detached solids need geometry and physics bodies prepared before
terrain publication so allocation failure cannot leave removed terrain without
its required pieces. This change supplies that private owner; it does not yet
change scene publication.

`rf_geomod_piece_batch_open` stages subdivision using a local RNG, owns the
resulting geometry bank, allocates body records together, and prepares every
body's sphere storage. Only complete success publishes the owner and RNG.
Failure releases initialized bodies and geometry. Close is idempotent. Geometry
views and mutable body pointers remain valid until close.

The budget includes subdivision peak and subsequent simultaneous geometry,
owner/body records and sphere storage. Worker scratch is gone before bodies
are allocated, so those phases are not incorrectly added together. Allocator
metadata is excluded. No renderer or scene resources are included yet.

## Verification

Release rf_geomod_disconnected_tests passes. Three seeds produce:

| Seed | Pieces | Resident PC bytes | Peak PC bytes |
| --- | ---: | ---: | ---: |
| 0 | 11 | 140960 | 933044 |
| 12345 | 10 | 140372 | 933044 |
| 24690 | 11 | 143024 | 933044 |

Each body's state and sphere array matches separately prepared reference
bodies; geometry and RNG match direct subdivision. Closing twice is safe.
Peak-minus-one budget rejection preserves the output and RNG; this exercises
worker rejection, not an injected late sphere-allocation failure.

Stock-profile NXDK build passes, recorded in
artifacts/geomod-postedit-re/staged-piece-batch-xbox.log. Build success does not
establish native runtime or visual acceptance.

## Remaining integration

Stage these owners with terrain edits and transfer ownership only on successful
publication. Resolve source T-junctions, renderer material/lightmap lifetime,
body scheduling and response, rollback under late allocation failure, actor
notifications, save policy, and stock64MiB runtime verification. No new native
visuals were produced by this change.

## Extracted replay composition

The owner is now exercised directly by the private extraction callback before
retained terrain compaction. Four successive cuts, including checkpoint reload,
produce one body at prefix1 and11 subdivided bodies at prefix4. Reload matches
geometry, face metadata, filters, physics state, spheres and final RNG. Every
terminal piece has exact closed-edge adjacency.

This initially failed on the eight-face prefix4 detached slab. Its boundary
contained long edges opposite multiple shorter coplanar-face edges. Subdivision
input now inserts existing exactly collinear vertices along each edge in sorted
segment order, interpolating that face's UVs. Positions are copied unchanged;
there is no epsilon weld. Face metadata and filters retain source order. Fixed
128-corner/32-face queue capacity and64-corner per-face limit still apply.
Off-line near matches are not normalized. This is a practical topology adapter,
not a claim about the original game's exact representation.

Prefix1 batch resident/peak:131600/296472 PC bytes.
Prefix4 batch resident/peak:139760/933636 PC bytes.
Release geomod_disconnected and geomod_extracted_replay pass. Stock NXDK build
passes in artifacts/geomod-postedit-re/extracted-piece-batch-xbox.log.
Live scene extraction remains disabled: replay composition is evidence for the
handoff, not native motion/rendering acceptance. Broader non-axis topology and
late allocation rollback remain open.

## Current-pose collision queries

`rf_geomod_piece_batch_sweep` queries each owned polygon set at the physics
body's current position/orientation, limits subsequent tests to the nearest
fraction, and transforms the accepted contact back to world space. Equal
fractions retain the earlier piece. It allocates nothing. Misses preserve the
contact; invalid queries preserve both contact and matched outputs.

The extracted replay test moves and rotates the one-piece batch and the last
piece of the11-piece batch (other pieces are placed far away). Across52 ray and
sphere contacts it checks face identity, analytical contact fraction and world
normal; miss and negative-radius controls verify unchanged outputs. Body poses
are restored before reload comparisons. This does not yet exercise overlapping
pieces, texture-alpha queries or scene collision registration.

Release geomod_extracted_replay passes. Stock NXDK build is recorded in
artifacts/geomod-postedit-re/moving-piece-query-xbox.log. No simulation scheduler
or live rendering was added; this connects owned collision geometry to mutable
body poses so scene movement/weapons can consume it next.

## Retained registry and edit staging

rf_geomod_piece_registry holds up to16 active batches and16 replacement-stage
batches. Append keeps the historical prefix/ordinal identity and RNG before/after
states for each batch. A replay emission reuses the owned batch and restores its
post-construction RNG state instead of creating a duplicate or resetting body
motion. Append assumes unchanged historical extraction; replacing history must
use replacement mode. This is scoped to one terrain owner.

Begin/rewind/emit stage changes. Rewind is required before each full terrain
reconstruction, including clone decode followed by mutation, and retains pending
batches for duplicate suppression. Abort frees only pending batches. Commit is
infallible and transfers pointers after the outer terrain/render publication;
replacement commit then frees old batches. Empty replacement supports reset.
Close releases both sets. Borrowed batch pointers remain live until replacement
commit or close; there is no individual retirement API yet.

The registry budget includes its allocation, old and staged resident batches,
and concurrent new batch construction scratch. Terrain/render allocations are
external and must be included by the scene's total-budget reservation. Material
parameters are supplied by the caller. This does not implement scheduling,
drawing or active-body save serialization.

The production terrain extraction test verifies a moved body keeps its pointer
and position through later cuts and two reconstruction traversals. A rejected
fourth edit allocates a new batch, then abort restores registry bytes exactly
while preserving the original body. Successful retry publishes the second batch.
Reset empties the registry; double close is safe. PC Release test passes; stock
NXDK build log: artifacts/geomod-postedit-re/piece-registry-xbox.log.
The scene has not yet enabled this registry.

## Shared scene transaction integration

scene_terrain_edit_transaction_pieces now extends the actual authored scene
clone/decode/mutate/publish wrapper with registry begin, extraction configuration,
replay rewind, commit and abort. Legacy scene calls still use its NULL-registry
wrapper; live activation remains pending drawing/material/budget integration.
The caller must reserve the full registry budget in external_peak_bytes.

The real-core scene transaction test splits a solid, stages bodies before its
publication callback, rejects publication and verifies no active bodies/bytes
leak, then retries successfully. Moving the retained body to X123 followed by
clone decode and a new edit keeps the same batch pointer and position. Reset
publishes no pieces and returns registry resident bytes to the empty baseline.

That reset test exposed old-history emissions from clone decode surviving into
an empty reset. Replacement transactions now discard clone-only staging before
mutation, leaving the old live registry intact until successful publication.

Release geomod_edit_transaction passes, including its pre-existing identity,
generation, history and collision comparisons. Stock NXDK scene compilation and
link pass: artifacts/geomod-postedit-re/piece-scene-transaction-xbox.log. The
first incremental invocation skipped the changed .inc dependency; scene.c's
mtime was then refreshed and actual scene.obj recompilation was verified.
No live scene or rendering acceptance was performed.

## Pose-based drawing path

rf_preview_geomod_pose projects local piece polygons at the supplied body
position/orientation through the shared clipping and draw-vertex generator.
No geometry copies or heap allocations are needed. Backface selection transforms
the camera into local space. UV/material slots remain corner/face-owned; supplied
vertex RGB follows the piece. NULL colors retain diagnostic directional tint,
not final world lighting or detached lightmap ownership.

scene_stream now has a nullable detached_pieces registry and its regular draw
path submits every active batch using each body's current pose. Scene shutdown
closes the registry. rf_scene_detached_pieces reports active/batches/pieces/draw
vertices/resident/status. No scene factory allocates or enables it yet, so this
is compiled draw integration, not a visible gameplay change.

The extracted replay test compares the complete projected output against an
independently transformed world mesh and rebuilt collision planes. Identity and
90-degree rotated poses, translation and near-camera clipping produce33 visible
vertices total; one close pose correctly has no front-facing visible output.
All projected position, UV/material, color and lightmap fields match exactly.
Insufficient output capacity preserves prior bytes and counts. These tests do
not establish rendered appearance.

Release extracted_replay and interior_faces pass. The full PC preview scene
build passes (artifacts/geomod-postedit-re/detached-draw-pc.log). Stock NXDK build
passes (artifacts/geomod-postedit-re/detached-draw-xbox.log). Live ownership
activation, material resolution, total budgets, checkpoint behavior, motion,
collision registration and visual inspection remain unfinished.
