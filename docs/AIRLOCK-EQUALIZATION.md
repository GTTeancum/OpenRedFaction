# Bounded live airlock equalization

Source-staged against `6a3d0446dd9885f799ac0430235abd11e3e8a389` on 2026-10-10.
Independently source-reviewed; compilation and all new action/save/load behavior
remain unverified. No build,
test, gameplay fixture, campaign route, emulator run, image or asset edit was
performed for this slice. See `AIRLOCK-EQUALIZATION-SOURCE.md` for original
addresses and the exact authored L11S3 consumer; RFTC2 is a separate prerequisite.

## Scope and evidence

This closes the scene's previously immediate airlock dispatch for a supported
paired chamber. Original L11S3 has triggers 2658 and 6298, chamber UID 2453
(room index 52), four two-key translation door controllers, and an actual
Load_Level link from 6298 to L12S1. Controller key UIDs are not room UIDs.

Original object+3c corresponds to `rf_group_runtime_entry.pose.public_position`.
The first-key factory chain and exact original closed coordinates put the
selected controllers strictly inside the source door bounds, but runtime
containing-room lookup has not been executed or verified. Startup admission
requires its actual hit in the expected adjacent room. No bounds-only match
or hard-coded L11S3 identifier can stand in for that query.

`scene_airlock.inc` conservatively supports exactly two peer triggers per
chamber, linked two-key translations, two chamber portals, and two distinct
adjacent door rooms with portal counts (1,2) or (2,1). Every linked controller
on a trigger must initially resolve to its same door room; peers must represent
opposite door rooms. Non-profile topology retains the existing peer-closed interlock behavior; it
does not receive guessed pressure state. A recognized topology whose controller
room cannot be proved stays explicitly blocked without rejecting an otherwise
valid level. Allocation/corruption failures retain ordinary error handling. The existing stricter all-peer/all-controller
closed-key0 interlock is unchanged; the original tests fewer peers/controllers.

Original 4ccf80 initializes dynamic pressure to the inverse of the far-side
room's static outside bit for this admitted topology. Selected L11S3 initializes
pressure 1. The immutable serialized room byte30, original room+42, remains
unchanged. Existing outside damage and its gasp owner remain unchanged.
No oxygen system or equalization sound/polish is added.

## Live state and dispatch

- An allocated scene-local owner retains at most 32 chambers, 64 triggers and
  128 controller bindings. Its complete fixed payload is charged alongside a
  temporary portal graph against a 128 KiB startup peak budget. The graph is
  immediately freed. The source-derived Xbox32-bit owner is 5420 bytes and its
  mission before/next candidate is 10844 bytes. Together with RFTC2's 32768-byte
  ledger extension, incremental resident payload is 38188 bytes and combined
  resident plus restore-stage payload is 81800 bytes, excluding small globals
  and allocator overhead. Existing world-file and load budgets remain binding.
  No gameplay-time allocation occurs.
- Controller bindings retain complete generation-qualified handles. Ordinary
  current collision-world committed-pose containing queries update a cached room on a qualified hit;
  a miss retains the previous room, as original 48a190 does. An unknown initial
  room remains unsupported. The additional strict-interior gate is an explicit
  conservative port restriction, not an original airlock predicate.
- Accepted contacts still pass the existing actor/contact/Use/limit/cooldown
  gates once. Side selection uses the global local player's committed body
  room, not the contacting NPC's room. This body query is an explicit port
  frame-phase adaptation of the original global player's retained room.
- When the player is in the chamber, the first authored linked controller's
  retained room selects the side. Its one-portal room always requires a wait;
  its two-portal room compares the far-side static outside bit. Otherwise the
  player's room provides that bit. Equality with dynamic pressure requires
  equalization. Unsupported cached-side topology stays blocked.
- Acceptance stores the complete actor handle unchanged and arms 1000 ms.
  Own or peer pending work blocks new acceptance. No activation count, ordinary
  trigger cooldown or linked effect changes at acceptance.
- A once-per-simulation service runs after clearing the frame trigger flag and
  before contact polling. The first service after the 1000-ms expiry separately
  arms 50 ms and returns. A later service after that expiry completes without
  a new contact/Use check, actor lookup, liveness check or disabled-state check.
- Completion proves that the original trigger itself still owns its full
  handle and authored chamber. A removed/replaced trigger retires its receipt
  without changing pressure or invoking effects, then fails closed. Accepted
  actor removal/death does not invalidate the actor token.
- Completion consumes its receipt before callbacks, toggles pressure exactly
  once, and uses the existing `rf_runtime_trigger_fire_links` shared activation
  path. That path owns ordinary activation count, cooldown and ordered effects.
  Reentrant acceptance/save is blocked while dispatching; the accepted trigger
  handle is copied before dispatch, and owner pointer plus generation are
  checked before any post-callback owner access. Partial callback
  failures are not replayed and retain an explicit fault guard.
