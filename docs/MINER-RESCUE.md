# L2S2a miner rescue investigation

The rescue remains blocked. This is not a playable-encounter or Xbox validation.

The authored Use trigger5658 links Goto8481, Goto8482, Invert8468, Pin Door
key8512, Message4785, Remove_Object8073 and Set_Friendliness8483. Switch5671
enables the initially disabled trigger. The trigger's script string is `-1`;
the current live contact filter rejects every nonempty script string.
Across the recorded 2,367 trigger records this is the only nonempty value.

Original RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:
loader465510 calls459430 for the script name;459430 searches its name table
and returns -1 when absent. Treating this authored string as no script is a
reasonable first-pass interpretation, not proof of every named-script rule.

Temporarily permitting the sentinel and placing the player inside trigger5658,
enabling it with authored Switch5671, and holding Use produces:

```
Trigger 5658 actor 0 frame 0 failed (-2)
```

The event callbacks report no failure; the controller branch rejects
`RF_GROUP_RUNTIME_ROTATION_PENDING` with RF_FORMAT. The linked Pin Door has
one key8512, mover8497, rotation -120 degrees, and Gate_Open/Gate_End/Gate_Close
sounds. The live tick and mover propagation also exclude rotation, so merely
accepting its activation would leave a stationary door. The sentinel bypass
was reverted until actual rotating movement, collision and rendering exist.

Reproduce the contact setup with `python tools/replay_miner_rescue.py`.
It uses the new headless-only RF_REPLAY_TRIGGER_UID placement helper, keeps
normal trigger eligibility and timing, and records telemetry without calling
an exit0 a successful rescue. With the current filter the probe completes
900 frames with no scripted movement or attacks. Experimental failure evidence
is local at artifacts/miner-encounter/contact.log.

Earlier direct Goto8482 testing reached only52 movement ticks and758 blocked
ticks in1,200 frames, with no Attack; it bypassed the rescue door chain and is
not evidence of a shooting defect. Likewise direct Attack5668/8496 fixtures
omitted authored UnHide5667/8491. NPC trigger5673 owns those events and should
be reached by the miner through the working rescue route.

Next: implement bounded rotating controller activation/tick and mover pose
propagation, permit the no-script sentinel, then verify the door opens and the
miner crosses5673 to activate the guard encounter. Verify on stock64MiB XEMU
when no manual Red Faction session is open. No emulator was launched here.
