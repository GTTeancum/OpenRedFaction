# Authored Jeep driver check

2026-09-30: **Prepared, not executed.** The helper added only
`tools/xemu_npc_jeep_seat.py` and these notes. No fixture preparation, Python
execution, build, emulator, image capture or PC test was run in this task.
Parent retains serial build/native ownership.

## Scope and evidence

The harness copies complete installed `levels2.vpp/L12S1.rfl` records for
`miner1` UID7646 and `Jeep01` UID7629 into empty CTF06 geometry. Only their
transforms change; the original signed seat reference, class, vitals and
loadout remain intact. Preparation checks the original link and exact
non-transform bytes, then re-decodes the copied records. The installed
assignment is documented in [the seat audit](research/NPC-VEHICLE-SEAT-ASSIGNMENT.md).

The disposable archive names this level `L12S1.rfl` because the current
vehicle selector admits that exact level/Jeep UID pair. This is explicitly a
fixture alias, not a campaign traversal. The Jeep starts six units along Z
from the CTF spawn and 0.8 units higher; normal rigid support owns settling.
This placement is unverified until the native run. Actor staging uses the
installed `Jeep01.v3m` first-LOD `interface_1` attachment position, read from
its actual model record. Runtime seating must then republish the model tag.

Existing process-local setup dispatch permits `Follow_Waypoints` type28 and
`Slay_Object` type1 (`scene.c::scene_fire_setup_event`). Xbox
`player_poll_paced` fires the first setup UID at frame0 and its second at
frame60. The fixture therefore needs no new scene behavior: a synthetic
one-node route starts at frame0, and Slay targets the real driver at frame60.
The point is 100 units away so command inhibition can be distinguished from
route completion over this short run. Navigation, waypoint and event bytes
use the existing installed-format readers/encoders; preparation performs a
structural navigation and level round trip.

## Intended native assertions

The standard runner uses neutral process-local RFI6 input for180 frames on
the isolated harness HDD, samples memory before frame60, and reads final
memory. It does not send host input or capture images. The pre-death sample
must occur at frame15..59; missing that window is a failure, not an inferred
pass.

- One requested/attached/live seat, UID7646 linked to UID7629, action13,
  positive finite health, actual model-tag index and finite body position.
- Repeated successful seat updates verify the physical pair: the existing
  `scene_npc_seat_pair` checks actor parent, host occupant and Jeep driver
  handles together before each update. This avoids guessing runtime C ABI
  offsets to read the driver pointer separately.
- No player vehicle session, handheld NPC shots, turret operator or runtime
  seating/vehicle errors.
- A live route issues commands before death. Normal Slay records exactly one
  death for UID7646; the seat detaches once and has zero active occupants.
  The route remains active for the same event and host, but its command count
  is bounded at frame60(+1 for scheduling). This checks driver-dependent
  command inhibition, not immediate physical stopping; momentum may remain.
- Guest reaches phase5/frame180 with64MiB and free pages. This is completion
  context; the functional state assertions above determine the result.

Optional `--link-only` leaves the actor alive and instead requires continued
route command generation and attachment through frame180. It also compares
the final actor-to-host distance against the actual model attachment offset
length. This checks model-derived placement without claiming full orientation
equality. The default death run cannot use the final actor position for this
check because the actor has detached.

## Parent commands after current native work ends

From the authoritative repository root:

```text
python tools/xemu_npc_jeep_seat.py --prepare-only artifacts/npc-jeep-seat-prepared
python tools/xemu_npc_jeep_seat.py
```

The optional living-driver alternative is
`python tools/xemu_npc_jeep_seat.py --link-only`; it is not a required second
run after a successful bounded death check. Reports and copied asset recipes
stay under ignored `artifacts/`. The runner restores disc flags and repacks
the original disc in its finalizer, and refuses to launch beside an existing
project XEMU session.

This harness does not cover seated Jeep save/load, player takeover,
dismounting, broad route traversal, visual animation, audio or host death.
It makes no claim that those systems are complete.

## Native execution

`artifacts/xemu/npc-jeep-seat-20260930-133204/report.json` passes the default
180-frame stock64MiB run. The live probe verifies the authored driver pair,
action13 and repeated seat publication. Normal Slay detaches the driver once;
subsequent autonomous route commands stop while the route remains retained.
Disc inputs restore successfully. This establishes driver-dependent control,
not immediate braking, inspected animation, player gunner coexistence or saves.