- Original 4bf660's linked type22 different-level test suppresses movers while
  events and other ordinary links still dispatch. Port comparison first uses
  existing Load_Level normalization (.rfl/.d4l/bare targets), then compares
  case-insensitively with the current archive level name. L11S3 trigger6298
  therefore retains its actual Load_Level event and suppresses its door links.

## Persistence and lifecycle

The separately staged RFTC2 codec appends `airlock_chamber_uid` and
`airlock_pressure` to each retained trigger row. Pressure 0 with chamber0 is
legacy/unowned; 1 encodes dynamic0; 2 encodes dynamic1. The codec remains
responsible for structural validation and agreement between explicit rows.

Production section capture, actual world snapshots and the existing optional
checkpoint probe explicitly overlay scene pressure after the generic trigger
capture zero-initializes its extended state. Every supported peer gets the
same chamber UID and pressure, including disabled/retired peers. The complete
session ledger is retained; existing checkpoint capacities are not enlarged.

A save is temporarily rejected while a wait, reentrant dispatch or dispatch
fault exists. An ordinary section exit remains in the scene until its wait
finishes, with a second guard before section-history mutation. A consumed
Load_Level callback can enqueue its own exit. Explicit quickload and fresh
restart discard the old timeline and bypass this handoff wait. Changed pressure
itself is never a save prohibition.

The controller room cache is unsaved. Save admission re-queries each current
controller pose and requires an actual hit in exactly its cached room; a miss
or disagreement rejects that transient endpoint. This prevents an unsaved
retained-room miss from changing side semantics on reload.

`scene_world_mission_prepare_with_movers` receives the already staged saved
controller entries from `world->movers` and candidate static collision from
`world->world.world`; the original current-world preparation
API remains available to existing source callers/probes. Its airlock candidate:

1. Validates every explicit current-level row against an actual source-admitted
   trigger and exact chamber UID, rejecting mixed legacy/explicit peers or
   disagreement. Other-level pressure remains in the complete ledger.
2. Initializes legacy current-level pressure from the original source default,
   never from mutated live pressure.
3. Reconstructs controller caches from candidate saved controller public poses,
   requiring actual qualified room hits and unchanged source/handle binding.
4. Retains and validates a complete before-image of the live airlock owner.

Preparation and the full loader's `scene_world_mission_assign_owners` do not modify live pressure,
controller caches, pending work or fault state. Only full successful world
publication AND successful storage close install the candidate and clear its
old-timeline waits/fault. Rejected preparation and storage failure preserve
this owner. No sound or gameplay dispatch occurs during restore. The existing
whole-world loader's broader publication/rollback policy is not expanded.
The existing standalone `scene_world_mission_assign` remains a complete
component commit by combining ordinary owner assignment with airlock assignment.
Runtime queries follow published scene collision overlays, whose existing
contract preserves authored room indices; they do not pin the startup collision
allocation or dereference an obsolete overlay.

Section initialization reconstructs source topology before startup events and
restores the section's retained pressure alongside its normal trigger history.
Section close frees the owner. Successful same-level load replaces the entire
RFTC ledger, so legacy loads cannot inherit omitted pressure from a later save.

## Integration surface

New scene-private services:

- `scene_airlock_open/close`: stable resolved-trigger/controller scene lifetime
- `scene_airlock_tick(world,now,frame,particles)`: independently serviced accepted work
- `scene_airlock_admit(trigger,actor,now,&ready)`: immediate/deferred contact gate
- `scene_airlock_checkpoint_admit(world)`: transient/cached-room save preflight
- `scene_airlock_capture`: populate one generic captured trigger row
- `scene_airlock_restore_prepare/validate/assign`: candidate-only pressure/cache
  transaction, assigned only at the existing post-storage-success boundary
- `scene_airlock_history_restore`: section revisit/default application

No new public core API, format envelope, actor identity mapping, static geometry
mutation, standalone test fixture or test runner is added. The parent owns the
scheduled Xbox compilation and any bounded consumer validation. Actual L11S3
locator admission, delayed interaction, link suppression, cache continuity,
RFTC2 round-trip and legacy restore remain unverified until that batch.


## 14:00 build evidence

The 2026-10-10 14:00 Xbox build compiled this source at `1fd5bfba`. The accompanying original L3S1 neutral startup passed on stock 64 MiB, but that level has no authored airlock. Airlock action, room-owner admission and pressure/save restoration remain runtime-unverified. See [the hourly report](HOURLY-20261010-1400.md).
