# Weapon pickups from an explicitly unarmed state

Status: source-written; compilation and runtime are unverified. Parent owns
shared scene wiring and the scheduled stock-64-MiB Xbox validation batch.
No helper build, test, emulator run, PC check, fixture, capture, commit or
cleanup was performed for this slice.

## Original evidence

- RF.exe `45a9d1..45aa03` selects the accepted weapon as the primary when the
  current primary is negative. This branch is independent of the subsequent
  armed-player preference/Autoswitch decisions and player bytes `+f40/+f41`.
- New ownership follows `45a7b3 -> 45a839`. Accepted ammunition replenishment
  for an already-owned weapon follows `45a90b -> 45a9ba`. Both reach the
  selection join at `45a9be`. Therefore this is not a first-ownership-only
  rule: the live operation must accept `grant.acquired || grant.rounds`.
- An ownership grant with zero ammunition still qualifies. This adapter also
  preserves the port's existing pickup-acceptance policy: an already-owned
  weapon whose pickup adds no ammunition does not qualify. The original
  negative-primary check at `45a9d1` does not itself test for positive rounds.
- Ammo-only item dispatch at `45a479 -> 45a500` returns before the weapon
  selection tail. Adding ammunition alone must not end explicit unarmed state,
  even when that ammunition's mapped weapon is already owned.
- Corpse drops are original world items: `42aed0 -> 459a90` finds a weapon's
  item class, whose `+0x40` field is its gives-weapon flag, and `42b226 -> 459100`
  creates the item. Collection reaches
  `4597b0 -> 45a3d0 -> 45a420 -> 45a6d0`, so successful corpse-drop collection
  belongs to the same bounded unarmed selection rule.

The retained general `campaign_ammo_publish` unarmed guard is intentional.
Removing it would make ammo-only grants and unrelated HUD publication choose a
weapon. The new selection consumer runs only at a proven successful item-grant
boundary; it does not change general automatic selection or profile options.

## Selection-only adapter

`src/diagnostic/scene_unarmed_pickup_selection.inc` adds no persistent state,
allocation, inventory fields or save-format changes. Its public scene-local
entry point is:

```c
static uint32_t scene_unarmed_pickup_select(scene_stream *s, int32_t weapon,
    uint32_t gives_weapon, const rf_weapon_pickup_grant *grant);
```

It returns one if selection changed and zero otherwise. It is a nonfallible
post-grant presentation/selection step: a missing view or unsupported identity
does not undo an accepted grant, block retirement or change notices/audio.

Admission requires all of the following:

1. The successful grant was a gives-weapon item and actually acquired ownership
   or added rounds.
2. `campaign_explicit_unarmed` is set. The helper does not infer this state from
   an empty magazine, a missing model, an unowned previous slot or a HUD value.
3. The player is alive, on foot and outside an active undercover player-form
   override. Mounted/vehicle equipment and active forms keep their own policy.
4. The exact acquired catalog weapon ID is valid and really owned after the
   grant. Catalog IDs, item-class indices and first-person slot indices are
   not interchangeable.
5. That ID maps to an admitted primary slot with its actual loaded first-person
   view and any required live special-weapon resources.

Remote Charge selects slot 8, never the shared-ownership detonator slot 9, and
requires the already-loaded pair. Grenade, rocket, flame, shield and fusion
selection require their existing live resource admission. An absent model or
resource is not loaded on demand by this helper.

Machine Pistol uses its base ownership/cycling slot 13. Its completed mode is
preserved, including the distinct Special view 17 and definition when active;
both views/custom-action owners must already exist. A pending transition is
not overridden. The alternate ID never becomes an independently selectable
primary or gains invented ownership. Unlike depleted-grenade replacement,
pickup selection does not require positive ammunition: the original unarmed
branch permits a newly acquired empty gun, and the retained mode is not
silently changed to make ammunition available.

