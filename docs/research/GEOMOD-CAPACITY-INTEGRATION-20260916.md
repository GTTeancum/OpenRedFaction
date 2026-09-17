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

## Exact duplicate repair and sixteen-cut mapped continuation

Cut16 repair emitted the same triangle twice under the same support plane,
material, source face and birth tag, with slightly different interpolated UVs.
Chronological repair now retains the first exact-position, same-winding cyclic
copy. Different support/material/birth or reversed winding remains separate;
there is no proximity weld. Compacted face offsets and lineage move together.
Focused preservation controls pass and retained UV bytes stay unchanged.

The expanded 8192-corner/2048-face/16-cut diagnostic passes all sixteen
overlapping cuts, closed edge coverage, increasing excavated volume, ray
coverage and light-grid containment. With 256x256 material mapping, peak core
memory is2040896 bytes, below the2097152-byte budget. A fifteen-cut RGCH
reload reproduces all mesh bytes; its sixteenth cut matches uninterrupted
vertex/face/UV and19868-byte history output exactly. The initial continuation
probe reused a collision-hit variable overwritten by junction queries; the
probe now snapshots the original blast center before those queries.

Source reconstruction policy is4, fingerprint
27e71b148896e2cc58d25fd03b6745f3c8554caa264e3ba8c2377db8dd788a66.
Older authored identities are intentionally rejected. Full PC build and all106
registered tests pass. NXDK build passes (existing .edata merge warning).
This is core capacity evidence, not live sixteen-cut Xbox acceptance: live
publication, atlas, save limits and stock-memory integration remain open.
No new native visual validation is claimed for policy4 in this update.

Evidence: artifacts/authored-post-live/sixteen-mapped-restart-fixed.log,
duplicate-full-build.log, duplicate-full-tests.log and duplicate-xbox-build.log.

Policy4 live PC two-shot restart also passes: restored next-blast RFCP, RGCH
and RGP match uninterrupted output (duplicate-restart.log).

## Expanded save-envelope capacity prerequisite

Publication face/vertex limits now live in geomod_limits.h and can be configured
consistently across a build. Authored layout validation uses that face limit.
RF_CHECKPOINT_FILE_MAX is likewise configurable, retaining110524 by default.
Isolated tests compile the actual layout/transport implementations with16 cuts,
2048 publication faces and131072 transport bytes; production defaults stay put.

The maximum RGCH24732 bytes plus128 admission rows,1024 map rows and2048 face
bindings occupies125500 RFDS bytes, or126076 including the576-byte composed
header/player record. This cannot fit the old110524-byte transport. The new
128KiB test profile admits and reads that entire layout, rejects excess counts
without changing output, and tests full-size disk payload store/load, truncation,
corruption fallback and write failures. Transport testing uses synthetic TEST
payload bytes; it does not prove live authored content or Xbox restart validity.
All108 registered tests pass. Evidence: capacity-save-full-build.log and
capacity-save-full-tests.log under artifacts/authored-post-live.

Remaining live dependencies: scene source vertex/sorted-index capacity, render
face and insertion budgets, atlas map residency, publication/digest buffers,
collision composition, reload staging, tooling transport bounds and stock Xbox
memory/visual acceptance. No shipping16-cut or native expanded-save claim.

## Expanded actual scene subdivision

The scene draw adapter now has configurable source-vertex, face, output-vertex
and byte bounds, with the existing shipping values unchanged. Sorted indices,
preflight, publication and lighting-stage source checks share the source limit;
a compile-time guard preserves uint16 indexing and output >= input capacity.

An isolated CPU test executes scene.c with8192 source vertices,2048 faces,
16384 output vertices and1MiB draw budget. A4500-vertex/1500-face input includes
a late-index T-junction on the first face. Preflight leaves the draw owner
unchanged; publication emits4501 vertices, interpolates the owning face UV to
0.5 (not the unrelated source UV99), preserves all other polygons/materials/
source IDs and source bytes, sorts every input index, and clears borrowed
collision vertex pointers. Over-capacity rejection preserves the prior owner.
Actual configured draw accounting is663572 bytes on this PC build. This is not
a total scene/staging budget or an Xbox allocation measurement.

