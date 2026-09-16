# Public terrain UV regression

Built and ran rf_geomod_uv_public_lineage_probe against the normal rf_core library.
The synthetic closed-solid/cavity test executes both committed cuts through public
terrain APIs; it does not include geomod.c or invoke a private mapper.

All16 cases accepted both edits;39 surviving exact-position/plane/material corner
comparisons found10 UV changes. Example: (1.25,9.25,-1) changes from
(-0.125,0.15625) to(1.15625,-0.125) after the second cut. The log is
artifacts/authored-post-live/uv-public-lineage.log. This is numeric reproduction,
not evidence of that exact corner appearing in an installed level.

The executable intentionally returns1 for a counterexample and is not a passing
CTest target. The production fix remains open. Current terrain_prepare rebuilds
all cutter faces and terrain_map_pending reprojects every generated face from its
new rounded normal. History import invokes the same full rebuild. Correcting only
live edits would therefore disagree with reloads. Preserve birth-generation UV
through chronological clipping, with transactional reconstruction and existing
support-plane identities; do not merely change the axis tie epsilon.

## Face lineage infrastructure

Added optional internal geomod_face_lineage scratch (2048bytes) and tagged
compaction/repair entry points. Different birth tags cannot merge; same-birth
compaction moves tags with removed face indices, and repair propagates each tag
to all concave partition outputs. Existing full-union callers pass NULL and
allocate no lineage scratch, preserving current production behavior.

geomod_face_lineage tests same-birth merge, differing-birth exclusion, index
movement, ordinary repair and one-to-many concave partition propagation. It and
five rebuilt geometry tests pass (polygon split, interior faces, repeated-cut
coverage, transaction storage, history check). The public UV probe still reports
16 accepted cases/39 comparisons/10 changed corners. This is a prerequisite,
not a texture fix or Xbox acceptance. Chronological old-face clipping, retained
support-plane provenance, new-face projection and transactional history replay
remain to connect. Existing resident-memory accounting is unchanged until a
caller opts into the separately owned scratch.

## Chronological solid prototype

Added range-bounded subtraction and prepare_solid_step for a private replay
owner. Old committed faces subtract only the newest cutter, retaining existing
UV interpolation and winding. The newest cutter is clipped against immutable
source and preceding cutters; optional birth tags prevent old/new compaction.
Selective mapping projects only new-tag faces after compaction. Full rebuild
entry points remain unchanged; no live terrain uses this prototype yet.

geomod_chronological_solid_tests reconstructs the actual two-cut history from the
public regression's immutable source using these steps. Four outward-solid cases
compare8 surviving exact corners, with zero UV changes (the original public
full-rebuild fixture changes2 of these). Cavity cases are explicitly excluded
from this prototype and remain to implement, not claimed fixed.

The strict rf_geomod_seed_adjacency check returns RF_FORMAT for all four existing
full-rebuild meshes AND all four chronological meshes. Initial failure output
was investigated against the baseline rather than treated as proof of a new
crack. Current test prints this diagnostic; its passing scope is inherited UV,
not manifold topology or collision equivalence. Shared-edge coverage/geometry
validation remains required before live integration. No native acceptance.

## Prototype geometry comparison

The established interior-fixture geometric edge-coverage algorithm was applied
with its unchanged1e-6 diagnostic distance and T-junction subdivision. Both full
rebuild and chronological solid prototype pass splits0/.2 and fail splits.1/.3.
Detailed failure output remains in solid-coverage-detail.log. This is an existing
coverage problem in this synthetic fixture, not proof of full topology validity.
The prototype test asserts matching coverage outcomes, explicitly retaining those
two known failures; it does not relabel them as manifold success.

Actual rf_geomod_collision_faces/tree queries compare each full-rebuild and
chronological output:726 rays and726 radius.1 sphere sweeps per case,5808 paired
queries total. Hit/miss and first-contact fraction agree within1e-5 in all four
cases;8 inherited exact corners retain UVs. These fixed-grid queries cannot prove
all collision equivalence. Results are in solid-collision-comparison.log.
The live mapper still uses the old path. Fixing support-plane/edge provenance,
adding cavity chronological replay and integrating budgeted transactional history
reconstruction remain required before production acceptance.

## Exact support tracking resolves solid prototype coverage failures

The solid chronological step now retains source/cutter supporting-plane IDs
through source intersection, old-face subtraction and new-face clipping. A
separate10240-byte previous-bank support buffer preserves4096 edge IDs and1024
face IDs while compaction writes the next bank. With lineage enabled, compaction
also refuses to merge differing support planes; it no longer loses an exact
support ID to UINT16_MAX. Legacy full-union calls remain unchanged.

