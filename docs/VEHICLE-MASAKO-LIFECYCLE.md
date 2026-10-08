# Masako's generated human phase

Implementation-only source slice, 2026-10-08. No build, executable invocation,
unit test or XEMU run was performed. The parent owns the next hourly batch.
The preceding distinct Fighter02/Goto slice remains separate.

## Recovered behavior

The recovered original RF.exe is 1,773,568 bytes, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Direct disassembly establishes these live integration requirements:

- 464696..464744: in L20S2, creation of `masako_fighter` immediately creates
  a second `masako` named `masako_endgame`, at the same transform, with hidden
  creation flag 2 and no authored UID. It publishes the child's handle to
  fighter field 146c, sets child 814 bit 10 and class 728 bit 100. Existing
  authored Masako18301 is a different actor and is not a substitute.
- 4204a1..42056b: on fighter death, a valid living linked actor receives the
  dying fighter's complete position/basis, is unhidden by 48a660, refreshes
  its room/pose, targets the local player through 409050 and enters ordinary
  combat through 408ac0.
- 42056e..4205e8: the player's current vehicle gets 814 bit 800, is detached
  and has life set to zero. Every fighter-class actor also has life set to
  zero. Predicate 40a210 tests class 724 bit 800 exactly, so hidden fighters
  are included. 418f80 sets the retired object bit during finalization.
- 41ee40/41f012..41f05f: after the human actor's death action finishes,
  `masako_endgame` enables trigger 4490 and dispatches event 18371. 4c0200
  only clears the trigger's disabled bit 10; it does not fire the trigger.
  Original event 18371 is Music_Start, `Progression_5.wav`.

The named-reference scan finds no separate named Masako fighter firing
scheduler. Both fighter classes use the shared class-bit/movement-9 combat
path. Ordinary vehicle acquisition and fire scheduling belong to the parallel
general-AI slice; the human uses the existing ordinary NPC combat owner.

## Implemented integration

`scene_masako_seed.inc` appends one explicit generated construction descriptor
after the unchanged authored seed prefix. It has no raw RFL bytes or source
span. `0x7f00126d` is a reserved port persistence key for the dependency of
parent 4717, not a fabricated authored entity UID. Collision with that key,
multiple boss parents or a missing actual Masako class fail before publication.
The original record bytes and class indices are preserved.

The generated actor shares the already loaded Masako class, skeletal model,
materials and motion resources with authored 18301, while owning a separate
pose, body, registry handle, inventory, AI, damage and persistence row. It
starts hidden with class defaults: 2,000 health, 4,000 armor and the scoped
assault rifle. It does not inherit the cutscene actor's instance overrides.
Class 728 bit 100 and child 814 bit 10 are retained. Existing nano-shield
contact debit and immunity operate on its independent armor.

The passive vehicle destruction callback now executes the boss handoff. Pose
math is prepared before publication, then the real fighter pose is installed,
the child is revealed and ordinary player-targeted NPC combat is enabled.
Fighter owners stop orders and become non-solid retained tombstones, keeping
their handles and life state for When_Dead and saves. The player's selected
vehicle uses its existing death/ejection owner and clearance checks.

The human phase observes the actual death playback through the model owner,
including a pose transferred to a corpse. It waits for the real action to end
before enabling trigger 4490 and starting event 18371. Separate effect bits
prevent repeat dispatch when a callback commits before returning an error.

RFMB1 adds a 16-byte phase row inside the vehicle save component, outside RFCL
and inside optional RFSW. It stores parent UID, generated role key, phase and
effect bits. RFNC/RFCH continue to own the child's full existing actor state;
RFVA/RFSV own the fighter. Restore joins these candidates before publication
and does not replay creation, handoff, damage, death or music. The switched
vehicle boot scanner recognizes the additional bounded wrapper.

Pre-feature saves without the generated row may add only its dormant hidden
constructor/history dependency, and only when the saved parent is still alive.
A legacy dead-parent save is ambiguous and rejects instead of inventing a
completed human phase. New saves require both the child row and RFMB phase.

## Remaining limits

- Runtime ownership, human combat, complete death transition, ordinary save
  continuation and stock-64-MiB headroom are unverified until the parent batch.
- Generated registry allocation follows the port's existing indexed startup
  order, not the original interleaved allocator order.
- Retail evasion/flee/flight tactics and the full original secondary firing
  scheduler are not claimed by this lifecycle slice. The opaque constructor
  fields 7c8=1 and 7cc=10 are recorded evidence; this port has no corresponding
  live AI service yet, and no meaning is invented for them.
- Vehicle destruction VFX/audio and forced blocked-seat ejection remain the
  existing first-pass limitations. The original health/dependency effects
  are implemented without relaxing the port's exit clearance policy.

No fixture or expanded test suite was added. Overall implementation remains
approximately 88%; vehicles remain approximately 95% pending integration.
