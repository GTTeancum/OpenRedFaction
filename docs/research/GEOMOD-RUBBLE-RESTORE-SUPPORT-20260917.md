# Restore support from settled detached geometry

Authored checkpoint staging now combines the static-world ground probe with a
read-only query of the privately restored piece registry. The closest actual
polygon contact wins; static geometry wins exact ties. A nearer unstable piece
rejects instead of falling through to a farther floor. A walkable normal still
requires Y>=0.5, and the independent full-body/world and piece-clearance gates
remain in place. No player relocation, support attachment or dynamic carry is
introduced.

The optional checkpoint support provider uses the same prepared lowest-sphere
probe as static standing validation and transforms world-space queries through
the saved chunk poses. Only inactive bodies with exactly zero linear and angular
velocity qualify as stable save support. This is an explicit bounded save policy,
not a claim of recovered original moving-platform persistence. Retired pieces
are already excluded by the shared registry sweep. Null/empty registries preserve
static-only behavior. Query errors leave output untouched and checkpoint failure
discards the private candidate before live publication.

Tests cover static ties, nearer unstable support, nonwalkable normals, malformed
provider outputs and provider failure. Real extracted-piece provider tests cover
active/inactive bodies, residual linear/angular velocity and empty registries.
The installed ctf06 scene checkpoint test positions an actual extracted post
piece in open room space and saves its body. It uses a small synthetic sphere
placed from the actual upward polygon centroid and normal. Restore preparation
accepts stable support, rejects the identical active body, and rejects surface
penetration. Every rejected candidate leaves the existing scene/serial/pending
ownership intact. This exercises real extraction, serialization, private body
restoration and scene staging, but is not an ordinary full-player jump/stand
replay or a native runtime acceptance test.

The initial helper commit left ordinary player acceptance open; the follow-up
below supplies movement/save/continuation evidence. Moving/rotating supports
remain deliberately excluded until attachment state is implemented.

Initial helper validation: all121 CTest cases passed and NXDK produced the
stock-profile XBE/ISO; native runtime evidence was added in the follow-up below.

## Ordinary player save capture and continuation

The first full-player replay exposed a second static-only gate in
scene_checkpoint_player_capture: restore staging supported the chunk, but save
capture still rejected reason4. Capture now uses the same combined support query
and checks full-body clearance against all live chunks before producing a save.
This does not relax the existing grounded/weapon-idle save profile.

`python tools/check_rubble_standing.py` reproduces ordinary process-local inputs:
rocket separation, walk from frame360, jump470, stop walking502 and save600.
Player settles at(-4.316600,0.495980,2.470174). Continuing200 frames produces
exactly the same checkpoint as800 uninterrupted frames. A second continuation
walks backward during frames20..99 and settles at(2.267623,-0.368479,2.470174),
again with byte-identical checkpoint versus uninterrupted play. No fixture
teleports the player, moves the chunk, or substitutes player collision spheres.
Retiring only the saved supporting chunk makes the load fail; this guards
against accepting a floating player without support. The below-camera chunk
need not contribute visible triangles from the close standing viewpoint.

Affected checkpoint-placement, scene-player-checkpoint and installed authored
scene tests pass after the capture correction. NXDK builds. Native standing
reload render-20260917-074907 completes200 frames with74 checks and4049 free
pages on stock64MiB. Native framebuffer inspected: elevated close view of the
post/roof, room, weapon and HUD; chunk support is below the camera. Checkpoint
and player/contact state match PC. Disc restored and owned emulator exited.
The jump approach itself is currently PC evidence; this native run starts from
its saved standing state.

Native walk-away reload render-20260917-075036 also completes200 frames and
passes74 checks with4049 free pages. PC/native final checkpoints and contact
state match. Framebuffer inspected: the camera has returned to floor height,
the tilted chunk remains ahead, and the room/weapon/HUD render normally. The
harness restored the disc and exited its emulator. No moving-body carry or
broader slope/edge acceptance is claimed.
