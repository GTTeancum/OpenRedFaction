# Historical Make_Fly event and actual actor speed

Written 2026-10-08 against isolated base
`eb58532b009adeec0f2c775e8aff4d620a7b0081`. No build, test, fixture or game
execution. Parent integration and the next hourly batch own verification.

## Original authored behavior

L20S1 contains Make_Fly event12460 linked to merc_com actor12459 and downstream
Goto_Player12462. Goto_Player12458 reaches12460, and UnHide12461 reveals the
same actor. The missing type26 consumer previously skipped the actor effect.

The editor label does not describe a flying implementation. In the original
RF.exe (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`):

- Generic ON4b9070 maps26 through4b90d1 to4b9540.
- 4b9540 traverses linked handles, resolves each entity with426fc0, and calls
  428030(entity,0) at4b9573.
- 428030 is the already reconstructed slow/standing transition. It calls
  427450 with speed mode0; walking classes select movement descriptor1,
  identity movement orientation and zero vertical body velocity. Its optional
  standing attempt intentionally does not stop the remaining transition when
  clearance prevents standing.
- Generic OFF4b9f80 maps26 to RET4ba008. Common link propagation is separate.

The actual merc_com table has max velocity6.0 and slow factor0.3. The existing
427450 reconstruction therefore computes speed1.8, with no synthetic flight
mode, class replacement, teleport or invented acceleration.

## Implemented gameplay path

A typed ON-only callback now routes generation-valid linked NPCs to the actual
slow helper. A real ground-refresh service supports its existing standing
attempt, using current full-body world/support queries and ordinary position
publication. Dead/unregistered actors and unknown linked ownership remain
excluded. The exact actor effect completes before common propagation reaches
Goto_Player12462; ordinary delayed activation uses the same callback gate.

Scripted NPC movement previously advanced every class at a hardcoded1.5m/s,
ignoring the already resolved movement.speed. The normal swept approach now
consumes that retained class/slow/alternate speed. Thus the event can actually
change travel pace rather than only changing unused metadata. Collision,
route ownership, arrival thresholds and event timing are unchanged. Existing
animation choices and footstep/stride polish are separate.

## Continuation

RFNC13 appends only three words after each variable-size row: movement-state
presence, descriptor slot and speed mode. Standing live NPCs capture these
values; restore validates the enabled descriptor and rebuilds speed/response
from the identity-bound authored class in the candidate stage. Assignment
publishes them with the admitted body and support, without replaying Make_Fly
or Goto. Existing settled-body, pose, resource and world-clearance gates remain.
Crouched/nonidentity movement ownership has no new save representation and is
explicitly outside this continuation admission. RFNC1-12 remain readable; the
component still writes its old version when no movement state is supplied.

No new retained per-NPC gameplay owner or per-frame allocation is introduced.
RFNC13 costs12 wire bytes per row when present; scene staging accounts for its
added settings through the existing sizeof-based budget. This is source-written
behavior and ordinary-save continuation, not native gameplay evidence.

Overall implementation remains approximately88%; mission/actor behavior is
runtime-unverified, with no automatic percentage uplift.
