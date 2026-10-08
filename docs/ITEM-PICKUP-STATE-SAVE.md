# Authored pickup-state continuation

Written 2026-10-08 against isolated base
`c23c30580d0b5781e57dec650f4360e56757fef0`. No builds, tests, new fixtures,
original-game execution or campaign traversal. Runtime verification is pending.

## Actual behavior and missing owner

The installed SP archive census contains Item_Pickup_State54 at L7S1 event4035,
L7S2 events21/4021/4052, and L8S3 event5166. Each addresses two actual item UIDs.
Original ON `4b9290` resolves item handles and clears item `+2bc` bit0; OFF
`4ba090` sets that bit. RF.exe SHA-256 is
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

The existing port callback represented that choice in `stream->pickup_state`:
0 inherits class policy, 1 enables pickup, 2 disables pickup. It did not retain
those overrides in the level/UID history or ordinary save. The immediately prior
admission slice therefore rejected nonzero overrides to avoid silently losing
the effect. This slice replaces that temporary guard with a real state owner.

## Implemented path

A bounded 1,024-byte state bank parallels the existing collected-item ledger's
stable level/UID slots. The actual event registers/resolves the item before
publishing its local and retained state. Collection/removal keeps the existing
retired bit and never resurrects the item. Level re-entry restores the state
through the same identity lookup; a new campaign clears it with the ledger.

The existing component is actually named RFIP1 on disk. New state-aware codec
calls write RFIP2 only when an override exists. RFIP2 retains the same identity,
checksum, level-name table and ordered level-index/UID/retired fields, adding a
state word per row. Its only values are 0, 1 or 2. RFIP1 is still written when
all states are zero; the legacy writer remains byte-compatible. RFIP1 reading
through the new API produces an all-zero state bank. Legacy callers that cannot
retain state reject RFIP2 before mutation instead of discarding its policy.

Ordinary capture validates the current item projection against its exact
level/UID history. Whole-world restore stages the decoded state bank, validates
all current-level override UIDs against actual authored items, matches live
items by UID rather than array order, checks both live pointers and prior state,
and publishes retirement plus pickup policy only after every world stage passes.
No Item_Pickup_State event is fired during load. The startup-grant ledger retains
its existing RFIP1 path. Pending type54 actions now use the world/inventory
capability admission because both their scheduler and effect owner are saved.

Storage cost is 1 KiB retained plus 2 KiB in the already budgeted mission stage;
RFIP2 adds at most 4 KiB to its wire payload. No per-frame allocation is added.
This covers ordinary save/fresh-load, in-session restore and section revisit in
code. It does not claim native validation, new item classes, multiplayer respawn,
visual/audio changes, or altered pickup quantity/range/retirement semantics.

Overall implementation remains approximately 88%. Objective/item continuation
is newly implemented and runtime-unverified, with no automatic percentage uplift.
