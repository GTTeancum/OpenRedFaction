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