All109 CTests pass; default NXDK build succeeds with its existing linker merge
warning. Logs: capacity-draw-build.log, capacity-draw-full-build.log,
capacity-draw-full-tests.log and capacity-draw-xbox-build.log under
artifacts/authored-post-live. No frame/GPU/native visual claim for this CPU test.
Atlas mapping, digest tables, terrain owner budgets and reload coexistence still
need coordinated expansion before enabling the larger profile in gameplay.

## Expanded publication/material save validation

Publication digest limits now use configured face/vertex limits; retained
material digest uses the same face bound. Scene digest chart/face tables and
checkpoint writer/reload face-map tails use that bound too. Collision digest
scratch retains1280 extra rows beyond the configured publication capacity
(default total2048 unchanged). Digest scratch budget is explicitly configurable
but stays512KiB by default: expanded owners must still pass a measured budget.

Isolated2048-face/8192-vertex digest builds process2048 faces and6144 corners.
Changing the last UV changes the publication hash; an invalid final chart
rejects without modifying output. Changing the final retained map binding
changes the material hash, while an invalid binding or excessive face count
rejects atomically. Existing independent canonical hashes still pass for both
profiles. These are synthetic capacity/validation tests, not authored geometry
or texture residency acceptance. The1024-map atlas journal remains unchanged.

All111 registered tests pass after full build. Default NXDK builds successfully
with the known linker merge warning. Logs: capacity-digest-build.log,
capacity-digest-full-build.log, capacity-digest-full-tests.log and
capacity-digest-xbox-build.log under artifacts/authored-post-live.
Shipping remains8 cuts. Next: coordinate expanded core/publication owners,
measure atlas and concurrent reload scratch, then validate actual extended
craters on PC and stock64MiB XEMU before activating the larger live profile.

## Integrated opt-in profile: live authored continuation passes

CMake RF_GEOMOD_EXPANDED_PROFILE=ON now compiles the full PC executable and core
consistently:16 cuts,8192 core/publication vertices,2048 faces,128KiB transport,
2MiB core budget,1MiB draw/digest budgets,2MiB writer ceiling and16MiB conservative
destruction ceiling. Default profile remains unchanged. Scene core creation,
edit clones, checkpoint restore and reservation checks share these configured
budgets; source vertex capacity also enters the existing scene checkpoint ID.
This experimental ceiling is not a claim of stock Xbox free RAM.

The first full level load exposed the atlas owner budget: larger face tables
require1335352 bytes, beyond the old1280KiB allowance. Expanded profile now
allows1536KiB while preserving512x512 atlas images,1024 journal maps and material
sampling rules. No extra atlas page or altered visual policy was introduced.

Build: cmake -S . -B build/pc-expanded -G "Visual Studio 17 2022" -A Win32
-DRF_GEOMOD_EXPANDED_PROFILE=ON, then build rf_pc_play Release.
check_authored_restart.py accepts --build-dir and --output-dir so experiments
use separate executables/results without disturbing ordinary test artifacts.
Expanded two-shot and reset-zero continuation both match uninterrupted RFCP,
RGCH and publication bytes. One-cut measured publication peak3150281 bytes,
draw663572 bytes and conservative destruction reservation12229376 bytes.
These small authored cases prove integration, not sixteen live crater acceptance.

Inspected native PC raster artifacts saved.png/control.png in
artifacts/authored-post-live/expanded-continuation: textured hall/beams, water,
post destruction/debris, weapon and HUD are present. Blast smoke obscures some
of the saved view. No original screenshots, host input, audio or Xbox visual
claim. No GitHub images added. Expanded-reset-continuation holds reset results.
All111 default CTests pass and default NXDK builds (existing merge warning).
Logs: integrated-capacity-build2.log, integrated-capacity-restart2.log,
integrated-capacity-reset.log, integrated-default-tests.log and
integrated-default-xbox.log under artifacts/authored-post-live.
Next: longer live blast coverage, atlas exhaustion/rollback, and expanded
stock64MiB XEMU memory/visual/save acceptance before enabling by default.

