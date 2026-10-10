# Depleted-grenade automatic replacement

Status: source-written, compilation and runtime unverified. No helper builds,
tests, fixtures, route runs, emulator sessions, captures, commits or cleanup were
performed for this slice. Parent owns shared scene wiring and the scheduled
Xbox batch.

## Original evidence

- RF.exe `4c6627..4c6631` resolves the authored Grenade identity into global
  `872118`.
- `4a6f41..4a6f57` allows that weapon through empty replacement even when the
  general Autoswitch Weapons option, player byte `+f40`, is disabled.
- `4a2974..4a2981` calls the empty consumer after ordinary projectile firing.
- `4a6f41..4a70db`, already reconstructed by `rf_weapon_decide_empty`, requires
  positive descriptor capacity, exhausted loaded plus reserve ammunition and
  no descriptor `+264` mask `0x20`, before selecting a replacement.
- `4a6e50`, already reconstructed by `rf_weapon_choose_available`, checks the
  player's 32 preference entries in order, requires actual ownership and checks
  loaded plus mapped reserve when capacity is positive.
- `4c2e55` parses `$Pref Position` into descriptor `+534`; `4c2c37` populates
  the authored 32-entry list; `4a23a0` copies it into player `+1154`.

### Explosive-defer preference is not an established retail default

The source consults player byte `+f41` when an eligible weapon has descriptor
`+268` mask `0x100`. A nonzero value defers it behind eligible unflagged weapons,
retaining the first flagged candidate only as a fallback.

A bounded read-only inspection of allocation/initialization `4a3310`, its
constructor `472ad0` and reset `4adba0` did not establish this byte's default.
Profile reader `4a8aa0` restores it from profile offset `+78`, bit 13, at
`4a8bbc..4a8bd0`; UI callback `452cf0` writes the inverse of its checkbox state.
Neither observation proves the initial default or the user's profile value.

This first-pass port explicitly uses `defer_flag=0`: strict authored ranking
among the admitted, usable candidates. This is a port policy, not a claim of
retail-default parity. No general Autoswitch option or saved preference is
invented.

## Live adapter

`src/diagnostic/scene_grenade_empty_selection.inc` owns only a 32-ID authored
preference list, a ready flag and a transient held-control mask (136 bytes).
The independent `rf_weapon_preference_load` reads the list from `weapons.tbl`;
it does not enlarge or change the existing supply catalog, its hash or saves.

The replacement decision consumes the actual player inventory. It constructs
bounded call-local supply/flag views from the retained supply and weapon-reset
catalogs, matching their IDs by name. No stock ammo, fabricated ownership,
ammo transfer or resource loading occurs. Candidates must map to an admitted
selectable slot and an existing first-person resource; projectile weapons also
need their corresponding live resource admission. The Remote Charge rank maps
to the charge slot, not the same-ID detonator slot.

The adapter invokes `rf_weapon_decide_empty` only after a successful grenade
publication and debit, with Grenade as `always_weapon`, general automatic
selection disabled, no passenger/linked owner and no paired-mode request.
Failed/full-pool throws, idle empty selections and non-grenade shots do not
request replacement. A remaining grenade or source flags preventing replacement
leave selection alone. With no suitable replacement the empty grenade remains
selected and manual cycling is still available.

The selected replacement goes through `campaign_select_primary`. Pending
conventional burst/delay and reload work are cleared, ammo is republished and
the new first-person weapon starts from an idle draw boundary. No animation
advance, projectile cancellation or scene reload is performed. The existing
conventional cooldown and the grenade throw's 180-tick cooldown remain intact.
Already-launched grenades keep their normal flight, fuse, damage and checkpoint
owner independently of the newly selected weapon.

Fire, alternate and reload controls held across the change are consumed until
their respective releases. This matters for automatic weapons: setting only
the conventional trigger's held bit does not prevent continuous fire. The
successful-change frame ends only its remaining weapon-input path, after all
special projectile/world ticks have run.

### Paired Machine Pistol and Undercover boundary

Completed Machine Pistol mode is preserved. Its ranked base identity must pass
the shared chooser, and the actual retained Special mode must also have usable
ammo and an admitted view. A Special-only-ammo candidate whose base mode is
empty is skipped. The actual-mode check is a bounded port guard against
selecting an unusable retained mode; it is stricter than the original chooser's
base-only ammunition test. General firearm depletion and paired replacement
remain outside this slice.