All four chronological split cases now PASS the same geometric coverage check,
including .1/.3 which still fail in the baseline. The test now requires closure,
rather than merely equality with baseline failure. No tolerance was enlarged and
no spatial vertex welding added. Actual shared-edge intersection helpers consume
the retained plane IDs. Twelve exact surviving-corner comparisons retain UVs;
5808 paired ray/sphere queries still agree within1e-5. Seven focused geometry
tests pass after rebuild. See solid-support-result.log and
solid-support-regression-build.log. Production/cavity integration remains open.
Additional optional prototype scratch is12288bytes for lineage+support, excluding
private replay mesh banks/work and tree allocation; this is not native RAM proof.

## Cavity extension and transactional replay preparation

prepare_chronological_step now handles outward solid and inward cavity sources.
Cavity cutter faces subtract the original empty volume using tracked supporting
planes before previous cutters clip them. Previous committed faces retain UVs
and exact support IDs. All16 solid/cavity cases pass geometric edge coverage;
62 inherited exact corners keep UVs;23232 paired ray/sphere queries agree within
1e-5. The historical target name geomod_chronological_solid now covers both modes.

terrain_prepare_chronological_mesh reconstructs all prefixes in disposable
storage using existing work scratch, accounts lineage/support/replay bytes within
the terrain budget, and copies only the successful final mesh into the inactive
live bank. Replay scratch is freed before collision-tree construction. Every
fixture compares that adapter's pending output byte-for-byte against standalone
chronological reconstruction and rejects insufficient budget without changing
live vertices/generation/edit state. Seven focused tests pass after rebuild.

Live terrain_prepare does not route through this adapter yet. Enabling it needs
broader history/reset/source tests, reconstruction policy identity updates,
installed post/cavity replay and native memory/performance acceptance. The
prototype omits the old cavity repair stage; tested geometry coverage passes,
but larger/adversarial shapes must establish whether exact support propagation
suffices or a provenance-preserving repair stage is still required.

## Live-route experiment finds large-cavity blocker

Routing terrain_prepare through chronological replay passed the public16-case UV
probe and PC authored-post restart/next-blast comparison. Broader holey01 stress
then exposed an uncovered edge on the second large cavity blast, about.0103 long
near(-16,-8.785,2.004). This is an actual unresolved coverage failure, unlike a
mere different polygon partition. The old cavity repair stage cannot be omitted
for broader acceptance.

Production terrain_prepare remains on the accepted full-union path. The proposed
route is compiled only by RF_GEOMOD_CHRONOLOGICAL_EXPERIMENT in the standalone
rf_geomod_chronological_stress_probe target (not CTest). Reproduce with that
executable and arguments Installed_Game artifacts/geomod-holey01-csg.bin; failure
log chronological-stress-repro.log is retained. Accepted PC executable rebuilt and
normal repeated-cut coverage passes. No changed Xbox acceptance claimed.

The original-template comparison now checks closed geometry and signed volume
agreement instead of exact old polygon partition; its independent analytic
collision queries still run against both paths. Mapping-only layout equality
remains enforced. Next action: propagate support IDs through cavity repair and
its partitions, then retain that provenance for the following chronological step.

## Repair provenance work in progress

The working tree extends cavity repair to emit face/edge support IDs alongside
birth tags. Boundary runs retain their prior support; partition diagonals carry
the containing face plane. Output support IDs replace the next-step cache only
after repair succeeds. Chronological replay now calls this repair each prefix.
This is unfinished and has NOT been promoted to the production route.

The original cut2 uncovered edge no longer fails: stress cuts1..3 are closed,
including their junction rays/body sweeps. Cut4 rejects RF_FORMAT, not RF_RANGE;
see cavity-provenance-stress.log. The16-case prototype now fails one collision
comparison: cavity roof-.75/split0, radius.1 sphere moving down from
(-.00499987602,15,-.786000013), fraction baseline.540684283 versus replay.0906838626.
It is not established which result is correct. See cavity-provenance-collision.log.
Do not loosen the collision tolerance or call the prototype accepted. Face-tag
unit test passes; accepted production repeated-cut coverage passes. Next: trace
repair/partition rejection and the differing contact before further activation.

## Repair tracing follow-up

The cut4 failure was partition rejection of repaired face160 (six corners).
Partition diagonals use their containing face plane as the edge support; those
identical plane pairs do not define a unique geometric edge and must not collect
other coplanar endpoints. Skipping that repair collection for plane==edge gets
stress cuts1..5 through closed coverage, junction rays/body sweeps, lighting and
near/short collision probes. The next rejection is the uncached comparison owner
at test line1540, not the original cut4 partition. Latest primary peak at cut5 is
1033632bytes. Do not infer the uncached rejection cause without further tracing.

The small-case differing sphere contact is on the replay's x=0 cavity wall near
(0,14,-.75), with center starting(-.00499987602,15,-.786000013), radius.1 and deltaY-10.
Baseline instead reports a much later edge near y9.5. Actual face vertices and IDs
are saved in cavity-contact-vertices.log. Analytic endpoint contact and the
original query semantics still need checking; no exception to the comparison
has been added. Fifteen cases pass; this one remains a failing test. Current
changes remain uncommitted and the production experiment flag remains off.

