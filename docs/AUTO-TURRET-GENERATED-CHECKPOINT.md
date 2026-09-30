# Generated Auto Turret checkpoint companion

Status: isolated checkpoint primitive plus pending-world adapter
`src/diagnostic/scene_turret_generated_checkpoint_adapter.inc`. The adapter is
ready for parent integration; it has not been built or runtime-verified.
RFTU1 now uses the authored owner prefix and rejects generated target aliases.

## Format and ownership

RFTU2 is an outer envelope around an authored-only RFTU1 payload, or an ordinary
vehicle payload when there are no authored turrets. This preserves the existing
RFTU1 row layout and validation rules. It does not send version 2 to a version 1
reader. There is at most one generated envelope.

| Offset | Encoding |
|---|---|
| 0 | Four bytes RFTU |
| 4 | u32 version 2 |
| 8 | u32 inner byte count |
| 12 | u32 generated row count, 1–128 |
| 16 | Existing inner payload, unchanged |
| 16 + inner bytes | Generated rows, 216 bytes each |

Generated row layout:

| Offset | Encoding |
|---|---|
| 0, 4 | u32 base authored UID, generated role 1 (AutoHead) |
| 8–191 | Existing 184-byte turret scalar layout; UID is UINT32_MAX, never an authored child UID |
| 192 | f32 base health |
| 196 | u32 base AI flags_7d0, including readiness |
| 200 | u32 base flags_810 for death consistency |
| 204 | u32 coupled death dispatch reason: 0 live, 1 base death, 2 head death |
| 208 | u32 target role: 0 ordinary UID/player/none, 1 generated AutoHead |
| 212 | u32 head AI flags_7d0 |

For target role 1, the old scalar target_uid field contains the target head's
base UID. For role 0 it retains the ordinary UID/player 0/UINT32_MAX none
convention. The generated role is separate from the UID namespace. No registry
handle, owner index, binding pointer or allocation address is serialized.
Class index remains a catalog compatibility check, as in RFTU1; parent must
retain a deterministic dependent-class catalog for the same checkpoint source.
The enclosing world checkpoint supplies its ordinary identity/checksum.

The companion owns generated head state and the base's generic AI flags because
the current NPC checkpoint restore does not retain flags_7d0. NPC checkpoint
ownership of base health, death, pose and ordinary NPC state remains unchanged.
Assignment restores the saved base AI flags into both view.flags_7d0 and its
firing.flags_7d0 mirror, after NPC assignment. Head health, armor, flags, actions,
weapons, independent basis/mount, angular velocity/force, target, burst and
remaining fire delay are retained. Fire delay rebases to the supplied restore
frame and the last-frame guard resets, preserving a pending action transition.

## Parent hooks

1. Include the companion after generated-head helpers, turret combat, ordinary
   turret checkpoint helpers and scalar codecs. No include or call was added
   to scene.c in this slice.
2. Keep generated owners as a stable suffix of scene_turrets. Extend existing
   RFTU1 capture/prepare/validate loops to use an explicit authored owner count
   and mapping instead of the total count. Its UID-based row validation still
   applies unchanged to authored rows. Do not temporarily change the global
   scene_turret_count, fabricate generated UIDs, or feed linked heads through
   scene_turret_checkpoint_capture_row. Authored targets that point at a
   generated head remain unsupported by RFTU1's target codec; reject that save
   explicitly until an authored-target extension exists.
3. Capture the authored/vehicle payload first, then call
   scene_turret_generated_checkpoint_capture(buffer, capacity, inner_bytes,
   frame, scratch_budget, &written). Rows are captured into bounded scratch
   before any caller bytes change. Zero generated rows leave the inner payload
   untouched. Take both NPC and turret snapshots at the same paused simulation
   boundary so base health/readiness cannot change between them.
4. Boot readers first call scene_turret_generated_checkpoint_inner. Then pass
   its inner bytes to scene_turret_checkpoint_inner and the existing vehicle
   decoder. Legacy vehicle and RFTU1 input passes through unchanged; unknown
   RFTU versions and nested version 2 envelopes are rejected.
