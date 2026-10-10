Parent integration note: independently source-reviewed and integrated after the 13:00 batch; awaits 14:00 compilation. All new drop, collection and save behavior remains unverified.

# Authored NPC death items

Ammo-profile extension 2026-10-10: source-staged against
`96a1a08d4c70da2fee640d445bae750c5f1153d8`; independently source-reviewed and parent-integrated, awaiting
14:00 hourly compilation. No test or runtime result is claimed.

Source-written 2026-10-10 against `02ea395e521a78793d0e4d197de3bc50eea7e17a`.
The candidate is staged outside the active repository. Compilation, checks,
Xbox runtime, collection, save/load and revisit behavior remain unverified.
Parent owns integration and scheduled validation; no new gameplay fixture or
campaign route is introduced.

## Supported original definitions

The configured item is independent of the already-implemented held-weapon drop.
The four supported definitions are grounded in inspected authored owners:

- `levels1.vpp/L3S1.rfl`, guard1 UID109: third loadout string `12mm_ammo`,
  installed `Item_pistol_ammo.V3D`, Count32, Ammo For `12mm handgun`,
  Gives Weapon false.
- `levels2.vpp/L11S3.rfl`, guard1 UID10493: `Medical Kit`, installed
  `powerup_healthpack01.V3D`, Count25, no weapon association.
- `levels1.vpp/L3S1.rfl`, guard2 UIDs407/781/2202: `5.56mm_ammo`, installed
  `Item_556mm_ammo.V3D`, Count42, Ammo For `Assault Rifle`, Gives Weapon false.
  UID2202 is authored hidden; it remains part of the retained selected owners.
- `levels1.vpp/L3S1.rfl`, guard1 UID784, and `levels2.vpp/L11S3.rfl`, guard1
  UIDs10490/10491: `10gauge_ammo`, installed `Item_Shotgun_ammo.V3D`, Count8,
  Ammo For `Shotgun`, Gives Weapon false.

The new profiles come from installed `tables.vpp/items.tbl` lines 252–261 and
285–294 respectively (SHA256
`6dd3d14d5b61a0f843c1dfc96d6fd2ec9f95b0fe43f86affcdb783de67b0fd30`).
Both have static meshes and only `spins_in_multi`; the existing pickup kinds 5/9
already bind their authored names and weapon slots. No new level is selected.

Only these four definitions are admitted. Empty, `none`, and unknown names
resolve to no supported configured item. No speculative item types, random
rewards, synthetic placed-item UID, weapon ownership grant, or corpse attachment
are added. Their table defaults are independent of NPC ammunition remaining.
First Aid Kit and other unproven configured profiles remain unsupported.

## Original evidence and reconstructed operation

The established original RF.exe SHA256 is
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

`464353` reads the third of seven loadout strings. `464c60..464c99` resolves
`none` to -1 or calls `459430` and stores the result at actor82c. The new
`rf_level_entity_death_item_read` reads that independent retained raw span after
full existing v180 record validation; it does not enlarge `rf_level_entity`,
change its ABI, or change authored/catalog hashes.

`402f65..402f91` computes animated-model maxY minus minY and stores actor7c4.
The animated path `503310 -> 501940 -> 501960 -> 504590` reads the first source
SUBM minimum/maximum. Scene class admission reads
`rf_model_file_part_metadata(file,0)` and retains that finite difference in each
owner. No physics-radius, sampled-stance or guessed-height substitute is used.

`4200c6..4204a0` is already reconstructed by `rf_entity_death_drop`. The scene
adapter invokes it once at the ordinary fatal transition, before corpse/robot
retirement, alongside the unchanged held-weapon drop. It performs exactly:

    rf_geometry_collision_world_sweep(collision,0x2000u,start,delta,.15f,1,&hit,&matched)

The original argument1 is reset-limit, not a hierarchy selector:
`4df1ef..4df1f1` replaces initial FLT_MAX with1.0. The adapter range-checks and
copies actual hit count, point and normal. It never adds a fallback floor ray.
The helper queries before checking all64 owned bytes; no hit or empty ownership
is a legitimate absent item, including NPCs that have no ammunition.

A stack-local creation receipt lets the helper finish its original offsets
before the final immutable ledger payload is published. Medical Kit adds+.05Y;
ordinary ammo adds hit normal times whole-static-model maxX. Static bounds come
from all retained source parts (`503310 -> 53c467`), without source-part offsets.
No archive I/O, random draws, resource allocation or gameplay grants occur here.

