# Capek shield speed, break fall and retained run default

The earlier fall/run and RFNC18/RFCH5 continuation was independently
source-reviewed and parent-integrated. This owner-local shield-speed follow-up
is independently source-reviewed and parent-integrated against
`58be1b12629a9304587ac1311ffbbd13246d81a2`, awaiting the scheduled
09:00 UTC Xbox batch. No compilation,
syntax checks, tests, fixtures, campaign routes, XEMU, images, grants or
original-input changes were performed. Action runtime and save/revisit
continuation remain unverified.

## Original evidence and bounded behavior

The existing contact reconstruction and independent read-only inspection use
original RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `0x4c5af7..0x4c5afa` calls `0x427df0(owner,1)` once the shield-debit branch
  exhausts armor. That contact remains consumed before ordinary direct health,
  radial and impact handling.
- `0x427e28` calls `0x4281a0`, setting body flag1, fall descriptor3 for actual
  Capek and identity movement orientation. Capek lacks the class0x400 bit that
  would select sub-fall8.
- `0x427e46..0x427e69` restores the saved authored class speed and changes the
  movement name to `run`. There is no direct attack, selected-weapon or cadence
  reset in the recovered routine.
- Constructor `0x422aed..0x422af0` saves authored speed; `0x422b16..0x422b2c`
  installs shielded class/live speed8.0 and hover. The original Capek class
  authors hover12, speed0.3 and flags0x0300011f.
- Actual selected Cane owners include L11S3 UID4938 and L8S4 UID8359. These are
  relevance evidence, not hardcoded activation, UID, level or weapon gates.

The new consumer qualifies immutable exact `Capek`, authored hover12 and the
Nano class bit. The actual break operation additionally requires the retained
living registered actor. It calls the existing fall operation before publishing
zero armor and the new owner latch. Failed fall admission does not consume
armor or set the latch. Invulnerable contacts, partial debits and Event76
ON/OFF do not create break history. Other Nano-class actors keep their existing
contact behavior.

After the break, `rf_scene_npc_normal` uses this owner's effective run1 default,
including normal landing. Slow and later Normal retain the latch. Shared class
data and movement configurations are never mutated. The exact XYZ-hover
predicate also excludes a broken owner. No weapon, ammunition, action,
windup, projectile or attack-clock state is reset by the break.

## Why explicit history is necessary

Armor0 cannot prove a shield break: authored instance vitals can start at zero,
and Event76 OFF permits ordinary health/armor damage to exhaust armor without
calling the consuming Nano-break routine. Current run1 also cannot prove a
break because Slow selects1. Inferring a persistent run default from either
would manufacture a transition on Normal or restore.

`capek_shield_broken` therefore records only the actual consuming break. It is
separate from the shield-disable flag, current descriptor and speed mode. Fresh
owners initialize it to zero; older saves restore zero without inferring old
history. Authored-zero or OFF-drained unbroken Capek retain unbroken movement
semantics; the newly implemented shielded-speed resolution below applies to
them too. Terminal owners retain the historical fact but never replay
locomotion on load.

## Owner-local shielded speed

Original `0x422aed..0x422af0` retains authored class speed, and
`0x422b16..0x422b26` installs8.0 into both class+0x50 and live actor+0x8c0.
The constructor later requests Normal at `0x422e19`. The class table continues
to retain authored0.3 in this reconstruction: a read-only owner resolver
substitutes8.0 only for exact authored Capek/hover12/Nano with latch0.
It never consults armor, shield-disabled flags, health, selected weapon,
current movement slot, level or UID. Other Nano actors are unchanged.

The existing `rf_movement_set_mode` still owns all arithmetic:
`0x4274d7..0x4274e1` uses base for Normal;
`0x4274fc..0x427502` uses alternate-factor times base;
`0x427536..0x42753c` uses Slow-factor times base. Forced action still requests
Slow and the SP network-override policy remains zero. Original Capek
`entity.tbl:602` authors0.3 without explicit Slow/Fast factors, which the
existing reader initializes to1. Thus ordinary Slow and Fast preserve8.0
before break and0.3 after a resolving setter; descriptor selection remains
separate. Shared class data and configs are never written.

Crucially, `0x427e66` restores only class/base speed, not live+0x8c0 or numeric
mode+0x8c4. The break therefore updates the retained latch and owner view's
base-speed mirror after successful fall admission, while preserving the live
speed/mode. The next ordinary Slow/Normal/set-speed call resolves from the
restored authored base. Ordinary landing already selects Slow or Normal.
No immediate live-speed reset, attack reset or new break phase is invented.
Event76 OFF/ON continues to change only shield contact gating: OFF-drained
and authored-zero owners remain unbroken8.0; toggling ON after a real break
does not restore8.0 or erase the run default.

Initialization, ordinary speed setters, live ground queries and exact Capek's
existing scripted ground path now use the same effective base. That ground
path retains the prior1.5 scaffold for unrelated actors. Pursuit still consumes
the existing live `movement.speed`, so shielded hover now advances at8.0 and
post-break translation naturally retains the original transient until a setter.
Candidate-world placement, passive-host support and seated placement resolve
from the saved candidate latch rather than the current live owner's latch.
Restore stages the base-speed view mirror and movement privately, then publishes
with the existing assignment phase. Terminal rows only update the base mirror;
they receive no speed setter or movement replay.

