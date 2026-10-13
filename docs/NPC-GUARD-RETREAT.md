# Bounded guard retreat and ordinary-combat return

October 12 update: retreat's three live translation queries now also compose
the grounded player; see [NPC-PLAYER-CONTACT.md](NPC-PLAYER-CONTACT.md).
This supersedes the player-obstruction limitation below for that narrow
movement consumer. General NPC/NPC collision is unchanged.

Status: the complete 29-file runtime/codec composition has independent source acceptance and is integrated after the October 11, 2026 17:00 UTC batch. It has not yet been compiled or run. The next ordinary stock-64-MiB Xbox build is planned for 18:00 UTC. Source review does not establish gameplay, protected-save compatibility, target ABI, stack fit or peak memory.

## First-playable behavior

An actually qualified guard1 with owned ordinary 12mm handgun and resolved authored Direct3 attack style may enter a short retreat after real surviving local-player damage. Qualification uses parsed class/style overrides, actual model/loadout, full actor/player registration and supported current owners. UID109 is an evidence anchor, never a gameplay gate. Unsupported actors retain their existing behavior without spending a retreat lottery.

The hit owner retains the source reset/speech/CRT ordering, one-in-four choice and finite 2–3-second duration. A real alternate navigation query supplies a finite prefix; the guard walks that prefix through existing steering, ground, sweep, commit and locomotion owners. Expiry, ordinary route refusal or exhaustion returns to the existing practical combat scheduler without inventing a new reaction delay. A later accepted hit owns any new query. No teleport, fabricated target, granted ammunition or forced hit is introduced.

The deliberate first-playable substitutes are finite-route return to the current ordinary driver and its existing physical policy. Original armed substate selection, original perception/peer notices/target history and anticipatory actor-stall scanning remain deferred. Existing NPC sweeps cover world, movers, passive vehicles and detached pieces; reciprocal living-actor blocking remains an existing limitation.

## Ownership and persistence

- Guard profile2 reuses the existing flight/navigation owner. Medic profile1 retains its separate qualification and wire meaning.
- Active retreat suppresses ordinary acquisition, pursuit, forced shots, firing and ammunition fallback for the full enemy-service iteration. Accepted RFNC27 pending reload completes once; fresh reload initiation is suppressed. A positive unowned legacy countdown refuses entry before onset effects and is not adopted.
- Successful death, retirement, incoming command/selection, player replacement or boarding cancels through the actual accepting owner. Failed/no-op operations preserve the episode. Cancellation distinguishes restoring still-owned action7 from preserving an already committed incoming action. Mandatory errors retain a fault latch and reached effects; no general rollback or retry is promised.
- RFNC28 adds a conditional guard tail after the actual RFNC27 reload tail:4 bytes absent,224 bytes present. Its56 declared32-bit words are logical wire accounting, not measured target ABI. Canonical absence, old1–27 decoding, profile exclusivity, finite route/deadline/geometry bounds and separate shared CRT versus spread RNG joins remain enforced.
- Active continuation preserves actual retreat clocks, typed prefix/start geometry, command/physical aliases and ordinary burst/deadlines. Restore does not replay hits, RNG, queries, resets, audio or ammunition transfer. After the finite episode ends, existing ordinary save timing and transient restrictions apply again.
- Section replacement refuses active or faulted guard state before history/world writes. Component-only export/load rejects active new state. Composed save/load uses two-sided on-foot/unoccupied admission, includes outgoing or incoming guard presence even without reload, seals all coupled resources/state, then closes native storage once before assignment. Incoming absence retires an outgoing episode only on successful publication.

## Budgets and validation limits

Constructor640KiB, world-stage2MiB, motion-cache1280KiB, complete-file110524-byte and stock64MiB policies remain unchanged. The source record gains224 declared bytes; the conservative wire-row allocation bound becomes2240. Existing sizeof-based stage/row charges and explicit class-metadata/resource-plan charges include new storage. Exact target sizes and peak fit remain unmeasured.

The prior reload/RFNC27 game compiled at noon and was retained by the ordinary 13:00–17:00 builds. Those builds do not compile or validate this new RFNC28 feature. The earlier verified saved-load host Candidate binds the prior e4b5 source/artifacts; it remains historical evidence and cannot be silently rebound to this changed game. Saved-disk backup permission and sufficient fresh capacity remain required before protected saved-load work.

The exact independently accepted source and ownership review is in [the joint review](NPC-GUARD-RETREAT-JOINT-REVIEW.md). Runtime validation remains future parent-owned Xbox work.
