# Exact unchanged mover projection reuse

The16:00 original L1S1 phase report still attributes2.978ms to CPU world
projection even with static geometry retained on the GPU. The same camera
position repeated229 of240frames. This source change removes repeated work
only when camera and mover poses are exactly unchanged; it is not evidence
of faster moving-camera gameplay and has not been measured.

With retained static-world admission, the CPU world prefix contains only the
immutable authored moving-solid meshes. Later actors, weapons and effects
append after that prefix. The new optional cache retains only the exact camera
and accepted mover pose/flags, source identities and output prefix length.
It reuses the existing vertex bytes in place, requiring no second vertex
buffer or copy. Current static-world GPU preparation, portal visibility,
listener, combat, actors, effects and simulation still run every frame.

Any bit change in camera position/basis, mover position/output basis/flags,
source owner, mapping owner, destination or prefix length forces ordinary
projection. The render-success0x10 marker is excluded from the key and is
published as before on a hit. Hidden flags remain effective. Active scope,
GeoMod, nonretained static world and diagnostic mode bypass and invalidate.
Successful whole-world restore invalidates explicitly. The source mover
geometry/material maps are immutable for their scene lifetime; generated
geometry never enters this cache.

Optional storage is52bytes per mover, capped at256movers (13KiB). Allocation
failure retains the original projector. There is no frame allocation or saved
state. The eight-word rf_scene_world_projection_cache records queries, hits,
misses, ineligible frames, pose bytes, prefix vertices, invalidations and
no-storage fallback. Source-written after16:00;17:00 parent runtime pending.
