# Campaign trigger contact readiness

The scene now polls the existing reconstructed 4bfc60 contact stage against the
registered campaign player after the physics position commit. This is readiness
instrumentation, not firing: activation counters, key handling and controller
motion are not consumed. A ready count can repeat while the player stays inside.

Loader evidence (original executable SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836):
465510 stores the low-byte value in local_3c, constructor input +68; 4bf970
copies this to runtime +2c4, the filter inspected by 4c06d0. The third raw
fields word enters constructor +24 and runtime +304 (attachment reference).
The live path currently accepts only filter zero without an attachment/script
or owner/class-dependent flags. Unsupported polls are counted explicitly.
Use input is currently zero. Ready is before the original key/activation gates.

rf_scene_trigger_contacts exposes six words: polls, ready polls, last ready UID,
lower-door UID8542 ready polls, skipped unsupported polls, last status.
These are shared PC output and XEMU memory observations; no per-tick allocation.

Validation:
- PC/NXDK builds and all six CTests pass.
- verify_trigger_poll.py: 4096 original/PC/NXDK cases pass. This validates the
  composed gate/contact/delay function, not original whole-scene scheduling.
- replay_door_contact.py --native, 180 staged frames, stock64MiB XEMU:
  artifacts/xemu/replay-20260910-164443/report.json. Both targets produce
  [8950,358,9599,179,1969,0]. UID8542 is ready on 179 simulation ticks.
  Existing mover collision and complete final contact metadata still match.
- PC authored-spawn 180-frame idle control: zero UID8542 readiness, no errors;
  artifacts/door-contact/spawn-idle.txt.

Next: connect ready activation to the ordered controller/event links and live
controller ticks, preserving key gates and effect ownership. Door opening and
traversal are not yet demonstrated. Other filters need resolved entity/attachment
ownership; this instrumentation must not become a silent replacement for them.
