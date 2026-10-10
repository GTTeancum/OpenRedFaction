Parent integration note: independently source-reviewed and integrated after the 13:00 batch; awaits 14:00 compilation. All new drop, collection and save behavior remains unverified.

# Configured NPC death-item persistence

Ammo-profile extension: source-staged against
`96a1a08d4c70da2fee640d445bae750c5f1153d8`, pending independent review and the
14:00 hourly parent batch. IDs 3/4 are added to the existing structural allowlist;
no RFNC20/RFCH6 layout, version, field, size or quantity rule changes.

Status: source-staged against `02ea395e521a78793d0e4d197de3bc50eea7e17a`;
compilation, codec execution and gameplay remain unverified until the parent-owned
hourly Xbox batch. This is reconstructed save ownership, not an original-game
save format. No original asset, source/catalog hash or earlier wire layout changes.

## Independent item identity

The configured death item is a separate obligation from the existing weapon
drop. The original `420390` / `42039d` path uses the configured item's default
count; it does not transfer or debit the NPC weapon's loaded/reserve ammunition.
The inspected original placed-owner records contain these supported names in
the third of their seven loadout strings:

- `levels1.vpp/L3S1.rfl`, `guard1` UID 109: `12mm_ammo`.
- `levels2.vpp/L11S3.rfl`, `guard1` UID 10493: `Medical Kit`.
- `levels1.vpp/L3S1.rfl`, `guard2` UIDs 407/781/2202: `5.56mm_ammo`.
- `levels1.vpp/L3S1.rfl`, `guard1` UID 784, and `levels2.vpp/L11S3.rfl`,
  `guard1` UIDs 10490/10491: `10gauge_ammo`.

The bounded durable definition IDs are independent of runtime item resources,
weapon catalog indices, actor handles and generated pickup UIDs:

- ID 1, `RF_CAMPAIGN_ITEM_MEDICAL_KIT`: `Medical Kit`, original `items.tbl`
  lines 355–363, default count 25. Difficulty/cap-scaled vital benefit does not
  change the saved quantity.
- ID 2, `RF_CAMPAIGN_ITEM_12MM_AMMO`: `12mm_ammo`, original `items.tbl`
  lines 240–250, default count 32, `Ammo For` the 12mm handgun, `gives_weapon=0`.
- ID 3, `RF_CAMPAIGN_ITEM_556MM_AMMO`: `5.56mm_ammo`, original `items.tbl`
  lines 252–261, default count 42, `Ammo For` Assault Rifle, `gives_weapon=0`.
- ID 4, `RF_CAMPAIGN_ITEM_10GAUGE_AMMO`: `10gauge_ammo`, original `items.tbl`
  lines 285–294, default count 8, `Ammo For` Shotgun, `gives_weapon=0`.

These authored examples and table defaults establish the bounded supported
set. IDs 1/2 retain their original meanings. The scene still proves each loaded
owner's exact configured string and current original-table quantity before
publication. The codec never guesses an authored item from a weapon ID or class.

## Public C contract

`rf_campaign_item_drop` is exactly 24 bytes:

    uint32_t state;
    int32_t stable_definition_id, quantity;
    float position[3];

`rf_campaign_actors.item_drops[RF_CAMPAIGN_ACTOR_SLOTS]` is a fixed 48 KiB lane
at the same persistent level/UID slots as existing actor history. It is separate
from `drops[]`; neither lane replaces the other. The matching current-level
record field is `rf_npc_checkpoint_record.item_drop`.

States are `RF_CAMPAIGN_ITEM_DROP_NONE=0`,
`RF_CAMPAIGN_ITEM_DROP_AVAILABLE=1`, and `RF_CAMPAIGN_ITEM_DROP_COLLECTED=2`.
State 0 must be bitwise all zero, including positive-zero float bits. States 1/2
retain a supported definition ID, positive signed 32-bit quantity and finite XYZ.
The collected tombstone retains its entire payload, so a later duplicate death
receipt cannot replenish an item. No borrowed pointers or dynamic handles enter
the durable lane.

