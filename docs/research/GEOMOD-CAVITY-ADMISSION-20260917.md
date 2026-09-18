# Ordinary wall admission boundary

The supported ctf06 sources are finite wooden solids. Ordinary room walls need
cavity semantics: brush66 has flags2 and14 inward-facing authored faces.
The existing core provides cavity cuts, but the live ctf06 authored loader and
publication path are restricted to outward solids. Removing the UID guard
would not correctly integrate those walls.

`python -B tools/inspect_geomod_cavity_candidates.py` audits the installed
geometry and the955-brush editor export. It checks global unique face identity,
counts every room3 compiled face, and records per-owner topology, winding and
plane residuals. Output artifacts/geomod-cavity-candidates.json carries both
input hashes. The older additive census now marks both beams as supported.

Room3 has790 compiled faces:364 owned by flags2 air brushes,340 by flags0
solids,4 by flags1 portal brushes, and82 unowned faces, all carrying liquid
bit4. The six supported posts/beams own32 compiled faces. These are geometry
coverage counts, not a measure of overall engine or game completion.

Brush66 owns140 compiled faces. Its authored mesh is closed, planar and convex
with inward orientation. All140 compiled polygons agree with their authored
plane within4.76837158203125e-7 and winding dot product is at least
0.9999999999999999. The compiled subset has189 exact-edge boundary edges;
this raw-edge result can include T-junction partitioning and must not be
interpreted as189 independent holes. It does establish that simply treating
those unprocessed compiled polygons as a closed source is unjustified.

## First wall cut candidate

Center(-33,4,8), conservative radius1.1 lies inside compiled face5964, authored
face398, in room3. Every authored brush AABB was tested against the expanded
cut bounds; only air66 overlaps. The center also has more than1.1 clearance
from every compiled polygon edge. This isolates the first admission fixture
from other authored brushes. It does not prove runtime collision, portal,
visibility, material or repeated-cut behavior.

Next implementation:

1. Decode inward air66 with original authored vertices, UVs and source-face
   identities; retain compiled wall windows and metadata as separate records.
2. Use the existing cavity Boolean core for an isolated wall cut, proving
   untouched compiled partitions and room/portal geometry remain unchanged.
3. Publish retained wall windows and generated crater faces together, then
   compose collision with the rest of room3 rather than replacing the room
   with the full authored cavity shell.
4. Integrate atlas ownership, save identity/history and transactional rollback;
   validate the isolated cut and traversal on stock64MiB before broadening the
   spatial eligibility domain or adding overlapping/adjacent air volumes.

No runtime source admission changed in this audit. General ordered editor CSG,
room-to-room destruction, and campaign GeoMod remain open. Existing beam
checks cannot establish those behaviors.
