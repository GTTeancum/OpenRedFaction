# Campaign Endgame first pass

Authored event type 71 now dispatches through the shared event runtime into a
level-owned terminal outcome. The original executable's `4bd0e0` handler calls
`43e9b0` with the event name; `call_credits` requests state 23 immediately,
while other names schedule a 1.5-second player fade and then state 19 failure.
The binary-derived boundary cases and authored event inventory are documented
in `research/FUTURE-CAMPAIGN-ENDGAME-20260915.md`.

This first pass freezes player input when a failure begins, fades the shared
PC/Xbox viewport to black over 1.5 simulation seconds, then displays the
English failure description selected by the authored reason key from the
installed `endgame.tbl`. A missing description falls back to the key. Use (`E`/`X`) after the
fade reopens the current section at its authored spawn with fresh campaign
state. Repeated requests cannot replace the first outcome. `call_credits` has
a distinct immediate terminal screen. Live quick-save rejects an active
terminal outcome because the fade and screen are not yet in the checkpoint
format. A quick-load request remains available through the existing frontend
path.

Focused verification used installed `L5S4.rfl` Endgame UID 5337
(`Undercover_Miner`). A 120-frame process-local PC replay reached
`CAMPAIGN_ENDGAME 1 1 0 5337 2 0`, and its final frame showed the black
mission-failure screen and reason. A 60-frame PC replay remained in fade
phase with 517 ms left and visibly darkened the level. The stock 64 MiB XEMU replay in
`artifacts/xemu/render-20260923-033013/` matched all six endgame counters,
finished with 5,921 available physical pages, and its native framebuffer
showed the same reason and recovery prompt. Both PC and NXDK builds passed.

A 160-frame input replay presses Use at frame 125 after the failure screen.
PC and stock 64 MiB XEMU both reopen `L5S4.rfl` at that frame, clear the
endgame state, and render the player at a fresh spawn. XEMU reports one
transition with UID `4294967292` and ends with 5,922 available pages in
`artifacts/xemu/render-20260923-034508/`. The native full reload took longer
than the harness default timeout; that run used `--seconds 300` and passed.

Event type 67 now walks only its authored links and clears entity flag
`0x00400000` from registered NPCs, preserving the other flags and synced
damage state. This matches original action `4b9440`. A PC replay of authored
`L5S2.rfl` UID 4700 resolved its one NPC link and dispatched the clear; that
NPC's flag was already clear, so this replay verifies routing but not a
nonzero-bit mutation. A 32-frame stock 64 MiB XEMU replay in
`artifacts/xemu/render-20260923-035337/` matches all four PC dispatch words,
shows the level in its native framebuffer, and ends with 4,348 available pages.

The installed table is read on demand from `tables.vpp` into a bounded 16 KiB
scratch allocation; the selected English description is retained in a 512-byte
scene buffer and the archive is closed. No original table text is tracked in
source. A 120-frame PC/XEMU replay of UID 5337 matches all four description
diagnostics (`172` bytes, FNV-1a `3061203645`) and displays the same four-line
message. The native capture is in
`artifacts/xemu/render-20260923-040131/`; it ends with 5,921 available pages.

Remaining work is language selection beyond English, a menu/load-slot
choice, credits sequence, critical-NPC death policy (including deciding when
that death invokes Endgame), a live flagged-NPC mutation, and checkpoint ownership
if saving during a terminal transition is later allowed.
