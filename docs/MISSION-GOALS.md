# Mission counters and persistence

The authored campaign contains Goal_Create (35), Goal_Check (36) and
Goal_Set (37). `tools/inspect_mission_goals.py` compares their names, flags and
words with the current shared C level reader and reports cross-section references.
For example, L8S1 declares VAT while L8S2 and L8S3 check it. L5S2 has four
ReadyToBlow setters and a threshold of four. These are gameplay dependencies,
not just text for an objectives screen.

Static inspection of the verified original RF.exe establishes these actions:

- 0x4bca30 increments the named local and/or retained goal counter.
- 0x4bca70 decrements the named local and/or retained goal counter.
- 0x4bc970 and 0x4bc9d0 compare the signed counter with the threshold and
  propagate on/off respectively only when the counter is at least the threshold.
  An absent goal does not propagate, even with a zero threshold.
- Checks look for a retained goal through 0x4b8680 before the local goal through
  0x4bd740. Local storage uses offset 0x2c0; retained storage uses offset 0x0c.

These addresses were read as disassembly; this milestone does not claim an
original-code execution oracle or completed loader-field mapping.

The shared campaign module now provides a fixed 64-entry owned counter store
(16,900 bytes). Names are copied, lookups fold ASCII case, counters support
increment/decrement and signed thresholds. Missing checks return false.
Explicit section cleanup retains persistent counters, and declaring a retained
counter again preserves its value. New campaigns start with zeroed storage.
Capacity and signed overflow return errors rather than allocate or invoke C
undefined behavior. These are explicit first-pass policies.

Still to complete: confirm Goal_Create field/lifetime mapping, connect the store
to scene initialization and event propagation, carry it through the PC/Xbox
loading loops, and verify an authored cross-section goal in XEMU. The module is
not yet connected to live gameplay. Goal HUD text, save files, killed actors,
pickups and general world-state persistence remain separate work.
