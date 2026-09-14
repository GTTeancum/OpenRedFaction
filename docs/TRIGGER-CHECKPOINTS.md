# Trigger activation checkpoints

Backtracking previously rebuilt every trigger's activation count, flags and
cooldown from its level record. A completed one-use contact trigger could become
eligible again. The shared scene now retains activation state by canonical
section name and authored UID, restoring it after resource/startup initialization
and before the first gameplay contact pass.

Saved fields are flags (excluding the per-tick fired bit64), activation count,
activation limit, object flags, last activation clock bits, cooldown remaining
and contact-delay remaining. Active deadlines pause while the section is absent
and are rebased onto the new section's clock. Expired deadlines restore as due;
inactive deadlines remain inactive. The activation timestamp is retained as
historical data; gameplay eligibility uses counts, flags and rebased deadlines.
Runtime handles, owner pointers, authored links, geometry and authored cooldown
length are rebuilt by normal loading and are never copied from a prior section.

This is practical session checkpoint policy, not full original level-state
restoration. Startup dispatch still runs before restore; auto-start actions can
repeat non-inventory side effects. Pending event timers, switch state, removed
object identity, moved trigger volumes and mover/controller state remain open.
The first-entry inventory policy is separate. No on-disk save/load is added.

The owner reserves172,040 bytes:128 section names,4096 UID keys and4096 compact
activation snapshots. Existing authored inventory reports2367 triggers across93
SP/MP level records, within that capacity. A new campaign clears the owner.
Registration is during scene initialization; capture before a successful handoff
updates the already registered slots without heap allocation. Full snapshots
are only marked valid after the trigger's timer conversion succeeds.

Focused mission_goal_dispatch tests cover one-use gate rejection after restore,
paused cooldown and wraparound, expired/inactive deadlines, contact delay,
rejected malformed timing without mutation, preservation of new runtime handles
and independent section keys. The PC L1S1/L1S2/back replay restores61 L1S1
triggers from112 registered section keys and keeps baton charge/item retirement.
This is runtime ownership plus gate coverage, not a played repeated-contact route.
TRIGGER_HISTORY reports total registered keys, restored current-section triggers,
saved current-section triggers and owner bytes.

Stock64MiB focused native render-20260914-165307 passes240 frames and both
handoffs. TRIGGER_HISTORY matches PC [112,61,0,172040]; the selected body,
mission-goal, inventory, pickup, combat, animation and audio checks also match.
This verifies native restoration counts and surrounding gameplay state; the
one-use/cooldown edge cases are covered by the focused helper tests above.

```powershell
python tools/xemu_render_check.py --input artifacts/riot-pickups/return.bin --item-uid 9463 --exit-uid 9019 --return-exit-uid 9346 --seconds 360
```
