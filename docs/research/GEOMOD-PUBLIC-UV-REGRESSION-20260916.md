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
