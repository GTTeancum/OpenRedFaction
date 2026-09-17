# Destruction capacity integration

The configured committed-cutter limit remains eight. This is distinct from the
original128 admission journal. No longer-session gameplay is claimed yet.

Added include/rf/geomod_limits.h as the shared cut-count and maximum RGCH size
definition. History maximum is28+1544*cut_limit; the1544 wire maximum is24
metadata bytes,60 vertices of20 bytes and20 faces of16 bytes. A compile-time
guard prevents expanding beyond the32-bit star mask representation.

Replaced independent eight-cut guards in retained-material digest, authored
owner extension, restored publication and journal import. Replaced12380-byte
limits/buffers in scene validation, edit transaction, core layout and affected
probe/test scratch. Scene transport maximum now aliases RF_CHECKPOINT_FILE_MAX.
The outer110524-byte cap intentionally remains independent: increasing cutter
capacity must still account for admission, chart, face and player payloads.

Seven rebuilt focused tests pass: geometry/interior, repeated cuts, history
validation, material digest, authored layout, owner extension and publication
candidate. Owner-extension boundary test now accepts the configured maximum
and rejects maximum+1 on both encode and decode, preserving outputs. NXDK
build passes; this mechanical refactor has no new native runtime acceptance.
Logs: artifacts/authored-post-live/capacity-{build,xbox-build}.log and
capacity-boundary-build.log.

Next: deliberately exercise16 cutters with retained old holes and save/reload,
measure chronological replay cost and transactional peaks, then decide the
geometry/tree/atlas budgets from those results. Existing eight-cut stress peaks
near1MiB, so a larger cut array cannot be assumed to fit the same core budget.
Do not evict earlier cuts or silently reset destruction to make room.


## Executed larger-profile probes

Added isolated rf_geomod_capacity_probe and rf_geomod_capacity_stress_probe
targets compiling their own geomod.c with RF_GEOMOD_CUT_LIMIT=16. The default
shared library and live PC/Xbox remain8. Do not mix a16-cut public struct owner
with an8-cut implementation; these targets call their own complete core.

The basic probe commits16 separated box recesses in both an outward solid and
an inward cavity. At every edit it queries all previous hole centers and
protected gaps against analytic expected ray fractions and checks edge closure.
Decode after15 followed by cut16 matches uninterrupted mesh, UVs, filters and
queries;17th cut rejects with identical serialized history and live geometry.
Both cases pass within1MiB per core. Cavity final peak937280 bytes, final
history9628 bytes. These are simple geometry tests, not full scene/native proof.

Original concave overlapping-template stress with16 slots rejects cut8 under
1MiB; allowing1088KiB passes8 at peak1052460 bytes. Requesting16 rejects cut9
with RF_RANGE at both1088KiB and2MiB. Therefore raising the memory allowance
alone does not resolve the ninth-cut failure. Output after8 is3888 corners and
770 faces, near the4096-corner/800-face owner limits. Exact ninth rejection
branch still needs instrumentation; do not claim which one fired from size alone.

Found a separate stale768-face guard in append_compact_lineage, while the
support-plane array already holds1024. Replaced with the actual array extent.
New internal boundary coverage appends800/1024 tracked faces, checks tags and
support IDs, then rejects overflow without changing counts. Five focused
geometry/UV/lineage tests pass; original-template ninth rejection persists.
NXDK build passes. No new native run or larger live profile acceptance claimed.

Logs under artifacts/authored-post-live: capacity16.log, capacity16-stress.log,
capacity16-eight-1088k.log, capacity16-sixteen-1088k.log,
capacity16-sixteen-2m.log, capacity16-supported-2m.log, and
capacity-support-{build,xbox-build}.log. RF_GEOMOD_STRESS_COUNT now accepts
6..configured limit; RF_GEOMOD_STRESS_BUDGET explicitly selects1..2MiB and
defaults to1MiB. Existing lower-face overflow checks remain enabled for8 cuts.


## Exact ninth-cut capacity and expanded workspace

Temporary source instrumentation (restored before committing) proves the
800-face owner rejects storage append at4022 existing corners plus3, with
800 faces already used. With1023 faces available, compaction rejects4094
existing corners plus4 at818 faces. Logs capacity-storage-trace.log and
capacity-corner-trace.log identify both actual branches rather than inferring
from peak memory. No trace prints remain in production.

