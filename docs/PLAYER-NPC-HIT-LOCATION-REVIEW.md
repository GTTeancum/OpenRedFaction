# Independent hit-location integration review

2026-10-10 UTC. Disposition: APPROVED for the bounded source integration described below. This is source review, not a compile, syntax check, test or gameplay result. No active repository source was changed by this reviewer.

The joint tree `/workspace/shared/hit-location-joint-next` was read after parent assembly from baseline `f6a3d0e202122a94bf5bb7252d42b6bd0a367f5c`. Every changed source/document hash below matches the independently reviewed staged component. The metadata/runtime interface, include order, field types, initialization, allocation/failure close and both ordinary player consumers were reviewed together.

## Exact artifacts

- Metadata patch: `ba04e8881bee2efe8bc828af3e06f15dd9237d38c489f51830df4c7a72077205`
- Runtime patch: `fed39b604e06277bb835f4b4075146f49e8b2bf2966fdcfaf6dadfdbe15eaa85`
- `include/rf/entity_assets.h`: `3013e3566a2fd0b766f490d5e5efb8ddf275202eedadf5fc11305b66b2f2e451`
- `src/core/entity_assets.c`: `67449d4d0e0efa39be7f74a44fe4466e3fbb78c3e4994dbcfe32606b1a3b9c97`
- `src/diagnostic/scene.c`: `8d6423deefa547cacd5fa4f8ddf962ecb719fb45e0d15288ce1f84f2b275b257`
- `src/diagnostic/scene_handheld_penetration.inc`: `80549a1dfc5bde0d32c6bbcc977084df432adc3897f6f68b2312b7e69a61906d`
- `src/diagnostic/scene_npc_hit_location_types.inc`: `4c623f15ec9acf06fac60a52839fe95e50a458656c547a3cf1e272658503481b`
- `src/diagnostic/scene_npc_hit_location.inc`: `bb1bd8c33ca967fc89e0cbdc686d288be080fcad8c8d807b3c5da2960bb6d203`
- `docs/PLAYER-NPC-HIT-LOCATION.md`: `b97275f52a26744ccf1ca2841c3511205b9f91f139362bd8111907de1ef39833`

## Admission and resource boundary

The new core resolver preserves the old class-sphere construction loop and physical projection; its complete ordered receipt retains actual model index, resolved radius and SP factor/default1. It adds no general positive-radius/scalar admission to unrelated classes. The scene adds mandatory metadata only for actual guard1, guard2 and medic1 names, with a real model, humanoid flag, actual CSPH count/index, positive finite radius and finite nonnegative scalar. Known qualified classes fail explicitly if their metadata is absent. Other classes and the documented unsupported geometry/target-lock branches keep their previous proxy/unscaled policy.

Class receipts use the existing NPC budget and normal/failure close, with 108 bytes per class and no per-actor persistence. Eight actual ordinary descriptors supply speed/radius through the separate additive table reader; static storage is 216 bytes. The resolver adds approximately512 transient stack bytes when the existing physical builder calls it. No save layout, candidate restore state or default actor state changes. Private restored poses use the same retained immutable class/model mapping; unrelated classes receive no speculative new scalar requirement.

## Coordinate, index and ordering proof

Original49afe0 prepares target-local query coordinates before calling5031f0 with flag2. In the staged wrapper, input flag0 means world start/displacement plus captured target origin/basis. `model_pose_coordinates` in core/collision.c performs the single subtract-and-rotate into local_start/local_displacement before posed tracing. Bit1 remains clear, so both routes use nearest contact. Returned local mesh contact is explicitly rotated and translated back into the captured world frame. The change does not omit an original face-selection flag.

Qualified broadphase is the original strict46c340 overlap of radius-expanded swept-segment AABB and retained target bounds. It does not impose the stronger old segment-versus-target-box or physical-sphere-hit veto. Current posed triangles retain actual0.02 metadata in their radius-expanded broadphase and thin-triangle branch. Raw CSPH centers are reposed from the current raw bone matrices, using the actual `definition->spheres[i].index`, then transformed into the same contact world frame.

The strict distance/radius minimum stores the chosen ordered class row in receipt.index. That row is not substituted for the actual CSPH index during posing, and no later gameplay consumer reads receipt.index. Identity validation uses the full handle, class, skeleton, pose pointer/generation and geometry. Double quotient versus stored-float minimum preserves the documented store order as a bounded portable projection; universal x87 bit identity remains unverified.

Actual accepted pellet origin/basis anchor both forward eligibility and undeviating planes. Existing spread draws occur once on their existing streams. Continuation advances only its query segment and visited handles under the documented instantaneous policy. It does not reconstruct original projectile flight or moving-target sweep scheduling.

## Accepted action and lifetime boundary

The new capture cannot receive a special weapon through ordinary fallthrough. Before generic debit, mounted vehicle/turret and slots5/8/9/10/12 return; slot11, explicit-unarmed and unowned return. Riot alternate begins its delayed owner and returns or returns during cooldown; the subsequent riot_selected block unconditionally returns for slot2. Slots4/6/7 are excluded from capture and dispatched separately. Remaining active slots are exactly the loaded0/1/3/13/14/15/16/17 set. Residual slot2 ray branches remain unreachable legacy code.

Accepted ordinary source/descriptor/slot/mode/base/kind/pellets/spread/origin/basis are captured before presentation/hearing. Post-presentation dispatch uses the captured accepted slot, and later pellets retain the same accepted descriptor. Source full identity and the existing successful-replacement epoch fence are checked without creating a new charge owner or requiring charge/FP admission.

The target receipt validates actual effects.flags_814/armor, class physics.flags, affiliations, linked handle/Use1 status, published position, full body bounds/basis/position, registry identity and current posed model generation. The earlier nonexistent effects.class_flags_724 access is absent in the frozen artifact. Damage callbacks are followed by same-full-handle registry/entity re-resolution before bookkeeping. Optional impact audio does not choose a location factor or decide damage.

Physical shield and Nano contacts receive fresh unscaled base. A declined shield-only candidate cannot fabricate a body hit. Qualified body damage is fresh base times the selected factor; the handheld penetration path then applies its existing attenuation before spending radius/advancing. Generic damage, AI scale, precision Sniper/Rail, mounted fire, melee and radial consumers remain outside this scalar slice.

## Disposition limits

The code and source-policy documentation agree on the explicit unsupported source0/target0 target-lock overlap, body0x400 and Nano geometry fallbacks; finite arithmetic and stale-owner cancellation remain port protections. No new save/history lane or delayed transaction is added. The prior21:00 neutral120 pass predates this work. An unchanged original L3S1 neutral startup is proportional to the new mandatory table/class admission, but cannot verify a hit factor, posed shot, shield interception or penetration result. Such gameplay behavior and Xbox compiler/arithmetic outcomes remain untested here.
