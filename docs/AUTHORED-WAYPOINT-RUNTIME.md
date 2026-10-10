# Authored guard waypoint patrol: runtime/parser slice

Baseline: `d1cf63798490e212096fae67a5fb7ab973827ee8`. Staged outside the active repository. This slice requires the matching route codec/history slice before integration. Source review only: no compilation, tests, gameplay execution, route exercise, or save operations were run.

## Actual owners

The already-used original L3S1 records contain these ordinary visible guard owners, each with authored AI byte 2:

| UID | Class | Named path | Mode | Actual navpoint UIDs |
| --- | --- | --- | --- | --- |
| 3 | guard1 | L3S1Guard1 | Ping Pong | 2140, 2144 |
| 407 | guard2 | L3S1Guard4 | Loop | 543, 2140, 2156 |
| 892 | guard1 | L3S1Guard3 | Ping Pong | 2130, 2127 |
| 897 | guard1 | L3S1Guard8 | Loop | 516, 2134, 2139 |
| 929 | guard1 | L3S1Guard5 | Ping Pong | 2140, 520 |

All selected nodes have finite positions, zero wait, no orientation override and no arrival tags. Exact positions, node indices, record fields and links are in `../authored-waypoint-next/actual-authored-patrols.json`. Actual overlapping commands are Goto UID 898 for actor 897 and Goto_Player UID 2196 for actor 892. No invented event ID or path point is used.

The bounded admission is raw guard1/guard2 + authored byte 2 + existing named Loop/Ping Pong path with at least two plain nodes. It is not a hardcoded UID allowlist. Other classes, one-way/custom paths and node waits/orientation/events remain deferred.

## Original source anchors

All disassemblies below are existing bounded extracts in `../authored-waypoint-next/`.

- `original-constructor-disassembly.txt`: 464212/464225 read the two strings; 46483c..464874 resolve nonempty path and mode into actor 748/74c. 4649ce maps authored byte 2 to default 4; 464a01 calls 407eb0, then 464a07 calls 407ee0.
- `original-waypoint-binding-disassembly.txt`: 469040 recognizes Loop=1 and Ping Pong=2. 468e80 resolves the actual named route. 407ee0 returns to default 4 through 4270f0 if the retained route exists, otherwise waiting 2.
- `original-follow-constructor-disassembly.txt`: 427149 resets cursor 6ac to zero, 427153 stores mode, and 40a9b0 resolves the first real waypoint. It does not clear shared direction 6b0. Constructor 402de8 -> 40a950 initializes that direction to zero.
- `original-patrol-lifecycle-disassembly.txt`: action 4 services movement in 4059f0, then normal sight at 405adf -> 403a80 can select combat. 408c75 writes current action 3. Authored patrol does not exclude ordinary acquisition as a mission Goto would.
- `original-combat-route-return-disassembly.txt`: combat target resolution at 405bc7; an absent full-handle target returns through 407ee0 at 405c12. The existing port's broader living-target/forget policy remains its approximation, now releasing patrol through the same bounded default helper.
- `original-waypoint-arrival-disassembly.txt`: 407a50 advances by shared direction, wraps Loop, and reverses Ping Pong. The selected plain nodes need no unimplemented wait/tag consumer.
- `original-waypoint-completion-disassembly.txt`: completed Goto action 2 returns through 40792a -> 407ee0; action 10 has a separate completion branch. Current port Goto_Player follows the live player and remains active near its destination, so it is not converted to default patrol on arrival.
- `original-set-ai-current-disassembly.txt` and baseline `docs/MEDIC-USE-ACTION.md`: Set_AI_Mode 34 calls current-only 407e20, not default setter 407eb0. No mutable default is introduced. Accepted Goto/Goto_Player set current 2/10; Attack sets 3. Attack OFF, Look_At OFF and Shoot_At OFF call original 407ee0. Play_Animation is animation-only in the original; this slice preserves the port's existing animation ownership policy instead of rebuilding that adapter.

## Runtime interface and ownership

`rf_level_entity_waypoint_read` projects the two retained strings only after full record validation, writes output only on success, and changes neither spawn ABI nor source hashing.

`campaign_npc_body.patrol` stores `enabled` and retained shared `reverse`. The immutable default is derived from the qualified raw descriptor. `script_move.origin` distinguishes NONE (including pursuit), EVENT (a real event UID) and ACTOR_DEFAULT (event zero). The current path/cursor and immutable authored descriptor are separate.

`scene_patrol_binding(index, path, mode)` requalifies the actual source binding. `scene_patrol_initialize` starts newly constructed qualified owners at cursor zero/direction zero. `scene_patrol_return` is only for actual default transitions: cursor zero with retained direction; it cannot replace a live movement, combat, look, shoot or scripted-animation owner. Restores must publish exact saved state instead of calling it.

Ordinary accepted acquisition can replace the default route with current combat 3; later Catatonic 1 retains its existing inert policy, including damage/alarm alerts. Explicit admitted Attack has separate precedence. Current explicit event movement continues to exclude pursuit according to the existing owner policy. Shared direction is retained across combat, Goto and qualified Follow_Waypoints replacement; ordinary unrelated event-route behavior stays unchanged. Original 407b12..407b26 applies direction before mode dispatch, and 407c33..407c44 wraps reverse Loop to count minus one without clearing direction. The qualified runtime mirrors that lower-end wrap. One-way replacement on an enabled patrol is deferred before any command mutation; its completion/default boundary is outside this selected-owner slice. The existing Set_AI_Mode event-route acquisition writes real EVENT provenance, restores it on failed admission and preserves the same qualified shared direction.

Accepted Look_At and Shoot_At stop a qualified default route after existing command admission. No new current 8/9 serialization is claimed for those absent selected-owner intersections. Existing command owners prevent an automatic patrol restart. Matching OFF uses the existing port owner-matching policy, which is narrower than original unconditional OFF. Existing Goto OFF behavior is retained as a port limitation. Active scripted animation prevents default return; this does not alter its existing animation implementation.

## Required codec contract and limitations

The matching codec owns conditional RFNC/RFCH representation, exact candidate validation and successful publication. The shared 36-byte actor-route lane carries flags/action/path offset/count/cursor/hash/origin/real event UID plus enabled/shared-direction bits. Same-level combat 3 must retain the supported existing combat payload. Current 10 is admitted only with qualified patrol plus a real Goto_Player command owner; inactive command state must not restart patrol.

Legacy absence clears newly initialized patrol only during successful restore of an existing saved owner, retaining that save's previous current behavior. Fresh actors still initialize normally. `scene_patrol_legacy_reset` never changes current AI; call ordering must ensure it cannot erase already restored legacy event/pursuit movement.

There is no section-combat conversion. Unsupported cross-section combat ownership must reject capture before history publication. Represented revisit state retains exact current route cursor/direction and rebuilds navigation under the existing revisit policy; it does not invoke a default-return reset. No save guard may be relaxed merely because startup movement now exists. Original full patrol AI is not claimed: steering, navigation search, sight range/cone/cadence, hearing, target liveness and forgetting still use the existing bounded port policies. Look_At/Shoot_At current-mode serialization intersections remain deferred and must retain their strict unsupported guards.
