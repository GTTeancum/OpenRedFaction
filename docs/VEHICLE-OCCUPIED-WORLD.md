# World motion while a vehicle is occupied

## Problem and implementation

The scene advanced and committed authored controllers only while the player was
on foot. Boarding a vehicle therefore stopped ordinary movers, attached chassis
and their control meshes/trigger volumes, although events and NPC script updates
continued afterward.

Controller advance/commit and unseated NPC side-push/support refresh now run once
in the common world step. Player force, roof/contact correction, support refresh
and walking physics retain their on-foot gate. On-foot ordering is unchanged:
force, controller advance, actor corrections/physics, controller commit, then
post-ground queries and later events/NPC motion. NPC side-push still consumes the
fresh pre/post-controller interval before events can relocate actors. Seated or
script-frozen NPCs retain their existing exclusion from free support refresh.

The earlier active-vehicle solver keeps its existing place in the frame. This
change does not reorder the entire physics system or add active-vehicle contact
response against newly swept moving geometry. The terminal defuse pause and
final presentation-only endpoint still suppress ordinary world stepping.

## Persistence and evidence

Existing controller, vehicle, NPC support and control-child save formats remain
unchanged. Restoration invalidates transient passive intervals; the next normal
controller update starts from restored poses and publishes a fresh interval.

Read-only `rf_scene_world_controller_steps[8]` records attempted common steps,
successful controller advances and commits, occupied/on-foot steps, last frame,
duplicate-frame count and controller status. Fresh binding resets the counters;
frame zero before a fresh load is still one normal eligible step.

## Stock 64 MiB cloud Xbox result

`tools/xemu_vehicle_occupied_movers.py` passes one bounded source/load sequence
in `artifacts/xemu/vehicle-occupied-movers-20261007-152108/report.json`:

- Source 180 frames: ordinary RFI6 Use boards Jeep 7629 at 90, exits at 120,
  and boards again at 150. All 179 eligible world steps advance and commit
  exactly once, including 59 occupied steps; no duplicate or controller error
  occurs. The independent Fighter 4717, its four control children and the
  supported living Eos 4716 keep moving throughout both transitions.
- The 4,480-byte ordinary save preserves occupied Jeep ownership, controller
  phase 2.216665, Fighter/NPC poses and exact health. Existing RFNC10, RFMC1,
  RFPC1, RFTC1, RFVA2/RFVC3 and RFEN6 formats are retained. The source snapshot
  comes from settled world step 178; active-vehicle endpoint state is captured
  independently after its normal final presentation update.
- Fresh load 80 frames: no reveal, controller start or roof-placement setup is
  replayed. Ordinary exit at 30 and boarding at 60 work. All 79 eligible steps
  advance and commit once, including 48 occupied steps, with stable full
  registry handles, exact seated links, living support ownership and vitals.
  The controller continues to phase 3.516664. Frame zero is the ordinary step
  before restore; frame one measures the first restored continuation tick.
- Every sampled NPC/Fighter horizontal displacement agrees exactly; vertical
  differences are at most 0.000000239 m. Accepted NPC support velocity agrees
  with the independently derived controller speed, and grounded mode persists.
  The first restored NPC and Fighter both advance by approximately
  `[0.029174805,-0.029166222,0]`. Clutter centers/bases, query/body publication,
  trigger volume, and Fighter pose agree with independent parent transforms.
- Source/load retain 5,529/5,369 free physical pages on 67,108,864 bytes of RAM.
  All original disc inputs are restored and hashed. The private proof archive
  retains each phase's exact prelaunch XBE, PE and map; both phases share PE
  SHA256 `fe8b1483dc12bef68af742cf4f7556d93150e4c303ad8d46a95cbeccdeef1f3d`.

The raw reports were independently reviewed. After launch, the read-only
validator was strengthened to require grounded mode, support velocity and
0.0005 m horizontal carry tolerance (below one movement tick), retaining
0.035 m vertical ground-fit tolerance. Both unchanged raw phases pass those
stronger checks, and their validation output exactly matches the original
report. `validation-review.json` records that revalidation; no gameplay source
or runtime inputs changed. Independent in-memory wrong-support, wrong-generation and missed-horizontal-
step cases are rejected, including the first restored carry.

NXDK builds and Python parsing pass. Compiler stack auditing retains the
128 KiB reserve and 45,212-byte listed scene subtotal; it does not measure
runtime stack high-water usage.

## Fixture scope

The test retains byte-exact L20S2 geometry and original Fighter, control and
friendly/unarmed Eos records, using the prior explicit six-second translation
fixture. A separate original Jeep is placed on independently decoded room 10,
floor face 78; its six body spheres fit a clear above-floor envelope. Only that
Jeep transform and player start are newly staged. Existing roof mode 9 places
the NPC at frame 40, and one explicit ordinary contact/Use callback starts the
controller at 45. Vehicle boarding and exit use the normal replay input path.

This proves the bounded common scheduling and ownership continuation. Natural
rider landing/trigger approach, crush, other mover classes, a global solver
reorder and audiovisual fidelity remain outside the result. No host input,
images, PC runtime or campaign walkthrough is used.
