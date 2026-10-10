# Residual NPC fire/reload presentation persistence

## Status and scope

The residual ordinary-fire extension and its action-39-only prerequisite are independently source-reviewed and parent-integrated, awaiting the 12:00 Xbox batch. The bounded per-slot composition correction below is source-written against `64161094bdd93d75efed3231bf17feaaaebb33b9` and independently source-reviewed and parent-integrated, awaiting the 12:00 Xbox batch. No build, test, new fixture, gameplay, route, worktree or cleanup was run by this helper. Compilation, save/load/save and actual action/visibility continuation remain unverified until parent-scheduled validation.

This admits retained ordinary-weapon fire/reload presentation after no ammunition obligation or combat/script-animation owner remains. The exact residual nonloop set is a nonempty subset of the selected weapon's mapped actions {2, 39}, at most one slot per distinct motion; additional nonloops require their own existing saved owner. Rocket retains only its prior action-39 exception. No wire-format version, saved owner, gameplay field or timing policy is added. This does not implement or unblock the deferred broad Shoot_Once target-release candidate.

## Source basis

Original RF.exe 1.20 NA SHA-256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `42c3c0–42c445` resolves nonmelee fire presentation and starts the selected action, falling back to action 2, at weight 1 and freeze false through `428c90` / `5033b0`; the current port explicitly selects action 2.
- `425648–425657` starts action `0x27` (39) at weight 1, with freeze false, through `428c90` / `5033b0`.
- `419d8c–419de4` completes staged ammunition and clears its flag/timer without stopping the model clip.
- `51bc53–51bc5c` keeps nonloop clips out of marker generation; `51be6a–51bf91` completes and removes unfrozen nonloops at their authored end without an attack callback.
- `4261a5–426273` branches a positive primary impact delay to an independent timer/debit owner; immediate factory dispatch is at `426274`/`426665`. Melee owns separate impact timers. Neither is inferred from a body clip.
- `41dbea–41dd49` can suspend model advancement by descriptor, room visibility and LOD. The force-advance high bit is not reload-owned.

The port already retains separate ammunition, attack and playback owners: `scene_ai_reload.inc`, `scene_ai_reload_completion.inc`, the finite transfer primitive in `scene_ai_gameplay.inc`, `campaign_enemy_tick`, `rf_motion_update`, and the complete RFNC playback payload. `campaign_enemy_fire_presentation` starts action 2 without installing a callback or delayed shot. For the nine synchronous ordinary primaries below, all contact/pellet handling completes before `enemy_shot_done` consumes a queued request. Starting presentation alone does not establish completion; save admission remains at the existing stable frame endpoint, and pending FIFO/cadence/target owners keep all existing checks.

`motion_control` adds or restarts only the requested motion, without stopping other nonloop slots. An ordinary queued shot can spend its last loaded round and retain action 2; a later request can start reload/action 39, then expire while the independently serviced reload completes. Visibility/LOD culling can suspend model advancement throughout, leaving both clips after no ammo obligation remains. Set_AI_Mode also clears combat ownership without stopping a reload. Neither deleting a clip at an ammo deadline, forcing completion, nor admitting only a single nonloop is justified. The profile retains both mapped clips unchanged; if the two actions alias one motion, it retains only one slot.

The existing `campaign_enemy_tick` pain gate deliberately bypasses a pending animation lock for `Shoot_Once` (`pending && !once`). Fire presentation then starts action 2 without stopping an independently owned pain action 22/23; culling can retain both. Removing combat ownership through the existing Set_AI_Mode path does not remove either clip. The prior helper incorrectly let this additional owned pain clip veto the residual weapon slot. Admission must compose the existing owners per slot instead of blocking that gameplay path or changing clip timing.

The nine ordinary primary declarations in the installed `weapons.tbl` have no primary `$Impact Delay:`; the existing reader defaults absent slots to zero. This is bounded source/table evidence, not a new parser or live-state requirement. Original marker consumers and the port's `rf_motion_update` do not grant pending attacks to residual nonloops; dominant looping footstep clocks/event bits remain serialized unchanged.

## Admission and continuation

The shared read-only `scene_npc_checkpoint_residual_weapon_clip` predicate is used on capture and candidate restore. Its callers require a living, nonretired actor, no active scripted animation or combat owner, and no ammunition continuation. Capture requires the actual live reload deadline to equal zero, including overdue deadlines. Restore requires an absent combat extension; the existing RFNC codec requires every field of that absent extension to be zero. The completion service must perform any finite transfer before capture can use this exception.

The helper verifies:

