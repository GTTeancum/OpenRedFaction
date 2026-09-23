# Campaign Endgame first pass

Authored event type 71 now dispatches through the shared event runtime into a
level-owned terminal outcome. The original executable's `4bd0e0` handler calls
`43e9b0` with the event name; `call_credits` requests state 23 immediately,
while other names schedule a 1.5-second player fade and then state 19 failure.
The binary-derived boundary cases and authored event inventory are documented
in `research/FUTURE-CAMPAIGN-ENDGAME-20260915.md`.

This first pass freezes player input when a failure begins, fades the shared
PC/Xbox viewport to black over 1.5 simulation seconds, then displays a
mission-failure screen with the authored reason key. Repeated requests cannot
replace the first outcome. `call_credits` has a distinct immediate terminal
screen. Live quick-save rejects an active terminal outcome because the fade and
screen are not yet in the checkpoint format. A quick-load request remains
available through the existing frontend path.

Focused verification used installed `L5S4.rfl` Endgame UID 5337
(`Undercover_Miner`). A 120-frame process-local PC replay reached
`CAMPAIGN_ENDGAME 1 1 0 5337 2 0`, and its final frame showed the black
mission-failure screen and reason. A 60-frame PC replay remained in fade
phase with 517 ms left and visibly darkened the level. The stock 64 MiB XEMU replay in
`artifacts/xemu/render-20260923-033013/` matched all six endgame counters,
finished with 5,921 available physical pages, and its native framebuffer
showed the same reason and recovery prompt. Both PC and NXDK builds passed.

The recovery prompt currently describes loading a save or restarting the
level; it is not an in-screen menu. Remaining work is localized `endgame.tbl`
failure text, an actual restart/menu choice, credits sequence, type 67
`Clear_Endgame_If_Killed` and its story-critical NPC death policy, and
checkpoint ownership if saving during a terminal transition is later allowed.
