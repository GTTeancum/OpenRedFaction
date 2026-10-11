# Ordinary guard1 / 12mm NPC reload

Status: the complete 22-file source candidate has independent source acceptance. It is uncompiled and runtime-unverified. Source acceptance does not establish protected-save compatibility or authorize activation. Exact source and review identities are recorded in [the joint review](NPC-ORDINARY-RELOAD-JOINT-REVIEW.md).

## Scope and original behavior

The feature admits only the qualified nonplayer `guard1` / ordinary `12mm handgun` capability. Every instance must prove its actual full object/entity registration, persistent identity, life state, selected finite-magazine weapon, `ult2_guard.vcm` model, model-ring/pose ownership and mapped nonloop action 39 resource. Weapon identity is resolved from the catalog. UID 109 is the original evidence anchor, not an admission gate; there is no hardcoded UID, level, scene slot, handle or numeric weapon ID. Other actor classes and weapons retain their established paths.

Original ordinary actor service requests an empty-magazine reload without requiring a combat target. For the qualified guard profile, the final deadline comes from mapped action 39, rather than the provisional weapon descriptor. The retained `ult2_reload.rfa` header spans ticks 2880 through 8320, a 5440-tick span. The original conversion yields a stored float of approximately 1.1333334446 seconds, truncates to 1133 milliseconds, then projects to 68 simulation frames at 60 Hz. The previous descriptor path produces 66 frames.

The implementation derives this interval from the actual authenticated mapping: existing duration primitive, explicit float store, multiplication by 1000 in double precision, positive integer truncation, then overflow-checked integer-millisecond ceiling at 60 Hz. It does not encode 68 as a gameplay constant or substitute a direct ceiling of raw seconds. Initiation, timing, persistence and transaction ownership form one complete feature.

## Runtime ownership and finite ammunition

- `combat_reload_pending` is the explicit owner, separate from the accepted weapon and deadline. Early service runs before medic, seat/operator, AI mode, target and awareness firing gates.
- New initiation requires a living standalone qualified actor, an empty valid magazine, positive finite reserve and no positive legacy reload deadline. Mapping, duration, start capacity, freeze/script ownership, reference headroom and required resource demand must succeed before acceptance.
- Acceptance publishes pending, weapon and deadline once, before its single presentation attempt. Optional Foley failure is nonfatal. A mandatory error after acceptance retains ownership and cannot replay the start on the next service.
- Completion uses current finite inventory exactly once, including a successful zero-ammunition transfer with the existing completion event 2. A per-iteration completed-weapon receipt prevents a second transfer or immediate new start. Completion does not require a target, a surviving animation clip, resident animation payload or newly imposed standalone state.
- Pending with remaining time zero is an accepted, due episode, not absence. It continues to block the later ammunition gate. Death and actual selected-weapon mismatch cancel through their explicit owner; accepted move cancellation, retirement, scripted drop, fallback and Riot disarm pair their existing reset with the new bit. Busy predicates recognize due-zero pending. Target release preserves reload.
- Existing positive legacy timers keep their original completion owner and deadline. They block a new start and are never adopted or retimed. Unrelated script, pain and genuine airborne-burn owners remain independent.

## Capture, restoration and RFNC27

The flat record adds three `uint32_t` fields: `reload_present`, `reload_weapon` and `reload_remaining`. Canonical absence is all zero. Present state has exclusive reload ownership, with both legacy combat reload words zero. A full magazine or exhausted reserve can remain valid after an episode has been accepted; start-only conditions are not reapplied to continuation.

Capture authenticates every claimed pending owner and saturates overdue remaining time to zero without completing it. It does not invent a combat row, target, RNG owner or animation slot. Actual reload/fire slots require their exact positive-weight unfinished residual proof; unrelated playback retains its own qualification. A logical episode may survive after its reload slot has disappeared.

Restore stages an overflow-checked `frame + remaining` deadline after exact scene qualification. Core bounds remaining time at 3600 frames; scene qualification additionally limits it to the actual mapped interval. Publication restores ownership without ammunition transfer, presentation restart, sound or time advancement. Incoming absence, changed weapon, death or retirement can replace an outgoing episode without inventing a living incoming reload resource.

