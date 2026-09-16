# Compact damage-ready clutter descriptor plan (2026-09-16)

Use an active-class sidecar, not another per-instance class copy. The506 ctf06 instances reference only three classes; all three use one yellboom vclip. Existing state already retains health/armor/flags, class_index and definition. Existing rf_clutter_class retains authored life, flags, explosion index, corpse name and shared timer. Keep those owners and store only missing gameplay metadata once per active class.

## Concrete fixed-width descriptor

Proposed research layout (not yet production):

```c
typedef struct rf_clutter_gameplay_class {
    float damage_factors[11];
    float explosion_radius, explosion_damage, explosion_offset[3];
    float debris_velocity;
    int32_t debris_model, debris_sound, vclip_owner, corpse_class;
    uint32_t class_index, ready_fields;
} rf_clutter_gameplay_class; /*92 bytes on PC/Xbox, no pointers*/
```

`class_index` refers to existing rf_clutter_classes.items, retaining exact authored first-match class identity. `vclip_owner` indexes a retained deduplicated effect owner; particle_count stays in that owner's rf_vclip_definition rather than being copied into506 instances. Existing class.timer remains the shared50ms game-time deadline. Owner initialization derives protectedbit4 from existing class.life<0 and copies life otherwise; the sidecar does not duplicate mutable health or timer. corpse_class is a resolved existing class index, not an inventory item.

A uint16 class-to-active-slot map uses UINT16_MAX for inactive;431 installed authored class rows require862 bytes. Three92-byte sidecars require276 bytes: total1138 additional retained metadata bytes, zero extra per-instance bytes. Existing first-match class loading must preserve duplicate rows rather than deduplicating by model or vclip: their cooldown ownership is class-specific. This budget excludes effect/model/audio payloads and allocator/header alignment; account each actual resource owner's resident/peak separately. Shared yellboom definition/resources should load once, not three or506 times.

## Exact loader touchpoints

`src/core/entity_assets.c:rf_clutter_definition_read` selects `$Class Name:`, requires model/material/life/flags, stops at `$Skin:` and uses duplicate-bit detection for scalar singleton fields. It currently ignores damage factors, explode radius/damage/offset and debris metadata. Extend a bounded gameplay reader over the same selected class span rather than re-reading the archive per instance. `clutter_archive_spans` and `clutter_archive_fetch` already provide ordered spans using one reusable table scratch buffer. Avoid making the already large rf_clutter_definition scratch object506 copies.

The current `rf_entity_damage_factors_read` cannot be called unchanged: it selects `$Name:`, whereas clutter uses `$Class Name:`. Reuse/extract its factor vocabulary/numeric parsing with an explicit selected-span input. Original40f72b..40f740 initializes11 floats to1;40f744..40f792 accepts repeated `$Damage Type Factor:` entries, resolves through48ab50, and overwrites the indexed factor (last matching entry wins). Unknown type is an original fatal path; port returns RF_FORMAT. Keep indices9/10 at1 unless actual parser evidence maps an authored name to them; ordinary damage can still supply those indices. Accept finite signed factors consistently with original arithmetic rather than silently clamping them.

Original40f4f0 defaults and addresses are already retained by tools/verify_clutter_class_defaults.py: explosion radius+5c defaults1, damage+60 defaults1; local offset+64 defaults zero via vector reset. Life+3c is required. Damage factors live+9c. Debris velocity+78 uses a separate original default/global when absent; the three current lamp entries explicitly supply3, so this proposal must not assert the research tool's fallback0 is a verified general default. Before broadening beyond these explicit entries, recover/use that global's actual default.

`rf_clutter_definition_bind` already resolves existing optional explosion names through rf_vclip_name_lookup, preserving-1 when absent/unresolved. Gameplay preparation must distinguish authored absence from missing named resource. `rf_vclip_definition_load` supplies signed particle_count, emitter recipe, foley and named explosion; original4c1460/4c1e90 establish its suppression meaning. Existing rf_clutter_classes_open_source provides two-pass measurement/publication; use the same failure-preserving pattern for the active sidecar/map.

Suggested APIs:

- `rf_clutter_gameplay_read(class_span,bytes,decoded)` — stack/reused scratch, names and finite numbers, no resources or publication.
- `rf_clutter_gameplay_bind(decoded,existing_class,index,resource_catalogs,candidate)` — resolved compact indices, no owner mutation; records unsupported feature/resource status.
- `rf_clutter_gameplay_open(active_class_indices,count,...,budget,owner)` — one measured metadata allocation plus deduplicated resource ownership, publish only after all active candidates needed for the chosen playable scope are valid.
- `rf_clutter_gameplay_initialize(existing_class,base_owner_state)` — nonallocating authored vitals/protection/class-flag assignment reused with rf_clutter_create. Do not invoke the full allocating factory over an already registered base owner; it would duplicate lifecycle work.

## Resource and malformed policy

The retained research tool tools/audit_clutter_gameplay_layout.py reads actual clutter/vclip table bytes, reports all three class facts and the1138-byte metadata accounting, and runs12 proposed policy cases. These are design-policy Python tests, not C parser/loader or original-game acceptance tests. Results: artifacts/geomod-postedit-re/clutter-gameplay-layout.json.

Reject nonfinite numbers, wrong factor/vector count and overlong fixed names; preserve candidate output and existing owner on failure. Finite negative life is valid/protected; zero life is valid and reaches the existing break update. Reject duplicate singleton tags; repeated damage-factor tags preserve last-wins semantics. Unsupported glass/corpse/resource paths need a clear readiness bit/status, not silent behavioral substitution.

For the strict first damage-ready profile, an explicitly named unresolved vclip or corpse returns RF_NOT_FOUND before enabling damage for that class. Existing optional visual binding may continue to report-1 independently; do not globally change its policy. Empty authored optional names remain valid. With yellboom particle_count60, ordinary mesh debris is suppressed, so not loading genericlight01_debris solely for the ordinary no-corpse lamp break is justified; missing that unused model must not prevent this supported profile. If particle_count<=0 and authored debris is required, missing mesh/sound must block the damage-ready profile or be exposed as unsupported, not silently replaced. Resolve vclip metadata before deciding which resources are required. Declared vclip particles/foley/named explosion themselves still require real effect ownership.

Keep the object present/collidable/renderable when its class is not damage-ready; report that limitation rather than deleting real clutter or making an unannounced indestructibility rule. Once activated, all57 numeric damage and42 effect/timer cases remain acceptance contracts, together with safe per-instance retirement/glare cleanup.

No production edits, builds or live processes. Next implementation should add the bounded reader/sidecar with C malformed/resource rollback tests before hooking live damage recipients.

Primary implementation update: isolated clutter_gameplay parser and clutter_damage arithmetic helpers are registered and pass CTest, including installed tables.vpp lamp definitions. Both compile with NXDK. Live hitscan/blast/rocket routing and deferred effects are not integrated yet.
