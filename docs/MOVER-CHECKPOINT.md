# Mover checkpoint component

RFMC1 is a standalone scalar-state component, not integrated ordinary-level
save support. Each UID-sorted row is 88 bytes; the header is 64 bytes with
version, exact size, FNV checksum and caller-supplied 32-byte authored identity.
At most 1,024 controllers are admitted structurally; the scene save budget may
impose a smaller limit. No memory allocation, runtime handles or pointers occur
in the serialized representation.

Controller identity is its first authored key UID, matching current scene
controller registration. Rows retain runtime kind and key count; motion flags,
mode, current/next/terminal key indices and phase; speed, distance, object flags,
position, pending position and velocity. Rotation uses the existing shared
runtime: distance is angle and speed is ramp elapsed. Runtime kinds retain the
historical `ROTATION_PENDING` constant despite implemented rotation ticks.

Capture stores remaining timer duration: -1 disabled, 0 elapsed and positive
pending milliseconds, bounded to the timer's unambiguous half-period. Restore
rebases against the new game clock. This intentionally does not preserve how
long an already elapsed deadline has been overdue. It preserves its ready state.

The public capture helper reads an entry without modifying it. Prepare validates
the saved first-key UID, kind and key count against a caller-rebound fresh entry,
then returns a candidate translation runtime. It does not touch the entry's
borrowed source, controller pose, attachment membership or collision owners.
Both encode and decode validate every row before publishing any output.

Required scene integration, not claims of this component:

- Bind identity to authored level/group/key bytes; reject missing, duplicate or
  mismatched controller identities and validate immutable flag/mode policy.
- Capture only at a completed movement/event boundary with no queued key
  arrivals, obstruction callbacks, sound requests or partially propagated poses.
- Stage the complete set before mutation, reconstruct controller and attached
  poses using authored first-key positions/bases, rotation signs and ordered
  contributions, then rebuild collision views and room placement. Do not save
  or replay sound handles, parent handles or borrowed pointers.
- Admit only supported attachment types; carried actors, unsupported general
  objects and other unsaved controller effects require a scene guard.
- Rotation dwell uses the same wrap-aware timer API as translation; focused
  gameplay checks cover waiting across wrap and resuming at the deadline.

Focused tests cover translation and signed rotation angles, timer wrap/rebasing,
disabled/elapsed deadlines, UID rebinding and atomic malformed-data rejection.
No PC/Xbox build or live restore was run when adding this component.

Integration correction: rotation dwell now uses rf_timer_pending, matching the shared wrap-aware clock contract. The focused rotation gameplay check verifies no premature movement across wrap and movement at the deadline.
