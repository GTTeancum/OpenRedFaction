# Shared world and mover materials

`rf_geometry_materials_open` is new residency scaffolding for the shared
renderer, not a recovered original allocation policy. It accepts an ordered
list of loaded geometries (world first, then movers), reads local texture
names and deduplicates case-insensitively in first-use order. The renderer
must map local texture `j` of geometry `i` through
`slots[offsets[i] + j]`; `offsets[count]` is the total reference count.
World indices are not guaranteed to survive deduplication either.

The bundle owns its mappings and decoded images. Geometries and VPP archives
can close after loading. Missing textures retain explicit shared missing slots;
corrupt or unsupported found images fail loading, as in the ordinary material
loader. Source geometries must stay stable during the call. Failure preserves
the output; close a successful bundle before reuse. No image pointer is owned
by two slots. Budget accounting includes the owner, arrays, images and temporary
name storage/pointers, excluding allocator metadata and caller-owned inputs.

`python tools/verify_geometry_materials.py` verifies all 68 inventoried mover
levels (5,919 world/mover texture references), first-use name mappings and
missing-slot sharing. Live Mines additionally compares every decoded image's
dimensions/checksum, status and archive selection against the ordinary named
loader. The C probe checks exact and one-byte-short peak budgets, repeated
close and image access after geometry/archive closure. This is PC execution;
the same code now runs in the Xbox scene through `rf_scene_world_open`.

Live Mines has six geometries, 28 references and 17 unique slots: 16 loaded,
one missing. Its mover names add no images to the world table. On the current
32-bit PC build the bundle retains 2,118,288 bytes and peaks at 2,120,108 bytes.
The report is `artifacts/geometry-materials-verification.json` (untracked).
Both builds and four existing CTest checks pass.

## World/mover scene composition

`rf_preview_build_world` counts then fills a single vertex allocation, with
world vertices followed by each mover in file order. It remaps every emitted
local material slot through the shared bundle. Optional committed runtime
poses use their position/output matrix; otherwise movers use file poses.
The material probe checks combined vertex bytes against separate world/mover
builds and explicit remapping across all 68 levels, including exact and
one-byte-short mesh budgets. Runtime-pose drawing remains to be connected and
validated in the frame loop. Projection, shading and camera remain diagnostic.

`rf_scene_world_open` loads mover source geometry with a 1 MiB budget plus the
geometry-pointer array, builds shared materials and projected geometry, then
releases the source movers and mapping arrays. Both PC and Xbox scene modes
use this before actor composition. Xbox releases the previous static-only
mesh/materials before reloading, avoiding overlapping texture copies.

XEMU run `artifacts/xemu/20260909-100917-280394/report.json` passes on 64 MiB.
The world draw prefix grows from 7,215 to 7,455 vertices (80 mover triangles),
with 8,838 total vertices on the final actor-state frame. Available memory
after upload is 42,860,544 bytes; after CPU mesh release, 45,383,680 bytes.
PC framebuffer comparison passes: 11 pixels exceed a channel error of three,
with mean maximum channel error 0.0318. The image is identical to the prior
miner-camera capture because the added geometry is occluded. It was not
reposted. Next: inspect a door directly, retain mover render geometry and
update it from committed runtime poses during rendering.

## Door inspection camera

`rf_scene_preview_mover_camera` centers a diagnostic view on the selected
mover's local vertex bounds, transforms its thinnest axis into world space,
and constructs a world-up camera. Signed distance chooses the viewing side.
This is inspection scaffolding, with no gameplay camera placement or camera
collision. A vertical viewing axis is rejected. Temporary mover data closes
after camera construction.

PC flag `--scene-door-states-last` selects mover 8544 at distance six, keeping
the usual scene command's actor UID 9858 and asset arguments. On Xbox, stage
an empty `door-view.flag` beside the existing scene/stream/states flags before
rebuilding the ISO. Run `python tools/xemu_smoke.py --door-view --reference
artifacts/door-scene.ppm` after generating the matching PC reference. Remove
the optional door flag and rebuild to restore the miner camera.

Native XEMU capture
`artifacts/xemu/20260909-101314-075642/framebuffer.png` shows the textured door
and surrounding frame/rock. Its report passes stock 64 MiB and PC comparison:
964 triangles, 123 pixels exceeding channel error three, mean maximum channel
error 0.1175. The actor remains outside this camera's view while its diagnostic
state schedule runs. This shows authored static poses, not visible door motion.
Fine diagonal artifacts are visible on the door surface in both backends;
their cause and PS2 fidelity remain open. Rendering must next retain local
geometry and update from committed resident mover poses in the frame loop.

## Reusable projected mesh

`rf_preview_update_world` uses the world builder's validation/counting pass,
then writes into a caller-owned vertex allocation. It allocates no memory and
retains the buffer address; `mesh.count` and `mesh.bytes` describe the used
range, while the caller tracks capacity separately. Source geometry, poses,
camera and material mappings must remain stable and must not alias the output
through both passes. Capacity/validation failures preserve the old frame.
The ordinary allocating builder shares this implementation. Visible faces now
reject out-of-range local texture indices during counting, before any writes.

