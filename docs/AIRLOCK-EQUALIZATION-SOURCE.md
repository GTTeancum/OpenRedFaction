# L11S3 airlock equalization: bounded original-source evidence

Source-only investigation on 2026-10-10, against OpenRedFaction-attack commit
`6a3d0446dd9885f799ac0430235abd11e3e8a389`.

## Result and limits

There is a concrete missing playable consumer: authored L11S3 trigger 6298
links Load_Level 6290, targeting L12S1. Original airlock admission can defer
that ordered dispatch for a 1000-ms timer followed by a separately armed
50-ms timer, then flip the chamber's dynamic pressure and suppress linked
mover activation for this different-level load. The current scene interlock
checks opposite closed doors but immediately dispatches after that check.

Only L3S1 and L11S3 authored trigger/room records and the positive L11S3 pair's
linked groups/event were decoded. L3S1 has 23 triggers and no populated
airlock chamber reference; L11S3 has 21 triggers and exactly two. No L2S1
asset inspection, campaign scan, route, fixture, original execution, build,
test, emulator, image, asset edit, gameplay grant or direct event occurred.
Raw source decoding and disassembly are not runtime verification.

Installed RF.exe SHA-256 was read and verified:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Authored owners

`levels2.vpp/L11S3.rfl`: archive entry offset 17197056, size 1905210.
Section offsets below are relative to the RFL entry and include its eight-byte
section header; record offsets are relative to the section payload.

- Trigger section: offset 1328457, size 2752.
- Moving-group section: offset 1302005, size 6772.
- Geometry section: offset 403172, size 754850.
- Event section: offset 1189623, size 6838.

Both triggers are named Trigger Door, are boxes, have fields[0] = 2453,
fields[1] = fields[2] = UINT32_MAX, script empty, value_byte = 0, Use flag,
unlimited activation count, and authored cooldown timing 6.0 seconds.

- UID 2658: payload offset 544, size 145; position
  (112.75, -67.375, -180.0625); ordered links [103, 105, 5181, 2660].
  Its tail_flag is 1, which takes loader 465938's initial-disable path;
  raw values are [6.0, 0.0]. No claim about later enabling is needed here.
- UID 6298: payload offset 2339, size 145; position
  (121.625, -67.375, -181.5625); ordered links [6290, 2663, 110, 108].
  Its tail_flag is 0 and raw values are [0.0, 0.0].

Linked groups are two-key translations, mode 2, selected initial key 0,
with zero authored rotation:

- airlock door A1: keys 103/104, mover 5182; first key
  (112.125, -67.125, -180.9375).
- airlock door A2: keys 105/106, mover 12148; first key
  (112.125, -67.125, -179.4375).
- airlock door B2: keys 110/111, mover 2705; first key
  (123.125, -67.125, -180.9375).
- airlock door B1: keys 108/109, mover 2706; first key
  (123.125, -67.125, -179.4375).

Event 6290: payload offset 6597, size 76, original type 22 Load_Level,
name L12S1A, delay 0, texts[0] = L12S1, no outgoing links. It is the first
ordered link of trigger 6298. Other listed non-controller UIDs retain their
normal resolution; do not silently drop or invent meanings for them.

## Exact chamber topology and static bytes

Geometry room 52 has UID 2453, raw room-record offset 3296, serialized byte
30 = 0 and byte 31 = 1. The pressure initializer's inputs are its ordered
portal adjacency, not the room-child/detail list.

- Portal 26, payload offset 7630: endpoints [51, 103].
- Portal 27, payload offset 7662: endpoints [104, 52].
- Portal 28, payload offset 7694: endpoints [103, 52].
- Chamber room 52 has two portals, [27, 28].
- Door room 103 has UID 105, raw offset 5438, and two portals [26, 28].
- Door room 104 has UID 108, raw offset 5480, and one portal [27].
- Far-side room 51 has UID 2147483595, raw offset 3247.

Rooms 51, 52, 103 and 104 all have static serialized outside byte 30 = 0.

Door room 103 bounds are
(111.8420181274, -68.6250991821, -182.0626068115) through
(112.4359741211, -65.6249008179, -178.0623931885).
Door room 104 bounds are
(122.7811584473, -68.6250991821, -182.0626068115) through
(123.6250991821, -65.6249008179, -178.0623931885).
The corresponding A/B first-key positions above are strict interior points
of these authored boxes. This is source geometry evidence, not an executed
containing-room query. Room UID 108 happens to match a B1 key UID; that
coincidence is not a valid room-binding rule.

## Original room and pressure ownership

- Loader 46575a..465765 reads fields[0] into the constructor's +1c field;
  constructor 4bfaa5..4bfaba copies it to trigger +2c0.
- Serialized room byte 30 becomes original room static byte +42 at
  4eda1f..4eda2a. Dynamic pressure is a separate room byte +44.
- Initializer 4ccf80 first sets dynamic +44 to logical-not static +42.
  It returns that default unless room UID +24 is not UINT32_MAX,
  the chamber has exactly two portals, and the two adjacent rooms have
  portal counts (1,2) or (2,1).
- In that special topology it chooses the adjacent two-portal room. It
  chooses portal slot 1 if slot 0 contains the chamber, otherwise slot 0,
  then chooses the endpoint other than the door room. At 4cd064..4cd07b,
  chamber +44 becomes logical-not that endpoint's static +42.
- Thus L11S3 chamber 2453 initializes pressure to 1, using far-side room
  51's static outside 0. No raw geometry byte needs to change.

