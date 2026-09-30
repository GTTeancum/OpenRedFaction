# NPC vehicle and turret seat assignment

2026-09-30. **Authored NPC seat assignment exists; the parser now exposes its
host UID, while runtime binding remains open.** It is a spawn/post-load UID binding, independent
of the player-enter/exit event polls. Implement that binding before inventing
proximity boarding. Four installed assignments resolve directly to entity hosts;
one is the L12S1 Jeep driver and three are stationary-turret operators.

This was a bounded static binary/data audit: no build, original-game execution,
Unicorn run, XEMU, screenshot or gameplay. Original `Installed_Game/RF.exe`
SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Static instruction reads corroborate retained Ghidra output; earlier seat
primitive probes establish attachment behavior separately.

## Original producer and consumer

| Address | Evidence |
| --- | --- |
| `0x4643d4..0x4643da` in entity loader `0x464010` | Read signed reference through `0x52c910`, version threshold `0x62`, default `-1`; retained export calls this `local_90`. |
| `0x464d9a..0x464dab` | Load that local and store entity `+0x750`. |
| `0x435f39..0x435faf` in post-load pass `0x435df0` | Walk live entity list; skip object flag bit2. If `entity+0x750 != -1`, call `0x4095c0(entity+0x2a0)` at `0x435fa7`. |
| `0x4095ca..0x4095e5` | Read controller `+0x4b0`, identical to entity `+0x750`; `0x48a4a0` resolves the authored object UID, then `0x426fc0` resolves its runtime entity handle. Missing UID returns without boarding. |
| `0x4095ef..0x4095fe` | Require a nonempty host seat array at `+0x8cc`. |
| `0x409604..0x40969f` | Select ordered seat and configure host look offsets; selection details below. |
| `0x40969f..0x4096a8` | Call common `0x427240(host, actor_handle, seat_tag)`. |
| `0x4096ad..0x4096b4` | Request actor action13 through `0x407e20`. |
| `0x4096b9..0x4096dc` | Unless host `+0x814` bit`0x4000`, request host action2; set host `+0x810` bit`0x10000`. |

Retained loader output: [464010.c.txt](../../artifacts/analysis/rf_b8fb9ab4c9bf/464010.c.txt).
UID lookup `0x48a4a0` compares object `+0x20` against the supplied UID, then
returns that object; it does not pick the nearest vehicle.

For a player-flagged actor (`0x4895d0` tests object flags bit8), the routine
chooses seat ordinal0 and configures host look offsets from class fields
`+0x6c/+0x78`. For an NPC, it chooses ordinal1 only when there is more than one
seat **and** `0x429990(host)` is false; otherwise ordinal0. `0x429990` compares
the `0x486c90` model-kind query to1. The installed Jeep is static model kind1,
so its NPC uses ordinal0 / `interface_1`, not the gunner seat. The NPC branch
clears the first words of host look-offset fields `+0x1434/+0x1440`.

The original caller does not check the attachment return before changing AI
actions. For a coherent port transaction, validate unoccupied ownership first
and publish action13 only after attachment succeeds. This is an explicit
implementation policy, not a claim of the original failure ordering.

## Installed campaign data

Read all **68 RFL entries / 1,610 entity records** from `levels1.vpp`,
`levels2.vpp`, `levels3.vpp`. Seven records have this signed reference other
than `-1`. Offsets below are relative to the beginning of the named RFL entry,
not to its containing VPP. Complete result:
[npc-seat-assignment.json](../../artifacts/secondary-re/vehicles/npc-seat-assignment.json).

| Archive / level | NPC UID / class | Host UID / class | NPC record offset | Host-reference field offset |
| --- | --- | --- | ---: | ---: |
| levels1 / L3S2 | 936 / guard1 | 935 / Stationary Turret | 1941465 | 1941637 |
| levels1 / L5S1 | 3023 / guard1 | 45 / Stationary Turret | 1434930 | 1435103 |
| levels2 / L12S1 | 7646 / miner1 | 7629 / Jeep01 | 4830359 | 4830512 |
| levels3 / L19S1 | 9882 / miner1 | 9877 / Stationary Turret_Plain | 1077877 | 1078058 |
| levels1 / L5S1 | 3040 / guard1 | 0 / unresolved | 1435348 | 1435519 |
| levels1 / L5S2 | 3926 / guard1 | 0 / unresolved | 3637204 | 3637376 |
| levels2 / L7S3 | 3959 / env_guard | 3958 / unresolved | 3576540 | 3576713 |

Unresolved means no entity with that UID in the same RFL entity section. Do
not infer replacement hosts, treat UID0 as the player, or synthesize a missing
seat. The original UID lookup is broader than the entity array; a non-entity
or dynamically supplied object has not been ruled out by this census.

### Exact parser hook

`src/core/level.c::entity_spawn_read` reads an 18-byte block after
the seven post-vitals strings. In installed v180 records it contains:

- bytes0..1: two AI bytes;
- bytes2..5: scripted-action count, zero in every scanned record;
- bytes6..9: **signed initial seat-host UID**, default `-1`;
- bytes10..13 and14..17: two other references, outside this task.

