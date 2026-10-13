# Live NPC translation against the player

Source baseline: `6b49c6666c738745cd599b32b40c1339f48ca544`.
Written October 12, 2026; source review only, not compiled or exercised.

## Missing operational path

The existing player-first adapter in `scene_player_npc_contact.inc` prevents
ordinary moving-player translation through qualified living humanoids. It
cannot stop an NPC that moves afterward from passing through a stationary
player. `campaign_script_step`, medic flight and guard retreat previously
queried only the ordinary world/mover/passive-hull/detached-piece sweep.
That missing reverse movement consumer is the complete scope of this slice.

`scene_npc_player_contact.inc` now composes the actual grounded player into
ordinary NPC patrol, pursuit, Goto/Follow_Waypoints, medic-flight and
guard-retreat translation. Every replacement uphill/slide proposal queries
again. Grounded mover/fragment carry also composes the player, after its
support-only exclusion is restored. The existing movement owners still
stop/slide, or truncate carry to the nearest contact. No new command, target,
navigation route, animation or speed policy is introduced.

## Original grounding

Installed RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The bounded disassemblies retained outside tracked source cover:

- `48bf35..48bf92`: NPC-first/player-second classification, including source
  object `0x20000` rejection, nonvehicle use and the actual asymmetric extent
  comparison. Both `48bf64` and `48bf71` compare twice the player's physical
  extent to the NPC extent. The new path calls the existing reconstructed
  classifier in this order and requires its exact `0x20` result.
- `49ab00`: strict 3D broad bounds, maximum actual physical sphere radius on
  each body, horizontal relative ray and inward-only initial overlap. The
  live adapter follows the existing player-first geometric-fraction policy;
  it does not copy the extra divide at `49ad33` into a world-sweep fraction.
- `49dd0b..49dd16`: the original dynamic-velocity branch skips the impulse
  when the contact object is the player and the current actor is an NPC.
  This slice preserves the existing kinematic stop/slide response and does
  not push either body, invoke damage or invent a reciprocal impulse.

The installed L3S1 guard1 UID109 and miner player class/body evidence already
recorded in [PLAYER-NPC-CONTACT-EVIDENCE.md](PLAYER-NPC-CONTACT-EVIDENCE.md)
establish actual ordinary comparable living owners. UID109 is evidence, never
a gameplay allowlist. Current radii, class, state and identities are always
read from the live bodies. The class navigation radius is not substituted.

## Identity, ordering and budgets

The adapter reuses the existing living ordinary-player and humanoid-NPC
admission helpers: exact generation-bearing object/entity registrations,
owned loaded NPC pose, living health, descriptor1, unlinked/unseated,
visible/non-disabled and supported physical flags. It then uses the reversed
original pair classifier and original parent/hidden response gate. A proposal
must begin at that NPC's actual committed position.

The player tick precedes ordinary NPC stepping. Each query therefore freezes
the player at its current committed position, regardless of whether it moved
earlier that frame. It never consumes leftover player `next_position` or
adds a second velocity projection. Player velocity plus support velocity is
copied only as contact metadata. Existing world contacts and exact ties win;
only a strictly nearer player hit replaces them. An initial lateral overlap
blocks inward movement but allows separation, retaining the existing shape's
approach rule rather than adding overlap repair.

The query is synchronous and callback-free, as are the intervening steering,
angular preparation and ordinary position commit owners. No retained receipt
or asynchronous player pointer is stored. There is one player candidate per
actual NPC query, fixed stack locals and no allocation, persistent field,
new actor scan, actor-pair pool, resource demand or save-layout change. The
NPC pose helper's existing bounded registry/model lookups are reused.

## Deliberate boundaries and status

Generic `rf_scene_npc_body_sweep` is unchanged. Stance clearance, teleport,
candidate restore, floor/support queries and airborne/gravity sweeps retain
their existing policy. No contact is published as NPC support or written
into a persistent collision bank. Ordinary NPC/NPC crowd blocking is not
introduced: original `48bfbb..48c05f` uses different eligibility, and ordinary
small humanoid pairs generally do not pass that path. Vehicle pushing,
mounted actors, swimming/flying, corpses and cutscene motion are outside this
bounded consumer. Existing explicit placements and initial-overlap restore
allowances remain unchanged.

The new geometry is a practical sequential live-movement adapter, not the
original global coupled contact scheduler or exact double-scaled timing.
Compile/startup verification belongs to the parent's next hourly stock64
rollup; functional coverage belongs to testers. No build, syntax check, test,
fixture, runtime, route, emulator, image, HDD operation or publication was
performed for this slice. No generated game payloads require cleanup.
