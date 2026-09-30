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

rf_item_definition_read/load resolves a named items.tbl class into a472-byte
record: model name/type, associated weapon name, gives-weapon distinction,
SP count, no_pickup flag, optional pickup sound/range/gain and four English pickup notices (single, multi,
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
The first-pass class table now recognizes Miner Envirosuit, Doctor Uniform and
Silenced 12mm Handgun. The two suit classes account for ten formerly unsupported
placed items. Both are static
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
Doctor Uniform placed collection and animated Demo_K000 remain unverified or
unimplemented.

The follow-up Xbox run
`artifacts/xemu/nonweapon-pickup-20260929-221526/report.json` confirms that
the L6S3 scripted grant publishes the exact installed-table text “Miner
Envirosuit picked up” in guest memory, with the disc restored. Accepted
ordinary and scripted grants now select the authored singular, plural or
weapon-and-ammo notice from the actual grant amount. The native HUD pixels and
pickup audio were not inspected; placed suit collection is still open.

The Silenced 12mm Handgun is authored once in `train02.rfl` as UID6596. Its
installed table entry uses a static `weapon_silenced.v3d` world model and maps
to the existing `12mm handgun` weapon. The first-pass class binding therefore
uses that weapon's ownership and ammunition path; whether it also attaches a
suppressor or selects a distinct first-person view remains open. The
stock-64-MiB Xbox fixture at
`artifacts/xemu/silenced-pickup-20260929-223037/report.json` loaded the model
and collected UID6596 during a 140-frame run with 4,842 free physical pages.
The neighboring Sniper Rifle is placed less than 0.4 units away and collects first.
The starting pistol has a full reserve, so the fixture fired and reloaded it
before the silenced item's ordinary ammo grant could retire it. The final
guest pickup counter recorded both items and published the installed text
“12mm pistol with silencer picked up”; the test disc was restored. This
verifies inventory collection and notice publication, not that a suppressor
was attached or rendered on the first-person pistol.

Campaign scenes retain authored item records plus one availability byte per
record. The initial supported class was Handgun. Its table definition is checked
against the existing pistol world-model resource, and the pistol model is
included in resource selection even when no NPC carries it. No duplicate model
or texture allocation is introduced. The current class list is in
`src/diagnostic/scene_pickup_class_names.inc`; animated Demo_K000 remains open.

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


## Xbox pickup audio (2026-09-30)

Accepted placed-item collections now request spatial sound after committing the
inventory/vitals grant and retiring the item. Rejected/full, occluded and already
retired items do not reach this call. Missing audio remains nonfatal. Scripted
Give_Item does not acquire an invented world-pickup sound side effect.

Original RF.exe4594f0 selects class+0x28 when it has a registered sound, otherwise
sound12 for class weapon/ammo fields+0x3c/+0x40 and sound0 for other items.
RF.exe459520 plays that sound at item+0x3c with scale1 and category0. Installed
sounds.tbl defines weapon_pickup.wav at slot12 (distance5, volume0.7) and
powerup_pickup.wav at slot0 (distance2.5, volume0.7). The items.tbl Envirosuit
override is envsuit_pickup.wav, distance5, volume0.9.

The item reader owns override metadata and rejects malformed overrides without
publishing partial output. PCM loads on demand through the existing budgeted,
evictable sound bank and uses the normal spatial voice path; it does not enlarge
the1280KiB sample budget. The definition grows by72 bytes per loaded class.

Compiled NXDK parser check: tools/check_xbox_item_sound.py reads installed
Handgun,12mm_ammo and Miner Envirosuit definitions and rejects a malformed
sound override. Evidence: artifacts/xbox-item-sound.json. Placed Envirosuit and
powerup playback, dropped-weapon feedback and audible device output remain
unverified; no PC game, images or campaign playthrough is part of this check.

Stock64MiB XEMU140-frame rifle3415 check PASS: one collection starts exactly
one spatial slot12 voice, with nonempty PCM and authored distance5/volume0.7;
forward/back/forward selection and firing still pass. Free memory at completion:
7079 pages (27.65MiB). Harness flags and ISO were restored.
Evidence: artifacts/xemu/weapon-pickup-20260930-113501/report.json. This proves
sample loading and voice submission, not an independently inspected audible mix.


## Miner Envirosuit armor callback (2026-09-30)

RF.exe458960 clears the item callback table.4589b9 registers45a050 for the
Miner Envirosuit; Suit Repair uses45a1f0 and Medical/First Aid use45a2e0.
For single-player,45a050 resolves the receiving entity and item, rejects when
entity+0x38 armor is already at class+0x48 armor, otherwise copies the class
maximum into current armor and publishes the ordinary pickup notice. Quantity
is used for the notice, not the refill amount. This is a full refill, not one
armor point and not a clothing/undercover transition.

Placed-item4597c2 and scripted Give_Item4bb73e both dispatch class+0x48's
callback when present; they call default45a3d0 only when it is absent. The live
port now shares its suit refill helper between these paths. Full armor leaves
the placed suit available and does not issue its sound/notice; successful
collection retains the normal retirement and authored sound path. Scripted
full-armor rejection is a nonfatal no-op and does not emit pickup audio.

No Doctor Uniform or Silenced Handgun callback is registered by458960.
Doctor Uniform therefore follows the no-weapon notice branch45a4cc; its name
alone does not authorize an undercover state change. The installed Silenced
12mm Handgun class grants ordinary12mm handgun through45a6d0, which receives
weapon ID and quantity, not a suppressor flag. Its item model/name are distinct
from the separately implemented Undercover weapon and suppressor mechanism.
Do not add gameplay flags based on these item names without contrary evidence.

Native90-frame L8S4 suit8614 check: armor100 leaves the suit available; after
a process-local reduction to25, the ordinary collection restores100, retires
it once and starts its envsuit_pickup.wav override (distance5, gain0.9).
A scripted grant then restores50 to100, and a second full-armor scripted grant
adds no accepted grant or sound. Two unrelated nearby pickups precede the suit;
the first harness assertion incorrectly assumed zero other collections. The
retained guest data passes corrected before/after assertions without rerunning
the guest. tools/xemu_miner_suit.py now uses these assertions, and the existing
xemu_weapon_pickup.py --nonweapon option invokes this focused check.

Evidence: artifacts/xemu/miner-suit-20260930-114042/report.json retains the
initial assertion failure and guest data; validation.json records the corrected
PASS.5217 free pages (20.38MiB), flags/ISO restored, NXDK build passed. Audible
mix, images and natural campaign traversal were not inspected. General Medical
Kit/Suit Repair class limits and difficulty multipliers remain a separate gap.


## Health/armor class limits and scripted grants (2026-09-30)

Medical Kit, First Aid Kit and Suit Repair now use the receiving player's class
health/armor maximum instead of an unconditional100. Classes with no armor
capacity cannot consume a repair pack. The same helper now handles scripted
Give_Item grants for all three classes, which previously returned unsupported.
Full or zero-quantity grants do not consume placed items or issue feedback.

The shared rf_entity_vital_pickup_sp helper applies the installed593e34
multipliers2,1,0.8f,0.7f for difficulty0..3, followed by the45a1f0/45a2e0
+0.5/truncation rule and class cap. It uses exact integer representations of
these binary32 factors, avoiding x87 precision-mode dependence:25*0.7f rounds
to17 under the original extended-precision path. Invalid quantities, limits or
difficulty fail without publishing output. The live caller uses the retained
campaign difficulty, currently normal on a fresh campaign; menu selection and
the original global scale-bypass mode are not implemented here.

Focused native90-frame L1S1 check PASS: a full-health class with maximum80 leaves
Medical Kit9789 available; a staged deficit60 is restored to80 once, with one
slot0 powerup sound submission. Scripted Medical Kit and First Aid grants use
easy/hard multipliers; a zero-armor class rejects repair, a class with maximum150
caps repair from140 at150, and the subsequent full-armor grant does nothing.
Stock64MiB free memory3930 pages (15.35MiB), test flags/ISO restored.
Evidence: artifacts/xemu/vital-pickups-20260930-114554/report.json.

The final integer-rounding change was built during fixture restoration and
checked against the original binary in14 focused health/armor cases (all four
difficulties, fractional cap, zero cap, full cap) using compiled NXDK code.
Evidence: artifacts/xbox-vital-pickup.json and tools/check_xbox_vital_pickup.py.
Original lookup/player-notice boundaries are stubbed in that comparison; its
actual amount, cap and float conversion code executes. The native integration
run preceded only that arithmetic replacement; no second XEMU run, PC game,
image capture, audible-output inspection or campaign playthrough was performed.