5. During a full restore, call generated_checkpoint_prepare to allocate and
   decode the companion stage, returning the authored/vehicle bytes for their
   existing staging. The initial scene must already contain one registered
   restore owner per saved generated key. A dead base cannot be used with the
   live-only factory after NPC death publication; construct the owned scene
   first or supply the explicit restore owner path documented in the factory
   integration notes.
6. Admit all pending stages before assigning any. Call
   scene_turret_generated_checkpoint_admit(stage, read_pending_base,
   read_pending_target, context).
   The callback looks up base UID in the pending NPC restore stage and returns
   its saved health and death bit in flags_810. It must derive the bit from
   pending health/death state consistently, including retired/dead-pose rows;
   it must not read pre-load live health. It must explicitly fill the in-memory
   `retired` field; nonzero retirement is rejected before publication.
   Callback flags_7d0 is unused because
   the generated companion owns that snapshot. The companion requires exact
   agreement of base health and death and rejects any live child of a dead
   base, stale/recycled registrations, duplicate keys, invalid transforms,
   mismatched class, invalid target identity and self-targeting. Every non-none
   target additionally requires pending-world liveness through the target
   callback; role1 resolves against pending generated rows, not fresh head health.
7. Preserve owner registrations between admission and assignment. Assign NPC
   and ordinary turret stages first, then generated_checkpoint_assign. It does
   no allocation, callbacks, damage replay or duplicate death effects. The
   parent must retain the validated base/head owner wrappers through commit;
   if generic NPC restore retires/unregisters a dead base, provide a retained
   lifecycle owner or reject the unsupported restore before any assignment.
8. Restore/evaluate the base animation and publish live generated positions
   from interface_1 before combat/contact/draw. The companion preserves the
   saved independent mount and basis; do not replace them with the base's new
   transform. Dead children retain saved terminal position without requesting
   a live tag pose. Close the stage after commit or failure.

## Bounds and supported state

At most 128 generated rows, with the ordinary checkpoint maximum checked before
reading rows. Both prepare and capture take explicit allocation budgets.
Prepare and admit do not mutate live actors. Fresh runtime handles appear only
in the staged in-memory binding and are revalidated before commit.

A saved live child must have no coupled-death dispatch; a saved dead child must
have a terminal dispatch reason. Base-dead/child-live is rejected. Child-dead
with a surviving base is representable because shared damage may affect the
base without a guaranteed kill; the head remains detached and inert. Saved
basis is verified against the saved orthonormal mount and saved aim angles,
not against whatever basis an in-session base currently has.

Possession, occupied heads and active head burning remain unsupported and cause
capture rejection. Admission also rejects an occupied/possessed or currently
burning destination head so assignment cannot silently orphan those live
effect/control owners. General save support for retired/unregistered parents,
NPC targets pointing at generated children, or authored turret targets pointing
at generated children is not silently claimed by this companion. Legacy saves
without a companion leave generated state at the parent's explicit legacy
policy: full prepare/admission rejects absence of RFTU2 while generated owners
exist. Transport-only peeling remains a boot-safe passthrough; it does not admit
a composed restore or infer saved dead/live state.

## Pending-world adapter wiring

### Concrete patch plan against the current integration

These insertions describe the parent-owned files as inspected after seat commit
`fbd1533f`. Only this document was changed for this plan.

1. **scene.c includes:** Insert `#include "scene_turret_generated_checkpoint.inc"`
   immediately after `#include "scene_turret_checkpoint.inc"`. Generated runtime
   definitions already occur near line5475 and activation near line11115, so
   their types/functions are available here. Insert
   `#include "scene_turret_generated_checkpoint_adapter.inc"` immediately after
   `#include "scene_world_player_restore.inc"`, before the world snapshot/load
   includes. All adapter stage types are available at that point; this exact
   ordering needs no opaque accessors or additional prototypes.

