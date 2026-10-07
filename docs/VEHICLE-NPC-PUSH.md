# Moving passive chassis against NPCs

## Integrated response

Translated, attached passive vehicle hulls now sweep against eligible living
NPC bodies, then request the remaining chassis displacement through ordinary
body clearance. Previously only the player had this side-push path; NPC carry
required an already established support contact.

This is a bounded first-pass port response using the existing authored sphere
union and relative swept-sphere query, not a claim of a recovered retail pair
scheduler. The original and final chassis poses are recorded around the real
controller update. Changed orientation is rejected rather than approximated as
translation. A consumed controller epoch prevents replaying old movement when
the controller does not update, including while the player is occupied.

The pass runs after chassis motion and before ordinary event/NPC movement. NPC
pose/basis snapshots taken before controller callbacks also exclude bodies
relocated during an arrival callback. Hidden, nonliving, linked/seated, frozen,
unsupported movement-mode or invalid-mass actors are excluded; existing riders
on that exact host stay with the normal carry path.

Clearance retains compiled geometry, movers, fragments and other passive hulls,
and additionally checks the active host. Only the pushing owner is excluded,
through the existing synchronous linked-owner filter, restored before handling
any query error. The admitted position and bounds are staged before publishing
to the live NPC/model position. No second angular integration, health change,
support assignment, seat mutation or crush damage is performed.

The cached poses are interval-local, not new save state. Passive checkpoint
publication invalidates that cache; all persistent formats are unchanged.

## Bounded cloud Xbox fixture

`tools/xemu_vehicle_npc_push.py` keeps original L20S2 geometry, movers and clutter,
the Hanger Lift001 controller, its Fighter UID 4717 and unarmed friendly Eos
UID 4716. Eos retains the complete original entity record, including 1 HP.
Other actors, unrelated event chains, triggers and navigation are removed from
the disposable fixture. The original When_Dead setup event is explicitly
rewired to the lift output and original UnHide; normal event handling reveals
the retained hulls before the test placement. This is not a natural encounter
or campaign traversal claim.

Mode 6 places the idle NPC beside the actual moving hull at frame 40. Mode 7
also places a distinct stationary copy of that authored chassis, UID 915601,
beyond the NPC using complete sphere-union axis bounds, with a positive
separation check against every actual sphere pair. The copy is unparented and uses
the original nonboardable class. No guest memory writes or host input are used.

Read-only contact rows retain exact UIDs/full handles, real chassis start/end
positions, proposed/admitted/published NPC positions, contact/clearance
fractions, original health, support/link identity and the distinct blocker.
The two cases require both measurable open displacement and materially reduced
blocked displacement. Each run saves hashes and copies of its actual PE, map
and XBE before launch.

Both final 75-frame cloud XEMU runs pass with exactly 67,108,864 bytes of RAM:

- Open: 31 real contacts and admitted pushes move Eos 0.88159 m in X while the
  chassis travels 0.96277 m after placement. Every recorded displacement follows
  the real interval/contact fraction and publishes to the exact live actor.
- Blocked: all six initial actor/blocker sphere pairs are separated, with a
  minimum gap of 0.15293 m. The actor moves 0.15024 m before the distinct hull
  limits travel; 26 of 31 contacts are clamped. The stationary blocker retains
  its exact UID, full handle and pose. X travel is reduced by 0.73135 m versus
  the open case, rather than beginning from an overlapping fixture.
- Eos retains original health 1, no combat occurs, support/seat linkage is not
  rewritten by the push, the player remains alive, and both requested endpoints
  finish. Endpoint free pages are 5,702 in both final runs. Disc restoration
  passes; actual tested XBE/PE/map bytes and hashes are retained per case.

Final evidence: `artifacts/xemu/vehicle-npc-push-20261007-120754/report.json`.
The preceding `vehicle-npc-push-20261007-120253` run is preserved as superseded
fixture evidence: it passed its then-current assertions, but the blocker setup
only positioned the largest spheres and did not establish whole-hull initial
separation. The final fixture tightens that prerequisite and reruns both cases;
the gameplay displacement/clearance algorithm did not change for this retry.

## Remaining scope

Rotation, crush damage, crowded/multiple-actor interactions, passenger
architecture and naturally staged encounters remain deferred. The active-host
clearance path is source-reviewed but is not the blocker in this fixture.
Guarded skip paths are source-reviewed rather than individually runtime-tested.
No new save/load, animation, audio or visual claim is made.
