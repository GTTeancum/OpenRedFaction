# Linked NPC Teleport: implementation contract

Prepared 2026-10-03. This is a bounded static audit of installed `RF.exe`, SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`, plus a read-only scan of installed RFL records. No original game, reconstructed runtime, build or emulator was run. No implementation is claimed here.

## Actual campaign demand

Seven SP records use event type4, which current `src/core/event.c` does not dispatch or admit to the pending-action scheduler:

| Level | Event | Linked actor |
| --- | --- | --- |
| L1S3 | 9589,9590,9595,9598 | env_guard9449,9452,9454,9455 respectively |
| L12S1 | 9711 `teleport_out_of_jeep`, delay2.6s | miner7646; links10076/10178 are not entity records |
| L8S4 | 8357 | Capek8359; Delay8355 `Shot 3` explicitly links8357 |
| L20S2 | 18373 | Empty link list; no actor relocation from this record alone |

L12S1 destination is `(49.284691,-42.596020,190.100449)`. This is the real miner already bound to Jeep7629, so merely changing his position without detaching is insufficient. L8S4 destination is `(-35.879803,-44.258293,110.248924)`.

## Original contract, not inferred from Teleport_Player

- Factory `4b69d0` selects the generic `4bee70` constructor for type4, vtable `589c9c`. ON dispatcher `4b9070` maps type4 through `4b90ef` to `4b9650`. OFF dispatcher `4b9f80` maps type4 to the return at `4ba008`: no relocation. The common propagation predicate `4b8c40` returns true for type4. Preserve ordinary delay, source/actor and linked-event propagation on both action modes; relocation itself is ON only.
- `4b9650..4b9807` traverses the complete linked-handle array in order. Lookup `40a0e0` skips absent objects. `4b9693..4b96a2` admits original object kinds0(entity),1(item),4(clutter), corroborated by `reference/dashfaction/game_patch/rf/object.h`. An NPC-only first slice must report item/clutter support as outside scope, not reinterpret those kinds as NPCs. The event actor is not an implicit relocation target when links are empty.
- `4b96a8..4b96c3` resolves the target's parent at object+200 through entity lookup `426fc0`; if present, calls parent method `427380(child_handle,0)`. This is direct attachment release, with sound disabled. The result is ignored and relocation follows. It is not player-use exit, does not check speed/clearance, and does not apply a Never_Leave gate. `427380` clears the matching parent slot and child+200; it leaves attachment identifier+204 and linear vectors alone in the ordinary entity path.
- `4b96c8..4b9703` copies event position+40 into target+f0 and calls `48a230`, which publishes position at+3c/+e4/+f0, rebuilds radius bounds+190/+19c and adds object flag `0x04000000`. Copies event runtime basis+4c to target+48/+fc/+120. The extra stack vector copy at `409f40` is a vector assignment return, not a displacement or velocity calculation.
- If that target is an entity, `4b9708..4b9764` also writes its eye basis+7e0 and derives Euler values using `4fc060`, negating its first result before `42d840` writes+864. Serialized basis must first use the existing loader conversion: disk triples1,2,0. Do not apply the disk permutation twice.
- The ordinary relocation code and `48a230` do not zero target+144/+150. Preserve existing linear/angular motion rather than copying cutscene-stop behavior. Conditional model notification `503400` follows `40a1e0`; local movement predicate `42a0d0` gates `4a0770` placement. This audit does not justify unconditionally applying the player's downward ground-placement sweep to every NPC.
- `4b979b..4b97e7` reads movement descriptor+4, calls `407e20(entity+2a0,2,-1,-1)`, and refreshes navigation candidates with `40c2c0(destination, entity+7c0, entity+7c4, descriptor_is1, &entity+69c, &entity+6a0,0)`. In ordinary SP the AI action becomes Waiting2 and both target fields become invalid. `407e20` can remap2 to3 in alternate global/multiplayer modes; those are outside the SP slice. A teleport therefore must not retain an old active Goto/Attack path that immediately pulls the actor back.

## Minimal parent integration

1. Add a typed linked-object teleport callback beside `teleport_player`/`move_npc` in `include/rf/event.h`; add ON-only type4 dispatch and scheduled-event admission in `src/core/event.c`. Resolve and generation-check each link; skip unsupported/nonexistent kinds while allowing ordinary propagation to event links.
2. Use a dedicated NPC adapter with one bounded pending destination per admitted NPC owner if publication must wait for the safe frame boundary. Preserve link order/last destination for repeated requests. A deferred adapter must not cancel newer downstream movement orders: original relocation/reset occurs before common propagation. Prefer immediate publication at the established event-safe phase when available; otherwise order queued teleport and following actor orders explicitly.
3. Validate the finite destination/basis and prepare the next body/look values before mutation. For authored seated NPCs use `scene_npc_seat_unbind(handle,now)` from `scene_npc_seat_bind.inc`, which clears the genuine host driver/occupant ownership and prevents subsequent seat-pose snapback. Keep unrelated player gunner ownership intact. Unknown attachment families need an explicit unsupported result rather than clearing only the child link.
4. Apply the Waiting2 transition and clear stale scripted translation/navigation as part of relocation. `campaign_ai_mode_apply(owner,2,now)` already clears reactive/combat ownership but preserves some ordinary `script_move` routes; it alone is insufficient. Clear the prior route's active/retained state before any subsequently propagated Goto/Attack is dispatched. Retain inventory, vitals and existing velocity.
5. Synchronize body current/next position and orientation, world inertia tensor, NPC look basis/angles, model owner basis, bounds and published position via `rf_scene_npc_publish_position`. Clear obsolete mover/rubble support/contact caches, refresh current eye with `campaign_npc_eye_update`, and room with `rf_scene_npc_refresh_room`; normal NPC contact/navigation then starts at the new location. Do not teleport the Jeep along with its miner.
6. Focused future Xbox fixture: stage only original L12S1 event9711 and its real miner/Jeep pair; assert its2.6s delay, one detach, exact destination/basis, cleared NPC parent/host driver, Waiting2, and no seat snapback on subsequent ticks. An OFF control must leave pose/ownership unchanged. Add a small unseated Capek/guard case only if needed for the ordinary path. No campaign traversal is required.

The addresses above are static disassembly evidence. Navigation helper internals, all attachment families and exact model/room notifications have not been exhaustively reconstructed. No standalone helper was added because event publication order and shared actor owners require coordinated integration.
