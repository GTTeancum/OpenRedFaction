# L2S2a miner rescue investigation

The rotating door phase now works on PC; the miner's onward route remains
blocked. This is not a complete playable encounter or Xbox runtime validation.

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
one bound mover,62 matrix updates, and both authored Goto requests. The miner
still records834 blocked movement ticks and no scripted attack. Failure evidence
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

Next: repair the miner route (reported static room19/face443 obstacle), reach
NPC trigger5673 and verify the guard encounter. Validate rotation on stock64MiB
XEMU when no manual Red Faction session is open.
