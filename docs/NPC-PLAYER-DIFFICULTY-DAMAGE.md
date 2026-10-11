# Ordinary NPC hit to on-foot player: missing difficulty input

## Result

Positive, source-supported missing consumer: the final `campaign_enemy_tick` player body-hit call passes multiplier `1` into the already reconstructed single-player damage dispatcher instead of the current original difficulty row. The actual admitted L3S1 guard1 UID109, holding its finite 12mm handgun, is the concrete qualifying example. Its raw primary damage is 40; active difficulty index1 selects 0.25, so the original body damage entering the class/armor phase is 10, whereas the current call supplies 40. This is source arithmetic, not a newly observed hit.

The damage service, class factors, armor split, physical shield consumer and finite gun owner already exist. No replacement combat system is needed. Guard short-flight remains separately deferred and was not reopened.

Initial code inspection was at `2c7d33a6565c1057265f3213c0ea200aaeaca4a6`. Receipts and the staged proposal use `2e7599e5c611304dc95befa23e3476334ae18b2e`, whose only intervening changes are the 01:00 report and TO-DO.MD. Relevant code is identical. The parent-reported 01:00 build/startup pass does not validate this new proposal, shots, armor, or saves.

## Immutable bounded inputs

- Original RF.exe: `/workspace/shared/recovery/restored/Installed_Game/RF.exe`, SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
- Reused existing selected UID109 record, without scanning another level or actor: L3S1 entry offset19175424, bytes1604962, SHA256 `c66e3d2de723903b7bad2220d08bbc62942b9f36227c66dddca97d30e8f9f5dc`; record offset1125897, bytes211, SHA256 `272d88d9e9c53a06f0d19a8740f75deb1c5b6a8be415a10e204835c4db902a39`. Its selected primary is 12mm handgun, secondary none. Existing state and health/armor remain unchanged.
- Reused selected handgun block: weapons.tbl offset21425, SHA256 `3d7c3cc59865dc0bb059c51701d5965a45819bce600e35a24b4119cca1d8e9e8`. It has primary damage40, bullet kind1, SP clip16, reserve capacity125, and no authored AI damage override or piercing flag.
- Existing loadout adapter preserves actual named acquisition and finite reserve/clip ownership. Existing readiness/reload gate and accepted shot debit remain unchanged. No weapon, ammo or health grant is proposed.
- Reused selected guard1 class: SHA256 `f2d733d0c7aeda0642ccd43a99ea9d8178fc3e076ae149839159a51487cad3fd`. Its secondary flags contain only collide_player; the special source flag inspected by42cdb0 is absent.

Full reused inputs, fixed-address disassembly, constant bytes and line-numbered current excerpts are in this directory. SOURCE-IDENTITY.json records file hashes.

## Original ordering for this actual source and ordinary player target

1. Normal table setup4c2a20 selects the primary authored AI-scale lane and copies requested593e58 into active593e54. The existing parser defaults absent AI scale to1. Nonplayer primary getter4c8bd7..4c8bea multiplies runtime class120 by class108. The selected handgun therefore supplies40. This matches the port's `combat_enemy_primary_damage`.
2. Physical held shields are separate object contacts. The original held-shield eligibility at494e23..494e62 uses the selected shield/live clutter owner and incoming direction. The shield's410270 arithmetic uses its own class factor and durability. This is not the ordinary player entity armor owner.
3. Nano contact, if present, is an earlier separate consuming path at4c5a7f..4c5b5d; its debit comes from the raw getter before generic difficulty. This work does not add ordinary-player Nano support or change any Nano path.
4. Ordinary body preparation4c5cfc..4c5d6a obtains fresh damage. For a player target,42ce00 calls4895d0, then48a840 on the projectile source. The guard source lacks player bit8, so42ce39 branches to42d032 and returns exact1.0. There is no missing player hit-location factor required by this correction. Source42cdb0 also returns false for this guard1 class. Pistol has no piercing flag;4c9b20..4c9b47 leaves the amount unchanged.
5. Body call4c6118..4c6132 supplies force0, auxiliary UID-1 and the authored bullet kind to4892c0. That dispatcher first checks the target and raw minimum0.001, sets the damage flag, then performs ordinary invulnerability/armor-immunity checks. With SP globals64ecb9 and6fc4d8 zero and kind other than9,48aaf0's direct bit8 player branch selects the target;4894a0..4894da loads593e54 and applies593dd4[index], storing binary32 before41a350.
6. Exact original table bytes at593dd4 are `cdcccc3d 0000803e cdcccc3e cdcc0c3f`, corresponding to `.1f, .25f, .4f, .55f`. All four are below1; the current hardcoded1 matches none of them.
7. Entity41a3c1..41a421 applies the target's retained class damage factor, then41a7c0 absorbs `min(armor, scaled * .52f)` for bullet kind1. Armor and remaining health are stored in their original order. Thus, for a factor1 body with enough armor, index1's10 incoming gives approximately5.2 armor /4.8 health damage, rather than approximately20.8 /19.2 from40. These are explanatory arithmetic amounts, not staged vitals or runtime results. With exhausted armor the full class-scaled amount reaches health.

