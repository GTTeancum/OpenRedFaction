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


## Recorded input and capacity-failure diagnosis

The pacing harness records the 64-slot input ring at each observation. Extract a
contiguous observed prefix and replay it entirely inside the PC process:

```powershell
python tools/extract_player_replay.py artifacts/xemu/<run>/report.json artifacts/inputs.bin
./build/pc/Release/rf_pc_play.exe --replay Installed_Game artifacts/inputs.bin artifacts/replay.ppm
python tools/verify_player_replay.py
```

The binary contains one 24-byte little-endian record per tick: five floats
(move X/Y/Z, look pitch/yaw), then a uint32 crouch value. Replay accepts 1..60000
records, projects every tick, and rasterizes only the final tick. The existing
`--headless` route still rasterizes every frame. Neither mode opens a window or
injects host input. Extraction rejects gaps and conflicting observations, excludes
the potentially in-flight tail, and can add a stable failure snapshot's final
records. Live RAM reads remain non-atomic; this is an observed command history,
not a raw controller-device trace. Wrap, conflict and gap checks were exercised.
Run `pacing-20260909-211857` yielded 413 contiguous observed records, successfully
replayed by PC. That proves the acquisition/replay plumbing, not pose parity with
a simultaneously moving guest.

New failure evidence:

- `rf_animation_progress`: frame, operation stage, final signed status as uint32,
  render batch. Stages: 0 setup, 1 input, 2 controller/motion, 3 skeleton/cache,
  4 physics-body initialization, 5 view/projection, 6 vertex checks, 7 model draw,
  8 scene/presentation/physics sink, 9 completed frame.
- `rf_scene_profile_stage` now advances even when timing is disabled.
- `rf_preview_failure`: valid flag, face, fan corner, vertices used, vertex
  capacity, source face count, writing-pass flag, additional vertices required.
  This records capacity exhaustion; a zero valid flag does not diagnose other
  range-error causes. All three are included in failure snapshots and PC stderr.

A stationary full-yaw command reproduced RF_RANGE at tick 60; a diagonal turn
reproduced it at tick 250. Both were world-projection capacity failures at the
1 MiB staging-half limit. The first failure needed three more vertices after
18,723 of the 18,724 available slots. First-person mode has no visible actor, so
it can use the full existing 2 MiB allocation. It now falls back to transactional
two-pass projection when staging is too small, and omits the hidden actor copy
that would otherwise overwrite the expanded world. Ordinary views retain the
single-pass staging path. Neither CPU nor GPU allocation caps were increased.

Both former failures now complete 480 PC ticks. Peak world sizes are 1,966,440
bytes for yaw and 1,914,864 for the diagonal turn; upward-look and forward
movement cases also pass. The original 664-tick reference trace and final image
remain exact. Larger views can still fail at the genuine 2 MiB limit; visibility
and resource budgeting remain open. The historical tick-99 failure had no input
log, so it cannot be positively attributed to this reproduced cause. The wide
view sweeps were initially validated on PC; subsequent Xbox coverage is below.


Ordinary 664-tick turn regression `20260909-212051-203770` passes in stock 64 MiB
with PC-matching actor rings and final body state. It does not cover the newly
expanded wide-view range. The ongoing controller ISO was restored afterward.


## Xbox recorded-input replay

When `player-control.flag` is present, an optional `player-replay.bin` selects
recorded input instead of SDL controller polling. It uses the same 24-byte record
format as PC. File length must be a nonzero multiple of 24, at most 60,000 records.
The record count sets the finite run length; replay is unpaced and submits every
frame. Records stream from the file through stdio rather than allocating the
whole recording in Xbox RAM. Shared validation still checks each command.
Cleanup closes the file; absence of the file restores normal controller mode.

`rf_player_replay_diagnostic` records active, total records, consumed records, and
file-read status. Status describes the reader, not the complete game pipeline.
The QMP snapshot includes this alongside scene, animation and capacity failures.

Run the automated comparison from the project root:

```powershell
python tools/xemu_replay_check.py artifacts/input-replay/yaw-sweep.bin --require-wide
python tools/xemu_replay_check.py artifacts/input-replay/diagonal-turn.bin --require-wide
```

`tools/verify_player_replay.py` generates these fixtures. The Xbox check runs a PC
reference, temporarily packages the replay file, starts an isolated stock-64-MiB
XEMU, and compares the completed world/camera summary, 64-slot input history and
all 308 final body-state bytes. It also checks every frame was submitted and GPU
vertex capacity stayed at 2 MiB. `--require-wide` requires an observed completed
GPU submission above 1 MiB, as well as a world-summary peak above that threshold.
No OS input or framebuffer capture is used. The test restores the previous replay
file (or removes its temporary file) and rebuilds the normal ISO even on failure.
It uses the separate HDD base prepared by the pacing harness.

Yaw run `replay-20260909-212700` passes all 480 ticks with matching PC state and
hashes. World peak is 1,966,440 bytes; the largest sampled completed GPU submission
is 1,963,920 bytes. These observations explicitly exercise the expanded path on
Xbox, without claiming framebuffer/PS2 visual parity or hardware performance.


Diagonal-turn run `replay-20260909-212849` also passes all 480 ticks. World and
sampled completed GPU peaks both reach 1,914,864 bytes, with exact PC world/camera
summary, input ring and final body matches. Both formerly failing trajectories
therefore complete on the compiled Xbox target within stock 64 MiB and the same
2 MiB vertex allocation. This addresses the reproduced staging-capacity exits;
it does not reconstruct the unavailable input history of the original tick-99 exit.


After restoring the ISO without a replay file, normal controller/pacing run
`pacing-20260909-213120` passes. No additional manual controller claim is made;
this verifies the non-replay startup and continued runtime path.

For changed diagnostic disc flags, use tools/build-xbox.sh --repack. Do not
use make -W default.xbe: it can suppress XBE regeneration after an EXE rebuild
and leave emulator code inconsistent with the current symbol map. --repack
removes only the generated ISO and runs normal build dependencies.
