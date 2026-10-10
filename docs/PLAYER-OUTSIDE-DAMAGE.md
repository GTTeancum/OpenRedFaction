# Ordinary player outside-environment damage

Status: source-written on 2026-10-10 against
`4f4bca2adaba9ef750e2c24f93935f936f4f488b`, staged outside the active repository
for parent integration and the 11:00 UTC Xbox batch. Independent source and
documentation review passed with no blocking findings. Compilation and runtime
are unverified. No build, syntax check, test, new fixture, emulator run, PC run,
campaign route, campaign scan, original-input change, or cleanup was performed
for this slice. Parent owns integration and validation.

## Original contract

The independent original-input review identified this missing consumer of
`RF.exe` function `421170`. The general entity-update caller invokes liquid
damage `421240` at `41e549`, then outside damage `421170` at `41e54f`.
The original function is not an underwater breath timer:

- For an actor with local-player object flag `8`, positive current armor
  returns immediately. Class armor, appearance and actor `+810` suit bit
  `0x40` are not this condition.
- The actor must have a retained room with static outside byte `+42` set.
- Class `+724` must contain humanoid bit `0x20000`; class `+728` mutant bit
  `8` must be absent.
- The requested amount is `10 * frame_seconds`, stored as binary32. Damage
  kind is `10`, source and auxiliary UID are `UINT32_MAX`, and argument 6 and
  force are zero. The request belongs to the existing shared damage service.

Original loader `4eda1f..4eda2a` copies serialized room byte `30` to retained
static room byte `+42`. Predicate `428b50` reads `+42` directly. This is not
dynamic airlock pressure: `4ccf80` initializes separate `+44` from `!+42`,
`4cd064..4cd07b` derives `+44` from adjacent static outside rooms, and
`4bf854..4bf866` toggles `+44`. Consequently there is no fabricated airlock
exemption, byte-31 rejection, or dynamic pressure approximation here.

The source review found original L2S1 room `106`, UID `3229`, with serialized
byte `30 = 1`. This establishes an authored outside room only. No permitted
runtime exposure route was established, and there was no forced room/armor,
teleport, fixture, asset edit, or authored-encounter run. That evidence is not
proof of this adapter's live damage.

## Scene adapter and ownership

`src/diagnostic/scene_player_outside_damage.inc` adds a single allocation-free,
stateless gameplay adapter for the ordinary living local player. Qualification
requires all of the following in the current simulation step:

- An allocated player body and type-0 player registration/view.
- The full generation-qualified handle agrees across the registration,
  compact entity view and player damage owner.
- Both the object registry and entity registry resolve to those exact owners.
- Local-player bit `8`, positive current health, no removed/excluded object
  flags `2`/`0x4000`, and no dead entity bit `1`.
- No player attachment, active vehicle, operated stationary turret or active
  cutscene.

The attachment, vehicle, turret and cutscene gates are explicit bounded-port
scope. They are not claimed exemptions in original `421170`. The general
NPC consumer of outside damage and mounted-player ownership remain outside
this slice. Standing on an ordinary support does not by itself suppress this
on-foot consumer.

The adapter reads current armor only after the existing liquid tick finishes.
Liquid damage that drains armor can therefore make the same step eligible;
liquid damage that kills the player is declined by the living-owner gate.
Humanoid and mutant flags come directly from the retained damage owner,
`campaign_player_damage.class_flags` and
`campaign_player_damage.state.effects.class_flags_728`.
The implemented `miner1`, `parker_suit` and `parker_sci` forms have identical
installed class Flags including humanoid, and their Flags2 contain only
`collide_player`. Existing form transitions do not replace those class words;
for these implemented forms they remain correct. The actual transition's
current armor is authoritative, regardless of its model, form choice or
actor suit bit. Broader class-changing forms would need their own correct
class owner before this consumer could claim support for them.

Room selection is the actual committed body position through existing
`scene_player_room_locate(stream, 0, scene_actor_body.state.position, ...)`.
It never borrows the final camera, swim diagnostic, support, attached host or
an arbitrary room. The existing body cache is topology/position qualified.
Missing geometry/data/offset storage, a missing room, indices outside either
the collision or geometry room count, and truncated/out-of-range fixed room
headers decline exposure. The offset check uses `offset <= bytes` and
`bytes - offset >= 40`, avoiding addition overflow before reading byte `30`.
Successful geometry loading already validates the variable record tails;
this adapter does not reparse them. Other room-query failures propagate.

The current post-movement body-room sample is an explicit first-playable
phase adaptation. Original `421170` consumes retained room state before the
later original refresh. Exact original cached-room phase parity is not
claimed, and no player room ownership or geometry data is rewritten here.

## Dispatch and lifecycle

The sole simulation hook is immediately after successful
`campaign_liquid_damage_tick` and before the existing phase-1 profile mark.
It runs once in the same ordinary simulation block, outside rendering and
contact substeps, under the existing modal scheduler. Its include follows
the liquid tick definition, where the combat callbacks and damage service
are already available.

The call uses `rf_scene_player_damage_audio`, the current `particle_now`, the
same `(frame + 1) * scene_step_seconds` float clock as liquid damage, the
existing combat callbacks, and borrowed `combat_pain_random`. The new code
does not draw randomness or directly mutate vitals. Existing invulnerability,
class factors, armor/damage rules and error propagation remain authoritative.
Original kind-10 ordinary pain-sound and player-flash suppression remain in
the shared damage service. The normal next-step `campaign_life_input` owns
lethal handling, input suppression and existing restart/load behavior;
there is no second death path or immediate injected restart.

No oxygen depletion, underwater timer, hazard deadline, immunity grace,
accumulation, new save/checkpoint fields, or new retained gameplay owner is
introduced. Existing vitals and damage state retain their consequences
through the established save paths. Optional original gasp audio is a
separate deferred consumer, not substituted with ordinary pain audio here.

## Read-only diagnostic record

`rf_scene_player_outside_damage[16]` is 64 bytes of telemetry only:

- 0: simulation ticks
- 1: qualified living ordinary player owners
- 2: positive-current-armor rejections
- 3: nonhumanoid/mutant rejections in an outside room
- 4: missing/out-of-range/truncated room declines
- 5: actual outside-room memberships after the armor gate
- 6: shared-service requests
- 7: positive shared damage results
- 8/9: last requested full player handle and zero-based room index
- 10/11/12: requested amount, applied amount and resulting health, float bits
- 13: last request frame
- 14/15: returned-error count and latest tick status

Frame 0 resets only telemetry. Words 8, 9 and 13 start at `UINT32_MAX`;
the last-request words 8 through 13 persist across dry or armored ticks.
The array is not consulted for admission and adds no save payload or
same-frame scheduler. It performs no grants or injections. Parent may expose
it through existing diagnostics; no extra serial-output consumer is added.

## Remaining verification

Independent source/documentation review passed; the scheduled parent Xbox
compile is still required. A normal neutral startup can establish
compilation and bounded noninterference, but cannot prove outside exposure,
armor depletion ordering, lethal handling or save continuation. Actual
outside-damage and optional audio behavior remain explicitly unverified.
No new runtime route, setup mutation or per-slice fixture is requested by
this patch.