## Ordinary live rockets: journal exhaustion fixed, fifteen cuts reached

check_expanded_geomod_live.py generates ordinary process-local DEV rocket input,
records executable SHA, input, trace, RFDS, physical mesh and atlas audit, and
requires the requested committed count. The16-press run launched14 rockets and
committed13 cuts. A20-press run with the1024-map profile aborted when map count
hit1024 (atlas cursor410,143, so image area was not the limiting resource).
Legacy cavity edits publish core before lighting; failed atlas binding therefore
aborts the run rather than preserving the prior complete scene. This remains
an explicit rollback task, not a solved property of the expanded profile.

Added shared RF_GEOMOD_LIGHTMAP_LIMIT, default1024, for scene, authored import,
layout and material validation. The opt-in profile now uses2048 records and a
256KiB transport;512x512 atlas image remains unchanged. Expanded maximum layout
and transport tests follow these settings. Atlas owner measures1433656 bytes.

Twenty presses now complete2500 frames:18 rockets/admissions,15 committed cuts,
1347 map records,7844 physical and7856 draw vertices. Core peak2056404 bytes.
Rejections: frame517 RF_FORMAT/admission4, frame1994 RF_FORMAT/admission15,
frame2333 RF_RANGE/admission18. The sixteen-cut target remains FAIL. Retain
artifacts/geomod-expanded-maps for further topology/capacity investigation.

A no-fire fresh-process replay reloads all15 cuts/18 admissions. RFDS141400
bytes and physical mesh181564 bytes match exactly, and upper640x320 world
pixels match. Atlas audit metadata matches but seven maps differ in packed
pixels; this is unresolved (do not claim complete atlas equality). Comparison
JSON and restored artifacts: artifacts/geomod-expanded-maps-restored.
Inspected final PC frame: enclosing textured room, dark crater opening, launcher,
pickups and HUD present; crater readability/lighting parity is not established.

Default111 tests pass and NXDK builds with existing merge warning. Logs under
artifacts/authored-post-live: expanded-map-live.log, expanded-map-restore.log,
expanded-map-default-tests.log, expanded-map-xbox-build.log. No new native
expanded-profile execution or GitHub screenshots. Production stays8 cuts.

## Long-reload lightmap mismatch fixed: include padding in light admission

Dynamic light selection used only polygon min/max, but pixel evaluation samples
an affine rectangle with padding beyond those bounds. A transient light could
cause a map refresh that also picked up a steady light outside the polygon's
bounds. Fresh reload selection skipped that same map, making retained pixels
history-dependent. Selection now unions the original bounds with the four
actual sample-rectangle corner positions from rf_lightmap_sample_position.
The affine image/plane mapping makes those corners sufficient for its bounds.
Old/new lights still use the existing mark predicate; pixel math, UVs, atlas
placement, seeds and geometry are unchanged.

Repeated the identical20-press/15-committed-cut run and fresh no-fire reload.
All1347 atlas CSV records now match byte-for-byte, as do141400-byte RFDS and
181564-byte physical mesh; upper640x320 world pixels are equal. Inspected the
restored native PC raster: enclosing room textures, dark crater opening,
weapon/pickups/HUD are present. Different weapon ammo is expected because RFDS
is destruction-only; no claim of whole-player state continuity or visual parity.
The two RF_FORMAT rejections and final RF_RANGE rejection remain open.

Evidence: artifacts/geomod-padding-control and geomod-padding-restored, including
comparison.json; light-padding-live.log, light-padding-restored.log,
light-padding-tests.log and light-padding-xbox-build.log under authored-post-live.
Default111 CTests pass, NXDK builds with existing linker warning. Native expanded
profile still untested. Harness --compare-to now requires exact saved state,
physical mesh and atlas equality when used for continuation/reload regression.

## Offline live-admission reproduction

Added rf_geomod_live_history_probe (diagnostic, not a passing geometry test).
It reads an RFDS1 destruction checkpoint, the installed glass_house.rfl source
and RFCT template, then replays every recorded admission with the original
orientation RNG, including admissions whose CSG commit failed. It accepts only
the unconstrained fixture (all shallow vectors zero). It uses the actual
chronological implementation and checks every committed cutter's exact vertex/
UV/kernel bytes against RGCH, plus final cutter count and saved RNG state.