The material probe now compares three runtime pose configurations on all 68
levels (204 cases) against separate projection and explicit material remapping.
It uses authored transforms, then shifts of [1,2,3] and [2,4,6], keeping one
allocation throughout. All unused pose fields are poisoned, so rendering must
read the committed position/output matrix. Late invalid poses and a one-byte-
short capacity must leave mesh metadata and previous vertex bytes untouched.
The latter uses empty metadata to exercise counting rather than the old-size
precondition. These are PC checks; the new updater is compiled for NXDK but
has not yet replaced the scene's authored static projection or driven visible
motion. Retain the mover source/mappings and connect it to the frame loop next.

## Retained scene geometry owner

`rf_scene_world_open_retained` now exposes `rf_scene_world_geometry`, which
owns the mover payload/index arrays and local material mappings, borrows the
world geometry, and copies the inspection camera's position/orientation.
Images and the initial projected mesh are separate outputs with separate
ownership; later actor material appends cannot invalidate the retained mapping.
`rf_scene_world_update` rebuilds into a caller-owned allocation using a pose
array in mover order, checking its count. It performs no archive access or
allocation. The world geometry must remain alive. Closing the owner does not
close images or the projected mesh. `rf_scene_world_open` remains a wrapper
that closes the owner immediately after projection.

`python tools/verify_retained_scene.py` verifies 68 levels and 204 pose cases
after closing the level archive, poisoning the source level and closing the
material images. It compares the reused mesh against direct projection with
an independently saved camera and checks repeated owner closure. Maximum
retained owner/mover/mapping allocation is 307,984 bytes; the borrowed world,
images and vertex allocation are separate. This is PC lifetime evidence and
NXDK compilation, not Xbox retained lifetime or rendered movement.

For live door rendering, keep this owner in Xbox main through archive closure
and update from resident collision/controller poses. The GPU stream must track
its allocation capacity independently of the current world draw boundary:
clipping can change world vertex counts as doors move. The current renderer
still rejects a boundary change, so that integration remains open.

## Rendered door motion

Xbox scene loading now keeps the render geometry owner after initial actor
inspection and archive closure. `door-motion.flag` enables drawing from each
committed pose update. The CPU buffer and GPU buffer each retain their initial
1,210,528-byte capacity; current world vertex count may change with clipping.
The GPU stream checks current used bytes against its saved allocation capacity
instead of rejecting a changed world/actor draw boundary. The final CPU mesh
remains resident for native diagnostic inspection.

The visible harness initially used the old 0.25-second, panel-by-panel logic;
this caused the reported stepping and separate opening. Visible motion now
activates all four translation panels on the first tick, updates every panel
at 1/60 second, and renders after all commits. Default duration is 600 frames;
optional `door-motion-frames.txt` selects an endpoint from 1 to 600. This is a
fixed-step inspection schedule, not recovered trigger pairing or a real-time
gameplay clock. The non-rendering legacy motion check remains available.

`verify_door_cycle.py --smooth` compares 600 ticks for each of four real doors
against the original and compiled NXDK, including the exact float 1/60 time
step and integer millisecond timer values. All 2,400 ticks pass. XEMU's monitor
interleaves these PC traces in frame/group order, checks controller/collision
hashes, and checks every projected mesh against `rf_scene_check --pose-world`.
Frame counts and used/capacity bytes are validated independently.

Cross-backend mesh comparison exposed one frustum-edge vertex that differed
by 1/16 pixel and a few float bits. The diagnostic clipper now explicitly
rounds plane distances and interpolation stages to float precision on SSE
and x87. This fixes the mismatch without weakening the hash check. The prior
camera/material inspection relied only on framebuffer tolerance; this new
check verifies the pre-upload vertex stream too.

Open-door capture/report:
`artifacts/xemu/20260909-103453-382443/` passes on stock 64 MiB, with 120 rendered
steps, 480 controller ticks, 2,811 final vertices, mesh trace `eac34fab`, and
final mesh `2f4d4d70`. PC framebuffer maximum channel difference is one, with
no pixels above three. Retained mover source/mappings consume 21,265 bytes;
available memory with the CPU mesh retained is 44,343,296 bytes. The capture
shows both panels moving away from the doorway.

The full 600-frame synchronized cycle also passes in stock 64 MiB XEMU:
`artifacts/xemu/20260909-103538-711185/report.json`. Every mesh trace and all
2,400 controller/mover steps match PC; no additional screenshot was taken,
because the cycle returns to the previously shown closed position.

For an open-door reference, generate the pose sequence with
`xemu_smoke.py --door-motion --door-motion-frames 120 --no-capture`; it writes
`artifacts/door-render-final.bin`. PC preview mode `--scene-door-motion-last`
uses that pose-file path in the usual scene actor-UID argument position.
Pass its PPM output to the smoke tool's `--reference` for native comparison.
The flag file and expected frame argument must match. Rebuild the ISO after
changing staged files (remove only the generated ISO first if make considers
it current). Captures should only be requested for a new visible result.

Still open: actual trigger/event pairing, obstruction, sounds, runtime actor
updates alongside moving geometry, gameplay-clock scheduling, rendering
performance and the existing fine surface artifacts. The entire world is
currently reprojected for each diagnostic frame; this is not the final Xbox
visibility or rendering architecture.
