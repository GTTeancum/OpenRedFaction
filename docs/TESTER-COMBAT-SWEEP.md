# Tester combat capability sweep

Source inspection of `babf35f1` on 2026-10-11, followed by a staged handheld
Rocket Launcher alternate implementation. The actual checkout was not modified.
No build, test, emulator run, fixture, image, extraction output, Git write or
asset mutation. This is a capability inventory plus unverified source delivery,
not new runtime acceptance or retail-completeness evidence.

## Conclusion

The ordinary first-playable combat loop is assembled in source: select an owned
weapon, shoot/contact, debit ammunition, reload, damage living actors, enter
NPC death, obtain drops, and continue engagement. All 17 player equipment slots
have live consumers. The additional slot 17 is the Machine Pistol Special view,
not an eighteenth independently cyclable weapon.

No wholly absent basic shooting/damage/death implementation was identified in
this bounded sweep. Do not interpret old general statements that "weapons are
unfinished" as meaning the usable weapon set has not been connected. The concrete missing handheld Rocket Launcher lock-on alternate found during
the sweep is now implemented in the accompanying staged patch, with parent
integration/compilation and action runtime still pending. No other major absent
player fire mode was identified in this bounded inspection.

## Current player equipment inventory

| Slot | Equipment | Source-connected play |
| --- | --- | --- |
| 0 | 12mm handgun | Primary shots, spread, finite magazine/reserve, reload |
| 1 | Assault Rifle | Primary burst; held automatic alternate; shared finite supply |
| 2 | Riot Stick | Delayed primary melee contacts; charged alternate contact and finite charge/reload |
| 3 | Shotgun | Two-shell/eight-ray primary; one-shell/four-ray alternate; last-shell fallback |
| 4 | Rocket Launcher | Primary straight flights, contact/blast damage, reload and terrain dispatch; staged hold/acquire/release homing alternate |
| 5 | Grenade | Delayed primary/alternate throws; timed/contact mode, bounce, object damage/blast, finite reserve |
| 6 | Sniper Rifle | Primary precision contact/actor penetration, alternate scope, reload |
| 7 | Rail Driver | Primary penetrating ray, alternate scanner, reload |
| 8 | Remote Charge | Primary throw, alternate short placement toss, finite reserve, attachment and charge/detonator transition |
| 9 | Remote Charge Detonator | Either trigger requests owned charge detonation; no fabricated separate ammo pool |
| 10 | Flamethrower | Continuous primary fuel use and damage/burns; delayed alternate canister, full-reserve replacement, reload |
| 11 | Riot Shield | Equipped geometric protection/durability, delayed primary bash, break/removal/fallback |
| 12 | Fusion Launcher | Primary flight, direct/radial damage and reload; explicitly primary-only authored profile |
| 13 | Machine Pistol | Primary automatic fire; alternate changes between base and Special magazines using authored transitions |
| 14 | Heavy Machine Gun | Primary automatic and slower/tighter alternate fire; finite magazine/reload |
| 15 | Precision Rifle | Primary fire, alternate scope, finite magazine/reload |
| 16 | Undercover handgun | Primary shots, silencer custom transition, suppressed-hearing policy, finite magazine/reload |

This lists source-connected behavior, not fresh runtime verification or every
resource in every level at once.
Sparse resource demand, actual ownership and existing special-resource admission
still determine availability. It does not authorize changing stock-64-MiB limits.

## Source joins checked

- `src/diagnostic/scene.c`: `campaign_combat_input_tick`,
  `campaign_combat_tick`, `scene_player_weapon_advance`, `campaign_enemy_tick`,
  `rf_scene_npc_death_entry`, ordinary pickup/drop dispatch and life recovery.
- Ordinary gunfire captures a shot and resolves its actual ray/pellets through
  cover, shield/body and supported object consumers. Rocket and precision slots
  explicitly leave the generic ray path for their own consumers.
- `scene_player_riot_primary.inc`, `scene_player_riot_alternate.inc`,
  `scene_player_shield_melee.inc`, `scene_grenade_gameplay.inc`,
  `scene_remote_gameplay.inc`, `scene_flame_input.inc`,
  `scene_flame_canister.inc`, `scene_fusion_input.inc`,
  `scene_fusion_projectile.inc`, and `scene_precision_gameplay.inc` supply
  dedicated melee, delayed, projectile and precision ownership.
- `scene_machine_pistol_mode.inc`, `scene_undercover_carry.inc`, the live
  Undercover input/advance joins, `scene_conventional_fire_policy.inc` and the
  scope/scanner consumers distinguish real alternate attacks from mode changes.
- NPC ordinary engagement has affiliation/sight and opposed acquisition, pursuit,
  aim admission, finite ammo/reload and fallback, accepted fire presentation,
  intervening actor/cover resolution and death entry. Dedicated consumers also
  exist for placed creature/robot/boss weapons; unsupported items are not silently
  converted into hitscan. Their presence does not prove their action runtime.
- Existing finite flight pools and source/generation checks remain in place.
  The staged alternate adds 228 bytes to the scene owner and 48 diagnostic bytes,
  with no heap allocation, asset loading, new flight pool or disk format.

## Remaining functional issues

1. **Rocket alternate delivery needs integration and the scheduled Xbox batch.**
   `scene_player_rocket_lock.inc` now supplies acquisition, full-handle target
   retention, steering and release admission into the ordinary launcher path.
   Existing ammo, reload, projectile sweep, impact/blast and render/audio owners
   remain consumers. HUD has lock progress and release instructions. Current
   saves/in-session loads explicitly refuse acquisition or live guided flights;
   they never write them as ordinary rockets. See `PLAYER-ROCKET-LOCK-FIRST-PASS.md`
   for grounded controls, approximations and the verification boundary.
2. **Shared-ammo automatic reload is corrected in source, pending compilation.**
   `scene_weapon_auto_reload_ready` now uses the existing shared pickup policy's
   catalog-wide same-ammo maximum, admitting legitimate surplus above the
   selected gun's capacity, including after the larger gun is removed.
   Negative or over-bound reserve and negative matching capacities remain
   rejected. Manual/empty-trigger reload, timing and inventory transfer are
   unchanged. The source correction is integrated and independently reviewed;
   hourly Xbox compilation remains pending. No fresh gameplay result is claimed.

Other explicit first-pass limitations, such as remote placement's short toss,
exact homing/animation/effect fidelity, broader action/save coverage and isolated
shield/terrain nuances, should remain refinement work unless a tester or bounded
Xbox check establishes a concrete gameplay blocker.

## Verification boundary and suggested closure language

This sweep executed no gameplay. Existing source comments and documents include
historical Xbox results for subsets, while numerous recent slices are compiled
only. They do not collectively establish a fresh whole-set runtime pass.

Suggested status after parent integration: "The 17-slot player weapon set and
basic combat loop are source-integrated for first-playable testing, including
Rocket lock-on alternate. Shared-ammo auto-reload has a bounded source
correction awaiting compilation. Broad combat runtime acceptance is still
pending; a neutral startup cannot establish Rocket lock acquisition, steering
or damage."
