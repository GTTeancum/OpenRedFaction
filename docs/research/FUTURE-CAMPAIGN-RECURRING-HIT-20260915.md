# Cyclic_Timer20 and When_Hit52

19 original instruction scenarios pass in `tools/future_re/campaign_recurring_hit.py`; `recurring-hit.json` and47 authored records in `recurring-hit-authored.json`. Same original executable hash as companion reports. Actual timer factory/on/off/tick, generic event tick/When_Hit poll, damage-signal prefix and signal-clear prefix execute. Allocation, object resolution and linked effects are intercepted. No shared source/build/emulator.

## Cyclic_Timer20

Allocation4b69d0 maps20 to4b6d71,0x2d0 bytes, constructor4be740, vtable589a2c. Factory cdecl4b80b0(position,period_float,max_count,unlimited_byte) initializes:

| Offset | State |
| --- | --- |
|+2b8 byte|enabled=0|
|+2bc float|period seconds|
|+2c0 byte|unlimited|
|+2c4 i32|max count|
|+2c8 timer|deadline=current simulation clock (zero interval)|
|+2cc i32|fired count=0|

On4bb7a0 only enables; off4bb8a0 only disables. Neither resets count or deadline. Tick4bb7b0 first services base delayed event4b8ce0, then checks enabled, count/unlimited and due cycle timer. Due cycles offer each link independently to event4b6800 and mover46afa0. Events activate(-1,-1,1); movers receive retained event source+2ac/actor+2a8. Fixture deliberately resolves the same handle to both to verify independent paths. It increments fired count once and sets next deadline to **current time + truncated(period*1000)**, not prior deadline+period. Long stalls fire at most one cycle per tick; no catch-up burst.

Three finite/unlimited sequences execute. Limit2 at period0.5 fires immediately on first enabledtick1000, then1500, then remains exhausted at5000. Unlimited0 fires again at5000 and schedules5500. Limit0/unlimitedfalse never fires: count0 alone does not mean forever. Off suppresses work; re-enable of exhausted finite timer still does nothing. Re-enable of overdue unlimited timer fires once immediately. Common activation delays remain handled by existing base event code before enabling.

Authored25 timers: L1S1 UID9909 has words[5,0], values[1.5,0], flags[0,0]; all other24 have words[0,0], firstflag1 and positive periods. The loader462553..462563 forwards stack-held values/ESI to factory. Static loader register tracking confirms firstfloat=period, firstword=count, firstflag=unlimited:46227b stores first flag into base stack+10 (two pushed arguments shift visible offset to+18),462299 keeps first word inESI,4622ad stores first float at stack+14;462553..462563 passes these exact values. The probe executes factory directly, not full authored loader. ExamplesL6S3 timer6647 period3, L20S2 4960 period8, multipleL9S3/L17 recurring hazards.

## When_Hit52

Generic base vtable589c9c, base tick4b8ce0 calls4b8dd0. If common deadline+298 is still valid, the hit poll does nothing. Otherwise it scans all links as objects via40a0e0 until one has object flags+7c bit0x00200000. Then it scans the entire same link list as outgoing events, with mover fallback. Thus links mix watched objects and actions, like several original monitor events; do not assume every link is only an output.

It does not inspect its own disabled flag and does not clear the hit signal. Eight hit/pending/disabled fixtures show: pending common deadline blocks, own disabled bit does not block polling, unchanged hit signal fires again on a second poll. Unlike vehicle/countdown pulse consumers, multiple When_Hit monitors can observe the same damage bit before the common object update clears it.

Producer prefix4892c0 resolves object, rejects damage below float0.001, then sets0x00200000 before inspecting the invulnerability bit4. Eight damage/flag cases verify that0 and0.0009 do not set it, while0.001 and1 do even for invulnerable objects. Invulnerable path returns before health mutation but the event signal survives. Object update prefix487cf0 clears only0x00200000; ordinary unrelated bits remain. Probe stops before other object update/health work. Therefore **do not drive When_Hit only from health lost or killed actors**: accepted damage attempts against invulnerable objects can activate it.

Authored22 monitors include L1S1 UID8436, L1S3 9456, L2S1 7203/7215, L3S1 machinery and train02 UID9052 linking Endgame9051 plus object8378. This gives early campaign relevance beyond final escape missions.

## Minimal shared integration

Add compact cycle enabled/unlimited/count/deadline state to runtime events and service it even when no common delayed action is pending. Use existing timer helpers and actor/source propagation. Preserve count/deadline/enabled across same-level save; a timer restart is a distinct action policy, not implied by on.

For When_Hit, add an object damage notification bit or frame epoch set at accepted-damage admission before invulnerability response, covering intended actor/mover/clutter routes. Poll monitors after those producers and clear signal at the equivalent common object update boundary, after all observers. Exact producer/update ordering across the entire frame remains to verify; do not clear on first monitor. Retain normal activation policy on linked target events.

Next tests: full authored loader execution, zero/negative interval guard, duplicate monitored objects, delayed monitor becoming due on a hit frame, actor/mover/clutter damage coverage, same-frame multiple observers and save/restore. Existing19 cases establish core state machines, not visual hazards or complete damage-triggered campaign flows.