2. **scene_world_snapshot.inc capture block:** Immediately after the existing
   successful RFTU1 wrapper block and before `SNAP_END(RF_WORLD_VEHICLE)`, add:

   ```c
   if(!status){uint32_t wrapped=0;
       const rf_world_checkpoint_slice *npc=bundle.sections+RF_WORLD_NPC-1;
       status=scene_turret_generated_activation_checkpoint_admit();
       if(!status)status=scene_turret_generated_world_capture(buffer+at,
           RF_CHECKPOINT_FILE_MAX-at,bytes,combat_frame,2u*1024u*1024u,
           identity.bytes,npc->data,npc->bytes,&wrapped);
       if(!status)bytes=wrapped;
   }
   ```

   `bundle.sections[RF_WORLD_NPC-1]` already points at the earlier RFNC output.
   It stays outside the vehicle slice being shifted by wrapper `memmove`.
   `bytes` is the current wrapped vehicle-slice size, not total file size.
   Leave `at`, `total`, and `SNAP_END` unchanged. There is no new persistent
   capture allocation or cleanup variable: the adapter frees its temporary
   NPC catalog/rows on every path, before primitive generated-row allocation.
   Use one 2MiB scratch ceiling, not 2MiB plus another decoded-row allowance.

3. **scene_world_load.inc declarations:** Next to `turrets=NULL` and `seats=NULL`
   add `scene_turret_generated_checkpoint_stage *generated=NULL;`. Retain the
   existing 2MiB `budget`, `cost`, `vehicle_data`, and `vehicle_bytes` variables.
   No duplicate NPC row buffer or new callback context lifetime is needed.

4. **Load wrapper peel:** At the current
   `v=envelope.sections+RF_WORLD_VEHICLE-1; vehicle_data=v->data; vehicle_bytes=v->bytes;`
   block, after its `cost>budget` check but before RFTU1 prepare, insert:

   ```c
   phase="generated_turret";
   status=scene_turret_generated_checkpoint_prepare(vehicle_data,vehicle_bytes,
       0,budget-cost,&generated,&vehicle_data,&vehicle_bytes);
   if(status)goto done;
   if(generated)cost+=generated->allocated_bytes;
   ```

   Then retain existing RFTU1 → RFNS → RFVA → vehicle/RFVR decoding. The generated
   stage retains fresh handles and scalar values; the remaining `vehicle_data`
   view points inside the original file buffer. Do not peel first and then pass
   those inner bytes to generated prepare: prepare itself performs the peel and
   must receive the original outer RFTU2 bytes. Restore frame0 matches this
   loader's other cadence stages. Charge `generated->allocated_bytes` exactly
   once here; do not charge raw RFTU2 bytes, callback context, NPC rows already
   included in `world->allocated_bytes`, or the earlier freed RFNC resource scan.

5. **Load admission boundary:** Immediately after the existing
   `scene_world_environment_validate(environment)` succeeds, and before the
   `/* Parked publication ... */` block, insert:

   ```c
   phase="generated_admit";
   status=scene_turret_generated_world_admit(generated,world->npcs,&player,
       vehicle_present?&vehicle:NULL,(uint32_t)campaign_authored_vehicle_uid,
       passive,turrets);
   if(status)goto done;
   ```

   This reads all pending owners and allocates nothing. Do not perform generated
   assignment, activation reset, factory calls, death dispatch or live tag
   publication during prepare/admission. Existing terrain preparation commits
   occur earlier in the loader; this plan does not claim to make that preexisting
   terrain path globally rollback-safe or add further early mutations.

6. **Load assignment boundary:** Retain seat detach and ordinary mover/passive/
   clutter/NPC assignment order. Immediately after
   `scene_turret_checkpoint_assign(turrets)` insert:

   ```c
   scene_turret_generated_checkpoint_assign(generated);
   scene_turret_generated_activation_reset();
   ```

   Then keep `scene_npc_seat_checkpoint_assign(seats)` and existing pending death
   effect reset. The primitive assignment copies saved head pose and readiness
   with no fallible callbacks. Do not insert `scene_turret_generated_publish`
   as a new fallible operation inside this transaction. The ordinary generated
   update must republish live translation from the restored/evaluated base pose
   before the next combat/contact pass; its existing independent basis remains
   saved. Dead heads skip that live-tag update. No registry allocation, owner
   array movement, or compaction may occur between admission and assignment.