Made RF_GEOMOD_WORK_VERTICES and RF_GEOMOD_WORK_FACES configurable together
with provenance arrays and guards. Defaults remain4096/1024 and8 cuts. Only
rf_geomod_capacity_stress_probe selects8192 corners/2048 faces/16 cuts and a
2MiB core test budget. Its two stress owners request2047/2048 faces, preserving
a separate owner comparison. RF_GEOMOD_STRESS_FACES sets this diagnostic
allocation; it does not alter scene settings. All compilation units consuming
these struct definitions in that isolated target use the same definitions.

Nine overlapping original-template cuts pass closed coverage,4225 junction
rays/body sweeps per cut,1734 independent nearest/short segment comparisons
and generated light-grid containment checks. Cut9 peak1871616 bytes. Native
scene publication/atlas/save memory is NOT included in this isolated proof.

The old coverage endpoint x=-32 lies inside the ninth cavity (minimum x
-34.6900673). Updated test segments extend beyond the actual mesh AABB when
needed, keeping the same directions, independent triangle reference and
short-segment no-hit check. This corrects an invalid outside-endpoint premise;
no collision tolerance or hit assertion was relaxed.

Cut10 commits at peak1884888 bytes and passes closure and4225 junction
ray/body checks, then fails light-grid containment: face818 sample0,1 edge1
signed distance -1.05151169637e-5 against -1e-5 acceptance. Sample is
(-33.2298813,-11.544426,-2.44844055). Ten face vertices, UVs and plane are
retained in artifacts/authored-post-live/capacity-ten-mesh.csv. Next correction
needs a small reproduction of this actual face, not a tolerance increase.
Later rays/remaining six cuts were not executed.

Five default-profile geometry/UV/lineage tests pass and NXDK default build
passes. No expanded native acceptance. Logs capacity-nine.log,
capacity-ten-capture.log, capacity-expanded-build.log, capacity-expanded-xbox.log.


## Tenth-crater sample correction and eleventh regression

Added a small captured-face regression with ten binary32 vertices and the
actual collision plane. Before the fix, sample0,1 reproduces exactly
-1.05151169637e-5 edge distance. It now passes every grid sample against all
edge halfspaces and the existing plane-distance tolerance.

rf_geomod_light_grid_sample previously found a nearest projected edge point
then reconstructed the dropped coordinate from an approximate float plane.
That moved a rounded endpoint outside another edge. Outside-grid samples now
retain all three coordinates interpolated along the selected actual edge;
inside-grid samples still solve the plane as before. No acceptance tolerance
was changed. Core default interior/repeated tests and the new boundary test
pass; NXDK build passes, no new native rendering acceptance in this update.

The extended original-template run now clears all cut10 geometry, junction
ray/body, light-grid and independent nearest/short-ray checks. Cut11 commits
and clears closure and junction queries, then fails a separate light boundary:
face1049 (five vertices), sample4,0 edge3, signed distance
-1.02452005325e-5, point(-35.5186882,-10.9924421,-3.93110824).
Saved full mesh capacity-eleven-mesh.csv and isolated light-boundary-eleven.json
under artifacts/authored-post-live. This is still an unresolved regression;
no16-cut success or expanded live readiness is claimed.

Logs: light-boundary-fixed-build.log, capacity-fixed-boundary.log,
capacity-eleven-capture.log, light-boundary-xbox-build.log.


## Rounding-aware sample containment; thirteen cuts

Extended the small boundary regression with the five actual vertices of
eleventh-cut face1049. It reproduces the full stress failure exactly before
correction. The sampler now tests its rounded point against face halfspaces.
An outlying point moves toward the polygon centroid by the required
intersection fraction plus a float relative-error bound for the final store.
Mesh vertices, UVs, collision and test tolerances are unchanged. Samples
already inside all halfspaces are unchanged. Both captured grids and the
default interior/repeated coverage tests pass.

The larger probe clears thirteen overlapping original-template cuts: closed
geometry,4225 junction ray/body probes,1734 independent nearest/short rays,
all light-grid containment checks and increasing excavation volume per cut.
At13 peak1940664 bytes; generated light sample count55872. These are core
allocations, not full scene memory or native acceptance. The placement ray
also now extends beyond the live mesh AABB, using the same helper as coverage
rays; its former fixed endpoint becomes interior after twelve cuts.

Cut14 rejects RF_FORMAT during cavity repair partition_polygon on face1248,
seven vertices. Temporary instrumentation reports collision_mesh_face edge
halfspace rejection (line1686 at this revision); no debug instrumentation
remains. Next work is to retain that actual polygon and diagnose its plane/
partition geometry without weakening collision tolerance.

Logs: light-rounding-build.log, capacity-placement.log, capacity14-trace.log,
light-rounding-xbox-build.log. NXDK default build passes; no expanded live
profile or native visual acceptance claimed.

