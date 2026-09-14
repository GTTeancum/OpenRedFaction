# L2S2a miner rescue investigation

The focused PC/Xbox rescue chain now opens the door, moves the miner into the
guard trigger, fires the guards' weapons, and enters death state. PC also
verifies the miner's authored death watch. This is not a full player traversal.

The authored Use trigger5658 links Goto8481, Goto8482, Invert8468, Pin Door
key8512, Message4785, Remove_Object8073 and Set_Friendliness8483. Switch5671
enables the initially disabled trigger. The trigger's script string is `-1`;
the former live contact filter rejected every nonempty script string. The
shared contact path now accepts this no-script sentinel.
Across the recorded 2,367 trigger records this is the only nonempty value.

Original RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:
loader465510 calls459430 for the script name;459430 searches its name table
and returns -1 when absent. Treating this authored string as no script is a
reasonable first-pass interpretation, not proof of every named-script rule.

Before rotating support, permitting the sentinel and placing the player inside trigger5658,
enabling it with authored Switch5671, and holding Use produces:

```
Trigger 5658 actor 0 frame 0 failed (-2)
```

The event callbacks reported no failure; the controller branch rejected
`RF_GROUP_RUNTIME_ROTATION_PENDING` with RF_FORMAT. The linked Pin Door has
one key8512, mover8497, rotation -120 degrees, and Gate_Open/Gate_End/Gate_Close
sounds. The former live tick and mover propagation also excluded rotation, so merely
accepting its activation would leave a stationary door. The sentinel bypass
was initially reverted until actual rotating movement, collision and rendering existed.

Reproduce the contact setup with `python tools/replay_miner_rescue.py`.
It uses the new headless-only RF_REPLAY_TRIGGER_UID placement helper, keeps
normal trigger eligibility and timing, and records telemetry without calling
an exit0 a successful rescue. The new assertions verify the door phase:900
frames,61 active rotation ticks, one arrival at key8512, angle2.094395 radians,
one bound mover,62 matrix updates, and both authored Goto requests. Before the
route fix the miner recorded834 blocked movement ticks and no scripted attack. Failure evidence
is local at artifacts/miner-encounter/contact.log.

Earlier direct Goto8482 testing reached only52 movement ticks and758 blocked
ticks in1,200 frames, with no Attack; it bypassed the rescue door chain and is
not evidence of a shooting defect. Likewise direct Attack5668/8496 fixtures
omitted authored UnHide5667/8491. NPC trigger5673 owns those events and should
be reached by the miner through the working rescue route.

Implemented first-pass fixed-axis rotation in the shared core: loader463820
copies the key's up axis and negates authored degrees into radians;46a3d0
provides direction, timing, ramp and arrival-mode evidence. Rotation reuses
existing controller motion/timer storage (distance=angle, speed=ramp elapsed),
preserving the historical ROTATION_PENDING enum value. Mover contributions
rotate base positions about the hinge and base orientation axes, preserving
translation contributions. Collision views and the PC/Xbox world renderer read
the same committed matrices. Sphere bounds cover old/new mover origins;
continuous rotational sweep/crushing response is not claimed.

Arrival dispatches the first authored event link and existing mover sounds.
Finite zero-duration rotation snaps to its endpoint. The practical timing path
does not claim original x87 bit parity, every continuous/ramped mode, attached
actors/rider rotation, full obstruction reversal, pressure gates, or persistent
rotation on revisits. Linked arrival lists beyond the first remain incomplete.

Focused CTest rotation_gameplay verifies opening/closing, hinge placement and
orientation, bounds, matrix commit, automatic open/wait/close, zero-duration
snap and invalid-input state preservation. The three existing PC airlock cases
still pass. NXDK builds the XBE and ISO successfully; no emulator was launched.

## Route and encounter update

