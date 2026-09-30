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
