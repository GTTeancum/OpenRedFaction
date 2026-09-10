# Player input prototype

The Xbox build now polls SDL game controllers into the same shared scene input
interface used by PC tests. This is new port input policy, not a reconstruction
of the original PC keyboard/mouse mapping. The user confirmed movement on
September 9, 2026. Guest RAM also records non-neutral movement and look axes.

Controls: left stick moves, right stick looks, B holds crouch, Back+Start ends
the session. Sticks use an 18% radial deadzone with a unit-length diagonal cap;
disconnect produces neutral input and polling can reconnect a controller.
Look currently uses one radian/second at full input. Simulation remains fixed
at 1/60 second per produced frame; real-time pacing needs further work. There
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

The frontend caps frame production at 60 Hz. It still advances simulation by
1/60 second per rendered frame; software rendering below 60 FPS slows simulation.
Real-time catch-up and render/simulation scheduling remain open for both platforms.

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