The initial probe mistakenly passed recorded scale to the radius convenience
API; corrected to rf_geomod_terrain_cut_template_scale before drawing conclusions.
The corrected probe reproduces live RF_FORMAT at admissions4 and15 and RF_RANGE
at18, with15 committed cuts/7844 vertices/1542 faces. Re-running chronological
preparation directly returns the same errors: these fail before collision bind.
All serialized committed cutter bytes and the final RNG match the captured run.
Probe memory peaks differ slightly from scene because source filter/material
identities are reconstructed for geometry analysis, not a renderer binding.

Command: build/pc/Release/rf_geomod_live_history_probe.exe Installed_Game
build/data/geomod-template.bin artifacts/geomod-padding-control/state.rfds
Evidence: artifacts/authored-post-live/history-probe-build.log and
history-probe.log. No production geometry change or new native acceptance.
Next investigation can isolate reconstruction stages on this seconds-long
probe instead of rerunning2500 rendered frames for each candidate change.

## Center-fan partition provenance fixed; long-sequence closure still open

Offline instrumentation (artifact-only copy of geomod.c) localizes both
RF_FORMAT failures to repair requiring every emitted corner to occur in the
original polygon. The partition fallback legitimately creates an interior
center and triangular spokes. Captured face357 has7 boundary corners and new
center(-22.1371574,-11.1760864,5.28068638); face1179 has14 corners and center
(-33.4668045,-10.6334944,-4.51836681). Missing boundary membership is not malformed
geometry in these cases. Repair now registers either interior-endpoint edge as
a retained-endpoint diagonal; boundary edge handling is unchanged.

Focused captured7-corner regression emits7 triangles/21 vertices, keeps birth7
and support plane0, and verifies all7 shared spoke IDs match across opposite
triangle sides. First4 ordinary live cuts now commit; saved mesh1932vertices/
383faces passes independent closed-edge coverage. Inspecting the native PC
raster confirms the room, crater opening, launcher/pickups/HUD, without claiming
retail-quality crater lighting or full gameplay parity.

The20-press run commits its first15 admissions, then rejects16..18 with RF_RANGE;
no RF_FORMAT remains in this run. It has7929 vertices/1563faces. Critically, its
snapshot FAILS closure with overlapping edges near faces1110/1180/1185. The prior
unfixed fifteen-cut capture also FAILS closure. Neither long sequence is accepted
as watertight; this is the next topology task. Reload exactly matches141618-byte
RFDS, physical mesh and1349-map atlas audit (--compare-to checks all three).

Authored reconstruction policy5 fingerprint:
2ba1d56eceb5220d02787435e3580d05bc02d43a80a8a609b9ada67d3d6c095b.
Default111 tests and authored two-shot restart pass; NXDK builds with existing
merge warning. No expanded native run. Logs under authored-post-live:
core-trace/trace.log, fan-lineage-build.log, fan-four-closure.log, fan-closure.log,
fan-prior-closure.log, fan-restored.log, fan-full-tests.log,
fan-authored-restart.log and fan-xbox-build.log. Live artifacts use geomod-fan-*
folders. Source geometry guards and tolerances were not relaxed.

## First live closure failure isolated at admission13

Offline live-history replay can export each committed RGM1 with
RF_GEOMOD_PROBE_MESH_PREFIX and run the established numeric coverage check after
every admission with RF_GEOMOD_PROBE_CLOSURE=1. Coverage storage is configurable
for8192 vertices/2048 faces without changing default tests or tolerances. The
probe still validates all committed cutter bytes and final RNG; requested
closure failure returns exit2, rather than reporting a successful capture as
geometrically correct. This is diagnostic-only, not a shipping geometry change.

Both sequential independent snapshot checks and integrated prefix checks find
cuts1..12 closed and13..15 nonclosed. At13, face1107 is an isolated triangle with
edges3.20e-6..1.14e-5 long near(-35.36415,-10.33717,-4.23164). A second defect has
three-way edge coverage around faces1110/1111/1168/1170. These differ from the
exact same-position duplicate triangle case repaired earlier.