The separate held-weapon adapter is unchanged. Its finite one-magazine cap,
empty-weapon acquisition and floor-ray placement are deliberate first-pass
policies. Original `42ae10` excludes Grenade and moving-support physics
bit `0x400000`; an authored no-drop flag has not been established. Those exact
exclusions and physical tumbling remain deferred.

## Resource, draw and collection policy

At the existing pickup-resource startup, all retained current-level owners that
actually configure one of these definitions demand its existing static pickup
resource. Terminal owners also demand it so an earlier valid available-item save
can replace a collected timeline. Validated current-level/UID saved lanes are
included. This does not load every item class or depend on persistence
registration, which happens later. Existing2MiB per-resource and global texture
admission remain in force; exact mesh/name, finite bounds, quantity and
Gives Weapon false are checked.
The demand-kind reverse mapping explicitly selects IDs 1/2/3/4 for kinds 1/3/5/9
and rejects every other kind. It no longer treats all nonmedical demand as ID2.
The extension changes no resource/texture bound and adds no all-class preload.

Rendering and collection iterate the independent current-level actor-key ledger,
so removing a corpse or robot render body does not remove an item. Rendering
reuses static pickup submission at the saved position with identity basis.
Collection reuses living-player, radius2 and cover checks. Medical Kit uses the
existing difficulty/cap-aware vital grant. Ammo uses existing shared-ammo grant
with Gives Weapon false. A full player retains an available item. Successful
collection changes the lane to collected before optional existing notice/audio;
there is no weapon acquisition or auto-equip.

## Persistence and failure contract

The paired codec contribution adds `rf_campaign_item_drop`, a24-byte actor lane:
state0 absent,1 available,2 collected; stable definition1 Medical Kit,
2 12mm_ammo,3 5.56mm_ammo or4 10gauge_ammo;
quantity; final position. It shares the existing authored level/UID actor key
and leaves held-weapon drops and placed pickups independent. Each2048-slot store
copy grows48KiB. Scene owners add12 bytes (source height, one-shot attempt and
sticky fault); existing owner-budget `sizeof` accounting includes this. Restore
entries add32 bytes for read-only fault/attempt/lane snapshots. Existing RFNC/store
allocation sizes use their extended structure sizes; no budget is increased.
The ammo extension adds only structural ID admission and exact scene profile
bindings: RFNC20/RFCH6 layout, version selection, quantity validation, legacy
IDs 1/2, actor storage and assignment-only restore remain unchanged.

The scene captures the whole lane and checks exact authored profile/default
count. Incoming RFNC rows require the same semantic match and an already-loaded
resource for available items. The codec independently validates terminal state,
finite position and canonical absent payload. Its existing composed RFNC/RFCH
join compares all24 bytes by rebound level/UID before either state is published,
including before RFCH replaces the actor store.

Restore is assignment-only for this lane. It clears newer state when an old
absent lane is restored, never invokes death/query/create/grant, and does not
regenerate loot for legacy dead seeds. Collected tombstones retain payloads
through corpse/robot removal and campaign revisit.

A genuine runtime failure records a sticky owner fault while allowing the fatal
lifecycle and tick to finish. Ordinary strict NPC capture then rejects a lossy
save. A read-only guard also rejects section departure before any handoff-history
mutation, so teardown cannot discard a fault and permit a lossy later-level save.
The Xbox transition caller requires RF_OK before entering the next section.
No-hit/empty ownership sets no fault. Restore preparation may snapshot a faulty
current timeline without mutating its fault; it records/revalidates the attempt
and fault fields plus the complete prior lane. Only the final successful load,
after all publication and storage close, or teardown clears the transients.
Component-only restore clears after its successful commit. Failed load
preparation cannot unblock later saves or poison a valid older restore merely
by checking the fault.

## Deferred validation

Independent source review is required before parent integration. No build,
check, test, fixture, executable emulation, campaign route, screenshot, image,
or original-input change was run by this scene worker. Parent scheduled stock
64MiB validation must establish final compiled integration and runtime behavior;
source presence is not a runtime pass.


## 14:00 admission evidence

The 2026-10-10 14:00 Xbox build and original L3S1 neutral 120-frame startup passed at `1fd5bfba` on stock 64 MiB, admitting the mandatory configured-item metadata/resources. No drop creation, collection, tombstone, save/load or revisit behavior was exercised. See [the hourly report](HOURLY-20261010-1400.md).
