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

RFCH3 now persists remaining time, expiry pulse and difficulty; RFEC3 and
session event history persist type-84 armed/fired latches. Legacy RFCH1/2
restores a stopped countdown. In a process-local PC L15S1 replay, a frame-150
ordinary quicksave and frame-260 quickload complete successfully. Its final
remaining-time bits at frame 480 equal an uninterrupted 369-frame run
(`1142208396`). The reload needs a narrow authored-placement exception for
the stationary Auto Turret: its saved position is unchanged while its basis
rotates. Static-world center, mover and prop checks still run. The NXDK XBE/ISO
build passes, but this L15 save/load continuation has not run in XEMU.

L15S1 also contains inert unimplemented event types. The codec now represents
their local fields while ordinary capture rejects them if a delayed action is
pending; this does not implement the actions themselves.

L17S1 NPC capture previously rejected UID 20026's live mover-controller
backlink. RFNC4 now saves the controller's authored first-key UID and rebinds
it to a fresh handle during restore; RFNC1/2/3 remain readable. In a PC L17S1
frame-150 save, frame-260 load and 480-frame continuation, the final countdown
bits `1111717776` match an uninterrupted 369-frame run. The stock 64 MiB XEMU
L17S1 save and separate fresh-process load both pass in
`artifacts/xemu/render-20260923-025000/` and
`artifacts/xemu/render-20260923-025154/`. All 16 native saved components match
PC byte-for-byte. The native reload reaches 180 frames with 4522 available
pages (about 17.7 MiB); the captured frame shows the red room, weapon, HUD
and mission message. The native post-load countdown value was not sampled.

A process-local L17S1 replay reaches zero after 3400 frames: its named
one-second `When_Countdown_Reaches` warning fires and the final PC frame is
visibly black. L17S1 has no type-75 Over monitor, so the expiry pulse remains
pending there. A live authored exit UID 18201 enters L17S2 at frame 61; at
frame 180 the remaining float is 52.016712 seconds, matching continuous
countdown progress. Continuing that transitioned run to frame 3400 leaves
zero time and a consumed pulse, with L17S2's Over monitor firing and a visible
blackout. This establishes the section handoff and failure warning, not the
later mission outcome.

The authored `reactor_blast.wav` was present but could not load alongside
resident L17 audio under the former 1 MiB bank cap. A 1.25 MiB cap admits the
309434-byte sound on PC. A 90-frame stock 64 MiB XEMU replay of authored sound
UID 21210 matches PC's scripted sound state exactly: one request, one start,
zero failures, with 4607 pages (about 18.0 MiB) available
(`artifacts/xemu/render-20260923-031714/`). The native framebuffer was
inspected. The emulator run disables audio output, so audibility is not yet
verified. The replay checker now compares exact countdown and scripted-sound
state on each native run.

The shared combat HUD now displays a bounded `TIME MM:SS` while an authored
countdown is active, rounding remaining time upward so it does not show zero
before expiry; the final minute is amber and the final ten seconds red. A
90-frame L17S1 replay activating authored Begin UID 19998 passes on PC and
stock 64 MiB XEMU in `artifacts/xemu/render-20260923-050123/`. The native
framebuffer visibly shows `TIME 00:54`, the PC/Xbox countdown words match
exactly (`[1112936727,0,1]`), and 4,682 physical pages remain available.
This is a functional first-pass HUD, not a retail-style presentation claim.

Authored L17S3 Over UID 20682 activates Black_Out_Player UID 20683 and
Play_Sound UID 21194; the latter has an outgoing link to Endgame UID 21219
(`escape_pod`). The port's Play_Sound action previously suppressed the
ordinary outgoing-link callback, leaving this mission route at its blast
sound. It now forwards those links after servicing the sound. A focused shared
event check drives a normal expiry pulse through Over, sound and a delayed
Endgame request. A separate 120-frame process-local replay directly activates
authored Over UID 20682 to isolate the installed L17S3 chain; PC and stock
64 MiB XEMU both start sound UID 21194, enter Endgame UID 21219 and display
the installed `escape_pod` failure description. All PC/Xbox sound, endgame and
description words match in `artifacts/xemu/render-20260923-051331/`; the native
framebuffer was inspected and 4,646 physical pages remain available. Direct
activation does not prove a natural timed traversal or the escape success path.

Remaining countdown work: player-selected difficulty, retail HUD styling, natural
timed expiry through L17S3 and the escape branch, captured native audio output, native long-run
expiry, native post-load timer comparison and native L15 save/load verification.
