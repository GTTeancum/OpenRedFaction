# Campaign countdown events and timer persistence

Implementation update (2026-09-23): The first-pass shared runtime and scene
timer described by this research now live in `src/core/event.c` and
`src/diagnostic/scene.c`; see `docs/CAMPAIGN-COUNTDOWN-FIRST-PASS.md` for
current PC/Xbox evidence and remaining save/presentation work. The probe
scope below remains original-binary evidence, not a claim of full parity.

## Evidence and scope

42 original instruction scenarios pass in `tools/future_re/campaign_countdown.py`; results `countdown.json`, authored inventory `countdown-authored.json`. Same verified original executable as companion campaign reports. Actual Begin73, Reaches84, original string predicates, Over75 polling/array iteration and decrement arithmetic execute. Only linked object resolution/activation effects are intercepted. No live campaign, HUD, full persistence or port code runs.

At research time, the runtime action whitelist in src/core/event.c excluded73/74/75/84. These needed subclass-specific state and polling, not simply a generic switch. Authored inventory contains36 events in L15/L17, including cross-level threshold/expiry handlers. This report supplied the evidence for the first-pass campaign timer service.

## Begin73 and End74

Begin overrides action with thiscall4bd600. Authored integer is event+2b8. Normally signed int converts to float remaining seconds at global6460f8. Actual name equality5001d0 compares event string+1c against `station_blowup`. If equal, difficulty593e54 selects90,55,45,35 seconds for values0,1,2,3. Out-of-range4 writes nothing; fixture123 survives. All ten begin cases execute real string matching and arithmetic. It does not visibly reset Over pulse or existing threshold event latches.

End74 generic on-dispatch calls4b9b50, which directly writes remaining=0. It does not set expiry pulse. Generic outgoing links remain governed by existing event activation/propagation. Its off behavior is not established here.

Authored L15S1 UID9693 starts600 seconds; L17S1 UID19998 is named station_blowup with authored55, so difficulty override matters. Do not treat the authored55 as universal. Other numeric difficulty labels have not been inferred.

## Decrement and expiry

Original4332d2..43331a subtracts simulation seconds5a4014 only if remaining>0. If result<0 it clamps0 and ORs bit2 into6460ec. Exact equality to0 does **not** set expiry, and later ticks skip because remaining is no longer positive. Executed(1,.5),(1,1),(1,1.5),(0,.5) distinguish these edges. A port should use an explicit crossing-to-or-below-zero guard to avoid losing expiry on exact equality, documenting that improvement instead of reproducing the original bug.

Over75 per-frame4b8ea0 checks6460ec bit2, walks links, then clears bit2 **after** dispatch. Linked events resolve4b6800 and activate(-1,-1,1); otherwise mover46afa0 fallback dispatches46aba0(mover+2c,-1,-1). The actual generic tick4b8ce0 polls it even with no pending timer and without inspecting the monitor's disabled flag. Four cases confirm disabled/empty monitors still consume the pulse; second monitor gets nothing. Activation hook sees bit2 still set while linked actions run. Preserve ordering or explicitly define reentrant behavior; this differs from vehicle monitors which consume before dispatch.

## Reaches84

Subclass tick thiscall4bd290 uses:

- +2b8 byte: has observed countdown strictly above threshold (armed).
- +2b9 byte: fired latch.
- +2bc signed integer: threshold seconds.

It returns immediately for remaining<=0. If remaining>threshold it sets armed, even if already fired. Fired latch blocks repeat dispatch. Normal dispatch requires strict `0 < remaining < threshold`; equality does not fire.

On L17S1.rfl, L17S2.rfl or L17S3.rfl, events **whose name is not `countdown_sound`** additionally require armed.500290 is inequality, not equality. The probe executes actual predicates with ordinary L15 name, ordinary L17 name, and L17 countdown_sound, both initial armed states and remaining0/29/30/31. Therefore a newly loaded L17 ordinary60-second warning does not fire immediately when difficulty starts55seconds; it has never observed>60. countdown_sound bypasses that restriction. This exception is important across level transitions.

On firing, every link is independently offered to event4b6800, mover46afa0, and third resolver4c08e0 whose object is passed4c0200. It is not an exclusive event-else-mover choice. The routine then sets fired=1, even with empty links. It does not check its own disabled flag in the recovered body. Outgoing actual effects beyond activation are not executed by this probe.

## Authored mission use

L15S1 begins600 seconds; Over handlers recur in L15S2/L15S4. L15S4 threshold messages use60,45,30,9. L17S1 begins difficulty-dependent station_blowup; thresholds60,45,30,20,15,10,1. L17S2/L17S3 repeat60/45/30/20/15/10 and add89-second `countdown_sound`; L17S3 has an88-second ordinary handler. Countdown_End appears in L15S1,L15S4,L17S3. The data requires timer continuity across scenes; each level owns newly loaded monitor latches.

## Save and scene lifetime: static evidence

Snapshot writer4b4220 stores raw float6460f8 at snapshot+0xc4; restorer4b4bfd reloads it. Another snapshot update4b55a5 stores it at+0xc4. These establish explicit persistence of remaining seconds. Full original serialization functions are not executed here.

Level finish-load46114e checks434200 state: states9/10 skip clearing countdown; otherwise a true loader flagBL also skips clearing; if neither applies,46116b writes0. The exact caller meanings of9/10/BL remain unproven, so don't copy guessed constants into shared code. Integrate with existing campaign load-vs-new-game ownership: preserve remaining on campaign transitions/checkpoint restore, reset on new game as explicitly chosen. Fired/armed monitor state must also be considered for same-level saves; object save handlers remain to recover.

## Minimal later integration and verification

Add campaign-owned remaining seconds plus expiry pulse, independent of scene memory. Add two bytes per threshold monitor and authored signed threshold. Use existing simulation dt, ordered event ticking, activation and mover services. Expose Begin/End action callbacks and poll Over/Reaches outside the pending-deadline-only path. Keep display formatting/HUD separate from timer progression; no allocation is needed per frame.

Priorities: implement countdown state/polling and transition ownership before escape mission testing; exercise difficulty start, below/equal/above thresholds, L17 named exception, exact-zero correction, end-without-expiry, monitor ordering and empty links, restart after fired state, same-level save/load and cross-level remaining preservation. Resolve third object action and original latch persistence before claiming full parity. No campaign route progression or polished timer HUD is claimed.

Follow-on domain resolution:4c08e0 accepts only objectkind5 (trigger), and4c0200 clears trigger+2b0 disablebit0x10 without firing it. The health-threshold probe executes that helper and checks unrelated bits survive. Countdown Reaches should therefore enable linked triggers, not activate them immediately.
