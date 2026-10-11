# Ordinary player outside kind-10 difficulty: positive source finding

Inspected frozen HEAD `c30cd51a5306e2ae691774297d8e213b5d8ca74e`. Source-only result: the already-integrated ordinary outside consumer is missing the current difficulty input. Its hardcoded multiplier `1` is not an intentional force, kind, source-handle, or player-predicate bypass. This result follows the original outside call itself, not an analogy to NPC weapon hits.

## Exact original evidence

Source: read-only `Installed_Game/RF.exe`, SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `4211d9..4211f9` prepares the existing binary32 amount and calls `4892c0`. The eight cdecl arguments in call order are target handle, amount, source `-1`, argument4 `-1`, kind `10`, argument6 `0`, auxiliary UID `-1`, force `0`. In particular, `4211e5` pushes final argument zero and `4211eb` pushes kind10.
- `4892c0..4892fe` resolves the target and rejects a raw amount below binary32 `.001`. `4892fe..489312` sets object flag `0x200000` and tests the force argument's low byte. Nonzero force skips to dispatch `4894de`; outside supplies zero, so this bypass does not apply.
- `489318..48934f` retains ordinary invulnerability and armor-immunity checks. Those checks may reject damage; they do not establish a separate unscaled outside lane. No eligibility change is needed.
- In the established SP mode, `489350..489357` branches directly to `48947a`. There, `48947a..48947f` exempts only kind9. Kind10 continues through zero SP/alternate-mode globals at `489481..489491`, then the target predicate at `489494`.
- Predicate `48aafa..48ab04` returns low-byte1 immediately for target object bit8. This is exactly the bit required by the current ordinary outside consumer.
- `4894a0..4894a9` reads active difficulty index `593e54` and multiplies the request by table `593dd4[index]`; `4894da` stores binary32 before entity dispatch. Table bytes are `cdcccc3d 0000803e cdcccc3e cdcc0c3f`, the existing `.1f, .25f, .4f, .55f` rows. Source `-1` is not consulted to bypass this SP target-selected lane. Alternate and multiplayer modes are outside the existing dispatcher contract and this finding.
- `48950f..489524` then forwards the scaled amount, source, kind and auxiliary UID to entity `41a350`. Existing class, vitals and effects owners remain downstream.

Fixed-address disassembly and constant bytes are under `original/`. No original code was executed.

## Current missing join

`src/diagnostic/scene_player_outside_damage.inc:24` constructs kind10/force0, and its unchanged ordinary-owner gates require bit8. Lines69..70 call `rf_scene_player_damage_audio(handle, &request, 1, ...)`.

`scene.c:6996..7000` forwards that scalar to `campaign_player_damage_apply`; line6988 passes it unchanged into `rf_damage_dispatch_sp`. Its current player stage2 predicate at lines6915..6921 returns the target's bit8. `src/core/entity.c:602..609` therefore performs the ordinary kind-not9 multiplication, but with1. There is no compensating current-row read between the outside caller and the shared dispatcher.

The correction belongs at this already-qualified caller join. Keep request amount, force, kind, target/source/UID, raw minimum, immunity, class factors, damage effects, and explicit-multiplier APIs unchanged. The core dispatcher remains the single multiplication owner.

## Existing owners and ordering to preserve

- Original entity update calls liquid `421240` at `41e549`, then outside `421170` at `41e54f`.
- Current scene commits movement and support first, then calls liquid at `scene.c:21126`, outside at21127, and the independently eligible optional gasp owner at21128. The existing post-movement room phase is an explicitly documented port adaptation; this work does not change it.
- Outside retains its current full ordinary player registration/body/life/scope qualification, then current-armor and committed-room/class checks. A row lookup belongs only after those existing checks; it must not create exposure or alter their ordering.
- The existing current scalar owner is `rf_scene_campaign_countdown.difficulty`. No new setting, retained field or save owner is needed. RFCH3+ stores it at byte88; capture/preflight validate0..3, older versions prepare1, and publication assigns the countdown owner. This finding does not infer a live saved index from the default.
- The shared audio-capable damage service still owns class/vitals/effects and errors; outside's current clock and borrowed RNG stay unchanged. Existing kind10 pain/flash policy and independent gasp handling stay unchanged. Normal `campaign_life_input` owns subsequent lethal handling; no new death path or save replay is introduced.

## Minimal reuse proposal for independent review

The new NPC-specific helper at `scene.c:12094..12110` already contains the exact table and current-index validation. Its gates are not too narrow for the existing outside consumer: the latter already requires them and more. However, the helper is declared later than the outside include and is not itself a pure row lookup.

Factor only that unchanged table/index lookup into an earlier scene-local, allocation-free helper. Keep the NPC helper's full existing gates and fallback1 behavior; replace only its final lookup with the shared row lookup. Let outside call that pure lookup after all current outside admission checks and pass the result into its existing damage-audio call. Invalid current index should fail with the same explicit port `RF_FORMAT` guard on the qualified lane, without clamping/defaulting. No general API rewrite or duplicate table is needed.

The independent reviewer agreed with this extraction approach. `outside-difficulty.patch` now stages exactly two files outside the checkout against the frozen commit; full originals and proposed files are in `baseline/` and `stage/`. `STAGED-PATCH-IDENTITY.json` binds their hashes. The pure helper takes the current index explicitly, validates the output pointer, preserves `RF_FORMAT` for indices outside0..3, and writes its output only on success. The existing NPC helper retains its fallback1 and all gates; only its final row selection delegates to the pure helper. Outside selects after its unchanged eligibility checks and before request telemetry and dispatch. On an invalid current index, its existing error telemetry records the port guard; no request is counted or issued. No supported valid-state order changes.

Exact-patch independent review passed with no blocking findings; see `INDEPENDENT-SOURCE-REVIEW.md` and `INDEPENDENT-PATCH-REVIEW.md`. No compilation or tests have run for this stage. Repository integration additionally waits for the parent's frozen02:00 build and exact patch review.

## Scope and verification limits

This is a conditional source defect in an existing consumer, not evidence of an admitted exposed actor or observed health loss. No new asset scan, exposure case, room/armor mutation, oxygen work, NPC exposure work, source amount change, new fixture, test, build, PC/Xbox runtime, campaign route, forced event, cleanup, or repository edit was performed. The scheduled02:00 build belongs to the parent and does not validate any later staged change. Source identities and frozen clean checkout status are in `SOURCE-IDENTITY.json`; current line-numbered excerpts are under `current/`.

## Parent integration

Exact reviewed patch integrated after the02:00 build completed. NPC current-difficulty source compiled at02:00; this outside/shared-lookup successor awaits03:00 compilation. No exposure, audio or new save/runtime coverage is claimed. Durable source/evidence archive: Library libfile_96d4f24b834c819195fb962631c461d5 version0.