7. **Load cleanup:** At `done:`, add
   `scene_turret_generated_checkpoint_close(&generated);` alongside existing
   `scene_turret_checkpoint_close(&turrets)` and before `free(buffer)`. Call it
   on success and failure. The stage has no retained buffer pointers and no
   registry ownership; closing it must not close heads or bases.

8. **Remove temporary guards in the same integration batch:** In
   `scene_turret_checkpoint_capture`, remove the temporary
   `if(scene_turret_generated_count)return RF_NOT_FOUND;` and its pending-wiring
   comment. Move its current activation-admission call to the outer capture
   block in step2; do not execute it twice. In
   `scene_turret_checkpoint_prepare`, remove only
   `if(scene_turret_generated_count)return RF_NOT_FOUND; /* RFTU2 publication pending. */`.
   Preserve authored-prefix bounds, ordinary generated-target rejection, and
   the generated primitive's missing-RFTU2 rejection. Do not remove these guards
   in advance of complete capture, prepare, admit, assign and cleanup wiring.

The current `scene_world_snapshot_validate` is transport/identity-only and
`scene_world_quickload` calls it without interpreting vehicle payload bytes, so
these functions require no new semantic decode or stage allocation. Keep that
validation callback usable before fresh generated owners have been constructed.

Repository reader audit: searching C/C++ headers and includes for
`RF_WORLD_VEHICLE`, `RFTU`, `RFVA` and `RFNS` found no additional boot/resource
vehicle decoder. `src/core/world_checkpoint.c` validates section offsets,
lengths, identity and checksum while leaving component bytes opaque; it needs no
RFTU2-specific change.

Post-load contact timing audit found an integration follow-up: the generated
translation loop in `scene_turret_scene_tick` currently runs after
`scene_rockets_tick` and `campaign_enemy_tick` in `campaign_combat_tick`. Both
earlier paths can query turret contacts. Publish generated translation before
the first combat contact, for example before `scene_terrain_input` and
`scene_rockets_tick` after the combat frame guard, while leaving activation and
turret AI in their later slot. This avoids a stale restored head position during
early projectile/NPC contacts without introducing a fallible callback into the
restore assignment transaction. The change belongs to the parent's shared
combat integration; the adapter does not update runtime poses.

Include `scene_turret_generated_checkpoint_adapter.inc` after the primitive,
`scene_npc_checkpoint_restore.inc`, `scene_world_player_restore.inc`,
`scene_passive_vehicle_checkpoint.inc`, and the ordinary turret/vehicle stage
types. If generated definitions remain later in scene.c than world load/save,
forward-declare the generated-stage struct and these two adapter functions
before the world load/save includes; leave their definitions at the later
dependency-complete location. No global pending stage is needed.

After all world/NPC/player/vehicle/passive/RFTU1 and RFTU2 stages exist and their
ordinary validations pass, but before parked vehicle publication or any other
assignment, call:

```c
status=scene_turret_generated_world_admit(generated,world->npcs,&player,
    vehicle_present?&vehicle:NULL,(uint32_t)campaign_authored_vehicle_uid,
    passive,turrets);
if(status)goto done;
```

The wrapper creates a stack context and invokes both primitive callbacks. Base
facts come from the matching RFNC row, reject retired bases, and derive the
death bit from validated saved health. Settled registered dead-pose bases remain
eligible. Target category precedence matches the ordinary UID codec: player0,
authored vehicle, NPC, passive vehicle, authored turret. Role1 has a separate
lookup by base UID in the pending generated stage. Dead/hidden/retired targets
return alive0; unknown owners and passive RFVA1 rows without saved vitals reject
rather than reading fresh live health. The existing primitive still resolves
and revalidates fresh generation handles independently of saved liveness.

During snapshot creation, retain the existing outer-wrapper order:
vehicle/RFVR, RFVA, RFNS, RFTU1, then RFTU2. Before generated capture call
`scene_turret_generated_activation_checkpoint_admit()`; it rejects unsupported
in-flight optional ready animations without extending the wire format.
Replace direct generated capture with:

