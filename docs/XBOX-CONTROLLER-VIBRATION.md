# Xbox controller vibration

Source implementation, 2026-10-12. Uncompiled and untested by this task.
The parent owns the hourly stock64 build/startup smoke; testers own functional
coverage. No controller output, runtime, synthetic input or capture was run.

## Source basis and bridge

`rf_scene_player_feedback` already validates the local player and delegates
to `rf_camera_effect_start`. That owner validates finite strength/duration
and replaces camera feedback. Existing scene source identifies the local
camera behavior with `40e0b0` and authored shake scaling at `4bb66f..4bb686`.
Damage, explosion, weapon/contact and authored-event callers remain unchanged.

The optional `rf_scene_set_player_feedback_sink(sink, context)` registers a
void callback `(void *, float strength, float duration_seconds)`. It observes
only successful local-player feedback, after the original camera operation.
It does not alter camera state, damage results, RNG or callback error returns.
Replacing/disabling the sink first sends zero/zero to cancel old output;
NULL clears its context. The sink must not reenter scene/damage operations.
Scene resource teardown also sends zero/zero before resource retirement.

Xbox main binds `rf_xbox_input_player_feedback` only for paced live gameplay.
Replays and bounded diagnostic launches do not bind it. The common
`player_input_close` detaches it before controller teardown. Full-session
exit admission also rejects a still-registered sink/context. Other platforms
can leave the sink unset without changing their behavior.

## Native API evidence

The installed toolchain was read at `C:/nxdk/lib/sdl/SDL2`:

- `include/SDL_gamecontroller.h:374` declares `SDL_GameControllerRumble`
  with two 16-bit motor amplitudes and duration in milliseconds.
- `src/joystick/SDL_gamecontroller.c` forwards to `SDL_JoystickRumble`.
- `src/joystick/xbox/SDL_xboxjoystick.c:356` implements the two motors through
  `usbh_xid_rumble`, stores expiry, and reports failed output.
- Its update path at line 391 expires output; its close path at line 466
  sends zero. The app also uses `SDL_TICKS_PASSED` for wrap-safe deadlines.

These are existing SDK APIs, not newly copied driver code. No third-party
source or license notice is replaced.

## Bounded output and lifecycle

`input_feedback.inc` belongs to the existing input controller owner. It keeps
one pending request; the latest accepted feedback replaces it. Strength is
clamped to 0..1, mapped to the low motor's 0..65535 range, and the high motor
receives half that value. Positive duration becomes 1..250 ms. These motor
ratios and caps are first-pass port policy, not recovered retail tuning.

Pending requests carry an absolute SDL deadline. Time waiting for the next
input poll consumes the pulse; an already-expired request is discarded.
The ordinary poll submits at most one positive command, only after pause,
disconnect and exit checks. There is no effect queue, per-frame heap
allocation, background thread or extra controller handle. Added application
state is 28 bytes of input fields plus two scene pointers (eight bytes on
Xbox), before any compiler padding.

Zero/nonpositive or nonfinite requests cancel immediately. Pause entry and
pause edges, frontend entry, save/load chords, Back+Start, disconnect,
controller replacement, scene teardown, sink detach and input close all
discard pending requests and request zero output where a connected owner
exists. A replacement controller never inherits an old request. Resume does
not resurrect interrupted feedback.

Unsupported/failed output never fails gameplay. The current controller is
blocked from further positive commands until reopen/replacement. Failed zero
requests get at most three app attempts; SDL close retains its separate
zeroing path. Physical hardware failure is not proof that motors stopped.
Duration expiry is serviced by the existing input/SDL pump, so a main-thread
stall can delay physical expiry; there is no independent watchdog thread.
Explicit lifecycle cancellations occur before their normal blocking teardown.

## Parent composition

Base: published `6d1bf09b54625cade292445139cd9da0779634d4`. No unpublished
parent snapshots, menu/settings formats or remapping fields are included.
Apply the narrow hunks, preserving the parent's newer input/options code.

Shared `scene.c` hunks are confined to these original-base anchors:

- Around line 2390, `rf_scene_player_feedback`: registration and post-success
  notification (no changes to individual damage/AI callers).
- Around line 6470, `campaign_close_movers`: one cancellation call at entry.
- Around line 23139, `rf_scene_session_exit_status`: sink lifetime guard.

`main.c` has two inserted lines: detach in `player_input_close` (base line 68)
and live-only registration after `rf_scene_set_input` (base line 1163).
`input.c` adds its private include and lifecycle/poll hooks; `input.h` and
`scene_preview.h` add declarations only. Civilian flee, Tankbot/Snake combat,
audio settings, input mappings, saves and persistence are untouched.

Only source review and Git diff checks were performed. No build artifacts or
binary assets were generated; saved disks were not accessed. Cleanup remains
the parent's responsibility.
