# Independent source review: medic held-child pose publication

Date: 2026-10-10 UTC. Disposition: APPROVED for the exact two-file patch below as a bounded source correction. This is a source/data review, not a compilation or runtime result.

## Exact reviewed artifacts

- Patch `syringe-raw-hand-pose.patch`: SHA256 `1d0f37707fedd612ecb4d3a34d484d87679b71f697e281b9048e76da715ab758`.
- `source/src/diagnostic/scene_medic_syringe_owner.inc`: SHA256 `59a34a3e7dbbb1b2f16649590c0c478ffa8d3f89d4245e2bf4c122542d093de1`.
- `source/src/diagnostic/scene_npc_checkpoint_restore.inc`: SHA256 `15a88b466f68eb33be8d0d1fcd9031d3d325571fc27bb3deaa7b904672fa8eab`.

The source analysis started against the tested commit `65490a235229cafdff24c94c9a40ddf2bfefffa5`. At final review, parent integration had advanced the clean repository to `3027dc0e8066e4bdc73c1c4b5596c48a9df10731`; the two patch targets were unchanged before this correction. Parent owns integration and validation.

## Why this correction is justified

Original `420f90` obtains the actual held clutter and class hand tag, computes the posed transform via `5034f0`, publishes position, then copies nine-word raw matrices into child offsets +48, +fc and +120. The bounded `original-held-*.txt` excerpts trace the tag/bone matrix and copy helpers. There is no normalization at this hand-child boundary.

Current RFA packed-quaternion decoding and the skeletal sampler intentionally retain the authored quaternion scale. Therefore the composed hand matrix need not satisfy the unrelated rigid-clutter teleport validator's 0.001 row-dot/determinant tolerance. The actual medic mesh and original motions provide concrete witnesses, without changing any asset or running the engine:

- Initialized `tech01_stand.rfa` at source-derived tick320: local composed hand determinant about 0.9984300062.
- Advancing startup `medc_kneel_n_heal.rfa` at source-derived tick589: local composed hand determinant about 0.9985171269.

Both exceed the strict determinant tolerance. These numbers are original-byte/source-formula calculations before final actor-world composition, not captured runtime matrices. `held-pose-candidates.json` preserves the calculations and neighboring-cursor witnesses.

Actual startup auto trigger6437 links zero-delay Play_Animation2211, which selects the kneel loop for medic783. The initial stand advance precedes startup event selection. Weight changes do not bump pose generation. Thus a culled first update can retain the initial stand matrix, while a normally advanced update evaluates the kneel phase. Medic initialization sets action2, so invisible-room culling cannot be dismissed. This distinction is recorded rather than assuming kneel tick0.

## Admission and ownership review

The helper is local to the existing generated-syringe owner and has only two callers: the live held-child tick and private checkpoint candidate hand re-pose.

`scene_medic_syringe_child_valid` requires the actual registered full handle to resolve to the child's own state, matching token/address, exact syringe class index and definition pointer, and the syringe shared-model owner. It does not require live-list membership. Consequently the registered, hidden private restore candidate is admitted without substituting a fake live child or parent; existing restore preparation/validation retains parent identity and private-list checks.

Resource admission already qualifies the actual syringe class as metal, life50, static model, low class flags exactly0x80. Constructor collision admission uses class flags&6 to set physics0x20; can_carry0x80 alone supplies no collision. The new helper rechecks flags&6 and does not widen raw-matrix admission for arbitrary authored clutter or colliding props.

Finite position and matrix checks remain. Radius must remain finite and nonnegative. `rf_physics_tensor_world` performs its existing finite/overflow-checked computation; `rf_physics_publish_position` retains finite world-bounds validation. All fallible calculations operate on a local body copy. Child body, published/query position, matrix and room are assigned only after success. Object flags remain unchanged; the local body scheduling change from the publication primitive is replaced with the original body flags. The existing live caller continues to apply only its existing parent-hidden flag update.

The generic `scene_clutter_pose_publish` is untouched. No tolerance relaxation, quaternion normalization, swallowed error, save-format change, reserve/health grant, lifecycle change or new gameplay input is introduced. Both live and private re-pose paths receive the same authored-raw-matrix treatment. Include order already makes the helper visible to checkpoint restore.

## What the 19:00 evidence does and does not establish

The recorded phase `0x80000102` encodes `RF_FORMAT(-2)`. Stage80 is the live animation-stream callback. One successful presentation and one successful replay poll, with replay error0, place failure after first presentation and before the next successful input poll. Possible remaining locations include the platform immutable-storage check and post-sink frame0 simulation. Neutral input excludes medic Use acceptance/healing and its residual owned clip; generated syringe pose work still runs.

The captured arrays do not identify the exact failing callee. Approval of this intrinsic source/data correction is not a claim that the old run proved the syringe return, or that this correction fixes every startup failure. Reading the existing profile/syringe telemetry in the next parent-owned unchanged-input scheduled batch is proportionate. No retry, build, syntax check, test or game execution was performed by this review.