1. Exact persistent actor registration and entity lookup, authored skeletal class, ordinary array pose, loaded model, active-slot backlink, consistent model-ring neighbors, no corpse-owned pose, and the real 1..50-bone skeleton with matrices/generations required by the existing pose evaluator.
2. An owned selected weapon supported by the existing ordinary firearm selectors: `12mm handgun`, `Undercover 12mm handgun`, `Assault Rifle`, `Shotgun`, `Sniper Rifle`, `rail_gun`, `Machine Pistol`, `heavy_machine_gun`, and `scope_assault_rifle`. Their finite nonmelee/non-reserve-fed magazine definitions must agree and pass the existing ammo validator. Ordinary selection success is captured before the separate resource-admitted Rocket fallback, so Rocket retains action 39 only. Grenade, Machine Pistol Special, Vauss, Tankbot, melee, delayed and other special profiles gain no fire admission.
3. Reconstructed class/base/selected-weapon mapping, including the existing weapon-alias rules, matching every mapping field, action sound pointer and action-declaration bit. The candidate selected weapon and inventory are used during restore, never the destination actor's current selection/inventory.
4. Exact selected-weapon mapped action-2/39 clips, with action 2 enabled only for ordinary selection; no frozen instance or freeze designation; valid phase; resident catalog/cache identity; exact authored loop/envelope data; positive bounded duration; valid cursor and finite nonnegative weight. Live capture requires a positive resource reference for every active slot; prepared candidate resources may still have zero references. The requested residual nonloop weight must be positive and at most 1, and its cursor must be within the authored interval and precede its end. Loop cursors are not forced into that interval: a just-applied loop can validly retain tick 0 until the next advancing frame.
5. Only the exact requested mapped action 2/39 nonloop for ordinary primaries, or mapped action 39 for Rocket. It must actually occur exactly once; existing all-slot duplicate-motion rejection still bounds the residual set to one slot per distinct motion, at most two. Additional loops retain their existing clocks. Each other nonloop must independently pass the caller's existing ownership checks: capture checks each slot, and restore marks any unresolved slot for the exact candidate RFAP burn join and final validation. A residual slot grants no ownership to any other clip; an arbitrary extra nonloop still rejects the checkpoint.

Independently saved pain or burn state and its exact owned clip may coexist with the residual fire/reload set; the existing pain/burn checks still validate them. Residual unfinished-cursor and positive-at-most-one weight requirements apply only to the requested residual slot, without adding those restrictions to separately owned clips; all-slot identity, resources, finite nonnegative weights and duplicate checks remain unchanged. Hidden-but-living saved actors may retain this presentation once the reload deadline is zero. Object retirement bit 2 and death are excluded. Visibility/LOD and hide scheduling remain unchanged.

`combat_reload_weapon` is deliberately not consulted. It is stale outside combat, is not serialized there, and restore resets it to zero. Repeated save/load/save therefore depends only on represented selection, inventory and playback, with no invented shot, reload initiation, transfer, Foley, contact, event, launch or RNG draw.

## Minimal restore ownership join

A preexisting gap was found: capture rejected arbitrary unowned nonloop clips, while restore previously checked clip residency but did not mirror that ownership rule. Checking only a currently mapped action 39 would not reject a substituted/mismatched clip. A blanket early rejection would instead reject existing burn-owned clips, whose exact pain action lives in the independent RFAP payload and is joined after RFNC staging.

The bounded prerequisite is one private `unowned_motion_mask` in each restore-stage entry. Restore preserves all existing script/death/pain/combat exceptions, admits the narrow residual-weapon predicate, and marks remaining nonloop slots. Nothing new is serialized or added to live NPC state.

- Component-only prepare has no RFAP owner and rejects/discards a stage with unresolved slots.
- The existing RFAP burn join checks candidate burn identity, vitals, flags, pain RNG and source ownership as before. Each unresolved slot must equal that same candidate actor's exact saved burn-pain action in the reconstructed candidate selection. Current destination burn state is never used.
- Every unresolved slot is verified before any masks are cleared. A failure leaves the staged masks and all live actors/playback/references unchanged. The preexisting candidate source retirement rebinding remains staged only.
- The existing final NPC restore validation rejects any remaining mask. Publication stays assignment-only; no reload/action/audio callbacks or resource mutation are added.

This closes the preexisting mismatched/extra-unowned restore gap without granting generic one-shot admission or changing independent gameplay owners. The mask adds four bytes per staged NPC, subject to the existing budget calculation (structure alignment may absorb it on a target).

## Source-only caller audit

- `scene_world_load.inc`: resources are demanded before world preparation; NPC staging is followed by RFAP preparation/burn join, then `scene_npc_checkpoint_restore_validate`, then the existing assignment-only transaction. No actor publication is inserted before the ownership check.
- `scene_world_restore.inc`: the composed preparer uses `_prepare_mode`; its other wrapper is a read-only no-publication probe.
- `scene_npc_checkpoint_restore.inc`: component `_prepare` rejects unresolved masks; `_commit` always calls `_validate` before `_assign`. Fresh boot still compares the exact captured owner/pose prestate, and in-session/rollback still recaptures and compares the complete row. Existing `_prepare_mode` rejects any live corpse pool before staging and refuses a living candidate on an unregistered current actor; the ordinary world loader reconstructs fresh boot. This change does not add in-session resurrection or relax those preexisting limits.
- All existing storage, codec, full inventory, selected-weapon, resource headroom, controller, staged-pose evaluation, placement/support/pair clearance and stale-prestate checks remain in place. The helper allocates nothing and changes no state.
- `scene_npc_checkpoint_export_mode`, legacy composed capture, RFAP capture and player/composed paths retain all save-only transient vetoes for delayed melee, secondary cooldown/missile and Laser/TriBeam/Cane/Spit/Sonar reservations/flights, including callback phases. No guard moves into shared row capture, which restore preflight also consumes. Successful-load transient retirement stays unchanged.

## Still deferred

No runtime evidence is claimed for either capture success or restored continuation. A future parent-scheduled bounded check should distinguish fire-only/reload-only/both retained clips, cleared versus pending ammo obligations, offscreen playback, save/load/save with reset stale reload weapon, mismatched candidate selection or extra nonloop rejection, and independently owned pain/burn clips alongside residual fire/reload, component-only burn rejection, and existing combat ownership. No new fixture is supplied. The nine-weapon list is a selector boundary, not proof of a placed campaign owner for every weapon. Synchronous callbacks may establish independent later orders or effects; all resulting saved owners and save-only guards remain authoritative. The original force-advance flag projection, broader pending-reload ownership, and the broad Shoot_Once target-release candidate remain separate work.
