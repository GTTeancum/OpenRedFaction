# Ordinary One way actor-default patrols

Source-written, not compiled or runtime-verified. Staged against d6661eae1c0a064a2e02e302b906ef3a5128eb01 for parent integration and the next parent-owned hourly Xbox compile/package rollup. Testers own functional/route coverage; no new fixture, gameplay route, image, or original-code execution accompanies this slice.

## Implemented boundary

Ordinary grounded humanoid actor defaults can now bind an exact authored `One way` path with at least one node. Loop and Ping Pong still require two. Existing raw AI-default byte2, class/body/movement/seat restrictions, finite source checks, bounded event-tag qualification, actual stand/walk resources and playback-slot limits remain authoritative. There is no UID or level allowlist.

At a qualified default terminal arrival, the implementation prepares the immutable source binding, action4 transition and stand/walk residency before any tag effects. It then stops locomotion, dispatches the existing admitted ordered positive-delay node tags, rechecks generation/registration/exact route ownership, and publishes an assignment-only default rebind:

- active1, stop0, actor-default origin, no event/follow owner
- immutable path unchanged, mode0, cursor0, shared direction unchanged
- target equals authored node0; retained navigation cleared, route_index1, retry0
- no dwell or deadline; terminal wait and facing pause are skipped
- a proven unfrozen nonloop waypoint visual action and playback owner survive

The outer actor loop continues immediately after that publication. It does not recursively visit node0, including for singleton paths or reverse cursor0. The next ordinary service acquires/retries navigation under the port's existing bounded policy. This is a default rebind, not Loop wrapping, a new timer, or a permanent idle state. No new restart is introduced for arbitrary inactive action4 or rejected route owners. Existing combat, command, death, seat and suspension cancellation remains authoritative. The accepted-tag barrier faults on a post-dispatch ownership failure rather than replaying a prefix or saving partial advancement.

Qualified event-owned One way replacement remains refused by the existing movement/acquisition adapters and the narrowed provenance admission. Generic unqualified event-owned mode0 keeps its existing behavior.

## Original evidence

Read-only RF.exe SHA-256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

- 407a50 stops locomotion; 407a87..407adf captures reached-node wait/facing; 407aef..407b10 dispatches post-load event handles in order.
- 407b12..407b26 applies shared direction before exhaustion. Ordinary One way neither wraps nor reverses. Reverse cursor0 transiently advances to -1 in the original.
- 40a9b0 rejects an out-of-range selected cursor. Its later 40aa85 count>=2 check concerns the generated navigation route, not the authored path. One authored node is permitted.
- 407c92..407ca8 arms the reached-node wait only after successful navigation. An exhausted route skips the terminal wait.
- 407cb5..407cda invokes 407ee0 in the ordinary single-player branch. Default4 rebinds its retained immutable name/mode via 4270f0; 427149 writes cursor0, 427153 writes mode, and the function does not reset shared actor6b0 direction.
- 4270f0 attempts the graph route. On failure cursor0 was already written, and 407ee0 does not force waiting2. This slice does not claim perpetual movement, guaranteed route success, or exact synchronous original graph-replan timing.

The reviewed source assessment retained complete disassembly for these branches and a 27-default installed-source inventory. Removing seven empty paths and seven nonordinary Capek/bat/fish owners gives 13 ordinary candidates in six levels, including five singleton paths. L6S1: UIDs5125/5217/5223; L6S2:6522; L14S3:9621; L8S1:6469; L9S1:5753/5779/5816/5817; L9S2:5885/5895/5896. These are source demand, not a claim of live traversal or resource admission. All13 have no raw tags or orientation; only the final node of the two `rush2` owners has a positive one-second wait, which terminal completion skips. No authored incoming Follow_Waypoints joins target these13.

## Save and history

RFNC36 and RFCH13 are reader capabilities with exactly the preceding layouts. The40-byte in-memory route lane,36-byte fixed route wire lane, optional existing4-byte dwell suffix, move record, per-row maximums, aggregate storage caps, owner sizes, resource budgets and fixed pools are unchanged. No exhausted cursor/count sentinel, terminal-node field, pending-restart state or extra clock is serialized.

Writers conservatively promote every enabled patrol, even while an event-owned or absent current route interrupts it: the immutable default can still need the new reader on a later return. RFNC<=35 and RFCH<=12 reject explicit actor-default origin2/mode0. Older supported Loop/Ping Pong and generic unqualified event-owned mode0 remain readable. All-zero legacy absence stays absent and, only after successful publication, removes any newly constructed One way default.

Mode0 reverse is valid only for an enabled actor-default route, with route/shared-direction equality. Mode0 dwell requires at least two nodes and a genuine adjacent predecessor: forward cursor>0 or reverse cursor+1<count. Source admission still proves the actual positive authored wait and bounded remaining duration. A singleton cannot own dwell.

Same-level restore assigns the exact saved cursor, target, direction and optional wait without running arrival/default-return. A terminal save contains cursor0/node0, so loading never repeats the final visit or fires its tags. A singleton's next normal service may visit node0 again, just as uninterrupted service can. RFCH revisit keeps reconstructed body placement plus saved route selection and wait; a proven visual action still ends on unload under RFCH12 policy, while independent pending event history survives. No clip/tag replay is added.

## Two-sided native-close selection

Tagless and no-dwell One way defaults explicitly select the existing composed NPC publication transaction. The bounded source-only selector resolves every enabled outgoing owner's immutable default, including stopped, combat and event-route owners; it does not infer capability from current mode, active bit, dwell or tags. The incoming selector resolves each enabled saved record to exactly one current-level UID and the same immutable source. Missing or duplicate identity and a claimed source that cannot bind are errors, never a negative selector.

Both selectors run before world-load resource demand/reclamation. Either side selects the established current-player on-foot gate, incoming occupied-vehicle refusal, publication begin/seal, and successful native storage close before NPC assignment. Thus outgoing One way with incoming absence remains protected. The matching outgoing save gate avoids writing an unsupported occupied-player composition. Standalone capture/load refuses relevant outgoing/incoming One way source capability, including interrupted patrols; the low-level standalone codec also refuses explicit origin2/mode0 state.

The existing publication receipt already seals old/new route state, move, direction/dwell, script action, pose and exact resources, including requests38/39 and future-patrol stand/walk requests. No receipt fields or broad world-atomicity guarantee are added. Existing candidate provenance, target, RFNC/RFCH row joins and final-success-only publication remain mandatory after selection.

## Deferred scope

Qualified event-owned One way replacement, one-node Loop/Ping Pong, new actor classes, immediate tag callbacks, new pathfinding, exact original synchronous graph timing, and route/playthrough verification remain outside this slice. Resource or graph admission can still fail under current policy. Runtime behavior remains unverified pending the parent rollup and tester coverage.