Artifact-only instrumentation of repair inputs shows isolated triangle source
face1107 already arrives with3 corners, support plane1444 and edges1569,1573,
1610. The overlapping source face1168 already arrives as a triangle, plane1480,
edges1448,1448,2154, emitting output1170. Its vertices are
(-34.4876213,-11.0356483,-5.15673876),
(-34.5319481,-11.0991201,-5.11398983),
(-34.5609932,-11.1407175,-5.08597326).
Thus repair/partition does not first create these two triangles; investigate
tracked clipping/compaction input and supporting-plane intersections next.
No epsilon weld, area cutoff, guard relaxation or speculative deletion added.

Evidence under artifacts/authored-post-live: prefix-01..15.mesh,
prefix-01..13-closure.log, closure-prefix-combined.log (expected exit2),
overlap13/trace.log. Rebuilt dependent solid/public-UV regressions both pass.
No new Xbox build or visual acceptance is needed/claimed for this probe-only
change. Live defect remains open despite successful ordinary replay completion.


## Exact internal cutter edges remove the isolated cut13 triangle

Internal tetrahedron faces include the stored cutter kernel as their third
vertex. Previously corner_seed_edge retained only their two outer vertices,
so two internal faces sharing an outer vertex and kernel could not recover
their exact common edge. Retaining the kernel lets the existing canonical
line/plane intersection replace a rounded three-plane solve. No clipping
threshold, area cutoff or proximity weld was added.

The work owner grows by96 bytes for the default8-cut profile and192 bytes for
16 cuts; existing sizeof-based accounting includes this storage. Both public
star preparation and terrain replay populate it. The manual chronological
solid test replay now copies the kernel alongside planes and counts.
A focused regression checks the internal edge intersection and bit-identical
results when supporting face order is reversed.

Offline replay of geomod-fan-live/state.rfds still matches every committed
cutter position/UV/kernel and final RNG. Cuts1..12 pass closure. The isolated
triangle is removed, but cut13 still fails three-way edge coverage around
(-34.51,-11.07,-5.14). At15 the output is7927 vertices/1562 faces; admissions
16..18 remain RF_RANGE. This is a partial topology fix, not acceptance of the
long replay. The next investigation is tracked clipping of the remaining
near-collinear triangle with repeated supporting edge1448.

Authored reconstruction policy6 fingerprint:
3717136524d694fbba2e429c5a921633853f1a7a1f422cf293f8b6c7283f764c.
Earlier authored saves are intentionally rejected by source identity.

Validation: default PC build and all111 tests pass; NXDK produces default.xbe
and the ISO with the existing .edata merge warning. PC authored two-shot
save/reload then next blast matches uninterrupted RFCP(3884 bytes), RGCH(2508)
and RGP(1376) exactly. No new native Xbox run, expanded live rerender or visual
acceptance is claimed. Evidence: artifacts/authored-post-live/kernel-edge-*
logs and kernel-edge-continuation/report.json.


## Supporting-edge partition guard closes admission13

Tracing the entire chronological reconstruction changes the earlier diagnosis:
the remaining triangle predates the final repair, but an earlier repair in the
same replay creates it. On face plane1480, repair expands4 corners to10. Three
points along support1448 are slightly noncollinear after float storage. The
numeric diagonal guard accepts a chord and emits that boundary chain as a
thin triangle. Later repair reinserts its middle point into the neighboring
face, causing three-way edge coverage.

Partitioning with retained provenance now rejects candidate diagonals when
either boundary chain has one known supporting edge distinct from the face
plane. This uses the mathematical line identity, not a larger epsilon or
area cutoff. Alternate partitions keep the original vertices and UVs. The
legacy partition path without provenance retains its numeric behavior. A
regression fixture contains the10 captured positions, requires no output face
made solely from the straight run, checks valid pieces, and retains every
original vertex including its UVs.

