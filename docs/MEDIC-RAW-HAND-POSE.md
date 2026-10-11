# First-frame startup: generated medic hand pose

Baseline: tested `65490a235229cafdff24c94c9a40ddf2bfefffa5` (the two target files are also unchanged in parent integration `3027dc0e8066e4bdc73c1c4b5596c48a9df10731`). Source/data analysis and staged source only. No build, syntax check, test, engine execution, original-game execution, route, input fixture or runtime retry was performed.

## What the captured 19:00 result proves

The Xbox build passed. Original neutral L3S1 stopped with phase `0x80000102`: xbox/main.c encodes `0x80000100 | -status`, hence `RF_FORMAT(-2)`. Loading reached stage80, the animation-stream call. One successful render sink and one successful replay input read were captured; replay error field is zero. The existing result does not capture the exact post-render callee or profile stage. It is therefore incorrect to claim that the captured run proved the syringe callee. Parent is adding observation of existing diagnostic arrays to the next scheduled unchanged-input run.

## Proven source/data incompatibility

The real L3S1 medic783 owns the generated `syringe` child. Its actual startup trigger6437 is auto, enabled and scriptless; its links include Play_Animation2211. The latter is zero-delay, flags[0,0], `medc_kneel_n_heal.mvf`, linked to783. Trigger timing30 is reactivation cooldown, not initial delay. The bounded record evidence is in the two `medic-*-events/links.json` files.

The current child tick and private checkpoint re-pose both use `scene_clutter_pose_publish`. That rigid teleport service rejects row-dot/determinant deviations exceeding0.001. Actual authored packed-quaternion hand composition exceeds that threshold (see independently decoded actual asset evidence), and the original skeletal sampler intentionally does not normalize those quaternions.

Original `420f90` resolves the real held child and hand tag, obtains its composed transform, and directly copies nine-word matrices to child+48/+fc/+120. The saved `original-held-*.txt` excerpts qualify that path. They show no boundary orthonormalization. This is a genuine intrinsic incompatibility, whether or not it was the exact uncaptured19:00 return.

## Minimal staged correction

`scene_medic_syringe_pose_publish` accepts only the actual registered generated syringe, including a registered private restore candidate, with exact class/shared-model identity and noncolliding class flags. The live caller has already validated exact parent pointer/handle; restore retains its existing private parent/candidate validation. It retains finite position/matrix checks, finite nonnegative bounds radius, overflow-checked tensor computation, finite world-bound publication, and all-or-nothing child assignment. It preserves the raw authored matrix and existing scheduling/gameplay flags.

Only the live held-child tick and private candidate hand re-pose switch to that helper. Generic clutter's rigid validator is unchanged. No normalization, error suppression, input change, inventory grant, gameplay admission or save-format relaxation is introduced.

## First-tick timing qualification

Initial poses advance at1/30s using the base stand mapping before startup events. Medic base stand is `tech01_stand.mvf`; its actual RFA header spans160..9600. Startup loop selection zeroes old looping weights and installs kneel weight1 without changing the generation or shared phase. The script loop suppresses controller blending. The kneel header spans160..20800. If normal visibility advances it, the next1/60s update uses80ticks on the retained shared phase, yielding tick589 (phase0.02082512155175209) after initial stand tick320 (phase0.016949152573943138). If culled, the generation-ready cache retains the initialized stand pose. Medic initialization sets current/action2, so invisible-room culling cannot be ruled out from uncaptured telemetry. The actual-key calculation in `held-pose-candidates.json` covers both cases: retained stand320 has determinant0.9984300062, and advanced kneel589 has determinant0.9985171269. Both exceed the rigid0.001 tolerance. This proves that either admitted hand pose is incompatible if the syringe tick is reached, while another earlier return remains possible. None of these calculations executes the game or engine. The first-key witness remains explicitly separate from the captured failing cursor.

## Files

- `syringe-raw-hand-pose.patch`: complete two-file patch
- `source/src/diagnostic/scene_medic_syringe_owner.inc`: full staged owner
- `source/src/diagnostic/scene_npc_checkpoint_restore.inc`: full staged restore
- `original-held-*.txt`: bounded original disassembly
- `decode-authored-hand-chain.py` and authored JSON: read-only original-byte calculation
- Independent review is supplied separately before parent integration.

## Integration receipt

Parent integrated the independently approved exact patch at `465343c881aafbf28dc17eab8c4550152f88d87e` for the scheduled21:00 batch. This receipt reports source integration only; no runtime outcome is claimed. The generic rigid publisher remains SHA256 `e5bb2ed6befd69f7888331495c3add8a7d3629d474a6d6fe5ebc58af6b31e5e0`.

Patch SHA256: `1d0f37707fedd612ecb4d3a34d484d87679b71f697e281b9048e76da715ab758`.
Full staged owner: `59a34a3e7dbbb1b2f16649590c0c478ffa8d3f89d4245e2bf4c122542d093de1`.
Full staged restore: `15a88b466f68eb33be8d0d1fcd9031d3d325571fc27bb3deaa7b904672fa8eab`.

## 21:00 bounded runtime receipt

Tested source `6e7476a1502e2c4ff619a3f64019f3309b4ab2af` built and passed the unchanged original L3S1 neutral120 stock64MiB startup. The terminal syringe array recorded119 successful pose publications and fault0. All acceptance and original-input/restoration guards passed. This is bounded pose-publication evidence, not healing, rendering, private restore or save/revisit proof. The19:00 result still does not identify its exact uncaptured failing callee. See [the21:00 batch report](HOURLY-20261010-2100.md) for exact artifacts and limits.
