# Reverse_Mover first pass

Xbox runtime implementation for authored event type 89. The installed PC
binary is used as code evidence only; no original-game runtime comparison or
screenshots are involved.

The verified installed `RF.exe` factory at `0x4b69d0` maps type 89 through
case `0x4b6f97` to constructor `0x4be9b0`, which installs vtable `0x589acc`.
Its ON method at `0x4bd680` iterates linked UIDs, resolves kind-8 group
controllers via `0x46afa0`, and checks `0x46b2f0`. That predicate rejects
reversal during dwell (`flags & 1`) or when `next_key == -1`. For a moving
group, the method calls `0x46bae0` only when the current `0x2000` direction
bit matches the event byte at `+0x2b8`; the OFF slot uses the ordinary base
handler at `0x4b9f80`. The event decoder retains that authored byte in the
low byte of `words[0]`.

The shared event runtime now delegates linked controllers on ON to a scene
callback. The scene applies that direction and active-motion gate to its
translation runtime, then calls the already reconstructed
`rf_group_translation_reverse`. All eleven authored instances target
translation group keys. Idle/dwell events and opposite-direction motion
leave the group unchanged; ordinary mover checkpoint state owns the result.

The focused Xbox fixture starts L7S4's six three-second garbage doors through
Delay 11163 at frame zero and fires Reverse_Mover 11274 at frame 60. The
stock-64-MiB XEMU run finished 100 frames with 4,006 free pages and reported
six linked requests, six live reversals and authored direction 1. A preceding
run identified that the diagnostic setup whitelist rejected type 89 at frame
60; adding type 89 to that fixture path resolved the failure. The passing
run checks the event-to-group path and live reversal without campaign-route
playthrough or PC gameplay testing. Visual door content, sound direction,
direction-zero lift behavior, later natural triggers and saves remain open.