RFNC stores numeric mode rather than actual live speed. A new narrow RF_RANGE
capture guard rejects a living unretired broken Capek when its retained live
speed cannot be reconstructed from authored base and saved numeric mode.
It never silently omits the row, changes the live actor, or rejects terminal
history. Moving falls already usually fail existing quiet-velocity checks;
this additionally covers zero-velocity just-broken and frozen/disabled falls.
Normal landing or an ordinary setter removes the mismatch. Until then this
transient is deliberately unsupported for saving and component-only in-session
restore, whose resident admission also captures current state. Ordinary fresh
quickload can still restore a prior save. All prior quiet-velocity, motion,
clearance, support, identity and storage guards remain, alongside this new one.
No new RFNC/RFCH version is needed.

Old saves never carried actual live speed or inferred break history. RFNC1–12
latch0 has no movement continuation: exact living Capek now reconstructs
constructor Normal/hover (or existing disabled fallback0), preventing a later
in-session broken0.3/run state from leaking into a legacy load. Other legacy
owners keep their existing fallback. RFNC13–17 preserve their saved descriptor
and mode but use latch0's shielded8.0 base, including armor0 and OFF states.
RFNC18 uses its explicit latch: broken1 resolves authored0.3, unbroken0 uses8.0.
This intentionally supersedes the pre-speed implementation's authored0.3
unbroken loads, which cannot be distinguished on disk. RFCH1–4 likewise mean
unbroken0; RFCH5 retains the explicit latch. Living broken revisits reconstruct
authored-speed movement before ordinary fall from authored placement, without
replaying the unsaved break-frame transient or shield effects.

## Conditional save formats

### RFNC18 current-level owners

The record adds one32-bit boolean. RFNC18 appends4 bytes after the existing
holster tail on every row and is emitted only when at least one row has a
nonzero break latch. `RF_NPC_CHECKPOINT_ROW_MAX` changes1608 to1612; all earlier
row layouts and ordinary writer version selection remain unchanged. RFNC1–17
decode with latch0 through the existing zero-initialized record.

The codec requires boolean0/1. A living latch1 row requires armor0 and existing
movement continuation. Scene capture and candidate restore additionally require
exact authored Capek and current fall3, run1 or the existing disabled-descriptor
fallback0. Existing enabled-descriptor, stance, motion, quiet-velocity, support,
seat, clearance, resource, transient-attack and identity guards remain intact.
Terminal latch1 rows retain history without acquiring movement continuation.

Restore keeps the latch in the private saved row and publishes it only in the
existing assignment phase. Both current owner and campaign history are assigned,
including zero when loading an older or unbroken save in-session. Preparation
never calls fall, damage, weapon or shield presentation callbacks.

### RFCH5 all-level history and revisits

The campaign actor store adds one byte per slot,2048 bytes total. RFCH5 extends
only actor rows from56 to60 bytes, appending the same boolean as a32-bit LE word.
Switch/event rows and all other header/carry fields retain their existing layout.
RFCH5 is selected only while a history entry has latch1; RFCH1–4 read as zero.
The maximum disk budget accounts for at most8192 additional bytes; outer save
and staging budgets remain unchanged and binding.

Snapshot merges current latches into a private copy. The history latch is
independent of stale terminal revisit vitals: Remove_Object can retire an owner
before ordinary vitals capture, so a terminal history record must not couple the
latch to a possibly older armor value. Current immutable seed identity is checked
when rebinding; off-level identity is checked when that owner is reconstructed.

The RFCH decoder now explicitly selects section2 for actor decoding. This keeps
unretired event rows out of the actor branch, necessary before reading the new
60-byte actor tail. The shared section/version stride function drives size
preflight, decode and encode consistently. Boolean, checksum, length, key,
count and existing value validation remain in force.

The ordinary world loader joins each staged RFNC latch to the corresponding
RFCH level/UID entry before assignment-only publication, rejecting contradictory
components without modifying owners. Existing publication/storage-close ordering
is preserved; transient cancellation remains conditional on successful load and
storage close, as before this patch.

On an actual level revisit, an unretired living owner with retained break history
starts from authored placement with no retained floor contact. The consumer uses
ordinary fall/landing to leave startup hover and restore effective run. This is
the port's existing first-pass authored-placement revisit policy, not a new saved
position or shield-break replay. Retired/terminal owners never trigger that fall.

## Cost, status and deferred work

Runtime owner cost is one32-bit latch per allocated NPC; the existing640KiB
NPC-body admission accounts for `sizeof` automatically. The campaign store adds
2KiB, and private staging accounts for its changed `sizeof`. Current-level save
rows gain at most4 bytes each when RFNC18 is selected; history rows gain4 bytes
each when RFCH5 is selected. No resource, audio, particle or flight pool is added.

The speed follow-up adds no runtime-owner field, pool, resource or wire bytes.
The resolver copies one small config on the stack; private restore staging adds
one float base mirror and one movement-publication word per row (8 bytes),
with existing sizeof-based allocation budgets still binding. Shield presentation,
break/hum audio and other boss phases remain separate. No new phase, actor
activation, campaign route, diagnostic fixture or runtime proof is inferred.
Existing optional movement fixtures still expect immutable authored speed;
they were not changed or run and do not prove the new Capek owner policy.

Expected parent validation remains the scheduled Xbox batch; the specific
break→fall→land/run action, older/unbroken rollback, terminal history and
post-break save/revisit behavior have not been exercised by this source-only
slice. Existing Nano contact runtime results in NANO-SHIELD-CONTACT-FIRST-PASS.md
predate this movement/save implementation and do not validate it.
