# Failure-only NPC velocity observation: exact review

## Disposition

Approve the three-file immutable candidate v2 for parent integration and the next authorized hourly build/observation. This is static source approval only. No test, import, compiler, runtime, fixture, HDD or original-input operation was performed. No save-admission or airborne-continuation change is approved.

Baseline: `076c300d2036de5053172cad5991ddc6b01da60c`. Parent documentation-only HEAD `f5c00e6a510eb70c16d45eb5880ee94d51d2ce48` does not alter the reviewed source baseline.

Candidate directory: `/workspace/shared/npc929-velocity-save-next/versions/v2`.

- `implementation.patch`: `d56e9da2732f13bc57137bf18fccf9edaf4709bec9f879f6d3752a935512545a`
- `src/diagnostic/scene_npc_checkpoint_capture.inc`: `41ac996f18c83919ef71bd088032f0195c6ec7f0a3ac6f4a319e8b393f9408bb`
- `src/diagnostic/scene_npc_checkpoint_velocity_observe.inc`: `b3b1d3f8d4fc0aa20d42845ccef983ef818e22a7e786cc57c16cd32eacc4e892`
- `tools/xemu_natural_save_gameplay.py`: `cde478827c8eb1df47f1845dc1b1dcab93270e1641544d1355c8d94ff00b362a`
- `DESIGN.md`: `f0cd551378716c77496b2d77d612c8c371454d1360b611abac7e63bd286f570c`
- `FIELD-MAP.md`: `c8c4b3dc85dbb06d355432e041d9956340e52defd372a3e3d4cda142840cfb50`

## Exact source findings

The original finite/absolute-velocity predicate and its six-word reject conversion, tuple and RF_RANGE return remain unchanged. The new helper runs only after that exact rejection, using the already-selected index and already-computed terminal_dead value. The only additional game-side writes are into a separate fixed diagnostic array, which the reject helper clears on each rejection. There is no recapture, admission query, owner mutation, dynamic allocation, clock, random draw, registry lookup, callback, actor scan or pointer traversal through a pose, route node, contact token or sphere element.

All newly read fields/types were checked against baseline declarations. `campaign_modes` is exactly sixteen descriptors and the read requires slot < 16. `rf_physics_spheres` has uint32 count/allocated_bytes. `scene_piece_support` has scalar tag/piece/serial; its registry pointer is not followed. Physics/contact XYZ and scalar storage match the byte copies. Seed class accesses are pointer/count/class-bound guarded; model and pose-bank metadata have pointer/count checks. Persistence items are a fixed actor-slot array and both logical count and structural bound are checked. All required type/global declarations precede the include at scene.c's existing capture join.

The source-body allocation/storage observations are distinct from separately owned corpse/model-pose metadata. This matters because a terminal-dead source actor can reach the same velocity guard. Pointer presence is only a boolean observation, never proof of a live model or collision body. Validity bits describe which bounded groups were read, not gameplay admission.

The 128-word field map matches every actual store: format version/count; validity; exact original tuple; same index; health/death/mode/class/source-body and support state; all five dynamic vectors; inherited support velocity; contact history; route phase; source positions; model/source ownership. Words 121–127 remain cleared. The full existing uint64 epoch occupies words 115–116 in Xbox little-endian order. Raw epoch/handles are diagnostic facts, not newly invented lifetime or portable ownership. The validity store is last in source; the consumer reads after the rejection has completed, and no atomic concurrent-publication guarantee is claimed.

The new bank is 512 static bytes, with scalar/pointer locals and fixed-size copies only. The source-local allowance is conservative and not a compiled-stack or runtime-fit measurement.

## Resolved v1 defects

1. The model owner's `owned` member is an `rf_entity_owned_pose *`; v2 records `owned != NULL`, eliminating the incompatible pointer-to-integer assignment and matching its boolean field map.
2. `scene_robot_death_epoch` is uint64; v2 retains both words rather than truncating it.
3. Class index is now published only inside the valid class branch, so invalid optional groups remain zero as documented.

V1 was never integrated or run, so using diagnostic format version 1 for this first final implementation is coherent. The capture and harness files are byte-identical between staged v1 and v2; only the helper/map/design corrections changed. Full-file diffs independently match the supplied patch.

## Harness and evidence boundary

The harness delta is exactly one entry in the existing terminal SYMBOLS union: the new diagnostic symbol with count 128. LIVE_SYMBOLS, producer/consumer input, 180-frame producer duration, save frame 120, all success/failure checks, cleanup/restore, retry and stop behavior remain unchanged. The change does not initiate a new run or change the next authorized batch's timing. Parent owns build and runtime; no runtime before 06:00 is authorized by this review.

The previous 05:00 result still proves only UID 929's velocity rejection at frame 120. Its health, terminal state, movement descriptor, support and physical ownership remain unobserved. This patch collects those missing facts at the exact future failure point; it neither repairs that save nor establishes that a living-airborne profile is the right fix. The conditional airborne and guard1 pistol reload designs remain deferred.