Read-only original-source review on 2026-10-10 established why a companion
cannot simply be added to this fallback:

- `4c6609..4c6622`, with name strings at `5a3314` and `5a3324`, resolves
  `85ccd8` to Machine Pistol and `85cd00` to Machine Pistol Special.
- `403250..403270` checks only the requested ID's ownership byte; there is no
  alias lookup. `4a6e84..4a6ec6` checks that exact ranked ID, descriptor capacity,
  reserve and magazine before returning it. It does not inspect the mode mask
  or add the counterpart magazine/reserve. Thus Special-only ammunition also
  fails the original authored base-ID scan.
- Only after that choice, `4a4c91..4a4ca7` checks the entity mode mask through
  `42a6b0` and remaps the requested Machine Pistol to Special. The mode setters
  `42a76f..42a7b4` and `42a86a..42a8d8` update selection and mask state without
  swapping the two magazines. This does not establish an automatic preference
  for whichever mode happens to have ammunition.
- Original acquisition `45a7b8..45a7cd` separately acquires Special when the
  base gun is acquired. The current port deliberately uses base ownership for
  both modes, distinct magazine/reserve addresses and selectable slot 13;
  view slot 17 is not an independently owned gun. Its existing checkpoint
  admission rejects separate Special ownership. This review changes neither
  contract and performs no grant or ammunition transfer.
- Undercover is a separate selectable weapon, with its own ownership and
  magazine plus the shared 12mm reserve. Its suppressor mode does not change
  that identity. The installed authored map has no Undercover or Special rank;
  form entry explicitly requests its configured weapon at `4b02b1..4b02c3`.
  No inspected selection path aliases Undercover to the ordinary handgun's
  preference entry.

The review therefore adds no executable behavior. Admitting Special-only ammo,
choosing the opposite mode automatically, or borrowing the ordinary handgun's
rank for Undercover would require a separately chosen port policy. Preserve the
authored list and existing ownership instead of claiming those extensions are
original selection semantics. Manual cycling and explicit mode switching
remain available. No build, test, syntax check or runtime run was performed.

## Parent integration contract

1. Declare the static load/reset/input-reset functions before early callers;
   include `scene_grenade_empty_selection.inc` directly after
   `scene_grenade_gameplay.inc`, before `campaign_combat_tick`.
2. Call `scene_grenade_empty_reset()` on scene startup and teardown. After
   successful `rf_weapon_supply_load`, while the same tables archive is open,
   call `scene_grenade_empty_load(&tables)` and propagate errors. Weapon-reset
   flags are read only at the later gameplay decision, after normal scene load.
3. Call `scene_grenade_empty_input_reset()` for frame-zero initialization and
   ordinary player relocation/restore/respawn boundaries. This does not erase
   the authored list or readiness. No held-control latch is serialized.
4. Immediately after the `combat_frame` duplicate-frame guard, before
   `scene_vehicle_equipment_tick` or another input-masking helper, call
   `scene_grenade_empty_input_tick(on_foot)`. Vehicle controls are not masked.
5. Add a `uint32_t *released` out-parameter to `scene_grenades_tick`, initialize
   it to zero and set it to one only after successful `rf_grenade_throw_resolve`
   with `spawned=1` and actual projectile publication. Pass a frame-local flag;
   do not infer success from diagnostic counters or reserve changes alone.
6. Keep Remote Charge and flamethrower/canister/visual ticks running with the
   old grenade selection. Move/remove the redundant slot-return inside the
   flame-resource block so the post-block decision is reachable for Grenade.
7. After that block, immediately before the existing special/conventional
   weapon return guards, call
   `scene_grenade_empty_after_release(stream,frame,released,on_foot,&changed)`.
   Propagate errors, and return `RF_OK` when changed is nonzero. At this point
   all projectile/world ticks are already serviced and the replacement cannot
   fire, reload or toggle its mode in the same frame.

No new save layout, supply ABI change, campaign route or per-slice validation
fixture is required. Runtime behavior and compilation await the parent's
scheduled stock-64-MiB Xbox batch.
