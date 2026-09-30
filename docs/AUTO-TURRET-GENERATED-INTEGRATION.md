# Generated Auto Turret head integration slice

Status: implemented helper only; **not integrated, compiled or runtime-verified**.
The new `src/diagnostic/scene_turret_generated.inc` changes no existing source.
Evidence and authored instances are in [the binary report](research/AUTO-TURRET-BASE-HEAD.md).

## Implemented behavior

- Fixed binding storage capped at `SCENE_TURRET_OWNER_LIMIT` (128); no helper heap allocation.
- Stable key `{base_uid, role=SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD}`. The generated owner's ordinary `uid` and `seed` are UINT32_MAX, deliberately not an authored identity.
- Dependent model admission through existing `scene_turret_model_acquire`, sharing its budget, materials, collision and tag resources.
- Factory registers a real `scene_turret_owner` in a caller-reserved stable array slot. It applies Head class vitals/damage factors, forced affiliation 0, Vauss, host linkage and orientation-lock flag 0x100. It uses the actual base `interface_1` and evaluated skeletal pose.
- Pose publication copies only the attachment world position. The head basis, combat mount and angular state remain independent.
- Combat readiness checks generation-valid base and head registrations, live health, hidden/dead flags, the base AI-ready bit and intact attachment linkage.
- Key encode/resolve helpers support parent-owned save integration without synthesizing child UIDs.
- Coupled-death dispatch detaches and marks the binding before invoking the parent callback, preventing recursive duplicate dispatch. Binding storage resets only once live head registrations are gone.

## Required resource and allocation hooks

`scene_turret_generated_required(&n)` counts authored bases before allocating turret storage. This is a conservative capacity requirement; a base already dead at creation cannot create a live head. Check `authored_turrets + n <= SCENE_TURRET_OWNER_LIMIT` and allocate that capacity once. Never realloc registered owners: registry wrappers point into the array.

Retain real `Auto Turret Head` class metadata even when no level row names it. Parse the same fields as `rf_entity_seeds_open` (entity_assets.c, class construction near 3625), plus `rf_entity_physics_config_load(tables, "Auto Turret Head", ...)`. The helper takes its retained class index and parsed physics config; it does not manufacture a record. **Appending a class with record_index UINT32_MAX is not by itself safe**: current base-motion/model readers index records through class.record_index. Parent must supply an explicit dependent-class name resolver or retain/load the dependent class after those record-only readers finish. Audit the loops that run later too. The resulting stable class index must remain available to damage, combat, draw and checkpoint code for the level lifetime.

Inside `scene_turret_models_open`, after authored model admission but before the combined texture publication loop, call:

```c
status=scene_turret_generated_admit(head_class,meshes,maps,map_count,file,&used);
if(status)goto fail;
```

Use the same allocated `rf_model_file` scratch and `used` accounting. Ensure scratch exists even if no authored static turret was loaded. Ordinary class iteration must skip a dependent class without a level row; this helper admits it explicitly. Existing all-or-close failure cleanup owns partial resource loads. The helper reuses an existing class pair if already admitted. Its implementation may appear later in scene.c, so declare this signature before the resource include if retaining current include order.

After NPC/base registration and evaluated initial poses, before `scene_turret_combat_open`, append each live generated owner to the stable reserved allocation:

```c
uint32_t slot=scene_turret_count++;
status=scene_turret_generated_create(slot,base_slot,head_class,&head_config,vauss_id);
if(status){--scene_turret_count; /* slot is zero/unpublished on ordinary failure */}
```

First ensure slot is below allocated capacity. Do not publish all reserved empty slots in scene_turret_count: combat/open expects every counted owner to be registered. `RF_NOT_FOUND` for a dead/missing base means skip that child during initial construction; other failures abort scene construction. The successful factory binds through the dedicated table, which is the typed reverse base-to-child owner; generic seat iteration does not automatically see it. If general entity predicates must see base occupants, parent must publish a stable one-element occupant list or adapt those predicates to this table, never share a player seat by assumption.

## Tick, combat and death hooks

Include generated helper after NPC body, model/tag helpers and turret owner definitions. Each tick, after base animation and world pose publication and before head targeting/contact/draw, call `scene_turret_generated_publish(head_handle)`. `RF_NOT_FOUND` is a missing/dead dependency and must route lifecycle handling, not silently leave a live floating head. No missing-tag offset fallback exists. Static owner contacts/draw/eye queries read its position directly; if parent adds cached room/bounds data, refresh it after this publication.

Replace combat's unconditional nonnegative-linked-handle veto with a generated-role gate. Ordinary authored turrets retain the existing possession veto. Generated heads require `scene_turret_generated_ready` success and true output, including when already detached: do not accidentally allow a generated head to fire merely because death cleared linked_handle. Keep all existing weapon, AI mode, visibility and cover gates. No new turret firing algorithm is introduced here.