The existing core `rf_damage_dispatch_sp`, `rf_entity_damage_sp` and `rf_entity_damage_vitals_sp` already own steps5–7. Changing the raw shot amount or the AI-scale definition would incorrectly affect shield/prop/vehicle contacts and could double-apply difficulty.

## Current exact missing join

`scene.c:12085..12088` computes fresh authored AI-scaled damage. The finite inventory is checked at12377 onward and debited at12503..12509. Actor-ray selection and world/fragment/clutter/vehicle ordering precede shield commit. Physical player shield commit at12651..12654 receives raw `shot_damage` and consumes accepted hits, including break. Only surviving body hits reach the final block.

`scene.c:12674` constructs `{shot_damage, source, kind, 0, UINT32_MAX, 0}`. NPC targets retain their existing separate call. The player branch at12684 calls `rf_scene_player_damage(..., 1, ...)`. Its registered-player adapter passes that value directly to `rf_damage_dispatch_sp`; no intervening scale compensates for the hardcoded1.

The existing campaign difficulty owner is `rf_scene_campaign_countdown.difficulty`. New-game construction sets1. Existing pickup/countdown consumers already read that owner; this work does not add a difficulty menu or infer the actual runtime saved value from the default.

## Smallest staged implementation proposal

Only src/diagnostic/scene.c changes:

- Add one allocation-free, scene-local lookup with the exact four binary32 constants and explicit current-index0..3 validation.
- At the single final player body-hit branch, capture the same current target handle about to be passed, choose the multiplier, and pass it to the existing public damage API. There are no intervening callbacks in the selection-to-call interval.
- Qualify the current full registered type0 player owner with bit8 and existing ordinary on-foot scope: no linked owner, active driller, mounted turret, or active cutscene. These are explicit bounds on this correction, not original immunity rules. Outside that bound, retain the preexisting multiplier1 path; do not suppress hits or add fallback immunity.
- Preserve low-byte force bypass and kind9 exemption. The generic dispatcher still owns the sole actual multiplication and its original target/eligibility checks.
- Do not require a currently living/armed shooter after a shot was accepted. Do not reinterpret request.source, mutate shot_damage, add a source snapshot/lifecycle repair, alter target choice, or rewrite the general explicit-multiplier APIs.

This is a shared final consumer correction, not a UID109 or pistol exception. Its code footprint includes the remaining synchronous firearm and immediate-melee cases that reach this exact block, with the same generic original4892c0 semantics. Separate shotgun, actor-penetration, projectile, special-weapon, environmental and scripted-event damage consumers are outside this patch. No broader actor/weapon/campaign asset search was performed and no parity claim is made for those untouched consumers.

The staged patch is `guard-pistol-damage.patch`; the proposed full file is `stage/src/diagnostic/scene.c`. STAGED-PATCH-IDENTITY.json records baseline/staged/patch hashes. Independent hash-bound review passed for this exact patch before parent integration; see REVIEW.md. No repository edit has been made by this worker.

## Ownership and compatibility

- The table is16 constant bytes; temporary index/multiplier/target values are synchronous locals. No heap, per-NPC field, deadline, RNG draw, body pose, shot cadence, finite-ammo change, or additional gameplay owner.
- Difficulty remains owned by campaign history. RFCH3+ already stores it at header byte88; capture/preflight constrains0..3, and RFCH1/2 prepare defaults1. Successful publication assigns the saved countdown owner. No RFCH/RFNC/player/save layout or migration change is needed. Restoration is not replayed and saved vitals are not rescaled retroactively; the corrected current difficulty applies only to future qualifying body hits.
- The public explicit-multiplier APIs keep their meanings. Existing callers that deliberately pass1 keep doing so. Parent owns scene integration and serial scheduled Xbox validation.
- Existing callback/accepted-shot lifetime limitations are not repaired or expanded by this lookup. Full current target checks bound the lookup; the existing damage API still verifies registered ownership. This proposal does not claim original scheduling, trajectory or collision fidelity.
- Invalid current difficulty on the qualified lane returnsRF_FORMAT rather than clamping or inventing a default. This is an explicit port input guard. An invalid index outside the correction's qualified lane is not newly consulted.

## Verification limit and stopping condition

This task used fixed source reads, fixed binary disassembly and constant extraction only, then staged the parent-authorized minimal proposal outside the checkout. No builds, syntax checks, tests, new fixtures, route runs, emulator execution, image/audio work, health grants, source execution or cleanup were performed. No new action/save/runtime evidence is claimed. Independent source review is complete. Parent integration and the next scheduled 02:00 compilation remain separate; no action or save validation is claimed. Do not expand this into other damage sources or reopen guard flight.

## Parent integration

The exact independently reviewed source patch is integrated after the01:00 build. It awaits the02:00 Xbox build; no damage-action or runtime/save validation has run. The existing midnight startup remains the latest bounded runtime result.