RFNC27 is selected only when at least one row contains new reload presence. Its tail follows the actual flight and airborne tails and occupies 4 bytes when absent or 12 bytes when present. Once the component is promoted to 27, every row uses that conditional tail. No new presence leaves the previous version-selection policy unchanged. RFNC1-26 decode to canonical new-state absence. The structural row maximum is 2016 bytes; it is not a fixed wire stride or a guarantee that every structural combination is scene-admissible.

An older binary limited to RFNC26 cannot read a newly written RFNC27 component. Preserved old-format parsing is a source finding, not a protected-save or saved-consumer compatibility result. Component-only export/restore rejects new reload ownership because it lacks the composed world transaction owner; ordinary composed world save/load is the supported continuity path.

## Complete on-foot publication boundary

The loader selects the new transaction from the union of authenticated outgoing pending and incoming presence, before resource demand. Incoming absence or an older input format cannot bypass an outgoing episode. Malformed claims reject rather than fall back to legacy loading.

Both sides must satisfy the bounded on-foot policy. A reload-containing save requires the current player on foot. A participating load checks the current full player/control identity and incoming unoccupied, no-player-gunner, no-active-player-turret state. Existing qualified parked ownership can still admit an authored NPC driver. Occupied, Jeep-gunner, RFNS player-gunner or active player-turret participation is explicitly unsupported: reject the complete new-state save/load. Do not drop the receipt, cancel on boarding, broaden occupied support or choose a legacy fallback. No-new-owner loads retain their existing supported occupied behavior.

Resource demand includes both sides' mapped reload resources, even when no action 39 slot survives. Intact parsed metadata can authenticate a legitimately evicted zero-reference payload; actual active slots still require residency. The plan must fit currently resident plus all missing payload bytes under the existing cache ceiling before demand. The final seal requires requested payloads resident.

The final publication seal covers every exchanged NPC row and every touched resource, including unrelated shared borrowers. It checks full identities, arrays, model rings, live/private poses, collision generations, independent script/controller resources, medic-child lifetimes and every signed reference-count prefix in the existing old-decrement/new-increment order. It permits only the established unrelated RFNS linked-handle-to-absent projection. It is not a rollback snapshot.

After the retained second airborne identity check, concrete player recheck and final seal, the conditional branch attempts native storage close immediately before the first NPC/RFAP assignment. The attempted flag is set first; a failing close is not retried even if mounted state remains. Earlier errors retain ordinary cleanup close. Successful close preserves the existing assignment, history/rebind, finish, scope release, optional audio resume and cleanup order.

Prepublication or close failure leaves the coupled NPC/reload/inventory/playback/reference/pose/script/pain/burn/retirement replacement unpublished. Earlier terrain, parked, mover, clutter and audio effects retain their established policy. No discarded-world rollback, outgoing live-burn rollback, arbitrary callback support, unloaded aging or RFCH ammunition continuity is claimed.

## Budgets and remaining validation

Limits remain unchanged: stock 64 MiB target, 640 KiB NPC constructor, 2 MiB world stage, 1280 KiB motion cache and 110,524-byte checkpoint file. Oversized state may correctly reject.

Source accounting gives a record increase from 2080 to 2092 bytes and a one-row header-plus-maximum-wire buffer increase from 2068 to 2080. Simultaneous decoded plus saved/before records add 36 bytes per row, before the expanded body snapshot and staged deadline. The live pending word adds 4 declared bytes; actual padding is charged through `sizeof`. Inspection and request scratch are released before later stage allocations.

Resource requests retain capacity `20*N` without a new participant and use `20*N + N + B` for incoming rows N and current bodies B when the union participates. Checked accounting also charges the conditional player receipt/bookkeeping, full all-row seal and its pose/reference/child storage, separately from existing candidate poses and medic allocations. None of these declaration calculations measures target ABI, stack use or peak memory.

Before activation, the exact complete composition still requires protected current-save/consumer compatibility approval and focused Xbox compilation, runtime, stack and stock-memory validation. Due-zero, zero-transfer, clip-ending/eviction, old/new replacement, close failure and occupied rejection need focused validation. No completion date or tested compatibility is asserted.
