# Same-pistol 828 reaction: exact source disposition

2026-10-11 04:46 UTC. **Accept the exact candidate below for parent-owned integration.** This is static source approval only. No compiler, syntax check, test, fixture, emulator, image, save edit or repository mutation was performed by this reviewer. Compilation, runtime reaction/continuation and actual stack fit remain unverified.

## Exact inputs

Baseline HEAD independently read: `f4980b975aca21856f9fc0faf8b8e916126ed9f4`.

Design `/workspace/shared/ai-pistol-reaction-next/DESIGN.md`, independently read and approved before implementation: `0b4a482dc7fb4f9d03ddcbcca498a5640e4d58a043ba679d1c4cd6a5c41ff4ea`.

Immutable candidate `/workspace/shared/ai-pistol-reaction-next/versions/v1`:

- `implementation.patch`: `9b856d58b1f4a09bc36f7d4c353b12450df2c4a21e4912b8d256342cc0804b4d`
- `src/diagnostic/scene.c`: `63accf720d62169d4547d3038535af52b290d457f58c79202a29d7503a28851d`
- `src/diagnostic/scene_ai_pistol_location.inc`: `80cca65dc54b33f105493200ec9d40f56c3a0aa2d728bdd422e490b832189acf`
- `IMPLEMENTATION.md`: `ac203e0d6b1f3d0f3f2423ab36e38aad568022d8d16d37e294f749f03cfb65c4`

The two complete staged source files were independently diffed against the baseline. Their differences match the reviewed patch. The implementation document accurately describes those differences and their limits. It explicitly corrects the approved design's imprecise phrase about `4c600f`: that call is penetration `4c9b20`; generic damage follows downstream.

## Original branch and actual owner

The preceding accepted AI pistol contact/scalar lane remains the sole owner. Its original physical-sphere prerequisites, standalone guard1 primary-pistol source, bounded guard1/guard2/medic1 targets, source/target identity, unsupported 0/0 policy, raw shield/Nano handling and instantaneous scheduling were separately reviewed in `EXACT-IMPLEMENTATION-REVIEW.md` (`9072934614eaec714cf8a7927688bdfbfc4fc165de36839ecc6db041faa5dc56`). This candidate does not expand that admission.

Independently inspected original excerpts are now packaged in `ai-pistol-reaction-next/evidence`:

- `original-selected-weapon-bindings.txt` plus `original-selected-weapon-strings.txt`: `12mm handgun` string `5a3304` is looked up at `4c65fa` and bound to `872114` at `4c6604`. It is distinct from every exceptional descriptor compared at `4c5d95..4c5dcb`, including Shotgun. No source-position distance exception is needed for this exact pistol.
- `original-location-application.txt`: scalar/index return from `42ce00` is followed by humanoid predicate `40a150` and dead/excluded predicate `427020` at `4c5d6e..4c5d89`. There is no AI-source exclusion in this path.
- `original-humanoid-gate.txt` and `original-reaction-actor-gate.txt`: the gates are class `724 & 0x20000` and actor `810 & 1`, respectively.
- `original-published-local-transform.txt` and `original-location-hit-damage.txt`: reaction side uses actual contact minus published actor `3c`, projected through raw published `48`, not physical `e4/fc`. Index 1 uses local Z and writes 24 for strictly positive, otherwise 25. Index 0 uses local X and writes 27 for strictly positive, otherwise 26. Index 2 and unknown index do not write `828`. The separate `824` death/head work is excluded.

Current `rf_scene_npc_commit_ordinary` and `rf_scene_npc_publish_position` establish the published owners: `campaign_model_owners[slot].basis` and `owner->published`. The existing pain controller reads `pain.selected_action`; generic damage notifications route to that retained controller. The new write therefore completes a concrete consumer.

## Exact code checks

