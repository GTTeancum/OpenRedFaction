# Room liquid face collision: original instruction evidence (2026-09-15)

## Executed evidence

`tools/verify_liquid_face_collision.py` executes the whole original
`4dec10` face collision routine with **no function substitutions or hooks**.
Thirty cases pass; raw outputs are in `liquid-face-collision.json`. The fixture is
a valid four-corner horizontal face, upward normal, flags4, ring next/previous
links, material-1, and initial nearest-hit fraction1. It uses original point/sphere
plane tests, AABB checks and polygon inclusion. x87 precision is53 bits.

| Case | Original result |
| --- | --- |
| Start y1, delta y-2, radius0 | fraction.5, contact on plane, normal up |
| Same, radius.1 | fraction.449999988079071, contact on plane |
| Start below plane, either upward or further downward | no hit |
| On plane, moving downward | hit fraction0 |
| On plane, moving upward | no hit |
| Parallel above plane | no hit |
| Start y.05, radius.1, moving down | hit fraction0 |
| Point start1/delta-1 | hit fraction1 |
| Sphere radius.125/start1/delta-.875 | hit fraction1 |
| Sphere radius.1/start1/delta-.9 | no hit (float endpoint difference) |

`4ded7d` selects point versus sphere using radius threshold5894a0. Point plane
routine506550 rejects negative signed start distance. Sphere5071b0 rejects
non-inward travel and negative signed start distance; starting in front but within
radius returns zero fraction. The collider saves the **surface contact point**,
not the projectile center, and the actual face pointer at result+24. Liquid is
classified downstream using this pointer:49bf20..49bf42 invokes416260(face+28)
and converts the result to a0/1 liquid discriminator.

A fraction1 collision exists at this layer. Previously executed4a01b0 movement
commits that endpoint and skips the weapon contact callback, so the flight API's
endpoint handling remains justified. Do not reinterpret fraction1 as splash.

## Original room routing (disassembly, not whole-function execution)

`4df1c0` contains a separate liquid pass at4df5eb..4df65b:

1. Query+50 must include0x1000.
2. Room+184 contains_liquid must be nonzero.
3. Query AABB must overlap room+8/+14 bounds through507990.
4. Iterate room face list+28; call416260(face+28), and invoke4dec10 only for bit4.
5. Reuse nearest-hit output; querybit1 can stop at the first accepted hit.

Skyroom exclusion4df523..4df537 jumps **to this liquid pass**, bypassing ordinary
room geometry but not water. Treating skyroom exclusion as exclusion of every
surface would differ. Ordinary BSP traversal4deab0 invokes4dec10 on its stored
faces and has no separate bit4 rejection of its own. Whether all room BSP
construction excludes liquid is still unproved; do not claim the query traversal
itself enforces this separation.

## Surface ownership and depth

Primary independently inventoried94 installed levels:72 liquid rooms and1300
serialized faces with bit4, all belonging to liquid-marked rooms. This report did
not rerun that inventory; see primary's `tools/inspect_liquid_geometry.py`.

Original room liquid rebuild4cdc60 first removes old liquid faces from its room
list at4cdc71..4cdcbe, through4ce2a0 and4dfc70. Nonpositive depth clears the room
contains_liquid byte at4cdcc0..4cdcd3. If still liquid,4cdd98 computes
`height = room.minY + room.liquid_depth`, stored as float, with **no integer
rounding**.4cddb4 applies bit4 to the new face attributes.4cde1e..4cde76 constructs
four corners across the room bbox at this height,4cdf7f allocates the face,
4cdfa5..4ce025 adds four corners,4ce030 finalizes, and4ce047 calls4de690 with the
temporary solid and room for subsequent clipping/attachment. This last operation
has not yet been fully traced/executed here.

The rounded height in the previously inspected debris helper48fc10 is a separate
path and must not be copied into projectile room collision.

## Recommended reconstruction

Use authored liquid faces as bounded polygons and existing one-sided face/sphere
collision, gated by query1000 and the owning room's liquid metadata. Return face
flags/identity for liquid classification; preserve nearest solid versus liquid
ordering. Permit water pass despite the ordinary skyroom geometry exclusion.
Continue using the separate liquid-flight event and clear1000 after contact.
Never route water to terrain excavation. Keep dynamic water-level rebuilding
explicitly unsupported until clipping/ownership is established.

Remaining bounded task: trace BSP construction's liquid exclusion plus4de690's
clipping/attachment to prove how regenerated water faces stay out of ordinary
solid tree traversal. Polygon-edge sphere contacts were not independently varied
in this interior-contact matrix; no claim of exhaustive edge parity is made.

## Executed room-pass ordering and shared API

`liquid_room_routing.py` executes4df523..4df661 directly, supplying only
container operations and deterministic collider outcomes. Ten cases pass:
parent solid then detail solid then ordered water faces; query1 stops at the
first accepted pass; sky bypass still reaches water; query/room gates reject;
room AABB rejects; later equal-distance water replaces prior selection.
This controls routing, not additional geometry evidence. Its simulated collider
uses the separately proven inclusive nearest-fraction gate.

The additive shared `rf_collision_sweep_rooms_liquid` keeps old layouts/APIs.
Because reconstructed trees may contain liquid faces, its solid pass clears1000
so the existing face filter excludes those faces, then the separate water pass
uses the original query flags. This explicitly adapts reconstructed tree
ownership rather than claiming original BSP construction has been duplicated.
Standalone regression source is `tests/collision_liquid_room_tests.c`;
primary owns build/test registration and execution.

## Reconstructed integration

The additive room API, owned geometry adapter and live rocket route are connected. The query uses stable owned room markers and polygon views, including GeoMod overlay rebinding. The old APIs retain their layout/unsupported-query contract. Scene water entry routes to separate telemetry/audio request; only a terminal solid event enters blast/GeoMod.

`collision_liquid_rooms` passes ordering, sky, first/nearest/tie, disabled queries, duplicate suppression, callback rollback and endpoint cases. `tools/verify_liquid_world.py` checks1300 authored surface probes after source geometry closes:1058 water hits and242 nearer solids. Shared projectile continuation reports1058 liquid entries, including309 later solid hits in the same tick. This does not validate audible output or drawn ripples.

PC and NXDK builds pass. The900-frame dry DEV replay retains the exact8934-byte two-cut checkpoint and inspected output. Dynamic water-height rebuilding, drawn ripple VFX and native wet-scene validation remain open. Liquid iteration currently follows the owned tree's face order; exact original equal-distance face identity across reordered authored lists remains an integration limitation.