Implemented `int32_t seat_host_uid` in the named `rf_level_entity_spawn`
projection in `include/rf/level.h`; it reads little-endian at the validated
block start+6 with the existing full-record bounds/exhaustion checks. The
existing minimal parser fixture now includes absent(-1), unresolved0/3958 and
Jeep7629 values; it has not been built or executed in this helper turn.
Do not overload
`relationship_51c`, `friendliness`, a script event link or the support UID.
Nonzero scripted-action lists need their actual variable-size parser before
assuming that fixed offset; the current v180 parser already assumes they are
empty, and this audit checked that assumption for the installed SP corpus.

## Separate automatic child source

Factory `0x422360`, at `0x423974..0x4239d4`, compares the class name with
`"auto turret"` (literal `0x595aac`), resolves `"auto turret head"`
(`0x595ab8`) through `0x4251c0`, creates that child at the host transform,
clears child `+0x1f8`, sets child object flag`0x100`, then attaches the child
to the host's first seat. This is a real source of a child occupant, separate
from a skeletal NPC operator. The installed corpus contains19 Auto Turret
bases across9 levels; JSON lists every UID and RFL offset. Current explicit
AutoTurretHead support alone does not establish that base-driven creation.

Five direct-call candidates to common attach were identified and disassembled:
`0x405766` (AI update `0x405540`), `0x4096a8` (post-load assignment above),
`0x4239d4` (Auto Turret child), `0x4a1e41` (player Use), and `0x4b4ed1`
(restoration function `0x4b4c80`). The AI-update and restoration paths retain
additional lifecycle behavior; this bounded audit does not claim complete
reconstruction of them or exclude indirect calls.

## Minimal implementation sequence

1. Decode the named field, then perform one binding pass after all skeletal
   NPCs, static hosts and ordered model seat tags are registered. Resolve only
   the authored UID; preserve unresolved references as diagnostics.
2. Start with L12S1 miner7646→Jeep7629 and the three turret operators above.
   Reuse authored `interface_1` tags and the existing core ownership/math
   contracts. Retain generation-checked actor/host handles and one owner for
   each seat. The Jeep player-only `entry.occupant`/driver state must become
   an ordered live seat bank; never claim the player token is an NPC driver.
3. Put the seated NPC in action13, zero inherited free-body velocity, publish
   its body/eye from the host tag once per frame, and skip ordinary free NPC
   gravity/navigation. Propagate Set_AI_Mode to eligible occupants using the
   already recovered event semantics. Route driving and weapon firing must
   respect the actual seat occupant; turret AI currently treats occupied
   hosts as inert, so binding without an occupant firing/control adapter would
   break these placements.
4. Before removing an actor/host or changing level, detach both directions;
   exclude the seated body from self-collision. Extend checkpoint ownership
   using host UID, occupant UID and seat tag, and stage both endpoints before
   publication. Never persist runtime handles or restore action13 unbound.
5. Handle Auto Turret base→head creation as a separate typed child-owner slice;
   it needs synthetic stable child identity and host-relative pose/lifetime,
   rather than inserting a fake skeletal NPC.

Useful existing contracts: [seat attach](FUTURE-VEHICLES-SEAT-ATTACH-20260915.md),
[authored tags](secondary-re/vehicles-authored-seats-20260916.md),
[occupant pose](secondary-re/vehicles-occupant-pose-20260916.md), and
[AI mode propagation](FUTURE-CAMPAIGN-AI-MODE-UNDERCOVER-20260915.md).
No unused seat owner implementation was added by this research task.

## Parser implementation check

The named signed `seat_host_uid` is now decoded without changing record cursor
advancement or resolving references. The compiled NXDK parser matches six fields
against independent installed-byte decoding for 1,610 entities in 66 sections
(`artifacts/entity-spawn-verification.json`, `--xbox-only`). No PC binary ran.
This establishes parser output, not live boarding or driving; those require
the post-registration ownership and control adapters. The legacy verifier now
accounts for the full24-byte spawn result and supports Xbox-only checking.

## Jeep save audit and detached corpse admission (2026-09-30)

The live L12S1 miner7646 / Jeep7629 ordinary-save implementation is already
connected. `scene_npc_seat_checkpoint_hosts` reconstructs the driver's tag
transform from the pending RFCP Jeep pose. RFNC restore applies that basis and
position with a narrowly admitted no-floor seat; the world vehicle pair checker
allows only its own staged occupant overlap. Parked host publication retains
the fresh authored pair until every stage is admitted. RFNS detach/assign then
replaces the pair with saved ownership, and RFVR assign plus
`campaign_vehicle_mode_publish` restores the route index, active bit and AI mode
without dispatching Follow_Waypoints or Set_AI_Mode again. The existing
`scene_npc_seat_driver_admits` gate keeps an absent/dead driver from advancing a
retained route. An inactive route with a saved waiting/catatonic action remains
suspended. This is source integration evidence, not a new native Jeep save run.