Controller factory 4692c8..4692dc copies the first key position into its
factory parameters; 469304/46930d creates type 8 through 486da0. Factory
487089 calls room assignment 48a160, which writes object +0 and copies the
public position +3c to its query cache. The existing reconstructed
rf_group_controller_pose maps first-key public/physics positions, as described
in docs/COLLISION.md, Controller factory pose and zero radius.

Original object room refresh 48a190 calls containing-room query 4cd970 with
object +3c. It replaces the retained room at object +0 only on a hit and
preserves the previous room on a miss. Generic accessor 40a490 simply reads
the first word. The port has rf_entity_room_refresh and
rf_geometry_collision_world_locate, but rf_group_runtime_entry does not
currently own an equivalent retained controller-room field.

Therefore runtime reconstruction must establish controller-side room through
the actual containing-room query/owned membership, retaining conservative
failure behavior. AABB overlap alone, key UID equality, trigger position,
camera position or a fabricated room ID is insufficient. No query result for
this binding was executed in this investigation.

## Original admission and delayed completion

4bfc60 retains normal eligibility and contact-delay handling before airlock
logic. At 4bfdd4 it resolves the chamber UID through 45e7c0. Missing chamber
resolution has original direct-dispatch fallback; the existing port deliberately
blocks unsupported chambers instead, and that conservative policy can remain.

4bfdf6..4bfe96 finds another trigger with the same chamber and a linked type-8
controller. It rejects while that controller's next key is not -1, or the
peer trigger's pending timer +2e0 is set. Own pending timer +2e0 also rejects
rearming at 4bfe5a..4bfe69. The existing port's stricter all-peer, closed-key-0,
two-key-translation interlock should remain its bounded recognition policy.

Pressure-side selection is important:

1. 4bfe97 loads the GLOBAL local-player object at 5cb054 and reads its
   retained room. It does not use the contacting NPC's room.
2. If that room is not the chamber, it is the static-outside comparison source.
3. If the player is in the chamber, ordered trigger links select the first
   type-8 controller with a nonnull retained room.
4. A controller room with one portal unconditionally takes delayed
   equalization at 4bff0f -> 4bffc2.
5. Otherwise the first two portal records' endpoints are examined in order;
   the first endpoint neither controller room nor chamber is the comparison
   room. Unsupported broader layouts need not be admitted by the port.
6. 4bff7d..4bff9b requests equalization when comparison-room staticOutside
   equals chamber dynamicPressure. These bits have opposite meanings.
   Nonmatching state directly dispatches through 4c0220.

For the positive closed B2 controller room 104, the one-portal case explains
the missing delay before L11S3 trigger 6298's Load_Level. The A-side two-portal
case compares room 51 outside 0: initial pressure 1 needs no delay, whereas
pressure 0 after a completed flip does. The source binding must still be
validated by the actual containing-room service in any future execution.

Accepted delayed path:

- 4bffc6..4bffeb takes local-player +3c as sound position and requests sound
  ID 31. Audio reconstruction is separate from proving the mechanical delay.
- 4bfff5..4bfffa arms trigger +2e0 for 1000 ms; 4c0003..4c0006 stores the
  triggering actor handle at +2e4. It does not dispatch links or increment
  activation/cooldown bookkeeping yet.
- Independent global trigger service 4bf740 runs without requiring continued
  contact. After +2e0 expires, 4bf80d..4bf839 arms independent +2e8 for 50 ms
  if it is unset, then skips completion for that service visit.
- Once that second timer expires, 4bf854..4bf866 resolves the same chamber
  and toggles its dynamic +44. Then +2e8 is cleared.
- 4bf86f calls 4bf660 for mover suppression; 4bf88f invokes
  4c0220(trigger, retained actor, suppress_movers, 0).
- Only afterward does original code clear +2e0 and retained actor +2e4.
  The downstream 4c0220 owns ordered effects, activation count, cooldown
  and last activation time.

4bf660 scans linked events for type 22 Load_Level and compares its target
string with current level global 645fe4 using 500210. That function calls
case-insensitive 57c130 and normalizes nonzero comparison to Boolean 1 at
500242..500246. The predicate is inequality, not equality. A different-level
Load_Level therefore sets suppress_movers = 1. L11S3's target L12S1 qualifies:
its delayed completion must dispatch the event while suppressing type-8
controller activation. No synthetic trigger or direct-event shortcut is needed.

## Bounded lifecycle proposal, not original save parity

Keep static geometry immutable. Use a separate chamber-pressure owner and a
generation-qualified pending receipt that retains trigger, chamber and actor,
with the two independently armed timers. Service once per actual simulation
step rather than per actor contact, and prevent both same-trigger and peer
rearming while a receipt is active.

Completed pressure is durable gameplay state, not a disposable timer. A
conditional RFTC2 per-trigger chamber/pressure representation can reuse the
existing campaign-trigger history: same-level peers for a chamber must agree;
scene restore must bind rows to the authored chamber UID and topology. RFTC1
loads must explicitly reconstruct the topology-derived baseline and replace
omitted old history, rather than inheriting live mutated pressure.

Pending timer serialization can remain deferred behind a save-only transient
guard and a level-departure guard. The dependent guards must not deadlock the
pending trigger's own Load_Level: prepare a copied completion receipt, publish
the pressure exactly once, and retire the pending operation before its ordered
callback dispatch. This is a port callback-safety adaptation; original clears
the timer after dispatch. Partial dispatch/faults must not retry the pressure
flip or silently discard remaining ownership. Rejected loads preserve current
pending state; only successful final load commit may replace it.

The root/reviewer and implementation owners choose the exact guard/rollback
integration. No broader atmosphere, oxygen, drowning, damage, sound-bank or
campaign traversal work follows from this finding. Existing outside damage
and gasp continue to read static +42 / serialized byte 30 independently.
