# Vehicle event consumers: original-code evidence

## Result and scope

Twenty original RF.exe instruction scenarios pass in `tools/future_re/campaign_vehicle_events.py`, with results in `vehicle-events.json` and three authored records in `vehicle-events-authored.json` in the same directory. SHA256 is `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. This recovers event consumption and exit-lock semantics, not vehicle physics. It complements the vehicle agent's entry/use/exit work.

Actual original per-frame dispatcher `4b8ce0`, array operations, event activation `4b8b70`, no-op83 action and outgoing event link walk execute. Event/entity/mover lookup boundaries return fixture objects; outgoing mover and final event target effects are recorded instead of executed. Never_Leave runs through actual on/off virtual dispatchers. No port source, builds, emulator or stock process is used.

## Enter and attempted-exit pulses

| Type | Poll function (cdecl event pointer) | Local player pulse |
| --- | --- | --- |
|77 When_Enter_Vehicle|`4b8f30`|`[global7c75d4]+10 & 0x400`|
|78 When_Try_Exit_Vehicle|`4b8fd0`|`[global7c75d4]+10 & 0x800`|

Both require global entity pointer `5cb054 != NULL`, then read the separate local-player object at `7c75d4`. They clear only their pulse bit **before checking link count**. Remaining bits, including the other vehicle pulse, survive. They do not inspect their own event disabled bit at `+2b0`. Executing through `4b8ce0` with no pending timer verifies that disabled and empty-link monitors still consume the pulse. A second monitor of the same type receives nothing in that frame. Missing player leaves the pulse untouched.

Each monitor walks its live link array at `+29c` (count, capacity, pointer). For each handle, `4b6800` event lookup wins: actual activation `4b8b70` receives ECX=linked event and stack arguments `(-1,-1,1)`. Otherwise `46afa0` mover lookup is tried; a found mover causes `46aba0(mover+2c,-1,-1)`. Unresolved links do nothing. This is a narrower resolver than generic object link dispatch. The probe includes one event, one mover and one invalid handle; fixture linked83 forwards to target777 with source/actor -1. The linked event's normal activation policy remains in force, even though the monitor's own disabled flag is not consulted.

Vehicle agent independently identified producer `4a1970`: boarding sets0x400; attempted exit sets0x800. These flags are signals consumed in event-list order, not permanent occupied/unoccupied state and not broadcast to all monitors. Preserve tick ordering when connecting them.

## Never_Leave_Vehicle80

Actual on dispatcher `4b9070` invokes `4b9bd0(event)`; off `4b9f80` invokes `4ba3f0(event)`. Each walks links and resolves entities with `426fc0`. On ORs bit0x80 into entity DWORD `+814`; off ANDs its complement. Other31 bits remain intact; stale links are ignored. Four initial bit patterns verify both branches. No linked action propagation is performed by these helper bodies.

The vehicle agent identified this bit as an ordinary player-use exit gate. It is not a universal detach prohibition: scripted Teleport_Player63 and Cutscene55 call forced detach paths separately; see companion reports. Do not make Never_Leave globally disable all forced transitions.

## Authored use

Only three instances occur in the existing93-level event inventory:

- L12S1 UID9694 When_Enter_Vehicle links9692 (Follow_Waypoints, `jeep_path`, `One way`, linked7629),7629 (not an event in this inventory),10194 (Delay, linked10195).
- L7S3 UID7963 When_Try_Exit_Vehicle links7964 Message (empty authored texts). Resolve its runtime message behavior separately; do not invent a line of dialogue.
- L7S3 UID7961 Never_Leave_Vehicle has **no authored links**. Its on/off helper is therefore a no-op unless links are populated dynamically. Do not attach it implicitly to the current vehicle without evidence.

There is no authored same-type multi-monitor competition in this inventory, but the original behavior is now pinned by the executable edge cases.

## Minimal later integration

Extend the existing event tick whitelist in `src/core/event.c` for77/78 and add scene-owned vehicle signal storage fed by the future player-use service. Process monitors in existing source event order and consume before resolving links, including empty monitors. Do not route the monitor through generic delayed action admission or add a disabled guard absent from the original. Linked event activation should reuse normal runtime activation; mover fallback should use the existing mover service.

Type80 is a small scene entity flag service called from the existing startup/runtime event action dispatcher, retaining separate on/off behavior. Wire its flag into ordinary vehicle exit permission only after the vehicle agent's use routine semantics are integrated. No new generic event framework or permanent pointer-based state is needed.

Verification targets: both simultaneous pulses; missing local entity; disabled/empty first monitor stealing pulse; duplicate ordered monitors; linked event delay/disable behavior; event-vs-mover resolution precedence; unknown link; repeated lock/unlock preserving other flags; ordinary exit denied while scripted forced teleport remains permitted according to its own detach result handling. Current evidence covers all except linked target delay/disable combinations and integrated player-use production, which remain future tests.
