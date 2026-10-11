# AI pistol body-location exact implementation review

## Disposition

**Accept immutable v2 for static source review.** Full v1 review and exact v1-to-v2 comparison are complete. The two strict-positive living-admission corrections, documentation-only stack-accounting correction, and indentation changes were independently verified. No unresolved blocking defect was found in the reviewed bounded implementation.

This is a static source disposition, not compilation, test, stack-fit or gameplay validation. The checkout remains at `24c453fc7cf2d7da650d4031408e6423a3efba9f`; only outside-checkout evidence was written. No import, test, syntax check, build, runtime, HDD, fixture, image or repository change was performed by this reviewer.

## Exact reviewed inputs

Final v2 directory: `/workspace/shared/ai-hit-location-next/versions/v2`.

- `implementation.patch`: `7c8400119eb7ed040eed3138f2d32ebdb25b8305e2178519d9ff2d9e6394dc80`
- `IMPLEMENTATION.md`: `eeacd15823656dfccf13145959b045c8c81e8d2d70ef42736ac619c9b016444e`
- `src/diagnostic/scene.c`: `020b4e10628233c36f5b8b2f5b3970fb8ec31a5ce8b400406c270bfaef2c6057`
- `src/diagnostic/scene_ai_pistol_location.inc`: `f811e16d3dec9fe17c044f752f8643262123d50c76177344ad7df413e532fcf2`
- `src/diagnostic/scene_npc_hit_location.inc`: `29459efbc9d96f7553bcf906e6a61759d8f7f684c005b7860c40d3f1844d3858`

v1 directory: `/workspace/shared/ai-hit-location-next/versions/v1`.

- `implementation.patch`: `8bc69f0e22a7dccbdd2b6067ab390ce7221e1cf87deea0ddb2c0923e65c0a9ed`
- `src/diagnostic/scene.c`: `4e885d1723d8b86b94a4c74c983cdb3fe5e387ddc72da90692f84f4df5acde82`
- `src/diagnostic/scene_ai_pistol_location.inc`: `a51455bcfd47f2600cec0f3c05fcaea7a84eff57a83983732300901b0a518a90`
- `src/diagnostic/scene_npc_hit_location.inc`: `29459efbc9d96f7553bcf906e6a61759d8f7f684c005b7860c40d3f1844d3858`

Preimplementation contract: `62b7ffb9adc6ae025e3ccbe9245e8163a1c592daf1a16f893870641195840239`; independent design disposition: `84039e4bff68d19377734b7f13cf2b83155661b5c5d6bcd7e9628a8611b6b515`.

## Scope and original-source qualification

The independently retained original disassembly and `INDEPENDENT-SOURCE-REVIEW.md` establish the conditional positive AI-owned consumer. Original `48be00` does not add the PLAYER model-query flag for an AI projectile owner. The pair therefore uses `49a420` physical sphere contact rather than the PLAYER triangle query. Original `42ce00` still supplies the nonplayer humanoid scalar. The `4c8840` miss helper is not called for this ordinary unlinked nonplayer target. Live linked-Use1, the separate target-link/source check at `4c5a52`, affiliation/target560 and forward/plane eligibility must be resolved independently.

The new source admission binds actual primary pistol slot 0, its selected descriptor, real geometry/reset-name lookup, guard1 class and ordinary capabilities. `campaign_enemy_weapon_select` maps that slot to `campaign_primary + 0`; pointer equality is consequently a real binding, not a UID or catalog-order guess. The source is an unlinked registered kind-0 nonplayer without the special class flag. UID109 is evidence, not a runtime filter. The target classes are only guard1/guard2/medic1. Existing 0/0 affiliation, links, player-controlled targets, body-0x400, active Nano and unrelated-class cases retain explicit fallback. No current `combat_target` or intended handle is promoted to original target560. Guard-to-medic is a conditional positive; guard-to-guard parity and an observed shot are not claimed.

## Corrected review findings

1. The AI helper now demands `campaign_model_pose_ready` before scalar evaluation. Merely obtaining `campaign_model_pose` did not evaluate matrices. The demand follows the existing pose owner, does not advance animation, and errors remain errors.
2. Class-scope fallback precedes the new live linked-Use1 rejection. Thus the supported original rejection does not silently change unrelated-class collision policy.
3. The accepted qualified `Shoot_Once`/`Fire_No_Anim` head is consumed immediately after successful finite debit/capture and before presentation or contact callbacks. `campaign_script_single_pop` only updates counters and moves/zeros the bounded queue. Local `single` is redirected to the captured request, so presentation mode is not read from shifted queue storage. The later pop is disabled for an active receipt. Postcallback byte-equality is no longer used as queue ownership, and cancellation cannot replay the accepted shot or pop a replacement request.
4. The class lookup uses a local `const char *` and passes its address, rather than taking a pointer to the record's inline character array.
5. Source/target identity explicitly checks actor kind/type 0, registration/view/damage full handles, registration-view pointer, and both registries.
6. The implementer identified the final NaN health edge. Exact v2 retains the old strict-positive `!(health > 0)` rejection in both new NPC selector and contact-liveness predicates, preventing a NaN health value from newly qualifying.

