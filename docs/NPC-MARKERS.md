# Named NPC animation marker consumption

Original `RF.exe` SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

`51c420` reads the dominant active-slot index at model instance +1d48, resolves
its motion ID through the active slots at +12d4 and descriptor array +f5c, then
searches two names at motion descriptor +40 and +54. Each name is 16 bytes with
a following tick value. Empty names are skipped and comparison is case-sensitive.
The first exact match returns false if its event byte is clear. If set, it clears
only that byte (+1d44 or +1d45) and returns true in AL. A duplicate second name
is not consulted after a first match, even if only the second event is pending.
No dominant slot or absent name returns false without clearing any event.
Upper EAX bits are not a boolean contract; the reconstruction returns a normalized
result via `fired`.

`rf_motion_consume_marker` in the shared motion core implements this behavior
using resource-indexed `rf_motion_marker_names`. The port validates bounded slot,
resource and string views. Failure preserves playback state and output. Metadata
must describe the resolved dominant resource, not merely active-slot order.
This function does not play a sound or choose whether an actor should poll it.
The caller's footstep dispatch/gates and catalog name retention remain open.

`python tools/verify_motion_marker_consume.py` executes the complete original
routine unchanged and compares PC and NXDK machine code. It covers 512 cases
with 47 true events: selected slots/resource IDs, exact-case and mismatched names,
empty/missing/duplicate names and all two-bit event combinations. Four invalid
port input cases preserve state/output. Result: PASS in
`artifacts/motion-marker-consume.json`. Both builds and all nine CTest checks pass.