## Bounded replay scratch and eight-cut acceptance

The uncached sixth-cut rejection was RF_RANGE. Replay previously allocated both
private full-capacity mesh banks while the live inactive bank was unused. It now
allocates one private bank, borrows only the live inactive bank plus immutable
source, and alternates those two during reconstruction. The live current bank
and collision tree remain unchanged until caller publication. Final copy skips
identical pointers; no self-overlapping memcpy. Scratch is charged before
allocation and released before collision-tree construction.

The six-cut stress now passes with cut6 peak952960bytes. Explicit
RF_GEOMOD_STRESS_COUNT=8 also passes, including the deliberately768-face limited
owner's expected eighth-cut overflow and rollback. Logs replay-bank-stress.log
and replay-bank-eight.log retain full results. No Xbox acceptance yet.

The upper-rim analytic sphere/corner solution gives fraction.0906838846205 versus
new query.0906838626 (about2.2e-8), whereas baseline reports.540684283. This supports
the new first contact, but the comparative fixture still intentionally fails;
need establish the boundary geometry and explicit independent contact oracle
before changing its comparison policy. Prototype changes remain uncommitted.

## Rim discrepancy resolved without a collision exception

The general double-precision finite-segment capsule oracle is two-sided and
cannot decide the recovered one-sided face admission. collision_sphere_plane
requires strictly positive approach. Replay introduced x=-5.55e-17 at an intended
x=0 face, tilting its computed plane enough to admit a parallel sweep. Applying
exact axial supporting-plane coordinates after the corner solver AND its
coincident-plane interpolation fallback removes the residue. This uses exact
zero coefficients, not proximity welding or a changed collision tolerance.

All16 prototype cases now pass unchanged collision comparisons; no earlier-hit
exception is accepted. The two-sided edge oracle remains diagnostic only.
Seven rebuilt focused tests pass. Final explicit eight-cut experimental stress
also passes: repaired-final-build.log and repaired-final-eight.log. Replay
remains experimental until live source identity/save policy and native checks.
This supersedes the earlier suggestion that the new rim contact was acceptable:
the analytic contact existed, but the original one-sided admission excluded it.


## Live activation and native continuation (2026-09-16)

Chronological replay is now the default terrain reconstruction. The internal
comparison test explicitly selects the legacy baseline, keeping its collision
comparison independent. Removed an unused compaction wrapper rejected by NXDK.
The authored source fingerprint uses reconstruction publication policy2; digest
wire policies remain1. This separates old saves from changed reconstruction.

Five rebuilt focused CTests pass. The public live UV probe accepts16 cases with
67 matching surviving corners and zero UV changes. Both PC two-shot and
reset-zero checkpoint continuations match uninterrupted RFCP/RGCH/RGP bytes.
A retained prior-build checkpoint from render-20260916-184610 is rejected at
load with RF_FORMAT; no compatible-save migration is claimed.

NXDK build passes. Native run artifacts/xemu/render-20260916-192904 passes58
comparisons over200 frames on stock64MiB. Xbox checkpoint3884 bytes equals
both restarted and uninterrupted PC, SHA256
f02dc2faf1b11b7cb714f107ae6b155bc1f57eacf6a7dd0788ac684b4bbf3234.
Endpoint free memory16.19140625MiB. Disc restoration succeeded.
Inspected native framebuffer: textured hall, roof/beams, damaged post beneath
crosshair, water strips, launcher and HUD present. This single final capture
does not prove temporal texture stability, audible effects or retail fidelity.
UV stability is established by the bounded numeric fixtures, not this image.
Broader shapes, extended repeated cuts, visual fidelity and native reset-zero
acceptance after this activation remain open.


## Axis coverage and full-suite acceptance

The public API UV fixture now applies handedness-preserving cyclic axis
permutations to source, cutters, kernel and second-cut bounds. All48 cases
are required to succeed and each must compare at least one retained corner.
The48 accepted cases contain201 exact surviving corners with zero UV changes.
Registered geomod_public_uv_lineage in CTest so this former diagnostic cannot
silently regress after integration. These are synthetic two-cut cases, not
general arbitrary-rotation, authored-world or visual-fidelity acceptance.

A full rebuild followed by all104 existing tests found only the intentionally
changed authored-identity fingerprint still pinned to policy1. Updated that
expected identity to the policy2 fingerprint already independently observed
in the PC/Xbox continuation report. All105 registered tests now pass, including
the new public UV test. Logs: activation-full-build.log, activation-full-tests.log,
activation-regression-build.log and activation-regression-tests.log under
artifacts/authored-post-live. No production source changes in this follow-up.
