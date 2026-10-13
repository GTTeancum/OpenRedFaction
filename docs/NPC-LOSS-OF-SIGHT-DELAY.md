# Bounded loss-of-sight pursuit delay

Status: parent source review completed October 13, 2026. Pending hourly stock64 compilation; no functional coverage claim.

## Implemented scope

Ordinary standalone Direct/Evasive actors with an existing qualified Blind target retain an independent visible-to-hidden delay. A successful scheduled body/eye visibility sample arms an inclusive 2000–5000 ms delay only when the preceding sample was visible and the new sample is hidden. The initially disarmed state is distinct from elapsed. Seeing the target again preserves the clock, and another loss rearms it. A private RNG avoids consuming weapon spread or global gameplay RNG.

The existing Blind sampler supplies the sample, with its 30-frame stagger and at most two world/mover rays. No extra visibility rays are added. Cheap ordinary clock service saturates elapsed state through temporary suppression and timer wrap. Query results close epoch, full identity, target relation and retained receipt checks before publication. Source combat_obstructed directly invokes the existing geometry collision ray; it does not issue damage, audio or script commands.

At the existing ordinary ranged pursuit decision, an in-range actor starts pursuit due to lost sight only when the independent delay is armed and elapsed. Out-of-range pursuit stays immediate. Tactical local routes, Stand Ground and authored movement keep their existing ownership. Existing firing, paid attacks, reload, damage and Blind target-expiry rules are unchanged. The new receipt follows the exact Blind target lifecycle; target release, point orders, terminal retirement and seat transfer clear it through the existing centralized clear path. Same-target explicit Attack retains its clock and RNG.

## Explicit adaptations and exclusions

This is not the full original pursuit state machine. The existing practical primary range, 0.8 pursuit hysteresis, 15-frame navigation poll, pathfinding backend and 60-tick route retry remain. Original 406b70's state2 multiplier1.5, authored maximum of both weapon limits, early reload/pain/physical-crouch exclusions, independent route-retry clock and state1/2/11 transitions were mapped but are not partially substituted here.

The bounded sample replaces original cached actor/clutter/corpse visibility. Initially absent receipts, including old saves, begin disarmed on the next eligible live service; no visibility is inferred from Blind's initialized last_seen grace. New target identity also begins disarmed, an explicit target-bound adaptation from the original actor-lifetime clock. Same-target changes of explicit/reactive mode retain the clock. Unsupported actors and melee retain the prior practical pursuit path. Retained clocks survive temporary Disarm and unsupported current weapons; only the ordinary selected ranged consumer uses them.

No fixed last-known destination, post-expiry search phase or search duration is invented. The destination remains the current live target position. No vehicle or GeoMod implementation is changed.

## Source evidence

- 405cbe–405cd3: retain previous exact-visible state before visibility refresh.
- 405d43–405d6c: visible-to-neither-visible arms AI+238 for 2000–5000 ms.
- 40e3a2/40e3b0 and 40e67e/40e684: actor+2a0 AI constructor initializes +238 through 4fa340 to -1.
- 4fa3f0: negative/disarmed is not expired; 4fa3b0 uses inclusive random interval.
- 406c87–406caf: near-range hidden pursuit requires expired +238.
- 401075–4010a2: destination reads the current target position.

Original RF.exe SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

## RFNC40 continuation

Live receipt is20 source-layout bytes: presence/mode, full target handle, sight flags, deadline and RNG. Wire is20 bytes when present or4 when absent; the nonwire decoded profile adds4 bytes. The saved target is a UID, with zero denoting the player. Candidate preparation resolves it through the existing full-generation Blind target join and requires equality with the saved combat relation. No runtime handle is trusted from disk.

Disarmed and armed-expired are explicit flags; pending wire remaining time is1–5000 ms. Capture may represent an already-due clock as expired in the private output, but never ages or repairs the live actor. Candidate preparation, old/new seal and final-close checks retain the exact prepared receipt. Only successful restore_finish publishes it. Old-save absence stays absent until ordinary live admission. Canonical absence is allowed before first admission, and contains no stale clock or RNG.

All four live writer/reader adapters use the new API. Older mutating readers refuse40; read-only preflight validates it. No new resource demand or player/vehicle admission lane is introduced: every present delay row requires a present Blind receipt and its existing composed on-foot target checks.

## Memory

No file, row-count, NPC, motion or world budget is raised. Source Xbox32 upper bounds: NPC3224 (prior3192), decoded row2424 (prior2400), restore entry11556 (prior11456), shared seal row3404 (prior3360), crouch late snapshot6768 (prior6736). The late header40 and shared header9336 remain unchanged. Sample guard has a96-byte upper bound, plus24 bytes of copied ray deltas and ordinary locals. These are source limits awaiting the hourly compile, not measured sizeof results or a whole-stack measurement.

For N live actors, R retained rows and C independent late snapshots, conservative incremental overlap over the crouch baseline is32N+256R+32C bytes plus fixed stack locals. Direct retained components are24R decoded rows,100R restore entries and44R shared seal rows; the larger256R allowance covers extra working decoded copies and padding. All allocations continue to be charged through actual sizeof against the unchanged budgets.
