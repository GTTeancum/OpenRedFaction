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

rf_item_definition_read/load resolves a named items.tbl class into a400-byte
record: model name/type, associated weapon name, gives-weapon distinction,
SP count, no_pickup flag and four English pickup notices (single, multi,
weapon-and-ammo single, weapon-and-ammo multi). Count Single overrides Count regardless of order;
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

## Live Handgun pickup path

The installed campaign item audit finds 593 placed items in 31 classes.
The first-pass class table now recognizes Miner Envirosuit and Doctor Uniform,
which account for ten formerly unsupported placed items. Both are static
models with no weapon declaration in `items.tbl`. The original default grant
at `0x45a3d0` branches to `0x45a100` when both weapon and ammo IDs are absent;
that service handles the player pickup notice and returns without changing
weapon inventory. The port accordingly retires an accepted placed suit item
and submits its authored English pickup notice, and accepts the same names in
`Give_Item_To_Player`. Suit-specific hazard protection or disguise effects
have not been established or implemented.

The stock-64-MiB L6S3 Xbox run at
`artifacts/xemu/nonweapon-pickup-20260929-220900/report.json` completed 140
frames with one scripted Miner Envirosuit grant and 4,847 free pages; the test
disc was restored. The staged player never entered range of placed UID6935,
so that run does not verify placed-item retirement or the rendered notice.
Doctor Uniform and the remaining unsupported Silenced 12mm Handgun and
animated Demo_K000 classes are still unverified or unimplemented.

The follow-up Xbox run
`artifacts/xemu/nonweapon-pickup-20260929-221526/report.json` confirms that
the L6S3 scripted grant publishes the exact installed-table text “Miner
Envirosuit picked up” in guest memory, with the disc restored. Accepted
ordinary and scripted grants now select the authored singular, plural or
weapon-and-ammo notice from the actual grant amount. The native HUD pixels and
pickup audio were not inspected; placed suit collection is still open.

Campaign scenes retain authored item records plus one availability byte per
record. The first supported class is Handgun. Its table definition is checked
against the existing pistol world-model resource, and the pistol model is
included in resource selection even when no NPC carries it. No duplicate model
or texture allocation is introduced. Other item classes remain unsupported.

Available handguns draw at their authored position/orientation through the
shared static-model renderer and existing NPC scratch. Before combat, live
players within two units of the item (body origin) query precise geometry from
the eye to the pickup. Static/moving solids can prevent collection. A successful
inventory grant marks the item taken and it stops drawing. Full inventory adds
nothing and preserves the item. The authored SP record quantity is used.

This is first-pass scene availability: no item object-registry entry, mission
notification, pickup sound, respawn scheduling or moving-parent attachment yet.
Taken items remain taken across the existing in-place player respawn. Finite
inventory caps and actual starting supply remain as documented in FIRING-RUNTIME.

Process-local staging is exposed through RF_REPLAY_ITEM_UID and the XEMU harness
--item-uid option. It changes only the loaded test level's starting camera/body,
never host input. The separate campaign-item.bin flag is restored by the harness.

PC replay coverage: Handgun9427 in L1S1 visibly emits162 vertices; full reserve
keeps it visible; after spending/reloading one round, walking near it restores
one reserve round and removes it. Later spending/reloading cannot collect it
again. A depleted-magazine case grants all16 authored rounds. A synthetic solid
panel blocks the live collection function, and moving it aside permits exactly
one grant. All28 CTests pass. Evidence: artifacts/live-pickups/report.json.

Stock64MiB XEMU210-frame walk-and-collect PASS:Handgun9427 adds one missing
reserve round and disappears, matching PC state/HUD;6869 available pages.
Native framebuffer inspected. Both builds and28 CTests pass. Evidence:
artifacts/xemu/replay-20260914-023510/report.json. The obstruction/moved-panel
case is a PC integration test, not an authored native through-wall replay.


## First-pass health, armor and pistol-ammo pickups

Live collection also supports Medical Kit, Suit Repair and12mm_ammo. The three
classes retain static models/materials once per level (2MiB per-class load cap)
and share the existing render scratch. Unsupported classes remain absent.
Health and armor restore the authored quantity up to a provisional100 cap;
full values leave the item available and dead players cannot collect. Successful
collection removes the item once. A blue armor bar now accompanies health.
The same body-distance and precise solid-occlusion gate applies to every class.
This is a practical gameplay policy, not a claim of exact original pickup rules.

PC live replays use actual NPC shots to create deficits: Medical Kit9789 restores
4.8 health; Suit Repair9868 restores15.6 armor. Later enemy damage remains visible.
A full-health replay stays near the medical kit and leaves it visible/uncollected.
Integration tests cover full values, partial-to-cap grants, repeated collection,
dead-player rejection and ammo reserve capping. All28 CTests pass. Evidence:
artifacts/vital-pickups/report.json; tools/replay_vital_pickups.py reproduces it.
The12mm_ammo grant is integration-tested; an authored ammo-box render/collection
replay on a later level remains open. Pickup sounds, mission publication and
original starting grants remain open.

Stock64MiB XEMU300-frame repair replay PASS:15.6 armor restored once from
Suit Repair9868; final health71.2/armor84.4 reflect subsequent enemy hits.
Pickup/vitals state and sampled armor HUD pixels match PC;6837 available pages
(26.7MiB). Native framebuffer inspected. Evidence:
artifacts/xemu/replay-20260914-024732/report.json. Native medical-kit and
later-level ammo-box replays remain separate coverage items.

Live Assault Rifle and5.56mm_ammo class handling is connected. L4S5 rifle3415
grants ownership and42 loaded rounds, is removed once and can be selected with
D-pad Right/Tab. PC and stock64MiB XEMU pickup/fire replay pass; ammo-box
collection still needs an authored live replay. See FIRING-RUNTIME.md.
