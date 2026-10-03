# Scripted vehicle physics ordinary saves

Integrated `scene_vehicle_physics_checkpoint_decl.inc` and `scene_vehicle_physics_checkpoint.inc` into ordinary world capture/load. The helper changes no existing RFVC, RFVR, RFVA or RFNS format.

RFPV1 is inserted outside RFVA and inside RFNS. Header16: magicRFPV, version1, inner byte count, row count. Unchanged inner vehicle bytes follow, then72-byte rows: UID0, role4 (active0/passive1), scripted marker8, bodyflags12, pending safe exit16, object7c bits06000000 at20, entity814 bit800 at24, skip_forces28, velocityXYZ32, forceXYZ44, torqueXYZ56, reserved zero68. Active row precedes authored-order passive rows. No handles/pointers are serialized. Active UID0 remains the existing diagnostic profile identity only; authored hosts retain their real UIDs. RFWC supplies source identity and checksum.

Emit only if any vehicle has a scripted marker, pending exit or814800. Untouched saves remain byte-compatible. An immune ON can leave marker0 but still814800, which therefore also triggers emission. Once emitted, all current vehicle owners have rows so role/count/order admission is exact.

Exact parent wiring:

1. Include declarations before world snapshot/load. Include full helper after vehicle runtime and existing passive codec. All signatures are in the declaration file.
2. In `scene_world_snapshot_capture_mode`, after `scene_passive_vehicle_checkpoint_capture`, before RFNS capture: call `scene_vehicle_physics_checkpoint_capture(s,buffer+at,RF_CHECKPOINT_FILE_MAX-at,bytes,&wrapped)` and replace bytes on success.
3. In `scene_world_load`, declare `scene_vehicle_physics_checkpoint_stage *vehicle_physics=NULL`. After RFNS prepare and cost accounting, before RFVA test, call `scene_vehicle_physics_checkpoint_prepare(stream,vehicle_data,vehicle_bytes,budget-cost,&vehicle_physics,&vehicle_data,&vehicle_bytes)` and add `scene_vehicle_physics_checkpoint_bytes(vehicle_physics)` once. Check cost before subtraction.
4. After RFVC/RFVR/RFVA staging, and before any owner publication, call `scene_vehicle_physics_checkpoint_admit(stream,vehicle_physics,vehicle_present?&vehicle:NULL,passive)`. This verifies current generation identities, active saved velocity and pending occupancy, and exact passive staged flag mirrors. Existing full placement and pair checks remain unchanged; they already admit no-floor parked vehicles without bypassing geometry.
5. Call `scene_vehicle_physics_checkpoint_assign(stream,vehicle_physics)` after FINAL parked/occupied/gunner host publication, following the late occupied `scene_vehicle_checkpoint_publish` branch. Calling before that branch loses814/force/torque to the host publisher's reset. Passive assignment must also precede this helper because RFVA zeros carry velocity. The helper updates damage/view814 together. It does not run events, seat logic, exit queries or resource callbacks; a pending exit resumes on the next ordinary runtime tick.
6. Every cleanup path calls `scene_vehicle_physics_checkpoint_close(&vehicle_physics)`. Opaque byte accessor accounts stage+row allocation only; existing world buffer already owns encoded bytes.
7. Keep full legacy RFCP rejection: its vehicle bytes cannot carry this wrapper. Ordinary RFWC needs only the remaining NPC scripted-freeze rejection after RFPV is wired. No save scope relaxation for unsupported NPC state.

Legacy payloads pass through unchanged with NULL stage. Final assignment clears the new active/passive markers and pending exit, preserving historical bodyflag semantics and RFVA's own saved814. This prevents stale in-session freeze state leaking into an older save. Bodyfreeze is marker plus absence of80000000, never814800: the latter intentionally remains after OFF.

Active force/torque/skip_forces survive freeze and are restored after RFVC reconstructs velocity/momentum. Active velocity must match RFVC exactly. Passive rows restore instantaneous carry velocity after RFVA; ordinary group motion can update it next tick, since physics freeze does not pause parent movement. Pending exit requires saved player occupancy and814800. Passive rows forbid pending exit and unused active-force fields. Malformed fields, duplicate UIDs, unknown row roles, incorrect lengths, budgets and staged-flag disagreement reject before publication.

Telemetry `rf_scene_vehicle_physics_checkpoint[8]`: captures,rows,assigns,active marker,bodyflags,pending,passive frozen count,status. Immutable final assignment `rf_scene_vehicle_physics_restore[24]`: valid0,active UID1,marker2,bodyflags3,pending4,full7c5,full8146,skipforces7,velocity8..10,force11..13,torque14..16,passive count17,first passive UID18/marker19/bodyflags20/velocity21..23. Compare this live publication with the saved row; float entries contain IEEE bits. No runtime proof is claimed until the parent completes the bounded native save/load check.

Correction (2026-10-03): Original42d780 tests class724 bit1000, named `driller` in the original594598 flag table. The automatic local-player exit and814800 side effect are Driller-only; a Jeep freezes without either side effect. Frozen occupied vehicles retain safe voluntary exit, while boarding, driving, seat changes and new fire remain gated.


The earlier NPC/vehicle rejection guard covered only the legacy RFCP capture path. Ordinary RFWC now explicitly rejects unsupported frozen NPC state; vehicle state is preserved by RFPV rather than rejected. Legacy RFCP retains its full guard.

Validation history: source `vehicle-physics-20261003-161936` froze a moving Jeep and wrote the ordinary save. Its initial assertion incorrectly expected Driller-only automatic exit. Corrected validation of the retained data confirms both occupants, exact frozen pose and zero motion. Fresh-load `vehicle-physics-20261003-162533` restored RFPV and occupants exactly, but its test-only delayed wake had been scheduled before world restoration and was correctly replaced by the saved event queue. The final harness requests wake after restoration; it reuses the original save. These failures are retained, not relabeled as successful full runs.

## Xbox result

`artifacts/xemu/vehicle-physics-20261003-162859/report.json`: PASS on stock64MiB. Reuses the original240-frame source save, followed by180 fresh-load frames. Both living NPC driver and player gunner remain seated; frozen chassis position and zero velocity match exactly before/after load, and RFPV fields match immutable live restoration. No boarding/freeze replay occurs. Ordinary Invert requested after load executes OFF at2000ms and the retained route moves the chassis 2.2319m without reissuing the route. Source/load free pages: 2864/2704. Disc configuration restoration passes; NXDK builds pass.

Driller automatic exit, blocked exit, immunity, passive/group carry, held controls and audiovisual presentation are not verified by this Jeep fixture. The voluntary-exit-while-frozen fix is built but not separately exercised. Legacy RFCP retains its freeze-state rejection, and NPC/prop physics persistence remains separate.