Parent must drive base readiness from activation/awareness and optional ready-action completion. The helper deliberately does not force AI-ready bit 0 or treat model load as activation. Set the base class AI identity bit 0x20000000 through the NPC initializer and preserve its unarmed behavior. The absent authored idle_to_ready action must not leave it permanently inert. Current ordinary firearm pain code also synchronizes flags_7d0 with its firing state, so a one-off write without owning that synchronization may be lost.

Route base/head ordinary death through `scene_turret_generated_death(actor,dispatch,context)` before owner retirement. Callback reasons:

- BASE_DIED: set Head life to zero and run its ordinary death/lifecycle side effects (original 4190ab), rather than inventing an armor-dependent blast.
- HEAD_DIED: route 1000 damage, source UINT32_MAX, kind -1 to the live base through shared damage dispatch (original 419019–419050).

The callback must not compact/free owner storage while dispatch runs. The helper clears the head link and marks the relationship before callback, so recursively reaching the opposite death is a no-op. Dispatch is at most once, even when callback returns an error, because shared damage may already have produced side effects. Propagate such errors; do not blindly retry. This helper does not duplicate pain, audio, death events, corpses or registry retirement.

Scene teardown: stop combat, close/unregister turret owners, call `scene_turret_generated_close`, then close base/model resources. Repeated level loads must clear the old binding table before creation.

## Persistence and remaining integration limits

RFTU1 accepts authored turret UIDs only and currently rejects linked owners. It cannot save these new owners as-is. Parent must extend a versioned codec to store the generated key and binding/readiness along with existing aim, health, cadence and terminal death state. Generated keys also need target-reference support where ordinary UID codecs currently resolve actors. Do not put UINT32_MAX in an authored UID row or map the child to the base UID without a role tag.

On restore, fresh registry handles bind through the generated key, not saved raw handles. Keep independent aim/mount from saved state and republish only position from the restored base animation. Factory creates one child per base; do not create a second child during checkpoint publication. Binding.death_dispatched must restore coherently with dead owners. Initial construction skips dead bases, so restoring dead generated rows requires an explicit parent restore-only owner path; the live factory is intentionally not that path.

Before claiming the class works, parent must wire and establish one actual base creates one head, animated position with independent aim, activation/fire, either-side death, and ordinary save/load. No native run or build was performed for this slice, and existing standalone Stationary Turret checks do not cover it.

## Coupled-death dispatch adapter

`scene_turret_generated_death.inc` supplies `scene_turret_generated_death_join(actor, frame)`. Include it after `combat_death_start` and the ordinary combat effect callbacks; add its prototype before early NPC/static-owner death sites. The join validates generated key identity against the registered head, then delegates to the existing mark-before-callback guard. Non-generated actors are no-ops; errors for a known pair remain errors.

Required live hooks:

- In `rf_scene_npc_death_entry`, call the join after the existing already-dead guard and successful death-entry flag publication, before returning. Use the current scene frame. This covers ordinary damage, Slay and scripted lethal vitals; `combat_death_start` remains with its existing caller.
- In `scene_turret_damage_receive`, call on the actual live-to-dead branch after dead/model/flags/effect publication. Add the same hook where `campaign_script_vitals` directly marks a turret dead. Supply the frame through a parent accessor or explicit context: the owner include precedes `combat_frame`, so do not introduce an undeclared global reference.
- Propagate errors. Do not call during checkpoint assignment, initial dead-owner construction or normal whole-level teardown; those paths must not replay damage or presentation.

Base death sets head life to zero, then sends a zero-amount kind -1 request through `scene_turret_damage_receive`. The generic damage dispatcher skips zero damage, while the enclosing ordinary turret terminal branch publishes dead state/model/effects. This matches direct-life semantics without inventing a lethal amount or an armor/immunity loophole. A head without a corpse model needs the parent's normal no-replacement draw behavior.

Head death sends exactly 1000 damage, source UINT32_MAX, kind -1, force 0 through `rf_scene_npc_damage`. If base health reaches zero, the adapter calls `rf_scene_npc_death_entry` and `combat_death_start` only when `entered` is true. Immune or unusually high-health bases are not forcibly killed beyond that request. Recursive entry finds the already-marked pair and cannot damage twice.

Removal differs from lethal damage. Before removing a base, dispose of its generated head while both registrations resolve. A documented first-pass cleanup policy may call the base-side join before unregistering the base. Never wait for attachment publication after freeing the base. Pure head removal does not establish original lethal-head damage semantics: retain it until normal terminal handling or use an explicit non-damaging pair-retirement path. Do not compact/reallocate registered turret storage during dispatch. Ordinary teardown unregisters turret owners, clears generated bindings, then releases base resources.

No generated helper API change is required. Do not broadly normalize callback RF_NOT_FOUND to success; the join's explicit membership check separates unrelated actors from stale known pairs. Adapter is source-only pending parent integration/build/native validation.
