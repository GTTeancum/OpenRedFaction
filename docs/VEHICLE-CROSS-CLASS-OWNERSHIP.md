# Cross-class vehicle ownership: implementation handoff

Status: stock64MiB Xbox verifies Jeep-to-APC runtime switching; cross-class save continuation remains unverified. The extracted
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

## Concrete save extension proposal (ground profiles only)

Use **RFSW2**, preserving the existing24-byte header,320-byte rows, wrapper order
and every existing field offset. Header+16 remains the active profile. Consume
reserved row+60 for the parked profile (1Driller,2APC,3Jeep) on valid rows;
row+316 remains zero. Invalid rows still contain only UID and valid=0, with
bytes8–319 zero: passive aircraft/submarines need no fabricated ground profile.
Version1 continues to require both reserved words zero and infers each valid
row's profile from its header. Do not infer a version2 profile from the current
active host. Unknown versions and row profiles must fail before mutation.

Add `uint32_t profile` to `scene_vehicle_parked_state`; capture it when demoting
an active owner, retain it through slot exchanges, and verify it against that
row's source UID/class and passive resource kind (profile1→kind3,2→4,3→5).
Keep the256KiB bank cap based on actual `sizeof`, and the caller's staging budget.
Recommended writer policy: emit version1 only when active profile still matches
the level's ordinary boot profile and every valid bank has that same profile;
otherwise emit version2. A cached immutable original boot profile is necessary
for this choice. Always emitting version2 after cross-class support is enabled
is also safe for new readers, but forfeits old-reader compatibility unnecessarily.

Exact codec changes in `scene_vehicle_switch_checkpoint.inc`:

- `peek`: admit versions1/2 with unchanged extent/UID checks; validate row+60
  according to version and validity. Keep the current public signature.
- Add `row_identity(data,bytes,index,&uid,&valid,&profile)`; version1 returns
  header profile for valid rows, zero for invalid. Keep `row_uid` as its wrapper.
- `decode`: receive the wire version/header profile, resolve the row's profile,
  and initialize class-derived fields from that profile's retained resources.
  Current active-view/drill/inverse-inertia copies are incorrect cross-class.
- `valid`: use row-profile capacities, secondary capacity only for APC, exact
  row-profile seat tags, and Jeep role only for Jeep. Preserve quiet-motion,
  no-live-route, trigger/cooldown, freeze-marker and finite/basis admission.
- `live` and `prepare`: replace equality with the selected active resource kind
  by row-profile↔owner-kind agreement. Preserve UID/handle/generation, group and
  attachment checks. `prepare` must still match the live selected active header.
- `admit`/`assign`: retain RFVC active-profile equality, RFVA2 pose/UID/order and
  RFPV marker/body-flag consistency, then transfer the fully admitted bank only
  after existing final host/RFVA/RFPV publication. No class resource loads here.

The resource pack needs a read-only, allocation-free profile lookup (proposed
contract, not yet an implemented API) providing chassis/tag defaults, primary
and secondary definitions/capacities, drill defaults, and class physics defaults.
Build inertia with the existing ground initializer using the saved pose before
overlaying saved velocity/momentum/forces; never serialize pointers or borrow the
active class's tensor. Construct a fresh base `rf_entity_view` using the existing
entry setup policy (`class_type=use_kind`, speed, Jeep occupant_count2/other1),
then apply saved fields and RFVA flags. Leave dormant occupant pointers NULL;
promotion already rebinds to the live entry array. The initializer's small body
allocation must succeed in the prepare phase; assignment remains infallible.

Boot changes in `scene_vehicle_switch_boot.inc`:

1. Preserve the original selected UID/profile for legacy and hidden-host policy.
   Version1 retains the existing saved-profile==ordinary-profile restriction.
   Version2 may select another source-proven ground profile; it must not merely
   trust a header number. Keep source hash, positive UID, authored visibility,
   seat-parent, group and original vitals eligibility checks.
2. Read valid row identities, prove each class/profile using installed level
   records, and gather a required-profile mask including the active profile.
   Retain complete passive UID ordering and existing inactive-seat hints.
3. Publish `rf_scene_vehicle_enabled=profile` with selected UID/pose only after
   complete early admission, before passive binding and resource preparation.
   The current boot helper changes UID/pose but intentionally not this global.
4. Prepare all demanded profile packs before first renderer submission and
   before RFSW prepare; failure must reject the save rather than replace a
   parked profile's configuration with the active one.

