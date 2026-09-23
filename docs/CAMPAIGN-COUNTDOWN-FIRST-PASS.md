# Campaign countdown first pass

L15/L17 countdown events now run through the shared C event dispatcher on PC
and stock-memory Xbox. `Countdown_Begin` (73) sets the campaign-owned remaining
seconds from its authored integer. The `station_blowup` name selects the
original difficulty table (90/55/45/35); the current first-pass campaign
difficulty is fixed at index 1. `Countdown_End` (74) writes zero without
creating an expiry pulse. The 60 Hz simulation decrements a positive timer and
issues one pulse when it crosses or reaches zero. Treating exact zero as expiry
is a documented port correction to the original's strict-negative edge.

`When_Countdown_Over` (75) consumes the shared pulse in authored event order,
including an empty or disabled first monitor. It activates resolved event and
mover links before clearing the pulse. `When_Countdown_Reaches` (84) observes
strictly positive time below its threshold, never fires at equality, and
retains armed/fired latches per loaded scene. Ordinary L17S1/S2/S3 monitors
must first observe time above threshold; the authored `countdown_sound` name
bypasses that L17 arming rule. Its event, mover and trigger links are dispatched
independently, with trigger links enabling rather than activating them.

The source behavior is grounded in the installed original binary at
`0x4bd600` (Begin), `0x4b9b50` (End), `0x4332d2` (decrement), `0x4b8ea0`
(Over), and `0x4bd290` (Reaches); the executed 42-case probe and authored
inventory are in `docs/research/FUTURE-CAMPAIGN-COUNTDOWN-20260915.md`.
`event_countdown` tests the L17 exception, threshold equality, single expiry,
and End behavior. The native Xbox and PC builds pass.

In live PC L15S1, authored Begin UID 9693 leaves about 598 seconds after 180
replay frames. L17S1 Begin UID 19998 leaves about 52 seconds with difficulty
index 1. The stock 64 MiB XEMU L17S1 replay in
`artifacts/xemu/replay-20260923-022141/` matches the PC countdown's exact
remaining float bits, pending pulse, and difficulty at frame 180. That broad
replay reports FAIL later because the retained Xbox model path does not
populate a pickup CPU draw-vertex counter (PC 654, Xbox 0); gameplay pickup
counters match. The checker now treats that draw counter separately, but a
full rerun after the accounting change has not been made.

Remaining work: persist the global timer and type-84 latches in ordinary saves;
provide player-selected difficulty and countdown HUD/audio; verify actual
expiry and mission handoff in live L15/L17 sessions and preserve timer state
across a live section transition. The current timer survives an in-memory
campaign section handoff by scene ownership, but that handoff has not been
verified in a live countdown sequence. Saving a level containing these event
types is not yet admitted by the ordinary event checkpoint codec.