`rf_campaign_item_drop_validate(const rf_campaign_item_drop *)` performs those
structural checks without mutation. Null is `RF_RANGE`; malformed data is
`RF_FORMAT`; canonical data is `RF_OK`. Positive quantity is a structural bound,
not proof of an exact original item definition. That binding belongs to scene
admission.

`rf_campaign_actor_item_drop_emit(store, slot, stable_definition_id, quantity,
position)` validates the requested payload and existing lane before changing
anything. Bad arguments return `RF_RANGE`; malformed existing state returns
`RF_FORMAT`. If the lane is absent, it writes one available item. An already
available or collected lane returns `RF_OK` without changing its payload. It
does not allocate, grant a reward, modify vitals or retire the owner. The caller
must prove an actual qualifying fatal transition and reserve the persistent
actor slot before gameplay. Read, capture, restore and revisit paths must never
call this producer. After an accepted positive pickup benefit, collection
changes only state from available to collected.

## RFNC20

The RFNC writer selects 20 only if at least one row retains nonzero item state.
Every row in that component then appends 24 bytes after the existing RFNC19
Drone tail: state at +0, definition at +4, quantity at +8, and XYZ at +12/+16/+20.
All earlier fixed tails remain present. The unchanged base remains 600 bytes;
the maximum complete row increases from 1688 to 1712. Integer and binary32 fields
remain little-endian, under the existing checksum, identity and catalog hash.

A nonzero item lane requires both nonpositive finite health and either a
retired owner or an admitted terminal `dead_pose`. A merely administratively
retired actor with positive health cannot carry item history. Existing terminal
clip, flags, physics and owner checks remain in force. An available item and a
collected tombstone follow the same terminal-ownership rules.

RFNC1–19 remain readable and zero-initialize the missing lane. A zero lane does
not select 20, and malformed noncanonical zero state is rejected even when a
lower version would otherwise be chosen. Version selection is monotonic across
all rows: a later Drone owner cannot downgrade 20 to 19. Legacy decode does not
synthesize missing loot or infer historical emission from inventory/death.

## RFCH6 and composed publication

RFCH actor rows remain 56 bytes in 1–4 and 60 bytes in 5. Version 6 uses 84 bytes:
the Capek latch stays at 56 and the new item lane occupies 60–83. Versions 1–5
zero-initialize the absent lane. The writer selects 6 only when an actor slot
retains item history; a later Capek latch cannot downgrade 6 to 5.

RFCH nonzero item history requires either a retired actor key, or valid
captured nonpositive health for a still-registered fatal owner. Retired history
may retain stale living vitals from an earlier section capture; retirement
remains authoritative there. The later current-level RFNC join supplies the
stronger fatal-owner proof where a current owner exists. RFCH does not reconstruct
original producer history from stale vitals or create new pickups on load.

The structural RFCH maximum grows by 49152 bytes, from 240488 to 289640. Global
save/storage and scene-stage budgets are unchanged and can still reject an
oversized composed checkpoint. Each in-memory actor store copy also grows by
48 KiB; no unbounded container is added. Stock 64 MiB admission remains a required
parent-owned validation concern.

The existing prepublication
`scene_campaign_history_checkpoint_capek_admit` now also validates and compares
the complete 24-byte item lane for each current RFNC actor against its rebound
RFCH level/UID slot, including all-zero absence and collected payload. Mismatches
reject the composed load before either component can overwrite the other. It
retains the existing helper name to avoid unrelated call-site changes.

Scene integration must capture the independent lane, admit the exact authored
definition/default count and required resources, perform assignment-only
restore, and preserve strict producer/save guards. Legacy decode followed by
assignment must clear any newer in-session item history; death is never replayed
to fill the missing lane. These runtime/resource/capture responsibilities are
outside this codec-only patch.

## Deferred verification

No helper build, syntax compile, test run, emulator launch or gameplay fixture
was performed. Independent source review and the scheduled parent batch must
cover the exact applied source, especially mixed item/Drone and item/Capek row
ordering, canonical absence, collected tombstones, legacy rollback, malformed
tail lengths/IDs/quantity/XYZ, living-owner rejection, and whole-lane composed
join mismatch rejection. Existing old-save regression alone cannot establish
new emission, pickup, item-history persistence or action behavior.
