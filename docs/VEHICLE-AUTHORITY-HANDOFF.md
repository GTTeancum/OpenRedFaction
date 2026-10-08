# Independent and parked vehicle authority handoff

Source integration after the13:00 UTC build of542cb5e. This code awaits the
parent14:00 batch; no helper build, fixture or runtime test was run.

## Player promotion

Settled, empty independent APC/sub/Fighter owners now participate in ordinary
Use selection with existing distance, true seat geometry, transit clearance,
full wet/dry hull fit, class-resource availability and generation checks.
Masako remains a separate non-player profile. Active routes/combat, emitted
rounds, attachments, riders and living seat bindings still prevent promotion.

The selected owner receives the independent chassis's actual position/basis,
velocity, angular momentum, inertia and physics flags, plus retained finite
AI/Attack ammunition and accepted cooldowns. Only after all fallible preflight
and the two-pointer registry exchange does the secondary runtime retire. The
passive slot becomes the old selected owner. Neither UID nor generation handle
changes. No fresh ammo, fake NPC view or second rigid step is introduced.

## Parked scripted commands

A genuine authored Goto/Goto_Player/Attack command can now resume a compatible
parked APC/sub/Fighter. Its old bank lends retained rigid state to the real
independent runtime. The bank remains an inventory/control owner, explicitly
marked motion_delegated; it never integrates a competing body. Failure before
publication leaves the original parked authority intact.

RFSW5 uses previously-zero row316 to record this loan, preserving the320-byte
row shape and versions1–4. A delegated bank's saved physical fields are copied
from its live RFSV owner and joined exactly on restore, including pose, linear
velocity, momentum, force/torque, skip-forces and physics flags. A nondelegated
bank cannot coexist with a same-UID independent runtime. RFAI/RFAK remain the
trigger/flight authorities; their finite supply is mirrored without a held
trigger in the bank. A legacy load clears unrelated old parked state.

## Commands after promotion

Original parsed Goto events now follow APC/sub/Fighter ownership across a
promotion; Goto_Player remains sub/Fighter-only. Selected APC point movement
borrows the same recovered four-node graph command service as independent APCs,
with a navigation-only scratch object that is never registered or stepped as
physics. RFVR retains event/goal; fresh load replans the transient graph prefix.
Fighter point/player commands share the existing class-correct direct steering.
Player control retains priority. The recovered nonopposed Goto_Player
arrival (strict distance squared4 at404dc0) now releases the chase order, so
a friendly arrived hull is not permanently excluded from promotion. Completed one-way route descriptors can be
released when parking; suspended routes and unsupported future commands remain
protected rather than silently discarded.

## Evidence and boundaries

Original event dispatch and class semantics are documented in
VEHICLE-SECONDARY-SUBMARINE.md, VEHICLE-SCRIPTED-ATTACK.md and
VEHICLE-MASAKO-DISPATCH.md. This handoff is a port ownership transaction built
on the existing registry exchange, vehicle possession, accepted-body solver,
class loaders and ordinary save owners, not a claim of a recovered retail
whole-object migration routine. The only allowed profiles remain APC2, sub4
and Fighter5; no invented independent Jeep passenger binding is added.
