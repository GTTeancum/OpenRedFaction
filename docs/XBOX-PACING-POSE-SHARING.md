# Xbox frame pacing and shared NPC poses

## Changes

The live campaign pipeline presents before its physics/event tail. Its old
pacing check measured only work since the current input poll, so it missed the
expensive tail of the previous tick. Once behind, it repeatedly skipped eight
images out of nine even when skipping could not restore 60 Hz simulation.
`rf_frame_clock_present` now also checks the time since the last actual draw:
at 33 ms it presents regardless of catch-up debt. This is a responsiveness policy,
not a 30 FPS cap or a variable physics step. Fast frames still present at 60 Hz;
short stalls can still catch up, and long stalls retain the eight-tick debt cap.
The policy is shared by the PC and Xbox interactive builds; deterministic replays
do not use wall-clock pacing.

NPC animation now shares fully evaluated skeleton matrices when actors have
identical skeletons, active motion slots/cursors/weights, and pending root
translation within one simulation tick. It updates each actor's playback first,
preserving references, markers, controller behavior and script state. Cache hits
copy matrices and publish that actor's current generation. Collision preparation
and eye/world placement remain owned by each actor.

The batch has 16 entries and takes 42,076 bytes on the 32-bit PC/NXDK builds. It
resets before every NPC tick, so it cannot survive a level transition or reuse an
old motion allocation. Enabled overrides, partially current generation caches,
different owners and unsupported bounds use the original evaluator. Cache misses
use that evaluator too; they do not approximate key interpolation or alter poses.
No per-frame heap allocation is added. `rf_scene_pose_sharing[4]` reports the last
tick's hits, misses, bypasses and fixed storage bytes. The cache and immutable
skeleton/catalog payloads have a single simulation-thread owner.

## Verification

- PC/NXDK builds pass. Existing compiler warnings remain.
- 160 synthetic scalar/shared comparisons cover full matrices, generations,
  per-actor playback/events, reference counts, root displacement including signed
  zero, distinct ticks, eviction, disabled/enabled overrides, frozen partial/full
  caches, empty slots and generation wrap. 76 hits,78 misses,6 bypasses.
- The existing owned-pose and motion-residency tests pass.
- Frame-clock tests cover the actual post-presentation physics ordering, long
  stalls, fast catch-up and millisecond wrap. 8,000 compiled NXDK oracle cases pass.
- The 180-frame PC L1S1 replay at actor9858 has byte-identical reported state/pose
  diagnostics and final framebuffer with sharing enabled/disabled. Last tick:
  33 hits,5 misses,0 bypasses. This is correctness evidence, not a PC FPS claim.

Reproduction from the project root:

```powershell
cmake --build build/pc --config Release
ctest --test-dir build/pc -C Release --output-on-failure -R 'frame_clock_pacing|shared_pose_evaluation|owned_model_pose|npc_motion_residency'
python tools/verify_pose_sharing.py
python tools/verify_frame_clock.py
python tools/xemu_play.py
python tools/xemu_performance_watch.py artifacts/xemu/play-YYYYMMDD-HHMMSS/live.json --seconds 45
```

The launcher builds
an unlimited controller-driven L1S1 campaign ISO, copies it into the ignored run
directory, restores the normal disc staging, and starts a visible stock 64 MiB
XEMU process. It neither sends input nor closes the emulator. Its EEPROM/config
and snapshot HDD writes are isolated; subsequent builds cannot mutate its ISO.
The explicit controller GUID matches the user's observed XInput device and may
need updating on another machine. The performance watcher only reads guest RAM
and QMP status; it does not pause, capture, or close the session.

## Native evidence and limits

The initial bounded retained-GPU check was `render-20260914-142844`:
180 staged-camera frames, selected PC gameplay checks passed, 22.70 MiB available,
all106 queued retained model batches submitted (48 skeletal,58 rigid), no
retained fallback. USER observed about 23 FPS. Native means included 0.915 ms world
rebuilding and 11.350 ms NPC pose evaluation. Its image shows the mine/miner and
campaign caption, but is not a full visual/parity inspection. That very short
run is not comparable to historical L1S2 fixture timings.

The unlimited before run was `play-20260914-143443`, without replay or a frame
limit. A 30.015-second guest-counter interval measured 4.531 presentations/sec
versus 40.813 simulation ticks/sec, exposing the eight-skipped-draw pattern.
That window averaged 11.067 ms in NPC pose evaluation.

The new unlimited run is `play-20260914-144559`. After loading, a 45.016-second
window measured 27.612 presented FPS and 56.291 simulation ticks/sec. It had
22.10 MiB available on the verified 64 MiB guest. Last sampled NPC tick: 32 shared
poses, 4 evaluations, 0 bypasses. Its pose phase averaged 1.687 ms and the complete
physics/event phase 6.655 ms. The session remains open with controller input.

| Native live measurement | Before | After |
|---|---:|---:|
| Actual presentations/sec |4.531|27.612|
| Simulation ticks/sec |40.813|56.291|
| NPC pose evaluation |11.067 ms|1.687 ms|

The before and after values use explicit 30-second and 45-second windows. Both
use the same L1S1 manual campaign spawn configuration; the complete sampled
player body state matches between runs. These are separate from the earlier
staged 180-frame replay. 
Files remain under ignored `artifacts/xemu/`; no game assets
or additional screenshots are uploaded to GitHub.

These live reads are non-atomic and include host scheduling/user-input effects.
Profile phase means exclude loading and must not be added across nested groups.
The remaining checks are close-up poses/clipping/UV/depth behavior, combat,
first-person GPU adaptation, busy rooms and level-handoff allocation retirement.
This does not establish full campaign parity or stock-hardware performance.
