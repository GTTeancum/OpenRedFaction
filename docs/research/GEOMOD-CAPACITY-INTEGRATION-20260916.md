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
