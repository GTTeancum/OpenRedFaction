# Scripted vehicle physics freeze/wake

Integrated adapters: `src/diagnostic/scene_vehicle_physics_state.inc` and `scene_script_physics_state.inc`, with core event62 dispatch and NPC/rigid runtime gates. NXDK build passes. Vehicle runtime behavior is not yet independently exercised.

Evidence: Turn_Off_Physics ON4b9380 calls417e00 unless object7c has08000000. It changes bodyflags to `(old&67ffffff)|18000000`, zeroes velocity/angular/momentum only, then separately checks Driller class724bit1000, sets its814bit800 and forces local-player release. OFF4ba180 calls40a420: bodyflags OR80000000 and objectflags OR06000000. OFF does not clear814800. No confirmed persistent reboarding rule is inferred from that sticky flag.

Exact integration:

1. Add `uint32_t scripted_physics,scripted_exit_pending` to `scene_driller_runtime`; add `uint32_t scripted_physics,physics_body_flags` to `scene_passive_vehicle`. Zero initialization preserves existing ordinary behavior. Active physics already has `body_flags`.
2. Before the runtime include declare `static uint32_t scene_vehicle_physics_frozen(const scene_stream*);`, `static uint32_t scene_vehicle_physics_allows_control(const scene_stream*);` and `static int scene_vehicle_physics_exit_tick(scene_stream*,uint32_t*);`. Include the new helper immediately after `scene_driller_runtime.inc`.
3. Generic callback calls `scene_vehicle_physics_state(s,handle,enabled,&handled)` first; return its status when handled, otherwise try NPC/prop owners. enabled0 freezes, enabled1 wakes. Resolved active/passive handles require exact current registry identity. Call `scene_vehicle_physics_reset()` once during fresh scene setup.
4. Runtime ticks must call the forced-exit helper once while pending and publish its `changed` through the existing `scene_driller_player_publish` once. Preserve this result across damage/Use helpers that overwrite output flags. Skip ordinary Use in a forced-exit frame and deny boarding while frozen; retain voluntary safe exit for existing occupants. Deny Jeep seat switching while controls are disabled and consume its held cycle edge. A blocked exit retains the real session and retries; player death uses normal dead-owner release. Shared Jeep gunner release preserves its NPC driver.
5. Gate player control, route-command invocation (not only command.controlled), and submarine/Fighter script commands with `scene_vehicle_physics_allows_control(s)`. Skip the actual rigid/submersible solver when `scene_vehicle_physics_frozen(s)`, otherwise gravity would restart movement. Do not advance an autonomous waypoint cursor during freeze/pending exit. If forced exit is attempted before movement, suppress that frame's ordinary deferred APC exit so it cannot double-publish or reboard; alternatively perform forced exit after the accepted solver pose and before final publication.
6. Gate only new weapon input/launch permission: runtime Driller weapon fire, APC primary/secondary, submarine and Fighter launch predicates. Continue those weapon ticks so existing projectiles/effects finish. Do not return early from the overall campaign combat clock.
7. Passive group attachment updates continue. Freeze is not a mover stop: do not suppress parent-pose binding or repeatedly zero the parent's derived carry velocity. OFF preserves force/torque, pending exit and814800. Health, alive state, NPC seats and route identity are unchanged.
8. RFPV1 now preserves scripted vehicle physics in ordinary world saves; see VEHICLE-PHYSICS-SAVE.md. The legacy standalone RFCP path retains its rejection guard.

Telemetry: `rf_scene_vehicle_physics[16]` = requests,freeze requests,wake requests,immune freezes,exit requests,exits,blocked retries,errors,UID,handle,enabled,bodyflags,objectflags,flags814,frozen,pending. Latest request snapshot `rf_scene_vehicle_physics_apply[24]` = valid,UID,handle,enabled,event time(ms),bodyflags before/after,objectflags before/after,814 before/after,positionXYZ,velocity beforeXYZ,velocity afterXYZ,momentum afterXYZ,attached. Float entries are bit representations; snapshot is unchanged by later physics ticks.

The adapter projects rigid velocity/momentum onto the core primitive without touching force, torque or skip_forces. The primitive's immunity only skips body freeze; Driller814 and local-player release still occur for qualifying Drillers. Safe collision-based release replaces the original immediate release; no destination means a retained session and explicit retry, never a fabricated clear placement or host death.

## NPC first-pass integration

The callback resolves current registry generations, freezes ordinary NPC bodies using the core primitive, and prevents support refresh or scripted navigation from waking them immediately. OFF clears the scripted marker and resumes ordinary physics. Clutter receives the same core body operation; it has no separate runtime claim yet. Parent mover transforms, actor lifetime, health and animation remain separate from physics suspension.

Runtime gates cover Driller, APC, Jeep, submarine and Fighter control/new weapon emission while existing projectile ticks continue. Forced player release uses collision-safe exits and retains a blocked session for retry. Ordinary snapshots reject unsupported scripted NPC state. RFPV1 handles vehicle state; occupied exits and prop behavior remain follow-up work.

Focused core event/physics checks pass: exact masks, zeroed fields, preserved fields, immunity, linked order, delayed dispatch, stale handles and callback failures. Xbox evidence is recorded below after the bounded NPC fixture completes. No original-game playback or audiovisual verification is claimed.

## Stock64MiB Xbox result

`artifacts/xemu/scripted-physics-20261003-160544/report.json`: PASS,120 frames, 5765 free physical pages. Original L11S3 event10651 freezes eos10636 and miner10637 after ordinary UnHide in empty CTF06. At frame40 both remain at their staged y1 positions with zero linear/angular motion and unchanged health. Ordinary Invert at frame60 sends OFF: both resume gravity and settle lower, preserving horizontal position and health. Two freeze and two wake callbacks complete without missing owners or combat. Disc configuration restoration passes. Vehicle/prop runtime, saves and audiovisual behavior remain unverified.

Correction (2026-10-03): Original42d780 tests class724 bit1000, named `driller` in the original594598 flag table. The automatic local-player exit and814800 side effect are Driller-only; a Jeep freezes without either side effect. Frozen occupied vehicles retain safe voluntary exit, while boarding, driving, seat changes and new fire remain gated.