1. Snapshot initializes `pain_action = -1`; the zeroed receipt has `reaction_frame = 0`. Preparation resets both before its gates. Only an already qualified HIT with humanoid flag, clear actor-810 bit 1 and index 0/1 requests the published basis. Index 2, unsupported/fallback and rejected/missed contacts do not gain a new model-basis prerequisite or write the old hint.
2. The preparation function is defined before its sole call in the AI body query. It captures all nine raw published-basis floats and validates them. Point-minus-published-position is stored to float; the dot expression retains Z/Y/X product and sum ordering with double intermediates and a final float side store. The strictly positive comparison handles zero and negative zero as the nonpositive branch. No normalization or anatomical approximation is added. Portable projection is preserved; arbitrary x87 extended-precision equality is not claimed.
3. The scalar and physical contact code remain unchanged. The helper runs only after scalar factor/index and current pose/generation are resolved. Errors abort the contact; partially prepared state is never committed.
4. AI contact liveness checks existing epoch/source identity first, then target owner/full handle and geometry. When `reaction_frame` is set it additionally bounds the model-owner lookup and compares the captured published basis. Existing published-position, class/model/pose-generation and physical geometry checks remain. Frame changes cancel the retained contact; no replacement query occurs.
5. The caller retains raw shield and Nano consumption. Only a surviving qualified HIT first validates the scaled amount, then repeats the complete AI contact guard, conditionally stores exactly 24–27 to the selected same target, and immediately enters the unchanged `rf_scene_npc_damage`. There is no callback between final validation, store and damage entry. Earlier arbitration guarantees `victim` and the retained owner are the same; liveness checks that owner before this dereference.
6. There is no rollback when damage is suppressed or lethal, matching the original pre-damage publication. Existing same-handle postdamage/death bookkeeping, world-epoch exit, accepted FIFO preconsumption and request copy remain unchanged.
7. The patch does not invoke or edit a PLAYER source/reaction helper, write `824`, start a motion, write timers/RNG, add an owner/allocation, change receipt layout or broaden class/weapon admission. The shared PLAYER scalar/reaction include is not modified.

## Existing persistence is sufficient within its current rules

Independent read-only inspection covered current `scene_npc_pain_checkpoint.inc`, `npc_checkpoint.c`, scene capture/restore and burning checkpoint join. These proofs are target-state/class/selection based; none requires a PLAYER-origin shot.

- Core pain grammar already accepts -1 and 22–27; writer promotion uses RFNC24 for retained 24–27 or new residual semantics, and legacy versions retain their old grammar.
- Scene binding requires the exact named target class, registered/model ownership and actual base-or-weapon mapping. An unarmed medic uses its real base mapping.
- Retained hints survive expired timers; residual admission already covers exact mapped unfinished nonloop 22–27 clips, including zero weight and an older clip whose latest hint changed. It does not invent a pain start or derive new timers.
- Ordinary RFNC pain and burning RFAP ownership remain exclusive. The existing composed burn join discharges residual obligations only after genuine RFAP ownership is validated. No source receipt is serialized or replayed.

Therefore this sender-side publication introduces no new persistence language or layout requirement. No codec file, version, capture/restore guard, RFCH policy or burn ownership is changed. This is source compatibility reasoning, not an executed save/reload result.

Inspected unchanged baseline hashes:

- `scene_npc_hit_location.inc`: `29459efbc9d96f7553bcf906e6a61759d8f7f684c005b7860c40d3f1844d3858`
- `scene_npc_pain_checkpoint.inc`: `a31eb5dc0317d5121eb48c4b28b82151288c126129f532b047cee805cda94a62`
- `src/core/npc_checkpoint.c`: `4805d427bd7ba471582d4265453ca5175405fc5ac830067db57198abfde449d7`
- `scene_npc_checkpoint_capture.inc`: `1120291697096031d96b07a0f5b848c206d18ab5a2677bd14f6d2225ba3b793d`
- `scene_npc_checkpoint_restore.inc`: `4edef85277e9a6150fe03bd2027103c6e61eaeb940194a34e772ee4cb3bbc899`
- `scene_burning_checkpoint.inc`: `011e7abd2d4492646200a24cc38d456e2a20c131c70272a3b120df68bc8d32ea`

## Resources and disposition limits

The contact already has all three reused reaction fields, so there is no receipt or persistent-size growth. Preparation declares a 12-byte difference, 4-byte side, 4-byte axis and bounded loop locals; a conservative 64-byte additional declared-object allowance keeps prior 2596-byte accounting at most 2660, under the 3072-byte cap. Actual compiled frames, nested existing stacks, Xbox arithmetic, runtime animation, save continuation and stock-memory behavior remain for the parent-owned batch.

No unresolved blocking source defect was found. This approval applies only to the exact hashes above and grants no further actor/weapon scan, death/head work, implementation expansion or execution authority.
