# Thin fragment collision policy

The connected beam/post recut exposed a long six-face fragment whose original
4x4x4 mass sampling grid is empty. Its original-derived mass is positive, but
its collision sphere count and body radius were zero. It fell through the floor
while two companion pieces settled. Original499ed0 also performs no geometry
queries for an empty sphere list; this is not a restored-state discrepancy.

## Deliberate port improvement

Only when the recovered grid generates no spheres, body creation now samples
4x4x4 points across the actual mesh bounds. Signed triangle solid angles select
interior points, including points in concave solids. Each admitted sphere is
bounded by the nearest surface triangle, with its radius rounded down one float
ULP. A triangle's interior projection and all three edge distances participate.
This avoids replacing the piece with a large enclosing sphere.

The original mass, center, inertia and nonempty grid output are unchanged.
At most64 spheres are added, with no temporary heap allocation; persistent
sphere storage remains in the existing transactional body budget. If no sample
is admitted, creation fails and the enclosing edit rolls back. This is a
bounded sphere approximation, not exact mesh collision or retail equivalence.
Very small off-grid components and arbitrary contact directions need broader
qualification. Original fragment-fragment exclusion remains unchanged.

Affected old saves containing a zero-radius body do not silently acquire new
physics: reconstructed birth-bound validation rejects them. Saves without
affected empty-grid pieces retain their existing reconstruction. No file-format
change or relaxed validation was introduced.

## Verification

The extracted-replay test's thin box now has64 spheres, each checked against
every actual face plane to prove containment. Its zero inertia and original
mass are retained, reload reconstruction matches, and insufficient budget still
leaves the body unallocated. Existing nonempty32-sphere output is unchanged.

The scene digest fixture's long fragment now has a meaningful radius above the
player-support admission threshold. Sleeping support is accepted; active
support and penetrating placement still reject without publishing state.
All123 PC tests pass (`artifacts/thin-fragment-final-tests.log`).

`check_paired_reset_checkpoint.py --connected --settle` now saves at frame1020
after weapon cooldown ends, with all three fragments still active. Loading and
running430 further updates exactly matches uninterrupted1450-frame control.
All three fragments settle and remain present; sphere count is96, comprising
the new64 plus the unchanged32. The earlier frame990 attempt correctly rejected
an unsupported weapon-cooldown save, so the fixture uses the valid idle boundary.
PC log: `artifacts/thin-fragment-settle-pc.log`.

## Native acceptance

Initial run `render-20260917-184211` remains FAIL: NXDK nextafterf is an
asserting stub, leaving the guest stalled at the blast until the bounded
timeout. Replaced it with the identical positive-binary32 predecessor using
memcpy and an unsigned bit decrement. No tolerance or contact-radius expansion
was introduced; the failed report remains preserved.

`artifacts/xemu/render-20260917-184752` starts from the existing Xbox reset save
and executes171 records to the airborne checkpoint. All76 checks pass; its
5096-byte RFCP equals the uninterrupted1020-frame PC checkpoint exactly.
Native framebuffer inspected: damaged joint, three pieces and blast smoke are
visible.3454 free pages is13.492MiB endpoint headroom.

`artifacts/xemu/render-20260917-184916` loads that Xbox-created airborne save
and executes431 records (initialization plus430 updates). All76 checks pass;
the5096-byte RFCP equals uninterrupted1450-frame PC control exactly. All three
pieces are stopped and remain above the floor; native framebuffer inspected
shows the formerly missing long beam piece resting beside the other two.
3628 free pages is14.172MiB endpoint headroom, not an arbitrary-history minimum.
Both harness instances exited and restored their discs. No GitHub image added.

Final123-test PC suite passes after mesh-input validation was added. Logs:
`thin-fragment-final-guard-tests.log`, `thin-fragment-airborne-qualified-native.log`,
`thin-fragment-settle-native.log`, and `thin-fragment-settle-final-pc.log`, all
under artifacts. The replay additionally checks all three final center positions
against this test room's floor, so byte equality alone cannot mask fall-through.

## Concave shapes missed by both grids

A new closed, thin L-shaped fixture has two10-unit arms only0.2 wide and0.1
thick. All64 regular bounding-box sample points lie outside it; the previous
fallback rejects body creation (`artifacts/thin-concave-before.log`). Its faces
are outward convex quads/triangles, including a triangulated concave outline.

When regular sampling admits no sphere, body creation now follows each convex
face centroid's inward normal to the nearest other surface triangle. The
midpoint of that interval is a candidate; the same signed winding and nearest
surface checks must admit it. This preserves all existing regular-grid output
and the64-sphere limit. The method does not replace concave geometry with its
convex hull, increase mass, change inertia or disable collision failures.

The L fixture yields14 contained spheres in each of three cyclic axis
orientations. Analytical checks independently cover both arms, the re-entrant
corner, the outer bounds and thickness. Insufficient budget leaves body
ownership empty. Shared solid motion sweeps every generated sphere against a
floor, checks no sphere penetrates on any step, and requires contact and sleep
within600 frames in all three orientations. Full123-test suite passes; the
subsequently expanded motion test also passes its focused CTest run.

`tools/verify_thin_piece_spheres.py` executes compiled NXDK preparation under
Unicorn, intercepting only the final owned-body allocation boundary to read its
sphere arguments. All14 sphere records match PC byte-for-byte in all three
orientations. This does not claim original-game behavior or native L-fragment
motion. Logs: `thin-concave-tests.log`, `thin-concave-motion-ctest.log`, and
`thin-concave-nxdk-final.log` under artifacts.

The existing native beam continuation remains unchanged: run
`artifacts/xemu/render-20260917-185838` passes76 checks, and its5096-byte checkpoint
equals uninterrupted1450-frame PC control. Native framebuffer inspected with
three resting pieces;3628 free pages (14.172MiB) at the endpoint. The harness
restored the disc and exited. Log: `artifacts/thin-concave-regression-native.log`.
No additional GitHub image was added. Broader irregular and moving contacts,
and live native coverage of face-derived samples, remain open.
