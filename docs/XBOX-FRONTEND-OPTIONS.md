# Xbox frontend and controller options

Source implementation, October 12, 2026. This batch is uncompiled and unrun.
Only the parent-owned hourly stock64 Xbox batch may establish those results.
No build, test, runtime, campaign route, saved HDD read, disk operation, image,
fixture, repository mutation or upload was performed by this implementation lane.

## Tester flow

The startup menu now contains New Game and Options. D-pad up/down changes the
selection; A or Start opens it. New Game opens the difficulty chooser with Easy,
Medium, Hard and Impossible, mapped to the existing difficulty indices 0–3.
Medium/index1 remains the initial default. A on a difficulty starts a genuinely
fresh Live Mines session; B, Start or selecting Back returns to the main menu
without changing the accepted difficulty. This requires a second fresh A press
compared with the older one-action startup frontend.

Options is available from both startup and pause. D-pad up/down chooses:

- Look sensitivity: 25–200 percent, changed in 25-percent steps.
- Invert Y: on/off for the look stick.
- Stick deadzone: 0–40 percent, changed in one-percent steps for both sticks.
- Reset Defaults: 100-percent sensitivity, invert off, 18-percent deadzone.
- Back: return to the menu that opened Options.

D-pad left/right adjusts the selected setting. A increases a numeric setting,
toggles inversion, or activates Reset Defaults/Back. B returns to the caller;
Start returns to the startup menu from cold Options and resumes gameplay from
paused Options. Settings apply immediately and survive controller reconnect,
section reload, End Session and New Game. The screen explicitly says they are
kept until reboot. There is no file-backed settings persistence in this batch.

Sensitivity changes normalized response and remains clamped to [-1,1] by the
input owner, preserving ordinary player and turret input admission. It does not
increase the engine's maximum turning speed. Movement does not receive look
sensitivity or inversion; the shared existing scope multiplier is untouched.

## Ownership

The independent input lane owns input.c/h and process-lifetime integer options.
This lane consumes its getter/setter and Left/Right menu edges without changing
the gameplay/replay input record. Opening/closing a controller does not clear
options. All action buttons still require release across menu/gameplay entry.

The frontend and pause loop own navigation. Options keeps the existing paused
input provider and, during gameplay, suspended audio. It advances no simulation,
AI or mission clock. B returns to the still-paused session menu; Start resumes
through its existing neutral-input and frame-clock rebase path. Missing or
replaced controllers never manufacture an adjustment or resume press. The
Back+Start chord retains its existing whole-session exit behavior during play
and is ignored in the cold frontend where no session exists.

New Game difficulty has a separate process-local seed, initially1. The idle-only
setter and full-session reset share the exact same live/failed-retirement guard.
The ordinary cold campaign constructor reads the seed when resetting history;
setting only the current countdown would otherwise be overwritten by that
second reset. Cancellation changes no seed or live campaign state. Section
handoffs/restarts retain their existing countdown owner, and saved RFCH restore
continues to publish its own stored difficulty. No save format or admission
rule changes. The seed retains the last accepted New Game choice until reboot.

Existing pause enum values0–4 remain unchanged, with Options appended as5.
The six-row screen fits its existing footer and refusal lane. Drawing reuses
pbkit and the allocation-free procedural HUD, including a new percent glyph.
There are no new scene/image borrowers, allocations or external dependencies.

## Source-only review and next normal batch

Reviewed the changed call sites, integer input contract, exact idle guard,
constructor reset, save-history difficulty publication, menu cancel/rearm paths
and viewport coordinates. These are source findings, not compilation or native
behavior claims. The parent reports actual08 passed the earlier frontend lifecycle re-entry;
that historical result does not verify this new Options/difficulty UI.

The existing frontend diagnostic gets a three-word menu receipt: page, selection
and last accepted New Game difficulty. Page0 is main,1 difficulty and2 Options.
Frontend trace boundary8 means Options owns the loop. The older trace words keep
their existing semantics for ordinary frontend draws; they are not Options-loop
receipts. A bounded startup checker must now deliberately choose New Game,
release A, wait for the difficulty menu and press A again. Do not relabel the
old one-A observation as a regression or successful new-game start.

Useful proportional verification in the single parent batch: compile stock64;
open Options, change/revert controls, cancel the difficulty chooser, accept a
nondefault difficulty, and verify ordinary Start/Options/back/resume. Retained
values can be checked at the same End Session/New Game lifecycle boundary. No campaign playthrough or saved-HDD access is needed
for this first frontend/options batch. Startup Continue/Load remains a separate
slice using the existing save guards.