The concrete remaining admission hole is a settled detached driver's corpse
still occupying the Jeep's seat volume. RFNS correctly saves `active=0`, but
the ordinary vehicle pair checker consequently rejects that overlap. New
`src/diagnostic/scene_npc_jeep_seat_save_admit.inc` supplies a read-only predicate
for that contact. It requires the inactive authored RFNS actor/host/tag identity,
a pending RFNC terminal corpse (not retired/hidden, health <=0, no action13,
orders or support), and matching published corpse position/basis at the tag
computed from the **saved** Jeep pose. Position tolerance is .001 and basis
tolerance is .0001, matching the existing active-seat checks. Generation checks
resolve immutable tags only; fresh actor health never establishes saved death.
Displaced bodies, moving-support bodies and arbitrary live detached actors keep
ordinary clearance. No ownership, AI, route or event state is changed.

Exact parent integration (the helper does not edit these shared files):

1. Include `scene_npc_jeep_seat_save_admit.inc` immediately after
   `scene_npc_seat_checkpoint.inc` in `scene.c`; both are before
   `scene_world_vehicle_restore.inc`, so this predicate needs no early prototype.
2. Add `const scene_vehicle_checkpoint_record *record;` to
   `scene_world_vehicle_pair_context`, and assign `pairs.record=record` in
   `scene_world_vehicle_prepare` beside `pairs.seats=seats`.
3. In `scene_world_vehicle_other_pair`, retain the existing player case and
   bounds guard. For `j>=2`, return the existing
   `scene_npc_seat_checkpoint_owns(pairs->seats,pairs->host_uid,
   pairs->npcs->entries[j-2].saved.uid)` **or**
   `scene_npc_jeep_seat_save_dead_contact(pairs->seats,pairs->npcs,
   pairs->record,pairs->host_uid,j-2)`.
4. Keep RFNS inactive through `_admit`, `_detach` and `_assign`; do not set
   `pose_ready` or `active` for this contact. All existing NPC/world placement,
   corpse playback, non-Jeep pairs and final transaction validation still run.

This slice changes no wire format or scratch budget and does not loosen floor
or static-world placement. RFNC's existing yaw-only detached-body representation
still cannot preserve an independently tilted detached corpse; displaced or
falling corpses remain outside this narrow exception. No build, emulator run or
gameplay script was performed in this helper slice. Parent owns integration and
proportional Xbox verification.

### Bounded detached-driver save harness

`python tools/xemu_npc_jeep_detached_save.py` reuses the installed L12S1
miner7646/Jeep7629 records in the enemy-free CTF06 fixture. Ordinary setup
Set_AI_Mode at frame0 parks the Jeep; Slay_Object at frame60 detaches its driver.
It runs240 neutral source frames for corpse settling, writes a real ordinary
world save, then boots fresh for12 neutral frames with both setup events removed.
There is no route activation, desktop input, screenshot or checkpoint editing.

The harness checks RFNS inactive ownership, RFNC terminal death and settled
playback, RFVC unoccupied Jeep pose and RFVR catatonic mode. The independent
installed model tag must match the saved corpse position within .001. A mismatch
is reported as a fixture limitation, with a proposal to delay Slay through
another ordinary parked-mode setup event, rather than manufacturing save bytes.
Restore assertions cover actual empty actor/occupant/driver links, dead health,
no route-driving ticks, one restored corpse and no repeated Slay/death audio.

It requires the parent-owned `rf_scene_npc_jeep_detached_probe[16]`, refreshed
after seat processing: actor UID, host UID, actor handle, host handle, health
bits, actor AI mode, actor linked handle, Jeep occupant, Jeep driver, seat active,
route active, route AI mode, actor position(3 words), valid. Unlike the older
active-seat pose probe, this must sample inactive bindings too. AST parsing and
`--prepare-only artifacts/fixtures/npc-jeep-detached-save` pass; no build or
emulator was run by the helper. Parent owns the one native save/load cycle.

Detached Jeep corpses may settle within0.25 world units of the former authored seat tag; this narrow pair exception keeps the pending dead actor/Jeep overlap admissible after ordinary gravity while farther displacement still uses normal collision clearance. A first Xbox fixture at `artifacts/xemu/npc-jeep-detached-save-20260930-141324` rejected the former exact-position rule: the corpse had moved0.153 units from the tag. The revised envelope needs native continuation.

A stock64MiB Xbox retry loaded the same real ordinary save after the seat-envelope and independent-corpse-basis fix: `artifacts/xemu/npc-jeep-detached-load-retry-20260930-01/validation.json` passes source240/load12 with former driver UID7646 health-1, inactive seat, empty occupant/driver links, parked mode1, no route activity or repeated death. The source is `artifacts/xemu/npc-jeep-detached-save-20260930-141625/save/result.json`. Its original full harness report remains FAIL (pre-fix load); the retry reuses the saved HDD and verifies disc restoration. The now-native-verified exception admits only this authored former driver within0.25 units of its saved seat position; it does not bind corpse rotation to the pitched Jeep. Moving routes and broader bodies remain open.
