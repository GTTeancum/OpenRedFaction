# Authored Goto AI suppression

2026-10-10. Independently source-reviewed and parent-integrated after the successful 14:00 batch, based on `1fd5bfbab73716c5b25c8c4b75b8a597c6353ed4`. This new code awaits the 15:00 Xbox compilation. Gameplay, audio, save/load, memory admission and callback behavior remain unverified. No builds, syntax checks, tests, fixtures, forced events, campaign routes, emulator runs or PC work were performed for this new slice. The prior 14:00 result does not cover it.

## Implemented slice

The actual L11S3 Eos actor12084 is armed with a Machine Pistol. Its original Goto12127 ->12128 ->12129 chain, reached through When_Dead12067 -> Delay12085 -> Explode12078 -> Shake_Player12087, uses `words[1] == 1` on all three Gotos. The scene previously ignored this field, so ordinary sight/hearing/damage could enter combat during the authored move. The bounded authored census remains only the already-read L3S1/L11S3 inputs; no wider campaign inventory is implied.

A successfully admitted ordinary NPC Goto now copies the exact comparison `words[1] == 1` into its actor-local AI0x40000000 latch. Values other than1 clear it. The mutation resolves the complete registry/entity handle and never touches a shared class. The existing `ai_mode.flags_530` bit is canonical; only that bit is masked independently into `view.flags_7d0` and `firing.flags_7d0`. No live actor member, event scan or global runtime flag bank was added.

The flag suppresses fresh ordinary opposed/player acquisition, gunfire hearing and damage reaction. The seven direct damage-alert writers (six adapters plus the ordinary player ray) also obey the same gate, preventing their fallback alert assignment from bypassing the damage callback's rejection. Damage, pain, death, audio and disguise consequences of an already-existing alert retain their owners.

Only actual final authored destination completion clears the latch. Intermediate four-node route-window exhaustion, blocked motion, retry, physics freeze, queued shots, new Goto_Player/Follow_Waypoints commands, AI-mode/default changes and other order replacements do not clear it. Completion retains the existing body-space .25 arrival policy; it does not introduce original route solver parity. A later accepted Goto explicitly overwrites it.

## Firing and unsupported overlaps

This is a reaction/state-selection veto, not a universal firing mask. Original primary401580 does not test0x40000000, and408e90 tests bit1 only. The scene's finite reload-completion service, already-owned delayed strikes/projectiles, existing deadlines, ammunition and queued Shoot_Once modes0/1/2 are not gated or modified by this patch. Shoot_At explicitly owns fixed-point firing and remains allowed. The existing Goto command's preexisting combat/order reset policy is unchanged; this patch adds no new timer, ammo or reload resets.

Original Attack under the latch calls408ac0, which refuses state selection, then409050, which may change only target/cache and never action/state/route. The port has no save-representable target-only owner: setting combat_scripted/alert would replace the movement and enable pursuit/fire, while target alone fails existing RFNC admission. This narrow suppressed Attack ON overlap therefore returns RF_NOT_FOUND before any NPC mutation. At the real event type38 callback this is a benign unsupported target (`other_targets`), not a frame failure; common authored links still propagate. Attack OFF and all nonsuppressed Attack behavior are unchanged. Neither already-read L3S1 nor L11S3 contains an Attack record. Exact target-only ownership remains deferred.

Original Goto OFF is a no-op. By explicit scope decision this patch retains the port's existing OFF-stops-movement approximation, while preserving the suppression latch. It does not claim original OFF parity. Masako's separate reveal/target owner and explicit Alarm semantics are unchanged; their latch overlaps remain outside this slice. Ordinary original sight can retain target bookkeeping before its gated selector; the port gates its combined acquisition transition instead and does not claim exact cached-target parity.

Vehicles/generated-head autonomous AI, every other Goto flag, original route-admission retry timing, exact authored chained-delay travel, original burst/deadline scheduling and broader command-interaction parity are deferred. Existing navigation/movement, freeze, finite weapon services and script callbacks are retained. No campaign progression claim follows from this source patch.

## Persistence

The latch is independent of `script_move.active`, event UID, current AI mode, affiliation and physics state. Explicit replacements can leave it set. It is therefore captured directly on every living, terminal or retired NPC row, never inferred from an event or silently zeroed at death.

