# Native single-player pause

Status (2026-10-10): source-written and source-reviewed only. No compilation,
syntax checks, tests, emulator runs, forced input, screenshots or original-game
execution were performed for this slice. The next validation belongs to the
parent-coordinated 01:00 UTC Xbox batch. Physical controller and audible output
remain unverified.

## Player behavior and owner

Press and release Start to pause; press it again to resume. A held Start cannot
toggle repeatedly. Back+Start still exits, including while paused. Back+Y and
Back+X still request ordinary save/load during gameplay; paused input cannot
enqueue either request. Buttons/triggers held across resume must be released
before they can cause another action, including use, jump, fire, alternate,
reload, crouch, weapon cycle, defuse directions and save/load chord buttons.
Sticks are neutral on the resume sample and resume ordinary sampling afterward.

The last presented framebuffer remains visible. This does not add a pause menu,
overlay, save-slot selector or campaign progression. Controller disconnect does
not unpause; the existing USB polling can reopen a controller. Start already held
when a controller attaches is latched without inventing a new press. Input
open/close resets pause and action-release latches.

`src/platform/xbox/main.c:player_poll_paced` owns the pause lifetime. After the
existing replay branch and optional pacing step, it polls native input, then
holds the provider call on a Start edge. It sleeps 10 ms between controller
polls until a fresh Start edge or ordinary Back+Start exit. It does not return a
neutral gameplay tick repeatedly: the shared animation, physics, AI, events,
weapons, particles, defuse countdown and scene-frame counters do not advance.
The scene's deadlines are derived from its fixed-step frame index, so this
freezes them without replacing the game's shared timer implementation.

The parent-owned resume hook resets the pacing clock's `last_ms` and
`last_present_ms` to the current native tick and clears `credit`/`skipped`.
Counters remain intact and paused wall time never becomes catch-up debt. The
provider returns one neutral sample after resume. Shared `rf_scene_input`,
recorded input layouts and replay behavior are unchanged.

## Native audio lifetime

`rf_xbox_audio_set_paused` covers all 30 created ordinary SFX/voice-over slots
and the independent two-buffer music voice. It marks only currently playing
voices and uses `nxAudioVoicePause`, retaining their sample position, immutable
PCM descriptors, pins, loop flags and logical handles. It does not destroy,
reload, restart or reset ordinary voices or the music decoder on pause.

On resume, only marked voices still in `NX_PAUSED` are started. A naturally
completed `NX_STOPPED` one-shot is never restarted. Existing paused voices not
owned by this operation are not resumed, and stopping voices are left alone.
Release/reset/close clear the corresponding ownership marks. No new voice can
be admitted and no music decode/refill occurs while the adapter is paused.
An unopened optional audio backend returns success without doing anything.

State check, Pause/Start and mark publication share one outer DPC exclusion.
The pinned nxdk-audio implementation uses nested DPC exclusion around those
PIO commands; neither operation waits for a completion callback or destroys a
voice. Exclusion prevents a completion DPC from changing the software state
between the check and native operation. Hardware can nevertheless finish a
buffer immediately before its pause command takes effect. A later completion
DPC is allowed to release those already-consumed music lists while gameplay
is paused.

If both music lists are empty on resume, native Start would reject the voice.
The adapter instead leaves the existing `started`/revision/decoder state
intact. The next ordinary `music_poll` owns its existing drained-queue
release/recreate/refill path. That path continues the same decoder position;
it does not reload the track, rewind its PCM or deliberately skip samples.
If one or two lists remain, native Start resumes their existing hardware
cursor. Static voice descriptors stay pinned even after natural completion in
the installed backend and are released through their ordinary owner lifecycle.

Read-only diagnostic arrays:

- `rf_xbox_pause_diagnostic`: entries, resumes, held polls, status, entry frame,
  pacing rebases, active, reserved.
- `rf_xbox_audio_pause_diagnostic`: paused, entries, resumes, marked ordinary
  voices, marked music, completed voices not restarted, empty-music deferrals,
  API failures.

These counters are observation support, not proof of runtime pause or audibility.

## Original executable evidence

Read-only static disassembly of `Installed_Game/RF.exe`, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `0x436270` rejects an already-set freeze byte and the network-related gate
  `0x64ecb9`, sets `0x637086` and its companion mode byte `0x637087`, calls
  `0x505c70(1,-1)`, `0x5097d0`, and the clock pause wrapper `0x436260`, then
  sets the player control flag through `0x4a61f0`.
- `0x4362c0` has the complementary mode gate, clears both freeze bytes, calls
  `0x505c70(0,-1)`, `0x5097e0`, and resume wrapper `0x436250`, then conditionally
  clears that player flag. `0x436320` simply returns freeze byte `0x637086`.
- `0x436260` jumps to `0x4fa320`, incrementing pause depth `0x173c36c`;
  `0x436250` jumps to `0x4fa330`, decrementing it. `0x4fa2d0` advances game time
  `0x5a3ed8` only at depth zero, while the second clock `0x5a3edc` keeps advancing.
  The shared arithmetic is separately described in `docs/TIMERS.md`.
- `0x505c70` selects `0x543500`/`0x543520`, which dispatch to audio
  `0x522c00`/`0x522cb0` for the selected original backend. `0x522c00` stops
  currently active DirectSound buffers and marks their flag bit 2. Resume
  iterates only marked slots and calls `0x522c70`, which plays them with the
  retained loop policy and clears bit 2, without resetting their position.
- Existing static notes in `docs/VFX-RENDERING.md` identify the world-update
  gate in `0x433260`; `docs/CAMERA.md` identifies the action gate in `0x430c70`.

This establishes original freeze, timer and retained-audio responsibilities.
The Xbox Start mapping, blocking-provider structure and resume release gates
are explicit port policy, not claims of a recovered retail menu or scheduler.

## Changed files and remaining verification

Input/audio implementation lives only in `src/platform/xbox/input.c/.h` and
`src/platform/xbox/audio.c/.h`; main-loop integration is parent-owned in
`src/platform/xbox/main.c`. No external backend, original input, shared scene,
save format, native renderer, build recipe or test fixture changed for this
slice. The adapter adds only fixed scalar/diagnostic state and no PCM, heap or
GPU allocation.

The scheduled batch may establish compilation. Runtime pause, audio cursor
continuity, controller reconnect and resume feel must remain unverified until
observed through an authorized native session. No synthetic/forced input or
per-slice fixture is supplied as a substitute.

## 01:00 UTC compilation status

The October10 Xbox build and original L1S1 neutral120-frame stock64MiB startup
passed at7cdfd7a4. This establishes compilation and table admission only;
action-specific gameplay, physical controller pause, audio output and save
restoration remain unverified. See HOURLY-20261010-0100.md.
