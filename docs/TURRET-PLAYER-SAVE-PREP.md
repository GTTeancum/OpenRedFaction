# Stationary turret player save integration handoff

Status: integrated into ordinary snapshot/load; NXDK build and stock64MiB save/fresh-load continuation passed. This document records the integration contract.
Helper: `src/diagnostic/scene_turret_player_checkpoint.inc`.

## Scope and wire format

RFPL continues to store the actual standing player body; mounting does not relocate it.
RFTU1 already stores turret health, pose, angular state and actual aim.
Optional RFTP1 adds authored host UID/class, desired aim, anchor, body-eye offset,
remaining player fire cadence, burst and shared enemy spread RNG. No handles are serialized.
The header is16 bytes: magic RFTP, version1, inner byte count, row bytes64.
The64-byte row follows the inner payload:
UID0, class4, remaining8, burst12, RNG16, desired20, anchor32, eye offset44, zero56/60.
Wrapper order is RFCL → RFTP → existing RFTU3/2/1 → RFNS1/2 → RFVA/vehicle.
Unmounted capture passes through the inner bytes unchanged.

## Exact parent hooks

1. Before `scene_player_checkpoint.inc`, declare:
   ```c
   static void scene_turret_player_checkpoint_scope(uint32_t);
   static void scene_turret_player_checkpoint_reset(void);
   static uint32_t scene_turret_player_checkpoint_player_allowed(void);
   static uint32_t scene_turret_player_checkpoint_host_allowed(uint32_t);
   ```
   Include the full helper after `scene_world_player_restore.inc`, before snapshot/load.

2. In ordinary `scene_world_snapshot_capture_mode`, replace the blanket mounted
   rejection with `scene_turret_player_checkpoint_scope(1)` before player capture.
   Always call `scene_turret_player_checkpoint_scope(0)` at the common `done` label.
   Keep standalone RFCP capture outside this scope.

3. In `scene_checkpoint_player_state_scope`, reject an active turret mount unless
   `modes==2 && scene_turret_player_checkpoint_player_allowed()`.
   In the later compound guard, replace only `campaign_player_view.linked_handle!=-1`
   with `linked_handle!=-1 && !(modes==2 && scene_turret_player_checkpoint_player_allowed())`.
   Use the full `campaign_player_view.linked_handle` expression in code.
   Preserve all other standing-body, floor, support, weapon and look-state guards.

4. In `scene_turret_checkpoint_capture_row`, permit its occupant-count exception when
   either existing NPC-seat proof or `scene_turret_player_checkpoint_host_allowed(handle)`
   succeeds. Preserve burn and host parent-link checks. This scope admits only the
   exact live mounted host; ordinary standalone RFTU capture remains guarded.

5. At the end of vehicle wrapping in snapshot, after generated/retirement RFTU
   capture and before RFCL capture, call:
   ```c
   status=scene_turret_player_checkpoint_capture(buffer+at,
       RF_CHECKPOINT_FILE_MAX-at,bytes,combat_frame,&wrapped);
   if(!status)bytes=wrapped;
   ```
   Propagate failure before the RF_WORLD_VEHICLE component is committed.

6. In `scene_world_load`, declare `scene_turret_player_checkpoint_stage *turret_player=NULL`.
   After RFCL peeling and before RFTU peeling, check `cost<=budget`, then call:
   ```c
   status=scene_turret_player_checkpoint_prepare(vehicle_data,vehicle_bytes,
       budget-cost,&turret_player,&vehicle_data,&vehicle_bytes);
   if(status)goto done;
   if(turret_player)cost+=turret_player->allocated_bytes;
   ```
   Retain the unwrapped slice; do not reload it from the original envelope.

7. After ordinary standing-player/world staging and turret staging, before any
   transaction publication, call
   `scene_turret_player_checkpoint_admit(turret_player,turrets,&player,0)`.
   Frame0 matches the existing fresh-load combat clock. Preserve the current
   rejection of loads initiated while already mounted.

8. After RFTU/RFNS and ordinary player state publication, call
   `scene_turret_player_checkpoint_assign(turret_player)` before ordinary ticks.
   It installs preadmitted ownership pointers, player link and RNG only; no Use,
   sound, firing, body relocation or allocation occurs. At `done`, always call
   `scene_turret_player_checkpoint_close(&turret_player)`.

9. Fresh setup calls `scene_turret_player_checkpoint_reset()`.
   Probe `rf_scene_turret_player_checkpoint[20]` begins with capture count, restore count,
   UID, host handle, player handle, rebased due frame, burst and RNG. Words8–19 retain
   immutable restored desired aim, anchor, eye offset and actual turret aim (three floats each).

## Admission limits

Capture requires released mount Use/fire/alternate controls, zero look input and a
generation-valid living authored use4 Vauss mount. Desired aim and pending fire
cadence are preserved separately from actual RFTU aim. Saved NPC combat RNG must agree.
Restore keeps ordinary player world/floor/pair placement and requires the saved
player within the pending turret's use radius. Vehicle co-occupancy is rejected.
Initially NPC-operated turrets with a fresh operator/occupant binding remain rejected,
as do generated heads and loading while already mounted. No general overlap relaxation.
Remaining work: held-input capture, loads initiated while mounted and broader authored mounts; alternate turret weapons and seated presentation remain separate gameplay work.


## Source-save evidence

`artifacts/xemu/turret-player-save-20261003-142045` wrote an ordinary17,228-byte save on stock64MiB after seven mounted shots. RFTP retains4 frames of pending fire delay, RNG3250303071, desired/actual turret aim and the original body-eye offset/boarding anchor. The initial harness incorrectly required seated RFPL3; this mount retains a standing body, and the actual RFPL1 record is correct. The validator now accepts supported standing RFPL1/2/4 with the standing flag. The original failed report is retained. `--resume-saved-run` reuses the untouched source save and private HDD and runs only fresh-load continuation. Native load PASS: `resume-20261003-142423/report.json` runs100 frames from this exact source save. It restores desired/actual aim, anchor, eye offset,4-frame deadline, burst and RNG bit-exactly; actual player/host links survive, 5 mounted shots fire without a new entry, and ordinary Use releases control. Source and load stay at stock64MiB; load free pages3183. Source, load and restoration builds pass, disc flags restore. No visual/audio, target damage, held-input save or in-session reload claim.
