# Authored pickup records

The shared C reader supports v180 RFL section0x40000. The section begins with
an unsigned record count. Each record contains UID, length-prefixed class
name, position, three orientation rows, length-prefixed script name, one
common byte, quantity, respawn seconds and team. Orientation rows are
reordered2,0,1 consistently with other retained level objects. Quantity and
team preserve signed32 bits; the common byte remains uninterpreted.

Format leads came from pinned Open Faction commit
e8a4a885ba866fc472702b3dc8a9e8208f9b91e4:
- common/include/formats/rfl_format.h, RFL_ITEMS
- shared/CItem.cpp, stream constructor

Those sources are GPL-3.0 references; no implementation was copied. The reader
uses this project's existing bounded cursor/string helpers. These are layout
facts validated against installed file boundaries, not a claim of recovered
original item-constructor or pickup behavior.

rf_level_items_begin/item_next allocate nothing and preserve cursor/output on
failure. Records require finite positions/matrices, bounded strings and exact
section exhaustion. The owner validates a complete pass before allocating one
array, then fills it on a second pass. It owns all strings; source archives may
close afterward. Budget includes the owner and record array, excluding allocator
metadata. No game objects, meshes, collision bindings or table defaults exist
in this layer.

Installed campaign validation:63 levels across levels1/2/3.vpp, all with item
sections,593 total pickups. Peak retained allocation is28824 bytes. Tests also
check a truncated final byte for every nonempty section, undersized budget,
repeat close and retained L1S1 data after closing its archive. Both PC and NXDK
build; all28 CTests pass. Evidence: artifacts/level-items-scan.log and
artifacts/level-items-tests.log.

L1S1 includes Handgun records9381,9427 and9644, each with quantity16, plus
remote charges, medical kits, suit repair and other equipment. Bind these
classes through items.tbl before deciding whether a pickup grants a weapon,
ammo or another benefit. Next: model/material ownership, retained availability,
proximity/occlusion checks, inventory grant and item removal. No live pickup
or XEMU residency validation is claimed yet.

## Item definitions and inventory grants

rf_item_definition_read/load resolves a named items.tbl class into a144-byte
record: model name/type, associated weapon name, gives-weapon distinction,
SP count and no_pickup flag. Count Single overrides Count regardless of order;
Count Multi is ignored. Unknown classes, duplicate modeled fields and invalid
counts preserve output on failure. Table scratch is bounded and temporary.
Non-weapon benefit callbacks and original item constructor state are not part
of this record.

Installed Handgun maps weapon_ultorgun.v3d to12mm handgun, gives ownership and
has base quantity16.12mm_ammo maps Item_pistol_ammo.V3D to the same weapon,
grants ammunition only and uses32 SP rounds rather than64 MP rounds. These
are installed table values; live integration must also honor the level record's
SP quantity and availability.

rf_weapon_pickup_grant_sp is explicitly first-pass policy: new ownership fills
the magazine then capped reserve via the existing acquire helper; already-owned
weapons and ammo-only grants add capped reserve. Its output distinguishes
new ownership from rounds added, allowing a scene caller to leave an item
available when both are zero. Negative/malformed state fails without mutation.
An oversized grant is capped before calling the original wrapping arithmetic
helper so it cannot wrap and remove reserve ammo.

Tests cover installed handgun/ammo definitions, SP override and no_pickup,
missing-class output preservation, initial acquisition, ammo-only addition,
partial capacity, full reserve, invalid quantity and maximal signed quantity.
Both builds and28 CTests pass. Live rendering, proximity/occlusion, item removal
and pickup sound/event integration remain open; no XEMU collection is claimed.
