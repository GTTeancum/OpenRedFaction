# Reconstructed game clocks and deadlines

Shared C lives in `include/rf/timer.h` and `src/core/timer.c`. It accepts explicit
clock state instead of storing original globals. Both PC and NXDK compile it.
No OS clock or Xbox interrupt source is wired to it yet.

Original 0x4fa2d0 advances game clock 0x5a3ed8 unless pause depth 0x173c36c is
nonzero, and always advances the other clock at 0x5a3edc. `real_ms` names that
unpaused clock; it is advanced by the caller's delta, not read from wall time.
0x4fa320/0x4fa330 increment/decrement pause depth. The safe C API rejects resume
at depth zero and overflow at INT32_MAX, instead of allowing unmatched nesting.

The period constant is 0x3ff1a100, or 1,072,800,000 ms. Time values include both
zero and the period endpoint. Wrap uses strict greater-than: reaching PERIOD
exactly retains PERIOD rather than storing zero. The API accepts clocks in
[0,PERIOD] and forward deltas up to one period, preserving this representation.

0x4fa360 sets a deadline from game time plus a signed offset. Positive offsets
use the same strict wrap rule; negative offsets add the period only when the
sum is negative. Negative offsets therefore mean deadlines in the past, not
disabled timers. The C API bounds offsets to [-PERIOD,PERIOD]. Invalid inputs
preserve state. Clear methods 0x4fa350/0x4fa3e0 store -1; query methods treat
any negative deadline as inactive.

0x4fa3f0 tests expiration using half-period 0x1ff8d080. If deadline is greater
than now, the timer is expired only when that difference exceeds half a period.
Otherwise it is expired when now minus deadline is at most half a period.
At equality with now it is expired. An inactive timer is never expired.

0x4fa420 returns signed remaining milliseconds, subtracting or adding PERIOD
when the signed difference crosses either half-period boundary. Exactly plus
half remains positive; exactly minus half remains negative. Inactive timers
return PERIOD, not zero. Thus these queries assume deadlines are interpreted
within half a clock cycle; they do not track an unbounded epoch count.

`tools/verify_timer.py` compares complete original implementations for advance,
pause/resume, set/clear, expiration and remaining time across 9,800 cases. It
checks both clocks, pause depth, deadline and query output. Six additional
C-only malformed cases check unchanged state. Local evidence is
`artifacts/timer-verification.json`. Random-range timer setup 0x4fa3b0, platform
time sources, and runtime integration are not covered.

The turn-animation helper calls 0x4fa360 with 1,200 ms for entity offsets
+79c, +4d0, +4d4, +744 and +798, at calls 0x41fc2e/3e/4e/5e/6e. It also sets
+7bc to one, starts an action and calls movement routine 0x427450; those steps
must be integrated with these deadlines before the full helper is recovered.
The earlier selector uses the same setter with 800 ms at entity +744.
