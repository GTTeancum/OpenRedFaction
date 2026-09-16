# Countdown monitor save-state gap

Eight original instruction cases pass in `tools/future_re/campaign_countdown_save_gap.py`; results `countdown-save-gap.json`. Actual generic pending-event collector4bdaa0 and encoder4bda40 execute; only handle-to-UID conversion48a4f0 is intercepted. This is bounded evidence about one serializer, not proof that no other original save path exists.

Threshold constructor/factory4b8a00 calls4b6870(uid,84), then explicitly clears both armed+2b8 and fired+2b9 and sets signed threshold+2bc. Hence ordinary scene reconstruction resets both unless later restore applies state.

Generic collector4bdaa0 selects events with pending common deadline+298. A Reaches84 fixture with no deadline is omitted whether armed/fired are0 or1. With pendingdeadline1500 atclock1000, it writes exactly the same20-byte record for all four latch combinations. Record4bda40 contains:

| Offset | Value |
| --- | --- |
|0 u32|event UID+24|
|4 i16|encoded common delay+294 through4b5c00|
|6 u16|on/off mode byte+2b4 widened|
|8 i32|remaining common deadline through4b55e0|
|12 u32|actor+2a8 converted to UID|
|16 u32|source+2ac converted to UID|

Neither threshold latch is represented. The countdown float itself is separately stored at snapshot+0xc4, as the previous report establishes. Therefore a port cannot assume saving global remaining plus this generic pending-event record preserves threshold behavior on same-level reload. Restoring remaining29 with a previously fired ordinary threshold30 could fire it again if latch is reset, while L17 ordinary events can instead miss a later threshold because armed reset.

The smallest robust shared save addition is per-threshold event UID plus armed/fired bits, validated against level asset identity/type84 at restore, alongside existing event save fields and campaign remaining. Keep one-shot state on same-level checkpoint restore; initialize fresh on authored new-level load. This is a port implementation recommendation, not a claim of original full save format parity.

Remaining RE: identify every original save collector invoked by the full snapshot path before claiming original omission. The bounded generic collector is now proven insufficient; that is enough to avoid losing future port state. No source edits, builds, emulator or full save were run.