The actual offline replay now passes closure for admissions1..13. Admission14
has an unmatched0.194811786 edge from(-33.929184,-11.5151424,-4.45199156) to
(-33.9995575,-11.400835,-4.59317589), continuing at15. At15 there are7930 vertices
and1564 faces;16..18 still reject with RF_RANGE. All committed cutter bytes and
final RNG match the captured history. Do not interpret this as a watertight
15-cut result or native expanded-profile acceptance.

Authored reconstruction policy7 fingerprint:
f61d8e8261a9f499009a86cde7038f4fe735a3396bc83f07e2ac04b71dc45bfd.
Default build completed;110 tests passed in the full run, with only the stale
identity fingerprint failing. After updating that expected policy fingerprint,
the manifest test passed too. PC authored restart/next-blast RFCP, RGCH and RGP
match uninterrupted results. NXDK build succeeds with the existing merge
warning. No new Xbox runtime or visual acceptance is claimed.

Evidence: artifacts/authored-post-live/overlap-kernel/trace.log and guard.log
(artifact-only instrumented builds), support-run-closure.log (shipping source,
expected closure failure at14), support-run-full-tests.log,
support-run-xbox-build.log and support-run-continuation/report.json.


## Shared diagonal repair closes the fifteen-cut history

The admission14 unmatched edge is a T-junction on recorded diagonal2163,
face plane1480. Face1180 has the shortened segment while face1184 has the
longer opposing segment. Their midpoint separation is1.06905975e-6, beyond
the independent closure check's1e-6 threshold. Both faces exist; this is not
a missing surface. The repair previously skipped all partition diagonals.

Repair now includes exact retained points with the same face/diagonal ID,
using the existing ordered insertion and face-local UV interpolation. No
geometric proximity merge or tolerance increase is needed. A focused fixture
checks insertion of a rounded crossing into the unsplit neighbor, with the
receiving face retaining its UV seam. Unknown supports and self-plane supports
remain excluded. The diagonal registry already provides stable edge identity.

Shipping-source offline replay passes closure at every committed admission1..15
and matches the captured cutter bytes and RNG. At15 the mesh has7942 vertices,
1565 faces and a2041816-byte peak;16..18 return RF_RANGE, with2054752 peak.
The capacity rejection is still open. This evidence is numeric history replay,
not a new ordinary live run or stock64MiB Xbox runtime acceptance.

Authored policy8 fingerprint:
9f7a490e6c302611d8bf3293b59dc470c8d17f6f8fb889f3aacc2a0b5a7ae8b4.
Validation:110 tests passed in the full run; the sole stale identity expectation
was updated for policy8 and its targeted test then passed. Authored two-shot
restart/next-blast matches uninterrupted RFCP/RGCH/RGP. NXDK builds successfully
with the existing linker merge warning. Shipping capacity defaults remain8.

Evidence: artifacts/authored-post-live/edge14/trace.log and repair.log,
diagonal-repair-closure.log (exit0), diagonal-repair-full-tests.log,
diagonal-repair-xbox-build.log and diagonal-repair-continuation/report.json.
Next: rebuild the expanded ordinary PC scene, verify the long live geometry
and save/reload, then assess native stock64MiB residency and cut16 capacity.


## Ordinary PC gameplay reaches sixteen cuts with closed geometry

Rebuilt the expanded scene from04fe1823 and executed20 trigger pulses over2500
frames through the ordinary process-local rocket/DEV room path. Eighteen rockets
produce16 committed cuts; the last2 reject at the configured16-cut history cap.
The new impacts differ from the old history after geometry repairs, so this
result does not mean the old sequence's capacity problem is solved.

The original harness invocation expected15 and correctly flagged the count
mismatch (actual16). Its report remains FAIL rather than being rewritten.
Independent inspection of the physical mesh passes closure:7784 vertices,
1541 faces. Replaying the new recorded history passes closure after each of
all16 admissions and matches all cutter bytes and final RNG. This provides
independent evidence for the actual16-cut outcome despite the stale harness
expectation. Recorded live core peak is2056596 bytes; the offline probe peak
is2035936. Treat these as different measured paths.

