# Cross-class vehicle ownership: implementation handoff

Status: design only; runtime switching remains same-class. The extracted
`scene_vehicle_profile_resources_open` preserves the former startup load order,
caps, weapon initialization, damage prototype and seat validation. Authored/DEV
pose selection now precedes it. Parent reports the existing 480-frame Jeep
switch regression passed after extraction (2026-10-03).

## Ownership that must survive a handoff

| State | Current owner | Required cross-class treatment |
|---|---|---|
| Chassis geometry, tags, material metadata | `stream->driller`, or `passive_vehicle_resources[kind]` | Exactly one close owner. Demote outgoing active pointer into its class slot; promote target slot and clear that slot. |
| Chassis material indices | `driller_base/textures`, passive equivalents | Transfer with chassis; never recompute or merge the same material images twice. |
| Cockpit and auxiliary geometry | `driller_cockpit`, bits, Jeep gun, APC mortar, submarine torpedo | Retain in a bounded per-profile resource pack, including their base/count indices and cockpit `texture_base[]`. |
| Texture pixels | Shared scene `rf_materials` after merge | Scene retains ownership until scene close; resource close must not free transferred pixels. |
| Collision spheres | `passive_vehicle_spheres[kind]`, active runtime physics | Preserve class-keyed passive arrays; rebuild active typed physics, sphere/spring/configuration and inertia for target class. |
| Instance state | Active runtime or `scene_vehicle_parked_state` | Preserve per-UID damage, physics, ammo, cooldown/RNG, route, aim, flags, and seat state independently from shared class resources. |

Active-class passive resource slots are currently **NULL fallback**, not aliases.
`scene_vehicle_resources_open(..., NULL, ...)` still retains every model tag;
it merely leaves `seat=-1`. A promoted passive chassis can resolve and validate
the genuine `interface_1` (and Jeep `interface_2`) without loading another chassis.
The outgoing chassis must become the old kind's fallback owner before changing
the global active profile. Profile-to-kind mapping: Driller1→3, APC2→4, Jeep3→5,
sub4→0, Fighter5→1; passive kind2 is `masako_fighter`, not an admitted active profile.

## Renderer constraint

`scene_driller_materials_merge` transfers image descriptors/pixels into scene
materials and empties the source texture bank. `scene_driller_draw` uses the
saved material base plus each model material's local slot. There is no vehicle
mesh-base index to exchange: retained models are keyed by the geometry pointer,
batch and bone count (`src/platform/xbox/retained_models.h`). Keep geometry
allocations stable until scene teardown; freeing/reallocating them can alias a
cached identity. Frame mesh insertion positions are generated per draw.

The streaming Xbox renderer uploads its texture table once and reuses it when
the `rf_materials` pointer matches. Appending materials during Use does **not**
upload them or resize that table; it also changes fallback/lightmap offsets.
Therefore the narrow next implementation should prepare only the admitted
additional ground profile's cockpit/weapon resources and merge all its materials
before the first renderer submission. Dynamic upload/cache invalidation is an
explicit blocker for an on-demand resource-loading approach.

## Proposed transaction, initially ground profiles 1–3 only

1. At startup, inventory eligible classes, reuse existing chassis, and prepare
   bounded profile extras before material publication. Split the extracted
   opener into chassis admission and profile extras; it currently requires an
   empty stream and always loads a chassis, so cannot itself promote a borrow.
2. On Use, apply existing dependency, quiescence, range, liveness and clearance
   gates. Stage target typed physics/configuration and parked state without
   registry publication. Do not call `scene_driller_runtime_open` as a staging
   API: it registers owners and installs damage ownership through entry setup.
3. Preflight all allocations, seat/tag resolution and collision placement. Keep
   the existing runtime allocation if practical, with a prepared target physics
   value; no allocation or fallible owner setup may follow registry exchange.
4. Exchange checked registry pointers; publish active/passive UID and entity
   slots together with resource pointers, base/count indices and active profile.
   Update `rf_scene_vehicle_enabled`, authored UID/handle/pose, target damage
   prototype and weapon definitions, then restore target instance state.
5. Rebind runtime self-pointers: collision spheres, support physics/collision,
   entry resource/physics/collision and occupant pointer, damage entry/player,
   contact source/stream. Reinitialize class seat policy and close/restart the
   old engine sound. Preserve passive health/death/flags as authoritative.
   Do not copy a temporary whole runtime containing stale internal addresses.

## Memory and persistence gates

Existing loader caps: chassis Driller1MiB/other active2MiB/passive3MiB;
cockpit2MiB; bits512KiB; Jeep gun576KiB; APC mortar/sub torpedo512KiB each.
Metadata readers typically use sequential512KiB scratch, not permanent512KiB
allocations. These are per-loader caps, not an aggregate64MiB guarantee. Reuse
chassis; account actual retained extras plus sequential scratch peak, transferred
scene images and GPU storage. Enforce512 texture slots and the existing20MiB
image budget (21MiB on existing shield/fusion path); retained model cache adds
up to4MiB. Include current scene memory and contiguous-allocation headroom before
admitting an additional profile; failed admission must preserve current boarding.

`scene_vehicle_parked_state`/RFSW1 currently assume one class: inverse inertia,
definitions and view class are reconstructed from current active resources.
Cross-class save needs a versioned extension or source-UID-derived per-row class
validation, profile-specific reconstruction, and boot selection admitting the
saved active class before passive binding/resources. Preserve RFSW1 reads and
RFVA/RFPV authoritative state; never silently decode another class using the
active class's configuration. Submarine/Fighter motion, liquid pointers and
weapon state remain a later extension, not implied by ground-profile support.
