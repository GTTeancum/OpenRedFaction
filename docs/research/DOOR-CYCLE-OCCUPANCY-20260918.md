# Lower-door cycle fixture is occupied

The current 420-frame fixture reports eight occupancy holds, zero reversals and two arrivals. New opt-in RF_REPLAY_DOOR_TRACE output identifies actual hold time, controller, deadline, player position and cumulative NPC occupancy admissions. It is excluded from native Xbox compilation and changes no gameplay behavior.

At 3400/3433ms, NPC occupancy keeps both leaves open and pushes their deadlines to4400/4433ms. By those deadlines, the original fixture has already returned the player to the occupancy volume. Player occupancy continues renewing the deadlines. A bounded delay control (90 idle frames instead of30) leaves the NPC occupying the trigger at4400/4433ms; returning the player again maintains occupancy afterward. That experimental input change was reverted, not accepted by loosening assertions.

Evidence: artifacts/door-cycle-current.log, door-cycle-trace.log, door-cycle-delayed.log, and the final detailed artifacts/door-contact/pc.txt. The trace run is PC only. This establishes why the current close/reopen assertion fails; it does not prove complete closing/reopening or that every occupancy policy is correct. The old fixture no longer provides evidence of a controller timing defect. Opening/traversal remain previously verified.

Per engine-first prioritization, do not spend another broad test pass on this route now. Keep the missing unoccupied-cycle coverage in TO-DO and proceed to remaining weapon integration. A future cycle fixture must explicitly distinguish occupied hold, unoccupied close and reopening; do not remove NPC occupancy from production to satisfy it.
