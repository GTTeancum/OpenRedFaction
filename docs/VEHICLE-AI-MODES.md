# Vehicle AI modes

The authored L12S1 Set_AI_Mode9710 targets Jeep7629 (authored enum0,
runtime catatonic1, delay2.5s). Original4bc6c0 accepts entity handles,
including vehicles; the previous scene adapter searched only NPC bodies and
silently counted this target as unsupported.

The active vehicle owner now accepts catatonic1, waiting2, default-1 and
waypoints4. A mode change stops the autonomous route; waypoints resumes a
retained valid cursor or acquires the unique authored Follow_Waypoints path.
Driver commands stop, but rigid velocity continues through ordinary physics
and player controls remain available. Explicit subsequent movement events
install their new route. This steering policy is a first-playable adaptation,
not a claim to reproduce the complete original vehicle AI scheduler.

The registered vehicle view and route owner publish the same action.
RFVR3 adds action, active state and route kind to the existing vehicle trailer
(40 bytes, compared with32 for RFVR1/2). It preserves suspended routes and
mode-only idle vehicles, and still accepts legacy route records. Authored
path identity, cursor and pursuit targets validate before save publication
or load assignment. No pointer or runtime handle is serialized.

NPC passenger propagation, turret cleanup, collecting and motion-detect
vehicle behavior remain unsupported. No visual or interactive output claim.

Validation: NXDK build succeeds. Bounded tools/xemu_vehicle_ai_mode.py starts
Follow_Waypoints9692, fires9710 with its authored delay, requires driving to
stop, then reloads through the ordinary HDD world save. Native PASS: artifacts/xemu/vehicle-ai-mode-20260930-121141/report.json.
The source runs260frames, records195 autonomous drive ticks, then stops with
the route at node1 of34. RFVR3 saves action1 and the suspended cursor; the
fresh32-frame load retains action1 and issues zero autonomous drive commands.
Source endpoint has3092freepages (12.08MiB), load2852 (11.14MiB); flags
and ISO restore successfully.
This proves the authored stop and save continuation, not passenger behavior,
waypoint-mode resume, physical stopping distance or rendered output.
The initial120836 run reached scene initialization but rejected setup type28;
the diagnostic event admission list now includes Follow_Waypoints. That run
provides no evidence about vehicle mode behavior.

## Retained stopped-route save correction (2026-09-30)

Catatonic-to-Waypoints resume already exists in
`campaign_vehicle_set_ai_mode`: action4 copies the retained route, validates its
remaining cursor, sets active, and leaves index/reverse/physical pose untouched.
Only first acquisition initializes index/reverse. `campaign_set_ai_mode_acquiring`
dispatches vehicle handles to this implementation; the existing rigid route
command issues throttle/turn only when the actual authored driver admits it.
RFNS load restores that driver before RFVR assignment/mode publication. No
second resume adapter or event reactivation is needed.

A separate concrete continuation loss exists after `Follow_Waypoints OFF`.
Its ON handler installs a path with `ai_action=0`; its matching OFF handler
clears only `active`, retaining event, index, reverse and path. The RFVR writer's
old `!active && !ai_action` early return omitted all those retained values.
After loading that save, a subsequent Waypoints request reacquired the path at
index0 instead of the suspended index. This affects authored Jeep routes and
the same shared owner in other vehicle profiles; no campaign playthrough is
needed to establish the control-flow loss.

New `scene_vehicle_route_retention.inc` chooses the trailer from both explicit
mode and retained route state. It rejects half-bound path metadata instead of
silently dropping it. Exact parent integration in
`scene_world_vehicle_route_checkpoint.inc`:

1. Include `scene_vehicle_route_retention.inc` after the existing header guard
   (the containing translation unit already defines `scene_campaign_vehicle_route`).
2. Add `present` beside the writer's local `kind`.
3. Replace the old early return with
   `status=scene_world_vehicle_route_capture_shape(&campaign_vehicle_route,&present,&kind);`
   followed by `if(status)return status; if(!present)return RF_OK;`.
4. Remove the later duplicate `kind=...` assignment. Keep capacity checks,
   the existing RFVR3 encoder and authored-path round-trip validation unchanged.

No new wire version is necessary: RFVR3 already reads inactive kind1 with
action0 and retains its event/cursor, and permits a completed index==count when
inactive. Existing no-route/no-mode saves still omit the trailer. RFVR1/2 and
legacy bare RFVC remain readable; already-created saves that omitted a stopped
route cannot recover their lost cursor. An explicitly catatonic route continues
to save exactly as before. The helper allocates nothing, changes no live state,
replays no event and cannot teleport a host. No helper build/emulator run was
performed; parent owns integration and focused Xbox verification.

## Bounded OFF/save/resume fixture

`tools/xemu_vehicle_route_off_save.py` copies the existing authored L12S1
Jeep7629/miner7646 fixture into CTF06 and replaces only navigation/path/events.
A two-node path starts at the Jeep's initial X/Z, so the normal driver reaches
cursor1 immediately without a campaign journey. Follow_Waypoints runs at frame0;
an ordinary Invert event sends OFF at frame60; a90-frame source writes the real
save. RFNS must preserve the active driver and RFVR3 must preserve inactive
kind1, original event, index1/count2 and action0.

The fresh load runs72 neutral frames. An empty Invert at frame0 has no linked
effects; a probe near frame20 must see the stopped restored cursor and zero
drive ticks. Set_AI_Mode Waypoints at frame60 must resume index1, issue driving
commands and leave restored arrivals at0. Replaying Follow_Waypoints would
reset the cursor, so it is not used during load. The first node is synthetic
and already reached; this proves cursor retention rather than campaign travel.

Exact no-teleport transition evidence uses the parent-owned
`rf_scene_vehicle_resume_probe[12]` captured only on successful action4 changes:
calls, previous index, new index, previous event, new event, host XYZ before,
host XYZ after, new active bit. Both XYZ triples must match bit-for-bit; the
ordinary rigid solver may move afterward. The helper runs no build or emulator.
AST parsing, fixture preparation and independent event/navigation decoding pass;
parent owns native execution. `--prepare-only` and `--validate-existing` remain
read/fixture-only entry points, with no desktop input or images.

The parent integrated the probe and the focused stock-64-MiB Xbox source/load check passes offline validation from `artifacts/xemu/vehicle-route-off-save-20260930-144626/validation.json`: inactive cursor1 survives RFVR3, the driver remains seated before resume, action4 keeps cursor/event and exact host position, and the route drives afterward. The native runner initially marked this completed run FAIL because its pre-resume seat assertion used shutdown counters; the corrected validator passed against the saved source/load telemetry, with the original report preserved.
