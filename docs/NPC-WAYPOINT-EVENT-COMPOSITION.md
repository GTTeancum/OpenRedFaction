# Delayed waypoint-event composition

Source-written for Xbox; uncompiled and not runtime-verified. Builds/package rollups remain parent-owned. No functional fixtures or campaign-route testing were added.

## Implemented scope

Ordinary qualified Loop/Ping Pong actor defaults now admit and schedule the surviving event tags at their reached nodes. Existing orientation, finite dwell and original-inert tags remain supported. The actual installed demand is six nodes on three actors:

- L5S1 tech3045: nodes3246/3247 schedule Play_Animation3220/3248 after0.5 seconds; each waits5 seconds.
- L19S2B tech12014: nodes12016/12015 schedule Play_Animation12024/12023 after0.1 seconds; each has an oriented2.5-second wait.
- L19S2B merc12050: nodes9477/9484 schedule ordered Look_At12157/12160 and Message12153/12161 with4-second/1-second delays and zero node wait. The events keep their existing payloads and linked recipient actors.

Qualification is structural, not a UID or level allowlist. Only positive finite delayed Play_Animation11 (unfrozen nonloop filename action), Look_At7 and Message15 are admitted. Immediate tags, other event classes, loop/freeze animation and One way defaults remain outside this slice.

## Original contracts

Original4613dc..461451 resolves node tags through event-only4b6820; one missing/non-event UID clears the entire list through40cb40. Valid duplicates remain ordered. The immutable source list is never rewritten.

Original407a50 stops locomotion, captures the reached node's wait/facing, fires ordered tags at407aef..407b10 and then advances the cursor/direction at407b12. Dispatch4b6760/4b6800 uses full registered event handles, source=-1, actor=-1 and normal ON/mode1. The adapter calls ordinary rf_runtime_event_fire with those sentinels and the current event clock. Positive runtime delay is checked against the authored delay before dispatch, so this arrival stack only schedules; it does not execute actions or recursively follow links.

Original4bafe0..4bb0c3 starts an unfrozen action without replacing AI4. The delayed action's exact event-to-path-to-actor-to-catalog-motion join qualifies coexistence at execution time. A temporary arrival flag or pending deadline never creates animation ownership.

## Lifetime and cancellation

The exact actor-default route, next cursor, shared direction, target and dwell survive a proven visual action. Stand/walk looping clips remain controlled by the existing controller. Replacing a previous proven action affects only its owned slot; normal completion releases it through existing playback. A clip may finish after the node wait without extending the wait or resetting the route.

Other commands retain their existing cancellation semantics. Goto/Follow_Waypoints/Attack and death cancel the current scripted animation as before. Look_At may clear a default route/dwell while an ordinary nonloop visual action finishes. No route is resurrected when a previously scheduled event later fires after route replacement. Event deadlines remain owned by the ordinary event runtime.

A servicing guard covers the interval before the first scheduled tag through cursor/wait publication. Actor generation, entity identity, source binding generation and route identity are checked before effects and after each accepted scheduling operation. An error after an attempted dispatch leaves a sticky fault: the prefix is never retried and partial state is not saveable or eligible for section handoff. Fresh scene construction resets the fault; a failed save/load does not.

## Persistence

RFNC35 and RFCH12 are semantic reader capabilities with the preceding layouts. Writers promote all enabled patrols, including future arrivals. Existing RFNC next-cursor/dwell, script metadata and complete playback/controller fields compose with RFEC remaining deadlines/mode/source/actor. No tag or animation is replayed to restore them. Legacy absent routes remain absent.

A complete candidate RFEC join is mandatory for tagged scene restoration. Both outgoing and incoming tagged future owners or pending bound events select the protected final-storage-close publication path before resource demand, including zero/expired dwell and events whose route was replaced. Save admission uses the same on-foot restriction. The existing candidate/resource/reference seal includes route/move/script identity and the immutable binding. Candidate assignment occurs only after successful final storage close; rejected loading preserves the current route, playback and event/history state.

Section history intentionally retains its reconstructed-placement policy: next cursor/direction/dwell and unexecuted delayed events survive. An already-running proven visual waypoint action ends with the unloaded scene and is never restarted on revisit. Failed section capture does not stop live playback. Other unsupported section overlaps remain refused.

## Bounds

- Fixed binding:64 node rows of12 bytes,128 event handles of4 bytes, and24 metadata bytes =1,304 bytes. No per-NPC fields or wire bytes.
- Effective list maximum16; whole binding maximum128 handles. Original-inert nodes retain zero effective count. Unsupported profiles remain rejected, never partially dispatched.
- Restore-only structural growth:8 bytes in the NPC stage plus1,312 in its publication seal =1,320 bytes. Existing resource requests and row capacities are unchanged.
- Helper scratch stays bounded below1KiB; no arrival allocation, timer pool, generic scheduler or new per-frame all-actor pass.
- Existing16 motion slots,1,280KiB motion cache,2MiB world-load budget,RFNC2240-byte row cap and existing history/save/storage limits remain authoritative. Exact final stock64 residency is not claimed.