- RFNC21 conditionally appends one LE32 compact0/1 field after the RFNC20 item tail on every row. The writer selects21 only when at least one row has the latch; prior version-selection precedence is preserved. Pre21 decode supplies0. New values above1 are invalid. Header/version/span/length/hash checks and disjoint-output two-pass validation remain unchanged apart from the added field and structural bound.
- RFCH7 conditionally appends LE32 compact0/1 at actor-row offset84, growing those rows from84 to88 bytes. The independent actor history bank stores one byte per existing level/UID slot. Pre7 supplies0. In-memory section capture and revisit assignment retain it independently of the existing unsaved AI-mode/route sidecar.
- Current-level RFNC and all-level RFCH must have exactly equal latch values for the same staged level/UID, including terminal/retired records, before publication. Legacy/new combinations with conflicting values reject rather than choosing a component.
- RFNC candidates, in-session before-state comparisons and fresh owner snapshots retain the field. Capture also rejects inconsistent AI/view/firing mirror bits. Candidate preparation and validation do not publish the latch. The final successful NPC restore_finish masks only0x40000000 into all three representations, including legacy-zero restoration; no event replay, callback, target reset, ammo reset or class mutation occurs. Full-world load reaches it only after fallible vehicle publication and storage close succeed; standalone component commit also calls finish. RFCH assignment and slot rebinding are moved together to that same success branch, so a failed close preserves the prior lane together with its level/UID keys. This is new-latch failure protection, not whole-world rollback: preexisting world/vitals/pose assignments can already have happened before a later failure.
- Existing RFTU2/3 generated-base snapshots already serialize the complete7d0 flags. Their capture and candidate admission now cross-check this one overlapping bit against RFNC. Full-world RFTU owner assignment preserves the prior canonical bit in all mirrors until NPC restore_finish; its complete standalone assignment still publishes the candidate bit. No RFTU format change or new turret gameplay admission is introduced.
- Existing omitted AI-mode/shield sidecar guards and all previous motion/physics/resource/placement checks remain intact. RFCH7 does not make those unrelated sidecars persistent.

## Source evidence

Original executable SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- Loader46229b/4622a6 reads word0/word1;4623ea..4623fb passes them to factory4b7d00;4b7d3b..4b7d44 stores exact `word1 == 1` at event+2bb.
- Goto ON4bab00:4bab4a..4bab67 passes that byte to40cfc0. After its successful4271b0 admission branch,40d053..40d06d sets/clears entity+7d0 bit0x40000000, then40d080..40d08f chooses action2/state1. AI is embedded at entity+2a0, so AI+530 aliases entity+7d0.
-408346 skips noise peers with the bit;408634 and408932 reject reactions;408ae2 returns before AI selection. `rf_entity_ai_select` already retains the latter source gate, while this patch connects scene consumers.
- Action2 dispatcher403828[1] ->4036b9 ->403880; state1 at4039ce..4039e0 calls40b6d0 ->40b825 ->40bb70. Final route advance40c2a0 returns-1 when current index+18 reaches count-1;40bf25..40bfb3 then calls407820, whose40786b AND0xbd7fffff clears the bit. Do not confuse action6 handler40537a with the specific Goto path.
- Goto_Player4babb0 uses0xfffff7f7, preserving40000000. SP action setter407e20 and default407ee0 preserve it. Goto/Goto_Player OFF dispatches to4b9f80/4ba008 no-op.
- Attack4bcb39/4bcb75 calls408ac0 before409050. Setter409050..409135 only changes target/cache/timestamps/timers, not action/state/route/suppression. Event.c type38 maps RF_NOT_FOUND to nonfatal `other_targets`; common action2 retains link dispatch.
- Shoot_At4b95e0 explicitly selects action9 at4b9629 and stores its point at4b9638. Its handler4051f0 invokes425830 at4052d4 and426ca0 at4052de without a suppression-bit test. Shoot_Once4bad80 invokes independent425830/426ca0 as well.401580 checks408e90 bit1, not40000000;409280 owns continuing fire work independently.

## Source-derived memory/layout impact

These are declaration/wire arithmetic, not measured compiled sizes or runtime peaks.

