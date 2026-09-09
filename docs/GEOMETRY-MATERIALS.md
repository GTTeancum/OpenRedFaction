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
