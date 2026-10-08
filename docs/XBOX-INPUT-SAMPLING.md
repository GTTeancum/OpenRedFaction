# Bounded Xbox controller sampling

Source audit of the bundled NXDK SDL2 found avoidable repeated controller
work in `src/platform/xbox/input.c`. The game discarded every SDL event with
`while(SDL_PollEvent(...))`, then called `SDL_GameControllerUpdate`.

In this SDL version, `SDL_PollEvent` calls `SDL_WaitEventTimeout(event,0)`,
which calls `SDL_PumpEvents` on every invocation. Pumping invokes
`SDL_JoystickUpdate`, including Xbox controller report decoding and
`SDL_XBOX_JoystickDetect`→`usbh_pooling_hubs`. Even the final empty poll pumps;
each queued event therefore added another controller/hub update. The explicit
GameControllerUpdate added one more. This work scaled with event queue length.

The Xbox input owner now pumps once, handles the same connection/selection
logic, and flushes the ignored event queue. The bundled SDL_FlushEvents does
not pump. A newly opened controller receives one explicit update immediately,
because it was not open during the first pump. Ordinary connected frames have
one application-requested pump rather than queued-events-plus-two updates.
No third-party SDL source is changed or copied.

Source references in the bundled NXDK checkout:
- lib/sdl/SDL2/src/events/SDL_events.c: SDL_PollEvent,
  SDL_WaitEventTimeout, SDL_PumpEvents and SDL_FlushEvents
- lib/sdl/SDL2/src/joystick/SDL_gamecontroller.c: SDL_GameControllerUpdate
- lib/sdl/SDL2/src/joystick/xbox/SDL_xboxjoystick.c:
  SDL_XBOX_JoystickDetect and SDL_XBOX_JoystickUpdate

The mapping, radial deadzone, trigger threshold, chord handling, analog gain
and existing fixed60Hz physics remain unchanged. Input is still sampled after
the pacing wait and before the current frame's look/combat preparation.
The shared frame clock retains its existing bounded catch-up and presentation
fairness policy. This patch does not reorder the physics-after-presentation
pipeline or invent a faster aim sensitivity.

This is a source-established reduction of duplicate work, not a measured
latency/FPS result. No build, test, emulator, controller input or screenshots
were used. The parent owns the15:00 UTC consolidated Xbox pass; replay files
bypass this SDL controller path, so a replay-only result cannot measure it.
