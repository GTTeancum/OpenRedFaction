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


## Implemented geometry decoder and first core cut

`rf_geomod_authored_cavity_decode` now decodes air66 from the actual editor
section into an owned inward source and separate compiled wall windows. It
reuses the strict brush parser, texture/source identity resolution, closed-edge
and convex-plane checks, packed allocation and transactional output contract.
Outward validation temporarily reverses the inward source; original corner/UV
order and inward plane signs are then restored. No synthetic shell is added.
The ordinary finite-solid selection API still rejects UID66.

Integration exposed additional ownership detail: six air66-derived faces belong
to rooms268/270/297/299, and six room3 faces carry positive portal metadata.
The cavity decoder excludes both groups. Its editable-window list contains134
ordinary room3 polygons, copied exactly with source identity, compiled reference,
position and UV. Four authored shell faces have no ordinary visible counterpart;
they retain sentinel references and inherit room collision state from a valid
visible fallback, rather than borrowing doorway metadata. Neighbor solids are
empty because this API loads geometry only, not an ordered-CSG neighborhood.

The installed-asset regression passes the14-face inward source to the existing
cavity terrain core. One original-template cut at(-33,4,8), scale1.05000007,
produces60 faces and20 vertices beyond X=-33.1. Before the cut, the collision-tree
visibility segment from(-32.9,4,8) to(-33.1,4,8) is blocked; afterward it is clear.
A control atY7 stays blocked. This is a core ray test, not player traversal or
live room publication. Loader ownership retains20912 bytes with1055159-byte
accounted peak on PC. Truncated input and one-byte-under-budget admission reject
without publishing an owner; copied compiled windows match every position/UV.

The scene and checkpoint identity APIs still do not admit this cavity. Wall
window/crater publication, collision composition, atlas/saves and spatial
eligibility remain required before playable wall destruction can be claimed.

Validation: all123 PC tests pass; stock NXDK compile/link, XBE conversion and ISO
creation pass. Logs: artifacts/cavity-source-{all-build,tests,xbox}.log. No native
wall runtime was launched because live cavity publication is not implemented.


## Compiled-window publication and full-room collision composition

`rf_geomod_publication_build_cavity` now publishes an isolated inward cavity.
It starts retained polygons from the compiled windows and clips against the
remaining cavity terrain, preserving compiled UVs rather than reinterpolating
from the large authored face. Generated faces keep core crater UV/provenance.
It rejects neighbor solids, neighbor surfaces and neighbor voids: ordered CSG
interactions still require a separate qualified implementation. Existing solid
and connected-source publication paths keep their previous clipping direction.

The first test of the solid publisher on cavity geometry found a retained-area
error: compiled face5631 expected164.049065 but reconstructed164.047924 even
without a cut. The cavity path fixes the unnecessary reconstruction rather than
loosening the area assertion. Its uncut139 convex output pieces retain exact
compiled position/UV vertices and preserve the area of each of134 input wall
windows. Extra pieces arise from the existing strict convex partitioner, which
preserves float-bent edge vertices; face count alone is not a coverage check.

After the isolated wall cut there are185 published faces/880 vertices. The
selected wall reference5964 loses area; all133 other retained wall windows keep
their area within.001 square units and keep source/material identities. Crater
faces retain owner66 and reference5964. Capacity rejection and unsupported
neighbor admission leave the prior output view and sampled output rows intact.

The installed-room integration test converts publication to collision faces,
then uses the same composition owner as the scene against the full original
room3 collision tree. All656 unselected face descriptors, filters and borrowed
vertex pointers remain byte-identical, including portals/liquids/other brush
surfaces. The complete candidate contains those656 faces plus185 replacements.
The short ray through the crater is clear and the Y7 control remains blocked.
Composition resident/peak accounting is292012/424049 bytes on PC, excluding
caller-owned replacement arrays and original world storage as documented by
that API. This is not a whole-Xbox-memory measurement.

This qualifies the publication/composition components on installed geometry;
scene selection, spatial eligibility, atlas binding, save identity and native
runtime admission are still pending. No live wall screenshot is claimed.

Publication validation: all123 PC tests pass and stock NXDK compile/link/XBE/ISO
passes. Logs: artifacts/cavity-publication-{all-build,tests,xbox}.log.