Full PC rebuild and all106 registered CTests pass; see
light-rounding-full-build.log and light-rounding-full-tests.log.


## Isolated cut14 partition and rejected guard removal

Added rf_geomod_partition_capacity_probe (not CTest). It embeds the actual
seven vertices/UVs from failing face1248 and invokes the real partitioner;
exit2 reproduces the unresolved rejection. It enumerates valid ordered
vertex subsets to distinguish face validation from diagonal admission.
Current partition can write four pending pieces before rejecting; this is
disposable staging and the live edit still aborts atomically.

Corners4/5 share x=-41.7388382 and y=-8.34592628, with z=-8.83713913 and
-8.83714104. Parent support1596; outgoing edges1440,1720,1720,1720,1720,
1596,1440. Edge5 therefore denotes an earlier artificial partition diagonal.
These facts do NOT prove the two positions are one mathematical corner;
distance-only welding remains unjustified. Support1720 is plane
(0.545082092,0.831822276,0.104676485,30.6184616), parent1596 is
(-0.701539516,0.607983351,-0.371750712,-27.4924736).

The six-corner subset excluding5 and triangle4/5/6 independently pass the
face validator, but their proposed diagonal lies within the protected near-
boundary band. An experiment replacing the1e-12 squared-distance guard with
exact-zero rejection breaks edge closure on the FIRST original-template
stress crater. Restored the guard and rebuilt both normal and experimental
binaries; default interior/repeated-cut tests pass. No production source
change is included in this investigation.

Retained logs under artifacts/authored-post-live: partition14-capture.log,
partition14-support.log, partition14-subsets.log, partition14-diagonal.log,
partition14-isolated.log and partition14-restored-build.log. Next step: trace
which supporting edges generated the near-adjacent pair and preserve their
shared topology through clipping/repair rather than admitting thin slivers.


## Retained diagonal origins: cut14 fixed, fifteen cuts pass

Origin tracing shows both near-adjacent points come from the SAME artificial
diagonal, with one clipping call using its original span and the other using
a previously shortened span. Both have face/edge1596 and cut1720. Rounded
subdivision endpoints produce distinct intersections despite lexicographic
endpoint ordering. This establishes common construction, beyond mere proximity.

Added a bounded replay-local diagonal registry, exact canonical endpoint-pair
keys and separate uint16 support IDs. Repair assigns IDs to newly introduced
diagonals; later splits use the retained original endpoints and cut plane.
Edges inherited across repair retain the ID. Original material-plane support
and UV interpolation remain separate; no distance weld or diagonal-guard
relaxation. Registry storage is charged through the existing replay scratch
budget and freed before final collision binding. Default1024-record storage
is24580 bytes, extended2048-record storage49156 bytes, excluding its enclosing
support arrays/tag pointer. Capacity failure remains transactional.

An internal regression clips the captured long and shortened spans and proves
the generated position is bit-identical, with reverse endpoint registration
reusing the same ID. Public UV and legacy collision comparisons pass. Eight
original-template cuts pass the default1MiB budget (peak1021840). Fifteen
overlapping cuts pass the2MiB experimental profile (cut15 peak2023928).

Cut16 now commits but fails closed coverage: faces1512/1513 contain identical
three positions, with a small UV difference at one corner, and share an edge
in the same direction against face1382. Saved diagonal-sixteen-mesh.csv and
diagonal-sixteen-capture.log. This is the next open issue; do not claim16-cut
acceptance. The diagnostic snapshot partition probe remains historical: the
new construction prevents that old seven-corner face from being generated.

Reconstruction source policy advanced to3, fingerprint
6f3d7702a01da74e8a81a8745594bdca1cba9c378ffdb966dbde791cd4e9afb1.
All106 registered CTests pass after full rebuild. PC two-shot save/reload/
next-blast equals uninterrupted RFCP/RGCH/RGP. NXDK build passes.
Native render-20260916-201642 passes58 checks over200 frames;3884-byte Xbox
checkpoint equals uninterrupted PC, SHA256
5010a58fcb37b173fc9b0b01f0c496eda98478b9a7b550e32e3179f9b2a1ec41.
Endpoint free16.25390625MiB; disc restoration succeeded. Inspected framebuffer
contains textured hall/roof/supports, damaged post, water, launcher and HUD.
No retail-parity/audio/extended-profile visual claim.

Logs: partition14-origin.log, diagonal-origin-build.log, diagonal-live-eight.log,
diagonal-full-build.log, diagonal-full-tests.log, diagonal-native.log.
