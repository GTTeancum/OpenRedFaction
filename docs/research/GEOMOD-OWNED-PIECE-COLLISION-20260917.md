# Owned piece collision polygons

The piece bank now also owns contiguous local collision positions and bound
face records. Collision binding completes before the new entry/counts become
visible. These arrays remain valid through replay scratch reuse and subsequent
appends, and fit inside the bank's single checked allocation. Pointer-bearing
records precede float/word arrays for32-bit Xbox and64-bit PC alignment.

The existing `rf_collision_flat_faces` and contact-world conversion already
provide translated/rotated queries for this representation; no parallel ray
or sphere collision implementation was added. The extracted-replay fixture
checks all faces of both owned pieces in identity and rotated/translated poses,
using rays and radius0.25 sweeps. Expected approach fractions are0.5/0.4375;
hit face IDs and transformed normals agree. Misses preserve result storage.
Checks repeat on the independently reconstructed piece bank after reload.

A deliberately nonplanar quad fails collision binding after mesh staging. The
bank count and earlier entry pointers/geometry remain intact, and contact
queries on that earlier piece still pass. Across the test there are136 accepted
contact checks and five miss controls. Capacity, duplicate-ID and exact-budget
checks remain enabled. The128-corner/32-face/4-entry bank now reserves8296 bytes
on PC, replacing the geometry-only4304-byte figure; this is not a native memory
reading. No tree or unbounded append allocation is introduced.

Validation: geomod_extracted_replay passes; stock NXDK compilation succeeds.
This supplies stable collision geometry for object integration. It does not
simulate gravity, motion, mass/inertia, contact response, sleeping, rendering
or active scene registration, and production extraction is still disabled.
