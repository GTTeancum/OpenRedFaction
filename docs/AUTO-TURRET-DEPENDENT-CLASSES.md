# Auto Turret dependent class loading

Status: implemented in new `src/diagnostic/scene_turret_dependent_classes.inc`;
integrated and Xbox-built; runtime class loading remains unverified.

The installed game authors Auto Turret bases and creates Head actors at runtime.
This helper loads their real class metadata from entity.tbl and materials.tbl;
it never adds a fake level record, changes record counts or invents a child UID.

## Implemented APIs

`scene_turret_dependent_classes_open(seeds, tables, budget)` scans actual records
for an Auto Turret base. With no base it does nothing. With a base it reuses an
already authored Head class, or appends exactly one Head class at the stable end
of the class array. The appended definition has record_index UINT32_MAX, marking
that it has no record; no code may use that sentinel as an index.

The loader uses the same public class readers as rf_entity_seeds_open for
vitals, eye limits, unholster timing, rotation, corpse definition, damage
factors, physics, LOD and model selection. It additionally retains physics
sphere declarations and material coefficients for generated owner creation.
It verifies the selected model is the expected static turret_top01.v3d with no
replacement model. Parsing and allocation complete before publication, so an
error leaves seed arrays and the dependent sidecar unchanged. Existing class
indices and record/seed arrays remain unchanged.

`scene_turret_class_name(seeds, index, &name)` resolves an ordinary class through
its valid authored record, or the explicitly retained dependency through the
stable literal Auto Turret Head. An arbitrary missing record is rejected. The
resolver also verifies the record's seed actually belongs to that class.

`scene_turret_class_is_dependent(seeds, index)` identifies only the added class.
It does not classify an ordinary authored Head as missing a record.

`scene_turret_dependent_base_motions_open(...)` is a drop-in scene-level wrapper
for the existing base-motion loader. If no class was appended, it delegates
unchanged. Otherwise it loads the authored prefix using a borrowed seed view,
then appends one static state set with all skeletal state/action indices -1 and
the actual Head weapon-group/default-weapon values read against the same weapon
catalog. Primary must resolve to Vauss; secondary must be absent. The final
catalog has exactly the same class count as seeds/skeletons. No new core parser
API, fabricated record, fake animation, or duplicate weapon registry is used.

`scene_turret_dependent_classes_close()` clears the retained sidecar after
ordinary rf_entity_seeds_close has freed the class array. Calling it while the
seed classes are still live returns RF_RANGE, preventing accidental loss of the
name/physics dependency while actors can still reference it.

## Precise parent integration hooks

1. Include the helper after entity_assets types and campaign_seeds declaration,
   before loaders that call its functions. Call classes_open immediately after
   the final seed construction/dev-fixture normalization and before skeletons,
   render resources, pose arrays, audio arrays, motion catalogs or registered
   owners borrow class metadata. It can move the classes allocation; appending
   later is unsafe even if a name resolver has been added.
2. Replace the scene's rf_entity_base_motions_open call with the wrapper. Keep
   the full appended class count visible to skeleton/render/playback catalogs.
   Those catalogs check equal class counts. The skeleton loader already skips
   model_kind != 2 and leaves the Head skeleton index UINT32_MAX; no dummy
   skeleton or Head seed is necessary.
3. Replace direct class.record_index name reads in campaign audio setup with
   scene_turret_class_name and propagate failures. The audio arrays should be
   allocated with the complete class count so generated head impact/pain/death
   lookups do not overrun a prefix-only allocation.
4. In scene_turret_models_open, resolve each selected class name through the
   helper instead of requiring a record_index. It will then load the Head's
   actual static resource using the existing model path. The earlier generated
   resource-admission helper can remain as an idempotent hook; it recognizes an
   existing class pair and does not load the model twice.
5. In scene_turret_death_effects_open, use the same name resolver before reading
   explosion/effect metadata. An absent authored record is not an absent class.
6. In campaign_npc_bodies_open, initialize the class eye tag/parent to -1, then
   explicitly skip skeletal-body/first-actor setup for an added static Head:

```c
if (scene_turret_class_is_dependent(&campaign_seeds, cls) &&
    campaign_seeds.classes[cls].model_kind == 1)
    continue;
```

   Place this before scanning poses for `first`; the current no-matching-actor
   error would otherwise abort loading. Existing authored classes retain their
   current checks. Head movement/contact/draw belong to its static turret owner.
7. Pass `scene_turret_dependent.head_class` and the retained
   `head_physics` to generated_create. Obtain Vauss from the resolved base-motion
   weapon catalog/defaults, not a hard-coded slot. Reserve generated owner
   capacity independently, as documented in the factory integration notes.
8. Close registered owners/resources/catalogs first, then ordinary seeds, then
   the dependent sidecar. Reset it on failure teardown too. Never clear it
   before a late resource destructor still needs class identity.

## Source audit

Direct class-to-record references were audited in tracked C/include sources:

| Location / function | Requirement |
|---|---|
| entity_assets.c, rf_entity_base_motions_open around 1984–2005 | Wrapper presents only the authored prefix; no core edit required. |
| entity_assets.c, rf_entity_seeds_open around 3624 | Authored construction remains unchanged; append afterward. |
| scene.c, campaign audio setup around 2885 | Use name resolver and full-size class arrays. |
| scene_turret_models.inc around 78–81 | Replace record requirement and name read with resolver. |
| scene_turret_death_effects.inc around 94–95 | Use resolver for real Head effect metadata. |
| scene.c, dev seed normalization around 19210 | Run dependency append after this normalization, never before. |
| scene.c, campaign_npc_bodies_open around 5526 | Skip added static class before requiring a first authored actor. |

Additional catalog count checks appear in skeletal render resources, initial
pose playback and motion catalog selection. This is why the helper appends
before catalogs, and why silently keeping old class_count arrays is not a safe
shortcut. No direct record-index dependency was found in the static Head's
combat/resource query itself after these loader hooks.

## Memory and limitations

The scene cap is 640 total classes, matching current audio admission. Exactly
one dependency can be appended. Class-open budget includes existing seed
resident storage, the retained sidecar, one max(entity.tbl, materials.tbl)
scratch buffer and a replacement class array while the old array is still live.
The class allocation transfers to existing seed ownership; only the sidecar
remains separately accounted in `accounted_bytes`.

The motion wrapper likewise checks its peak with both old and replacement
class arrays alive and one entity.tbl scratch buffer. This conservative peak
can exceed the previous one-megabyte call budget on a class-heavy level. It
returns RF_RANGE instead of silently exceeding that budget. Parent may allocate
an explicitly accounted startup allowance or later add a core named-class
loader to avoid array duplication; do not remove the check. Final retained
motion overhead is one static rf_entity_state_set and no new motion resources.
Existing rf_motion_file entries borrow archives/resident data, not their old
class-array addresses, so the startup copy preserves those references.

This supplies metadata dependency and catalog compatibility only. Generated
actor creation, animated binding, base activation, death coupling, RFTU2 saves
and gameplay validation remain their separate integration hooks. No build or
runtime success is claimed for this slice.

Parent integration now inserts the dependency before skeleton/pose catalogs,
uses the base-motion wrapper, skips the dependent class in skeletal body setup,
and resolves class names for audio, static resources, death effects and firing
sounds. Both startup budgets explicitly allow2MiB; retained runtime resource
limits remain unchanged. This does not yet instantiate or activate heads.