A no-fire reload using expected16 and --compare-to passes:135598-byte RFDS,
physical mesh and all1267 lightmap records match exactly. The restored report
is PASS. The captured640x480 endpoint was inspected: textured room/floor,
launcher, HUD and dark excavated area are present. This is endpoint inspection,
not animation, audio or full visual fidelity acceptance. No screenshot was
uploaded to GitHub and no native Xbox run was performed.

Artifact-only tracing pinpoints the older geomod-fan-live history's rejection
in append_compact_lineage:8192 stored vertices plus3 incoming vertices
(faces1624), or plus5 on the later attempt. It is the vertex workspace guard,
not the2048-face cap or an inferred general heap shortage. Limits remain
unchanged pending a bounded solution and stock64MiB residency validation.

Evidence: artifacts/geomod-diagonal-live (initial expected-count failure),
artifacts/geomod-diagonal-restored/report.json (exact reload PASS),
artifacts/authored-post-live/diagonal-live-closure.log,
diagonal-new-history.log (all16 prefixes closed) and capacity16/trace.log.
Estimate remains ~50% overall/~71% GeoMod pending native runtime and broader
shape/material/collision coverage.


## Expanded NXDK profile and first stock64MiB runtime evidence

platforms/xbox/Makefile accepts RF_GEOMOD_EXPANDED_PROFILE=1 with the same15
compile definitions as CMake. tools/build-xbox.sh validates0/1 and updates a
profile stamp; all project objects depend on it so switching profiles rebuilds
shared ABI users. Default remains0/eight cuts. The XEMU render harness accepts
--expanded-geomod, builds the matching PC reference, selects the NXDK profile,
and uses its checkpoint, terrain, atlas, draw and retained-map limits. The
machine remains explicitly64MiB with a QMP memory-size assertion.

The first2500-frame run completed in XEMU but its already-loaded harness still
used the old110524-byte checkpoint assertion. It is correctly recorded FAIL:
artifacts/xemu/render-20260916-214317. Endpoint7904 pages=30.875MiB free. The
native framebuffer was inspected and shows the textured room, launcher/HUD,
and dark crater matching the PC composition. Its checkpoint/export and later
comparisons were not accepted because the harness stopped at that assertion.

After correcting all profile-specific bounds, a32-frame no-fire reload of the
16-cut PC checkpoint passes58 comparisons on stock64MiB:
artifacts/xemu/render-20260916-214825/report.json. Both checkpoints are135598
bytes, SHA2563e6b25d8b0ad55a92fbfb77583dbcb7f5e7ed9b7b385cbdeda77d75f6c7984bf.
The restored terrain counters and budgets match PC;8128 pages=31.75MiB remain
at endpoint. Its native capture was inspected: expected room, pickup weapons,
pistol and HUD; the default view faces away from the crater. This does not
verify destruction animations, audio or every geometry buffer on Xbox.

Both harness-owned emulators exited and all staged disc files were restored.
The preexisting Jedi Power Battles XEMU session was left untouched. No native
images were uploaded. The expanded XBE remains the last built artifact; a
normal build switches the stamp back to the default profile and recompiles.

Validation: all15 PC/NXDK profile definitions match, Python harness compiles,
NXDK expanded build succeeds with its existing merge warning, and the corrected
reload run passes58 checks. Repeat the full replay with corrected assertions
before claiming the entire sixteen-blast native sequence passes. Example:
python tools/xemu_render_check.py --expanded-geomod --dev-room --spawn --level
glass_house.rfl --archive levelsm.vpp --input artifacts/geomod-diagonal-live/input.bin
--geomod-checkpoint-out --seconds 600.


## Full sixteen-cut Xbox replay passes corrected harness

artifacts/xemu/render-20260916-215053/report.json is PASS with58 comparisons
through all2500 frames. Stock64MiB is asserted by QMP. PC and Xbox both commit
16 cuts, reject the final2 admissions at the16-cut history ceiling, and export
identical135598-byte RFDS checkpoints with SHA256
3e6b25d8b0ad55a92fbfb77583dbcb7f5e7ed9b7b385cbdeda77d75f6c7984bf.
Core counters match, including2056596-byte reported peak. Lowest sampled and
endpoint free memory is7904 pages=30.875MiB; this is not a continuous allocation
watermark. The earlier32-frame reload already passed58 comparisons separately.

