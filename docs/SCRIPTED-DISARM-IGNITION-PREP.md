# Scripted disarm and ignition

Prepared2026-10-03 from read-only installed SP level scans, current source and static disassembly of `Installed_Game/RF.exe`, SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. The initial audit used no original-game runtime or screenshots. Both callbacks are now integrated; native results are recorded below when available.

## Drop_Weapon81

All three installed SP records:

| Level | Event | Targets and context |
| --- | --- | --- |
| L1S1 | 9494 | env_guard8323; delay14.5s; record texts `none`, `miner_wrestleminer.mvf` |
| L13S3 | 9734 | miner8687 and nonentity UID9865; delay0 |
| L14S3 | 10607 | miner9642; explicitly linked from Delay10167 |

Factory `4b69d0` selects generic constructor `4bee70` for81. Generic ON branch `4b9176` calls `4b9c20`. That action walks linked handles, admits entities through `426fc0`, and if primary ID at entity+2a4 is not-1 invokes `41ae70(entity_handle,weapon)` followed by `42ae10(entity,0)`. At `4b9c7c..4b9ca9` it processes two held clutter handles at+145c/+1460 through `410c70` and `48ab40`, clearing resolved handles. At `4b9cab..4b9cc7` it zeros16 inventory words at+42c and sets primary+2a4 and secondary+2a8 to-1. This is a living-actor disarm, not a death callback or holster-only visual change. Exact lower-level drop physics/ammo semantics were not reconstructed in this bounded audit.

`src/core/event.c` now dispatches81 through a typed ordered linked-NPC callback and admits its pending delayed actions. Original OFF dispatch at4b9f80 sends both81 and82 to4ba008 (no-op); common propagation at4b8c40 admits both through4b8c5e with AL1. The implementation preserves that behavior and checks registry generations before each callback.

Scene hook: `campaign_weapon_drop_emit` in `src/diagnostic/scene.c` already maps supported primary weapons, retains finite quantities, places the drop and calls `rf_campaign_actor_drop_emit`. The latter accepts living actor slots; its single-drop ledger prevents duplicate emission. `campaign_weapon_drops_tick` and existing drop draw/resource demand own subsequent collection/presentation. The emitter itself does not disarm its owner. Compose admitted emission with explicit live inventory/primary/secondary removal, held-model cleanup and queued burst/reload/fire cancellation. Do not call `combat_death_start`, fabricate damage, clear unrelated route/health state or remove inventory before an emission error is resolved. Define the no-ground-hit/unsupported-gun policy explicitly; the current emitter can return success without producing a drop.

Bounded future check: use an original linked actor/event, verify actor remains alive, one collectible finite-ammo drop exists, held gun is absent and subsequent ordinary AI cannot keep firing that gun. Repeated action must not duplicate its drop. No campaign traversal is needed.

## Ignite_Entity82

Both installed SP records:

| Level | Event | Targets and context |
| --- | --- | --- |
| L2S1 | 7190 | riot_guard7188 and nonentity UIDs7189/7206/7211; linked from Make_Invulnerable7191 |
| L20S1 | 12694 | comp_tech12020 and nonentity UID12705; linked from Explode12436 |

Generic ON branch `4b9180` calls `4b9cf0`. It walks linked entity handles through `426fc0`, skips absent entities and actors already holding nonzero burn owner+13d8, then calls `42e910(target_handle,-1)` and stores the returned burn owner at+13d8 (`4b9d23..4b9d3b`). The action does not first apply a synthetic weapon hit. The invalid source is deliberate; do not substitute the player. Lower-level burn lifetime/immunity behavior is outside this static action audit.

The dispatcher and pending scheduler now admit82. The scene adapter uses existing `scene_burning_create(target,UINT32_MAX)` in `scene_burning.inc`, publishing its returned handle through the same damage-owner field used by existing ignition. Existing burning tick owns periodic damage, extinguish and death; do not duplicate those loops. Keep already-burning actors idempotent and preserve ordinary invulnerability handling, especially the real L2S1 setup chain.

Bounded future check: use original7190 or12694, verify a real burn owner without immediate fabricated damage, continued periodic processing and no duplicate owner when fired again. Inspect lifecycle facts through existing probes; no images or playthrough.

## Excluded false positive: Set_Liquid_Depth40

L5S2 has three records:3940(depth2.75,duration0.15),3948(depth8.5,duration4),4132(depth4,duration0.15);3940/4132 link room UID3134. However, installed original ON `4bcbe0` is a single RET. Local Dash Faction `game_patch/object/event.cpp` explicitly patches that address with `EventSetLiquidDepth_turn_on_new` and adjusts the short durations. Those records and the community patch alone do not prove an omitted original liquid-changing implementation. Do not prioritize it as a reconstructed gameplay gap without separate product intent or new evidence.


## Integrated first pass (2026-10-03)

`scene_scripted_disarm.inc` stages firing reset and playback changes before committing the ordinary grounded drop, then clears inventory, primary/secondary weapon IDs, queued scripted shots, burst and reload state. Health and unrelated movement routes are retained. Already-unarmed repeats are idempotent. Unsupported primary weapons, absent ground, already-used drop slots after rearming and resets requiring unavailable external callbacks reject without removing the weapon. Physical tumbling, held clutter and second-weapon drops remain outside this first pass.

`scene_scripted_ignite.inc` resolves a live registered NPC and creates one common burn with invalid source attribution, preserving health at creation and ignoring repeated ignition of the same burning owner. Authored ignition requests flame/impact resources even when no flamethrower is present. Burn reset now occurs at scene initialization rather than frame0, so startup-event ignition survives its first update. Damage, immunity and extinguishing remain owned by the shared burn loop.

Focused core verification: `rf_event_ai_mode_tests` passed ordered mixed-link dispatch, stale/duplicate targets, OFF behavior, downstream propagation, ordinary delays, missing backends and error stopping for teleport/disarm/ignition. No presentation claim is made from memory telemetry.


## Stock64MiB Xbox results

`artifacts/xemu/scripted-disarm-20261003-152546/report.json`: PASS,970 frames. Original L1S1 guard8323 and Drop_Weapon9494 retain their actual loadout, health and14.5-second delay. A quiet setup and transform-only staging place the actor in CTF06. Frame840 confirms no early drop. The action applies at14500ms, emits exactly one Riot Stick with100 charge and removes ownership/held model token/queued fire without changing health50. A UID-only duplicate of the same authored event applies at15500ms and is idempotent. The final live actor stays disarmed and its one drop remains available. This does not prove player collection, visual/audio presentation, active firing cancellation or save continuation.

`artifacts/xemu/scripted-ignite-20261003-152904/report.json`: PASS,180 frames. Original L20S1 technician12020 and Ignite_Entity12694 are staged in CTF06 with only the actor transform changed. The ordinary event creates one unowned burn; immediate health remains100. The repeated request at frame60 retains that burn and its lifetime. 12 periodic damage pulses reduce health to53.846160888671875; 180 ticks elapse without resetting the timer. One ordinary scene teardown retires the burn. No weapon hit, Slay, death or capacity failure contaminates the result. 6054 physical pages remain available. Full-lifetime mortality, extinguishing, saves and presentation were not exercised.

Both harnesses restore the ordinary disc inputs and rebuild successfully after their run. Disarm retained6082 free pages. No original-game runtime, screenshots, host input or campaign walkthrough was used.
