# Player input prototype

The Xbox build now polls SDL game controllers into the same shared scene input
interface used by PC tests. This is new port input policy, not a reconstruction
of the original PC keyboard/mouse mapping. The user confirmed movement on
September 9, 2026. Guest RAM also records non-neutral movement and look axes.

Controls: left stick moves, right stick looks, B holds crouch, Back+Start ends
the session. Sticks use an 18% radial deadzone with a unit-length diagonal cap;
disconnect produces neutral input and polling can reconnect a controller.
Look currently uses one radian/second at full input. Simulation remains fixed
at 1/60 second per simulation tick; interactive sessions now use shared pacing. There
is no weapon, jumping, combat, or campaign scripting yet.

The shared provider polls once before stance and animation. Its validated
movement/look/crouch state is reused by physics and camera; it replaces the
hardcoded command sequence. RF_NOT_FOUND from the provider stops cleanly before
a new frame, releasing stream resources. The scene retains bounded 64-frame
input/pose/physics rings; a zero frame limit runs up to UINT32_MAX frames.
The old frame-47 scripted clearance experiment is skipped for provider sessions,
because it assumes a crouched actor. Ordinary crouch/stand clearance still runs.

Xbox disc switches (in build/xbox/disc):

- `player-control.flag` enables controller input and first-person body streaming.
- `player-control-frames.txt` contains an optional unsigned frame limit. Zero or
  an absent file selects the ongoing session; 664 is the bounded harness run.
- Existing scene-preview/model-preview disc switches remain required by the
  diagnostic entry path. Rebuild the ISO after changing its source disc files.

The installed NXDK SDL library provides the platform driver. Only the game
process reads its controller. No OS keyboard/mouse/controller input is generated,
and no desktop automation or capture is used.

Verification:

- PC `rf_scene_check ... --input` replays the prior turn commands through the
  provider and reproduces the complete recorded route. `--neutral` exercises
  no-input ownership, and `--input-stop` cleanly ends an unbounded stream after
  three frames. These are process-local test inputs.
- `python tools/verify_xbox_input_adapter.py` executes compiled NXDK adapter
  code with simulated SDL API responses: nine cases cover deadzone, extrema,
  diagonal cap, crouch, disconnect/reconnect, exit chord and closed-state guard.
  This does not test a physical USB device.
- Live XEMU run `20260909-202201-153191` reached 664 frames in stock 64 MiB.
  SDL init/poll/cleanup telemetry is `[0x5246494e,1,664,0,0,0]`. The user moved
  during the neutral-input check; 24 of the final 64 input records are nonzero.
  The report correctly remains FAIL with `Controller input ring is not PC-neutral`.
  Subsequent PC-neutral pose comparisons were not executed. This run is evidence
  of input acquisition and user-confirmed movement, not a passing neutral replay.

Physical crouch/reconnect/exit behavior remains to be checked by the user.
Original player spawning/identity, timer ownership, camera collision and full
campaign entity scheduling also remain open.


## Windows PC frontend

From the repository root after a Release build:

```powershell
./build/pc/Release/rf_pc_play.exe Installed_Game
```

WASD moves, arrow keys look, Ctrl holds crouch, and Escape or closing the window
ends the session. Movement diagonals are normalized. Only the frontend's own
window messages supply keyboard state; losing focus clears held keys. No global
input polling, cursor capture, host input generation, or desktop automation is used.
This is port-owned input policy. It starts at the same diagnostic miner placement
as the Xbox input prototype, rather than the original campaign player spawn.

The shared software rasterizer renders at 640x480, with a 2 MiB combined mesh
cap. Resizing the window scales and letterboxes that buffer. Depth/RGB buffers
use 2,150,400 bytes; the Windows presentation buffer adds 1,228,800 bytes. Geometry,
materials and physics use the existing shared budgets. These allocation figures
are not a measurement of total process memory or full campaign residency.

Interactive PC and unbounded Xbox controller sessions use the same integer
60 Hz scheduler. Each simulation tick still advances by 1/60 second. When behind,
the runtime skips presentation while retaining physics, animation and scene
projection; subsequent ticks catch up using the current input state. It retains
at most eight ticks of elapsed-time debt and forces a presentation after eight
consecutive skips. If the current simulation/scene tick itself takes at least
17 ms, it presents immediately instead of making an overloaded runtime less
responsive. Excess time after long stalls is deliberately discarded.
This avoids unbounded catch-up but does not promise full speed if simulation itself
cannot sustain 60 ticks/second. Input is not historically replayed during catch-up.
This is new port scheduling policy, not the original game's recovered main loop.

Finite Xbox controller fixtures and PC headless replay remain unpaced so existing
frame-by-frame reference checks still exercise every image. On paced Xbox sessions,
`rf_diagnostic[37]` counts actual GPU submissions, not simulation ticks. The exported
32-byte `rf_player_frame_clock` records initialized, last milliseconds, tick credit,
consecutive skips, simulation ticks, presentation decisions, and a 64-bit discarded
unit count. PC obtains milliseconds from QPC; Xbox uses GetTickCount.
There are 60 units per millisecond and 1,000 per simulation tick.
QMP snapshots now include this structure. Reads during execution are non-atomic.

Non-interactive verification uses the same executable without creating a window:

```powershell
./build/pc/Release/rf_pc_play.exe --headless Installed_Game 664 artifacts/pc-play-replay.ppm
```

This replays the `rf_scene_check --input` commands and rasterizes all 664 frames.
The verified run matches the reference's 77-word final body state, 448-word input
ring and five-word camera/world summary exactly. Its final 640x480 image matches
`rf_pc_preview --scene-turn-last` byte-for-byte. Extracting the shared rasterizer
also preserves both 640x480 and 1920x1440 showcase images byte-for-byte. The full
PC build and all four CTest checks pass. Actual keyboard interaction, focus-loss
behavior, resizing and window presentation remain for manual testing; headless
verification does not exercise the Windows message or display path.


Pacing checks: `frame_clock_pacing` in CTest covers exact 60 Hz accumulation,
32-bit millisecond wrap, a 100 ms render stall, a long-pause debt cap and forced
presentation under overload. `python tools/verify_frame_clock.py` checks 8,000
irregular clock samples against the compiled NXDK functions, including the whole
clock state. `python tools/xemu_pacing_check.py` starts an isolated 64 MiB emulator
with unbounded controller mode, observes guest pacing counters and closes only
its own process. It neither captures a screen nor generates input. This live
check tests pacing progress, not original-game fidelity or campaign performance.


Initial live pacing run `pacing-20260909-204509` demonstrated only about nine
simulation ticks per guest second and persistent discarded debt. Skipping draws
was insufficient, so the scheduler was amended to present when simulation/scene
work itself exceeds 17 ms. This is evidence of a performance deficit, not a
60 Hz gameplay result. Removing unnecessary per-tick work remains required.
The harness uses XEMU's normal display backend with a hidden startup request;
no-display runs did not reach game telemetry on this machine.


With the overload rule, `pacing-20260909-204812` passed the stricter live check:
167 simulation steps, 166 presentation decisions, continued progress at the end,
and no error status over the 20-second observation. The observed rate was about
8.2 ticks per guest second, still far below target. Earlier run
`pacing-20260909-204643` stopped at tick 99 with status `0x80000104` (RF_RANGE).
Its report was corrected to FAIL after auditing the initially permissive progress
check. That error did not recur in the next run; its cause remains unresolved.
Future harness failures save the detailed guest-memory snapshot automatically.