No inner vehicle format expansion is needed solely for this ground-class
identity: active RFVC already has typed profiles, and RFVA/RFPV retain per-UID
passive state. `scene_world_load` currently chooses128/160-byte active records
from `rf_scene_vehicle_enabled`; correct early profile publication is therefore
mandatory. Keep direct-session load guards until a separate transaction can
stage a different active resource/runtime; fresh-scene quickload is the safe
existing route. Actual cross-class runtime promotion, profile-pack readiness
and this decoder refactor remain implementation blockers, not completed work.

## Runtime exchange checklist against the current structs

- **Selection:** replace the same-kind filter with admitted, prepared ground
  profile lookup. Test each candidate's own `physics.authored.use_radius`.
  Initialize best distance to infinity unless the living outgoing host itself
  is within its own radius; then let it compete by distance/UID. The current
  outgoing-radius cap incorrectly limits every other class (and wreck escape).
- **New/restored state:** construct target class physics defaults and base view
  before overlaying the bank. Set correct `view.occupant_count`, use kind and
  speed. Clear dormant `view.occupants` and `weapon_owner` aliases in the bank;
  on promotion bind occupants to `r->entry.occupants`, set both slots -1 and
  retain the existing no-active-session/player-saved/dependency admission.
- **Clearance:** use target tags/seat and target-class seat-basis/Jeep policy,
  not `s->driller`/old `r->entry`. Preserve the custom query's exclusion of
  target hull only and inclusion of the outgoing host and unrelated actors.
  Do not retain its stack `query_context` in the published runtime.
- **Demotion:** `parked=*target` currently copies the destination class. Explicitly
  set `parked.resource_kind` to the outgoing class, object_kind11, uid/handle,
  attached/group_owned0, outgoing full damage (including factors/class flags),
  freeze/body flags and pose. Prefer a zeroed passive pose initialized like
  `campaign_passive_vehicle_add`, then outgoing matrices/position/velocity;
  do not inherit the destination's radius/bounds/pose velocity. RFVA2 stores
  UID, pose and vitals, not resource kind or damage factors: fresh boot derives
  those from source UID, so live demotion must already agree with that source.
- **Physics:** replace the entire prepared `r->physics`, then overlay target
  rigid state/body flags. Refresh `r->collision.spheres/count/radius`, retain
  world/movers/flags, and copy it into `r->entry.collision`. Rebind support's
  stream/collision/physics pointers and reset **material=-1**, support_handle,
  hits and velocity. Merely clearing hits retains stale ground material.
- **Entry/damage/contact:** set `entry.resource/physics`, registration/view/host
  handles and entity slots together. Keep registration.view bound to its live
  view, damage.entry/player bound to live structs, and the global damage owner
  pointing at this same allocation. Copy target full passive damage, then
  compute host.alive and reset pending ejection/last damage telemetry as today.
  Refresh contact.stream/source and clear has_world. Ground profiles have no
  water callback/context; never retain a temporary profile owner's pointers.
- **Profile fields:** exchange chassis/cockpit/bits/gun/mortar pointers and all
  texture base/count indices. Publish target primary/secondary definitions and
  muzzle tags before restoring scheduler/reserves/RNG/cooldowns; clear inactive
  class fields and round arrays after existing no-pending-round admission.
  Restore drill metadata/tags plus counters, aim limits/state/eye/basis, Jeep
  role and seat; clear Jeep cycle latch. Resolve Jeep gun/muzzle poses from the
  new attachment. Driller `r->shapes[2]` must be prepared before commit, not read
  from disk during the subsequent drill tick. Reset driver_seconds for the
  first-pass cockpit clock policy; no saved per-UID clock exists presently.
- **Audio/ordering:** existing exchange leaves `r->audio` unchanged. Stop its
  actual slot/sample before restarting APC/Jeep/Driller's proper loop after
  profile publication. Treat restart as optional feedback, not transaction
  failure; validate any old-slot release before registry exchange. All class
  setup/tag/placement failures precede exchange. Afterwards publish scalar and
  pointer state only, rebind addresses, and let normal Use perform boarding.

Existing same-class guards on pending projectiles, routes, burns, occupants and
live support references remain necessary. None of this checklist enables
cross-class switching until the runtime and resource transaction are wired.

## Implemented resource preload slice (2026-10-03)

`scene_vehicle_profile_physics.inc` prepares typed physics without publishing a
runtime or changing the selected profile; ordinary startup now uses this path.
`scene_vehicle_profile_pack.inc` borrows existing passive chassis and owns only
cockpit, drill bits, Jeep gun or APC mortar resources and class configuration.
It resolves real seat tags and prepares physics, weapon and damage defaults.