## Private contact and scalar

The private source response has one zero-centered actual-radius sphere and kind 2; the target contains exactly three copied physical spheres, actual positive finite mass, and frozen current body position/orientation. Both contact limits start at 1. The explicit count/kind/body-0x400 guards force the smaller-projectile branch. No live response wrapper or publication is invoked.

The scratch projectile flags `0x80000870` are independently present at original factory `4c7934`. In the reused pure response only body-0x400 and deferred-contact 0x40000000 bits can affect this query. The former is excluded, the latter is absent from those projectile flags. One-versus-three kind 2 suppresses the reciprocal branch, proving that scratch source mass/handle are unread; target mass is real and positive. The supplied extra-velocity callback matches the function-pointer signature, returns NULL, and cannot dispatch gameplay.

The core keeps strict overlapping swept AABBs, radius-sum directional sphere query, original initial-overlap behavior, ordered strict-earlier selection and the actual surface point: world start plus travel times fraction minus world normal times projectile radius. The target's physically winning sphere does not select the damage factor. The resulting world point feeds the shared posed-CSPH ordered quotient selection.

Only the original arithmetic loop was extracted. Float/double expression order, float stores, strict quotient comparison, ordered factor/index assignment and all error checks are unchanged. The PLAYER caller invokes it at the original point, after its existing model contact and before its existing generation/reaction work. PLAYER receipt, triangle query, eligibility, source ownership, reaction and scalar amount remain unchanged. The shared helper is defined in the existing include before the new AI include.

## Arbitration, snapshot ownership and raw damage

Supported rejection/miss returns with `matched == 0` and never reaches `combat_body` fallback. The independent shield selector still runs. A shield-only candidate can win without manufacturing a body contact if the shield later declines. The selected NPC, slot, body flag/fraction, shield fraction/candidate and receipt move together under the original strict-nearer cross-actor comparison; PLAYER shield fields are cleared on an NPC winner.

Physical spheres are copied by value into the candidate and then winner receipt. Temporary response pointers refer only to that candidate's scratch array while the pure query runs. No response pointer survives a candidate overwrite. `sphere_owner` separately captures the original allocation pointer for later validation. The selected receipt captures identity, class, pose/generation, scalar/index/point, physical spheres, current transform/bounds, publication, flags, link, affiliation and armor.

World/fragment/clutter/vehicle cover limits use the winning body/shield fraction. All shields and Nano receive raw accepted damage; an accepted shield/Nano ends the shot. Only a surviving qualified body contact multiplies fresh captured AI base by the factor once, then submits generic NPC damage with multiplier 1. The player difficulty path remains its existing downstream path. There is no new RNG draw, reaction publication, persistent projectile or target-order change.

## Callback and completion audit

The qualified lane captures existing `scene_robot_death_epoch` before presentation. `scene_ai_pistol_source` checks epoch before accessing current source/registry state. `scene_ai_pistol_contact_live` first checks source/epoch and then target identity before reading captured-owner-dependent geometry. An epoch mismatch routes to completion and returns from the old enemy tick, rather than continuing through a replaced campaign timeline.

Caller return sites inspected: fire presentation; selected actor/shield query; vehicle contact selection; clutter contact/damage and its optional impact audio; fragment contact/damage and stop audio; vehicle damage and impact; PLAYER/NPC shield commit; Nano prepass; NPC damage and its feedback; NPC death entry/start; player damage; body impact; actor-hit probe and final queue/order/journal bookkeeping. Read-only geometry, scalar and difficulty helpers have no gameplay callback. Audio/contact outputs are copied before callbacks; qualified source/target guards precede the subsequent retained pointer use. On a consumed Nano path, completion supplies the fence before bookkeeping.

After generic NPC damage/death calls, the same source and full target handle/slot are re-resolved before health/death work. A retired/replaced target is not queried again. Completion checks epoch first, then source full identity, then any retained intended-victim identity before telemetry. Because the accepted queue head was already consumed, completion correctly does not need the source's damage-lane affiliation/link/class predicate merely to report the accepted shot. The captured scripted-mode/target pair gates Attack telemetry. A replaced source is never used for queue or order bookkeeping. Applied player damage is recorded explicitly for the qualified lane, preventing a vanished NPC pointer from being misreported as player damage.

## Resource and validation limits

The source declares one accepted shot (212 bytes, conservatively rounded to 216 on Xbox32), winner and candidate contacts (320 bytes each), two general-response views (232 bytes each), one projectile sphere (24), one eligibility view (152), and bounded scalar/loop/plane scratch. There is no recursion, candidate-sized array, new allocation, persistent state or new epoch. The conservative staged accounting remains below the approved 3072-byte declared-object budget. Exact v2 corrects the pure-response local count to 30 floats/120 bytes; its enclosing 512-byte allowance and 2596-byte conservative declared-object total are unchanged.

Compiler frames/spills, existing nested pose/cover/callback stack, actual Xbox32 compilation, stock-memory runtime, contact/damage behavior and regressions remain unmeasured. Parent-owned import and the scheduled serial Xbox batch are separate decisions and are not authorized by this review.
