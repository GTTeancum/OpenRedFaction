# First-entry inventory on section revisit

L1S1 startup strips the player through authored event8366. Reconstructing the
section on backtracking previously ran that callback again, queued a strip,
and cleared imported weapons when the first gameplay frame applied the queue.
The collected baton stayed retired, leaving the player without that weapon.

The shared scene now keeps a separate bounded campaign ledger keyed by canonical
section name. After a successful startup sweep, that section's first-entry
inventory setup is marked complete. Later startup sweeps suppress only immediate
Strip_Player_Weapons and Give_Item_To_Player callbacks. The guard ends before
normal gameplay; explicit or contact-triggered stripping/gifts still execute.
The first visit still runs the authored setup and preserves its operation order.
Environment startup still executes when reconstructing resources.

This is practical first-pass policy, not original saved-section restoration.
Delayed startup inventory operations are not attributed across the scheduler;
non-inventory startup side effects, trigger counters, pending timers, movers,
section-local goal state and full save/load remain open. Repeatable gameplay
inventory events are not globally marked consumed. Starting a new campaign clears the ledger alongside the other session owners.
The reset branch was corrected during the local-goal persistence follow-up.

The ledger reuses the tested bounded campaign key owner in a separate namespace:
128 canonical section names and1024 record slots,20,488 bytes including counters.
Only one record is used per section. No handles or pointers are retained.
Registration precedes startup; successful completion marks the record. Error
returns clear the suppression guard and leave completion unmarked.
STARTUP_INVENTORY diagnostics report revisit, skipped strips, skipped gifts and
owner bytes. These counters describe the current section, not cumulative totals.

PC tools/replay_riot_pickups.py passes first-entry strip, pickup acquisition,
duplicate-weapon ammunition and L1S1 -> L1S2 -> L1S1 retirement/inventory checks.
The returned player retains baton ID2 with100 charge; startup diagnostics show
one suppressed strip. Placement and exit dispatch are process-local fixtures;
this does not prove the walking route or a complete campaign.

The five existing strip/rearm checks also pass: unarmed play, later baton grant,
mid-play stripping, unarmed round trip and unarmed respawn. This verifies that
the startup guard does not suppress ordinary inventory operations.

The broad native replay-20260914-162753 completed240 frames and both handoffs,
but failed its ACTOR_FOLLOW_SUMMARY renderer comparison before inventory checks.
Native summary was [59,2166136261,0,1741293533,3145728], versus PC
[59,2260506114,12432,2643036455,3145728]. Preserve that failed report; it is not a
campaign-parity pass. Focused gameplay validation uses xemu_render_check.py with
explicit exit/return UIDs and checks startup inventory, pickup and ammo state.
Failure snapshots now also retain those fields before early unrelated assertions.

Stock64MiB focused native render-20260914-163342 PASS:240 frames, both section
handoffs, exact selected PC body, ammo, startup-inventory, pickup, combat,
weapon-animation and audio state. Returned baton ID2 has100 charge and zero
reserve; the startup receipt is [1,1,0,20488]. The earlier broad renderer mismatch
remains open and this focused pass does not supersede it.

```powershell
python tools/replay_riot_pickups.py
python tools/xemu_render_check.py --input artifacts/riot-pickups/return.bin --item-uid 9463 --exit-uid 9019 --return-exit-uid 9346 --seconds 360
```

The manual controller session was left untouched throughout these checks.

Trigger activation counts and cooldown/contact deadlines now have separate
section snapshots; see TRIGGER-CHECKPOINTS.md. This does not yet suppress
repeated auto-start side effects or restore pending event actions.