`scene_vehicle_profile_preload.inc` inventories additional live, visible,
ungrouped ground classes. It loads extras after ordinary model/pickup resources
and merges their materials before renderer submission. Pack merge preflights
all slots/bytes and reallocates once before transferring images. Partial load
or merge failure releases the optional pack and preserves the original host.
The pack and scene texture bank have distinct close ownership.

The additional pack budget is4MiB, including sequential loader scratch peaks.
Native admission reserves16MiB of currently available physical memory for later
lighting, renderer and gameplay work; this is a conservative policy, not proof
of sufficient contiguous GPU memory or every campaign level fitting. Material
merge respects the existing scene image budget and512-slot limit. Telemetry
`rf_scene_vehicle_profile_packs[16]` records demand/loaded/merged masks using
bit(profile), owned resident bytes after image transfer, additional material
slots/bytes, admission status and the pre-load available-page snapshot.

The mixed fixture offers `--preload-only` for120 neutral frames. It checks APC
pack admission and unchanged selected Jeep ownership; the separate switching
check stays gated until the real handoff is implemented. Neither mode claims
visual inspection. Transactional runtime publication and profile-aware RFSW2
save implementation remain open.
Native preload evidence: `artifacts/xemu/vehicle-mixed-preload-20261003-174759/report.json`
reports PASS on stock64MiB for120 neutral frames. APC demand/loaded/merged masks
are4/4/4, status0, with461651 bytes retained by the extras pack and929608 bytes
transferred to scene materials across30 slots. Admission observed9602 available
pages; final renderer/game memory observed5321 pages (about20.8MiB). Jeep7629
remained active profile3 with handle33358332 and no boarding/switching/firing.
Original disc configuration was restored and its Xbox image rebuilt. This proves
connected preload/ownership only; cross-class controls, rendering appearance,
save continuation and memory in other levels remain unverified.
## Runtime and save integration (2026-10-03, validation pending)

The startup ground pack now adopts already-merged active extras without
reloading them. Pack ownership outlives active-profile changes; scene fields
alias the current pack and teardown clears those aliases before closing packs.
The chassis still has exactly one owning active/passive slot. The outgoing
class's passive collision spheres are prepared even when no other vehicle of
that class initially exists. Additional Driller admission also loads its two
cut templates before any runtime handoff.

Ordinary Use considers each candidate's own range and prepared profile. Before
registry exchange it validates dependencies, quiet physics, resources, target
seat clearance and the outgoing audio slot. Publication transfers chassis and
material indices, installs class weapon/configuration defaults and full physics,
rebinds collision/support/entry pointers, then overlays per-UID state. The old
vehicle keeps its class, damage, identity and parked state. Audio restarts with
the new class. Existing same-class switching does not require optional packs.

RFSW2 keeps the24-byte envelope and320-byte rows, storing each valid parked
row's profile at offset60. RFSW1 still reads and writes ordinary same-class
saves. Boot validates saved classes against source UIDs, selects the saved
active class before resource loading and requests parked-class resources.
Foreign rows reconstruct physics, seat and weapons from their own class pack;
active-class rows retain the old live-resource fallback. Inactive authored Jeep
seat records resolve against the parked Jeep chassis when another class is
active. Direct-session and legacy active-switch restore restrictions remain.

The mixed Xbox harness is enabled for ordinary Jeep boarding/gunner fire,
exit, on-foot approach, APC boarding and firing. Generic entry telemetry records
range/path admission and actual poses for numeric diagnosis without images.
Runtime and save continuation results must be recorded separately below.
Native runtime evidence: `artifacts/xemu/vehicle-mixed-mixed-20261003-175927/report.json`
reports ONE_DIRECTION_FUNCTIONAL_PASS for380 frames. Ordinary Use boards
Jeep7629 at90; the gunner launches three shots and ammo falls999→996. After
normal exit210 and on-foot approach, Use270 switches to APC3303 and boards it.
Five APC primary launches consume999→994 rounds. Both original handles remain
stable (Jeep33358332, APC33292795), checked registry ownership is coherent,
there are two boards/one exit/one exchange and no rejected switch or gameplay
error. Final available memory is5317 pages (about20.8MiB). Original disc flags
were restored and Xbox image rebuilt. Return boarding, fresh-load RFSW2,
secondary fire, Driller switching and audiovisual appearance are not established
by this run. Vehicles estimate advances to approximately95%; overall remains88%.