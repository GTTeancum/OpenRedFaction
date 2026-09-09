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
the same code compiles with NXDK but is not yet called by the Xbox scene.

Live Mines has six geometries, 28 references and 17 unique slots: 16 loaded,
one missing. Its mover names add no images to the world table. On the current
32-bit PC build the bundle retains 2,118,288 bytes and peaks at 2,120,108 bytes.
The report is `artifacts/geometry-materials-verification.json` (untracked).
Both builds and four existing CTest checks pass. Next: remap projected vertices,
append visible mover meshes and use resident poses to update them per frame.
