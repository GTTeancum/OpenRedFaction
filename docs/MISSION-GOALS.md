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

The dispatcher and shared scene now use this store. Goal declarations run before
startup triggers; checks conditionally propagate both on and off, including timed
checks which read the counter when their delay expires. Setters update counts and
retain ordinary outgoing link propagation. Missing setters are harmless. A pending
campaign handoff retains persistent counters and drops section-local counters;
other scene starts initialize fresh state. Both platform loading loops use this
shared scene path. Separate save/load and restart policies remain open.

Loader disassembly at 0x462707 -> 0x4b85b0 establishes that Goal_Create uses
words[1] as the initial count and flags[0] == 1 for persistence. 0x4b85b0 restores
an existing persistent count on revisit. 0x4626d1 -> 0x4b8540 maps Goal_Check's
threshold from words[0]. The original persistent table at 0x4b8610 also has 64
slots; our combined local/persistent budget remains a first-pass policy.

`mission_goal_dispatch` fires the actual L8S1 VAT setter, closes its archive and
events, initializes L8S2, and tests its authored VAT check against a contained
trigger observer. It covers false/true thresholds, delayed counter changes, off
propagation, nonzero initial counts and revisit retention. This is shared runtime
coverage, not an end-to-end traversal of the laboratory or native XEMU evidence.

Goal_Create factory case35 at 0x4b7073 calls constructor 0x4beac0,
which installs vtable 0x589b0c. Its on action 0x4bcab0 and off action
0x4b9f80 are no-ops; ordinary propagation remains active.

Still to complete: native cross-section goal replay and a fresh-game/respawn
policy integrated with save files. Goal HUD text, killed actors, pickups and general world-state
persistence remain separate work.
