# Endgame71 and critical-NPC flag67

## Result

Nine original boundary scenarios pass in `tools/future_re/campaign_endgame.py`; results `endgame.json`;13 authored events inventoried in `endgame-authored.json`. Actual Endgame handler4bd0e0, name handling43e9b0, callback43e9a0 and Clear_Endgame action4b9440 execute. External player-transition and state-manager calls are intercepted. These establish routing and state requests, not rendered terminal screens or live fades. Same verified original RF.exe as other future campaign reports.

## Endgame is usually mission failure

Type71 allocation table4b69d0 maps through4b741a, allocating0x2b8 bytes and constructor4bed70, which installs vtable589c1c. Its on method is4bd0e0; ordinary generic4b9070 type71 is a no-op, so adding71 only to that generic switch would miss the actual behavior.

4bd0e0 takes event rfString name at+1c, retrieves C string4ff480, and calls43e9b0(name). The generic authored integer/float fields are not read by this path. Name `call_credits` is special: it directly requests434190(23,0). Other names are copied including terminator into global63aaf8, then call4a73e0(local player7c75d4,1.5f,callback43e9a0). Executing the callback requests434190(19,0). CasesShuttle,station_blowup,train02_end,call_credits,unknown_case verify exact branch/arguments; even an unknown name schedules the same transition. Do not silently interpret an unknown name as victory.

The stock `tables.vpp` entry `endgame.tbl` at offset245760,size8225 contains localized failure descriptions keyed by names includingShuttle,escape_pod,capek_lab,missile_center,Undercover_Miner,Hendrix_L19S3,train02_end. AuthoredShuttle describes missing the shuttle; escape_pod describes failing to reach the pod before station destruction. This is resource evidence that ordinary Endgame events are mission-failure presentation, not campaign success. Raw table was extracted only to ignored `artifacts/future-campaign-re/endgame-original.tbl`; do not publish copyrighted asset content with source.

In particular L15S4 When_Cutscene_Over19886 links Endgame19887 namedShuttle. That is a failure cinematic chain, not successful game completion. No authoredcall_credits event occurs in the93-level event inventory used here; credits may be requested through another route.

## Clear_Endgame_If_Killed67

Generic on dispatcher4b9070 calls cdecl4b9440(event). It walks links, resolves entities426fc0, and clears entity DWORD+810 bit0x00400000, retaining every other bit. Four initial flag patterns and one stale link verify behavior. No implicit current-actor target: an empty link list does nothing.

Static death logic42c19f checks this same bit and, under preceding death-policy gates, calls43e9b0 with entity name string+18. Those preceding gates have not been fully recovered here, so don't claim all deaths always end a mission. Another static check4b620a scans these marked entities with427020 and also gates on active cutscene45be80; its broader purpose remains to recover. The clear action should remove story-critical death consequences from precisely linked NPCs, leaving ordinary damage/death unaffected.

Authored67 appears L5S2 UID4700 linking4494, and L17S3 UID18728 with no links. As with other empty-link events, do not invent a target for the latter. Remaining eleven authored71 records select the table names above, with repeatedShuttle handlers acrossL15 escape levels.

## Minimal integration and remaining work

Add a scene/campaign terminal-outcome service that accepts the authored reason key, requests the transition after1.5 simulation seconds, then presents localized failure information and recovery/menu choices. A distinct credits request handlescall_credits. Use existing text/table infrastructure and shared state transitions; do not require cinematic polish to prevent continuing a failed mission. Retain named reason and transition state in a later full-game checkpoint only if saving during the transition is allowed.

Expose type67 as a linked-NPC flag mutation and connect the equivalent story-critical bit to the existing NPC death pipeline after its original gate conditions are verified. Under64MiB this requires a compact reason key plus timer/callback state, no new actor duplication or broad UI framework.

Tests next: actual4a73e0 timing/control gating; state19 table lookup/default behavior; case sensitivity/localization; marked NPC death with/without67; missing/duplicate names; scene unload during pending callback; repeatedEndgame requests. Current evidence stops before rendering/audio and does not establish mission-success or credits visuals.
