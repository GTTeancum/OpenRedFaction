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