Undercover weapon resources and pending suppressor work are respected. The
new automatic consumer conservatively leaves any active player-form override
alone. Existing manual cycling does not express a narrower form-only whitelist
that can be reused here; this is an explicit bounded adapter restriction, not
a claim that the original pickup code always rejected disguised players.
Normal form entry/exit, forced selection, manual cycling and suppressor state
are unchanged. A Silenced Handgun item is still its declared ordinary handgun
identity, not the separately implemented Undercover weapon.

On success, `campaign_select_primary` closes existing selection-bound weapon
audio and publishes the selected primary definition. The shared
`scene_player_auto_select_inputs()` clears old conventional burst/delay and
reload/alternate work, then latches held fire, alternate and reload controls
through the existing automatic-selection release gate. A held input cannot
fire, reload or toggle the newly selected weapon in the pickup frame or on the
next frame before release. The first-person pose/shot/reload cache is reset to
an idle draw boundary, ammo is republished and the existing selection counters
are updated. No extra animation step, projectile cancellation, ammo transfer,
inventory reset, world tick skip or scene reload occurs.

## Parent hook contract

Include the helper immediately after `campaign_ammo_publish`, once
`scene_stream`, selection functions, inventory/resource state and the
`scene_player_auto_select_inputs(void)` forward declaration are visible.

### Placed items, including extra firearm classes

In `campaign_pickups_tick`, retain an `acquired_weapon=-1` per-item local.
Populate it only in the weapon-grant branch, preserving the exact legacy grant
ID or resolving the validated extra pickup definition's declared `weapon`
through `campaign_weapon_supply.names`. After the existing zero-benefit check,
retirement and normal feedback, pass that ID, `definition->gives_weapon` and
the actual returned `grant` to the helper. Nonweapon items retain the invalid
ID, and ammo-only items fail the gives-weapon guard.

The extra classes do not use the legacy pickup-to-slot table. Their authoritative
weapon comes from `scene_extra_pickup_grant` resolving `definition->weapon`;
this includes Machine Pistol, heavy machine gun, scoped assault rifle and
shoulder cannon. The extra 7.62 mm and explosive 5.56 mm ammo classes remain
ammo-only and cannot select. Standard ammo boxes, health/armor and suit items
must likewise remain outside this selection effect. Preserve all existing
quantity conversion, shared reserve capacity, shield ownership/lifecycle,
pickup enablement, cover/distance, retirement, sound and message behavior.

### Corpse and scripted-drop ledger collection

In `campaign_weapon_drops_tick`, call the helper only inside the accepted
`grant.acquired || grant.rounds` block, passing `drop->weapon`, `1` and
`&grant`. The current live grant explicitly supplies gives-weapon=1 and the
drop's actual weapon ID. Preserve the existing collection and feedback order;
do not inspect whichever weapon the NPC presently holds after death.

Scripted NPC `Drop_Weapon` reuses this same finite, grounded drop ledger and
therefore uses the same collection hook. This adds no new disarm or drop
creation behavior. It also does not add support for unsupported drop weapons.

### Scripted item grants

Do not infer authority from a similarly named event. Existing documentation
records a `Give_Item` default callback, but the exact default weapon path must
be independently confirmed before hooking `campaign_apply_item_grant` for this
slice. Keep scripted grants unchanged until that same-path evidence is supplied.

### Unchanged boundaries

- This helper remains unarmed-only. The separate armed first-acquisition
  consumer now applies authored preference policy after successful pickups;
  see ARMED-PICKUP-SELECTION.md. Accepted refills still do not switch an armed
  player.
- Ammo-only items, full/no-benefit duplicate guns and invalid/unowned IDs never
  clear explicit unarmed state.
- Catalog resource demand does not grant ownership or imply selection.
- Grant amounts, reserves, loaded magazines and shared-pool limits remain owned
  by existing grant helpers.
- `campaign_ammo_publish` retains its explicit-unarmed early return.
- This unarmed helper does not own profile defaults, general ranking, armed
  autoswitch or paired-mode switching. The separately source-backed armed
  consumer leaves this helper and checkpoint semantics unchanged.

Source review is the only verification performed here. The parent's next
scheduled Xbox batch must establish compilation and any naturally available
bounded unarmed-pickup runtime case; no per-slice fixture was created.