The first conservative search selected nodes60 and56, but connector59 had
authored radius0.5 while the largest miner body sphere was0.6. Node59 was the
only connecting graph path:60 ->59 ->56. The full body can traverse that route;
the metadata rejection caused a direct approach into the wall instead.

The initial fix tried full clearance first, then a zero-radius graph
centerline search after no usable route. Height filtering remained active.
Actual movement retains all collision spheres and full world/mover sweeps.
This is a practical planning fallback, not a recovered value for original
entity radius+7c0. A failed physical step still stops the actor. A bounded
horizontal wall slide also removes only inward velocity and rechecks the full
body; it does not increase speed or bypass a second obstruction.

The900-frame rescue replay reports eight successful route requests,17 waypoint
advances, five authored movement requests,2,024
movement ticks across actors, and zero blocked steps. NPC contact activates
the guards' authored Attack/UnHide chain without forced attack events. Ten
enemy shots are presented; tracked attacker8490 fires five shots, and
When_Dead8611 fires at12,300ms for miner5458. The player remains alive.

A separate1,200-frame control starts authored Goto8482 with the Pin Door closed:
332 steps remain blocked against a mover, no door rotation occurs, and no
scripted attack starts. It proves the graph fallback still respects collision.
The four custom-animation regression cases pass, and NXDK builds successfully.
No emulator was launched or existing manual session changed.

Next: validate this encounter on stock64MiB XEMU and reach it through ordinary
player traversal. Verify other narrow navigation layouts, genuine dead ends,
corner handling and original navigation dimensions; the centerline fallback
can still select a route the physical body cannot traverse.

## Authored movement radius and player approach

`entity.tbl` explicitly gives miner1 `$Movement Radius: 0.5`, independently of
its collision spheres. The loader now retains this field, and live NPC route
requests use it. The original graph's0.5 connector is therefore eligible under
normal clearance rules. The same encounter passes with zero fallback searches;
the gate-closed control remains obstructed. The fallback is now limited to
classes lacking an authored movement radius, and full physical sweeps remain
unchanged. This is a table-backed navigation binding, not proof of the precise
original entity+7c0 assignment. CTest movement_radius checks the original table
and output preservation on a failed class lookup.

Player-contact trigger5670 normally dispatches Switch5671 and enables rescue
trigger5658. PC replays starting at5670 confirm that switch without any forced
setup event. The subsequent walking approach remains unverified: tested routes
stop outside the small Use volume or pass around its corner without entering.
Original4c0a80 uses the actor's movement segment against the box, so no wider
Use reach was introduced based on that failed approach. Starting inside5658
remains the focused encounter fixture; it is not ordinary traversal from spawn.
Local approach evidence is in artifacts/miner-traversal; headless PC output
now includes CAMPAIGN_FINAL_POSITION to make movement endpoints inspectable.

## Stock64MiB Xbox validation

Run `artifacts/xemu/render-20260914-193612/report.json` passes900 frames and24
selected state comparisons, ending with4,411 free pages (17.230MiB). Native
ROTATING_DOORS is `[61,1,8512,1074137746,1,62,0,0]`; SCRIPT_MOVE is
`[5,2024,0,0,3,5678,5674,0]`. SCRIPT_ATTACK, ten-shot ENEMY_FIRE, and the
single-death COMBAT_DEATH counters match the PC reference word for word.
The native harness does not separately compare the When_Dead8611 timer.

Reproduce with:

```
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --trigger-start-uid 5658 --setup-uid 5671 --input artifacts/miner-rescue-replay/contact.bin --seconds 240
```

The new trigger-start option uses the same one-time placement helper as PC.
It retains normal trigger eligibility/Use and does not itself fire the trigger;
the separate explicit Switch5671 setup remains part of this focused fixture.
Native checks now include rotation, scripted attack and enemy-fire telemetry.
The process guard confirmed no project emulator before launch; cleanup closed
only the harness process and restored/repacked the original disc settings.
Ordinary player approach to the Use volume remains the next traversal gap.