- Live `campaign_npc_body`: no new bytes. Existing AI/view/firing fields carry the bit.
- `rf_npc_checkpoint_record`: +4 bytes. All of its data members are32-bit-aligned scalar/array records; there are no pointers or doubles. Capture/decode arrays grow4*N. Restore entries contain saved+before rows and therefore add8*N bytes on the32-bit Xbox layout; owner_before itself is unchanged.
- RFNC structural maximum:1712 ->1716 bytes per row; individual stack wire scratch arrays add4 bytes, and local checkpoint rows add4 bytes. A version21 component uses4*N additional wire bytes relative to version20 (at most8192 bytes at2048 structural rows). If its previous highest required version was below20, all intervening version tails are also emitted as required by the existing versioned format; actual save growth is not always merely4*N. Promotion fromRFNC14 to21 adds136*N bytes; the19 authored L11S3 rows would add2584 bytes if all19 are admitted and14 was the prior required version.
- `rf_campaign_actors`: +2048 bytes for the new byte lane. The single live store and each private RFCH stage copy each grow2048 bytes. No second persistent bank is added. RFCH7's actor-row tail adds4*M wire bytes relative toRFCH6, at most8192 bytes; an older starting version also entails intervening tails.
- The history structural maximum increases8192 bytes. All existing caller allocation budgets and the Xbox `RF_CHECKPOINT_FILE_MAX=262144` total transport cap remain unchanged. Larger structurally valid histories can still be rejected by those unchanged practical budgets.
- RFTU payload/row sizes, scene resource catalogs, textures/audio, actor-body layouts, route arrays, FIFO capacity and frame allocations are unchanged. Actual linked .text/.bss deltas and stock64-MiB save/runtime admission await the parent batch.

## Final storage boundary audit

Full-world load previously assigned RFCH and rebound NPC history slots before a later fallible occupied-vehicle publication and storage close. Merely deferring the new byte lane while replacing its keys earlier would miskey the prior values on failure. This patch moves the single already-validated scene_campaign_history_checkpoint_assign_rebound call into the post-close success branch immediately before NPC restore_finish. The complete direct RFCH publisher remains unchanged. No new staging bank or global load-mode flag is introduced.

Every intervening consumer was source-inspected. None reads the actor/switch/event history stores, their saved arrays, persistence_slot, RFCH countdown, or RFCH import/export carry sidecars:

- Environment assignment consumes staged gravity/force/nav/cutscene/monitor/blackout/sight/RNG fields.
- Remote publication and restored remote selection use their own charge/follow-up pool; player body/look/vitals/inventory assignments use the admitted player candidate. Primary selection/ammo publication use current weapon catalogs, player inventory and current Machine Pistol mode, not its export sidecar.
- Weapon-mode assignment updates current Machine Pistol/Undercover/shield/Fusion/RNG owners; projectile assignment copies candidate flights and flame state. Its burn subassignment resolves fully registered live handles and restores pain deadlines; it does not consult actor history.
- Player-form publication and support writes are direct candidate assignments. Jeep-gunner publication uses staged session/player/host/seat owners and live handles.
- The potentially fallible occupied-vehicle publisher stages/restores rigid host and player occupancy, live registry/entity handles, pose/seat/weapon state and authored loadout. Its parked, combat, submarine/fighter, possession and loadout paths do not consume the moved history or latch lane.
- Vehicle physics/switch/secondary pool/allegiance/attack/AI assignments use admitted runtime pointers and vehicle-local state. Headlamp refresh touches vehicle/glare owners; loadout refresh resolves authored class metadata. Masako assignment changes only its admitted phase/effects.
- Turret-player publication uses staged host/player/aim ownership. Impact/audio relocation resets presentation state. Switch ambient restoration resolves live event switch_state and ambient instances, not campaign_switch_history/saved arrays.
- Remaining operations are support/telemetry/log fields, success markers and storage close. No ordinary gameplay tick or event dispatch is inserted between these assignments and final history+NPC finish.

Exact RFNC/RFCH/RFTU candidate joins and all rebind validation remain before any publication. After success, the whole history copy establishes the admitted level/UID slots before finish writes each NPC's latch into its now-rebound slot. Pre-close RFTU assignment is explicitly separate from complete component assignment so it cannot publish a new suppression bit early.
