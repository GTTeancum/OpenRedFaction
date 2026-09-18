# Mover activation is not a terrain-fragment wake producer

The controller activation wake helper must not be reused as a kind3 fragment rule merely because it already accepts prepared bounds. Its two original lists are entities5cb060 and projectiles872128. Extracted terrain fragments belong to5c98e8, as recorded by constructor4130b0 insertion and the earlier blast-dispatch audit.

## Executed activation-loop exclusion

`python -B tools/verify_group_wake.py` now independently populates kind3 list5c98e8 with a settled, in-range fragment while executing its existing2048 original46ae65..46af8d cases. Memory-read hooks watch both the fragment list-head link and the mapped fragment object. Neither is read in any case, and every fragment byte remains unchanged. The normal positive controls still mutate690 entity and2131 projectile records, and all original/PC/compiled-NXDK flag comparisons still pass. This extends the earlier corpus with a missing-owner negative control, not with2048 newly invented activation scenarios. Report: artifacts/group-wake-verification.json.

## Executed scheduler gate

`python -B tools/probe_fragment_schedule_admission.py` executes complete488030 and its real helpers without hooks. Thirty-two cases cross inactive/settled/active physics flags, ordinary/changed object flags, room visibility and parent association for a healthy kind3 body. All match the expected route and complete object mutation. The first helper4136d0 tests body active bit80000000; rejection marks object flag800000. Object change flags06000000 alone do not override inactivity. Active, unparented, visible controls admit; inactive cases reject. Report: artifacts/geomod-postedit-re/fragment-schedule-admission.json.

Both tools assert RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No original-game launch or screenshot is involved.

## Consequence and limits

Do not wire settled fragments into the controller's projectile wake list or label a swept-AABB wake on every moving frame as recovered retail behavior. The existing timed fragment scheduler's inactive gate remains consistent with the tested admission boundary. Other wake producers, support refresh and geometry-change notification may matter and are not excluded by these tests. A practical geometry-based support-loss wake can be a deliberate port policy, but it needs explicit bounded contact/support tests and native gameplay validation; it cannot be justified by the activation helper alone.

This pass changes probes and evidence only. No gameplay, Xbox build, visual change, performance gain or completed mover support is claimed. Next work is the actual support-loss/wake producer and its scene integration.
