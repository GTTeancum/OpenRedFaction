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