```c
const rf_world_checkpoint_slice *npc=bundle.sections+RF_WORLD_NPC-1;
status=scene_turret_generated_world_capture(buffer+at,RF_CHECKPOINT_FILE_MAX-at,
    bytes,combat_frame,2u*1024u*1024u,identity.bytes,npc->data,npc->bytes,&wrapped);
if(!status)bytes=wrapped;
```

Use the parent's chosen bounded scratch budget if different. The adapter
preflights and decodes that already-captured RFNC slice with the ordinary NPC
weapon catalog, charging both the temporary catalog and decoded records to the
budget. It rejects missing/retired bases and mismatched health/death facts before
touching the destination payload. Scratch is freed before primitive generated
row capture, so the two temporary allocations do not overlap. Simulation must
remain paused across component capture, as with the existing snapshot function.
No decoded NPC rows need survive the original NPC export. With no generated
owners, the wrapper preserves the primitive's no-op behavior.

For early declarations, the exact adapter signatures are:

```c
static int scene_turret_generated_world_admit(
    const scene_turret_generated_checkpoint_stage *,
    const scene_npc_checkpoint_restore_stage *,const scene_world_player_stage *,
    const scene_vehicle_checkpoint_record *,uint32_t,
    const scene_passive_vehicle_checkpoint_stage *,const scene_turret_checkpoint_stage *);
static int scene_turret_generated_world_capture(unsigned char *,uint32_t,uint32_t,
    uint32_t,uint32_t,const unsigned char [32],const void *,uint32_t,uint32_t *);
```

Keep generated assignment after NPC and authored turret assignment. Reapply
generated translation from the restored base animation only for live children;
dead children retain their saved final position. Call
`scene_turret_generated_activation_reset()` after successful assignment; it
resets transient tracking while preserving saved readiness flags and playback.

No build, emulator run, image or broad test was performed for this isolated
slice. Parent integration must establish bounded live aim/cadence restore and
coupled dead-state restore before claiming generated-head persistence works.

## Bounded ordinary-save harness

`tools/xemu_auto_turret_save.py` reuses the existing copied L3S2 Auto Turret
base1994 in isolated CTF06. Parent runs `python tools/xemu_auto_turret_save.py`
serially after confirming no project XEMU session is open. The script builds and
runs a 60-frame neutral source save, then a fresh 12-frame neutral load on its
owned test HDD; it restores disc inputs and repacks in `finally`. The earlier
combat check observed player death at frame86, so this continuation deliberately
ends after 72 simulated frames and treats any player death as failure.

It requires `rf_scene_turret_generated_restore_probe[32]` at generated assignment:
restore count, base UID, role, head handle, base handle, dead, health bits, armor
bits, target role, target UID, target handle, remaining cadence, rebased fire due,
restore frame, burst, combat action, basis9, angles3, position3, death dispatch.
This immutable probe proves exact saved aim/position/cadence restoration before
normal simulation advances. The ordinary live generated probe proves actual
parent linkage, evaluated interface position, preserved health/readiness, one
created head and no replayed death effects. Source telemetry samples before aim,
so its basis is not incorrectly compared to the later post-combat wire basis.

The harness decodes the real 216-byte RFTU2 row inside the exported RFWC file,
checks stock64MiB completion and ordinary loader success, then verifies target
continuation without reacquisition and damage when the saved cadence becomes due
during the bounded load. Coupled-dead saves are deferred: this existing generated
fixture has no clean damage injection, and this slice adds no new gameplay probe.
No screenshots, host input, campaign routes or PC execution are used.

`python tools/xemu_auto_turret_save.py --validate-existing <artifact-folder>`
only rechecks recorded JSON/save bytes after confirming disc restoration. The
helper has not run the harness; native execution belongs to the parent.

## Native live-head continuation (2026-09-30)

`artifacts/xemu/auto-turret-save-20260930-134916/report.json` passed on stock64MiB: the RFTU2 row from a60-frame source restored a unique linked head, exact saved aim and cadence, and resumed for12 frames with three more shots. The source fired five shots. There were no duplicate death effects; minimum free pages3356. Disc inputs restored. Dead or removed heads remain separate persistence work.