Native framebuffer was inspected: expected room/floor, rocket launcher and
HUD, and the dark crater at the aimed wall. No assertion of full destruction
animation/audio fidelity follows from an endpoint capture. Physical geometry
closure was independently verified on the corresponding PC16-cut result;
this run compares shared checkpoint/history plus reported gameplay/render
state, not every Xbox mesh byte. Harness-owned XEMU exited and disc state was
restored. The unrelated emulator was left untouched; no GitHub image added.

In parallel, artifact-only capacity experiments replayed the older15-cut
history with its rejected16th admission. A12288-vertex workspace/3MiB budget
accepts16 with8370 vertices/1649 faces and2595424 peak. Reducing to9216 vertices
and2359296-byte budget also accepts16 with all prefix closure checks passing,
peak2189920. Both experiments compare the original15 committed cutter records
and final RNG; the new16th cutter has no stored successful-record counterpart
in that old capture. The probe explicitly distinguishes this extra capacity
result. Evidence: artifacts/authored-post-live/capacity12k/trace.log and
capacity9k/trace.log. These are unintegrated PC-only candidates, not changed
shipping/expanded NXDK limits. The existing8-cut default and8192-vertex opt-in
profile remain unchanged.

Next: integrate a coherent bounded capacity change only with matching core,
scene, transport, draw and native memory validation; broaden crater appearance
and collision testing beyond this repeated wall-blast path. Estimate ~50%
overall/~72% GeoMod after the bounded full native sequence is now verified.


## Integrate the measured9216-vertex opt-in profile

The expanded CMake and NXDK profiles now use9216 core/publication/source
vertices,2048 faces and2359296 bytes for the terrain core. Render subdivision
capacity derives18432 vertices under the existing1MiB draw budget. The XEMU
harness uses the matching vertex/core bounds. Default8-cut behavior and the
existing8192-vertex diagnostic targets remain unchanged. No clipping or
geometry algorithm change accompanies this capacity integration.

The measured older history needs8370 final vertices at cut16. The prior9k
core probe covers it with closure at every admission and2189920 peak; the
larger capacity is therefore based on a failing fixture rather than an
unbounded allocation increase. Full legacy-history scene/native playback
still needs validation; do not promote the core-only result to that claim.

Ordinary expanded PC gameplay commits16 in2500 frames. Physical mesh and atlas
CSV are byte-identical to the previous8192-profile passing run. No-fire reload
matches checkpoint, physical mesh and atlas exactly. Authored-post two-shot
restart and reset-zero/re-cut both match uninterrupted RFCP/RGCH/RGP.

Stock64MiB32-frame reload passes58 comparisons:
artifacts/xemu/render-20260916-215845/report.json. PC/Xbox135598-byte checkpoint
SHA256520bbcf7f246acb03077cdfb31bdd064f8b09a3591bbbb2349d095901c978bfc matches.
Endpoint8090 pages=31.6015625MiB free,38 pages/152KiB less than the previous
profile's corresponding reload. Draw workspace722964 bytes matches PC.
Native endpoint capture was inspected: room, floor, pickup weapons, pistol
and HUD present; this view faces away from the crater. Native full-blast
acceptance remains on the preceding8192 profile until rerun. The owned emulator
exited and staged disc state was restored.

Capacity participates in checkpoint source identity, so prior-profile saves
are intentionally incompatible. Wire size/schema remain unchanged. The
expanded PC and NXDK definitions were checked equal; Python harness compiles.
PC build retains existing scene uninitialized-variable warnings and NXDK its
existing linker merge warning. No claim of warning-free builds.

Evidence: artifacts/geomod-9k-live/report.json, geomod-9k-restored/report.json,
artifacts/authored-post-live/capacity9k-authored/report.json and
capacity9k-reset/report.json, plus the native report above. Estimate remains
~50% overall/~72% GeoMod; wider shape/collision/fidelity work is still required.
