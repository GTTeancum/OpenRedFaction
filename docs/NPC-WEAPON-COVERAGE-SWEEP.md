# Authored NPC weapon coverage sweep

## Result and limits

The bounded October 12 source/data sweep found no additional substantially
missing selected NPC weapon family after Flamethrower and shoulder cannon.
This is implementation coverage, not a gameplay, save/load, campaign-route,
stock-memory or retail-fidelity pass. Shoulder cannon was integrated by the
parent during this sweep and awaits the sole 03:00 stock64 batch; Flamethrower
compiled in the 02:00 batch. Existing family-specific limitations remain.

Read-only parsing covered all **94 installed v180 RFLs**: 25 in levels1.vpp,
24 in levels2.vpp, 19 in levels3.vpp and 26 in levelsm.vpp. Their **1,610 entity
records** occur in 66 single-player/training levels; the other 28 levels have
no entity records. The scan resolved 63 installed class definitions and all
44 weapon names before applying actual instance overrides. It found:

- **32 nonempty effective primary families**, on 1,134 authored records.
- **Four nonempty effective secondary families**, on 24 authored records.
- 476 records with no selected primary and 1,586 with no selected secondary.

Counts are authored rows, including hidden, disabled, dead or repeated-section
placements; they are not simultaneous live actors. Secondary totals overlap
primary totals. Generated actors are explained separately below.

Resolution follows [scene_npc_loadout.inc](../src/diagnostic/scene_npc_loadout.inc):
class defaults first; explicit `none` clears; empty/unknown names inherit;
`glock` aliases `12mm handgun`. The v180 record layout is in
[level.c](../src/core/level.c), `rf_level_entity_next` and
`rf_level_entity_ai_read`. Merely finding a weapon in weapons.tbl was not
counted as an NPC implementation gap.

## Primary coverage

The ordinary selectors below feed `campaign_enemy_tick` in
[scene.c](../src/diagnostic/scene.c), with real acquisition, aim/range, supply,
attack dispatch and damage paths. Projectile selectors dispatch their dedicated
consumer before the generic firearm ray. Mounted/vehicle families use their
separate owners and are not admitted as ordinary humanoid rays.

| Actual effective primary | Authored rows | Live admission / consumer |
| --- | ---: | --- |
| 12mm handgun | 279 | [ordinary selector](../src/diagnostic/scene_ai_weapon_selection.inc), pistol contact/damage in scene.c |
| Assault Rifle | 91 | ordinary selector, finite firearm ray in scene.c |
| Riot Stick | 25 | ordinary selector, `campaign_enemy_melee_actor_contact` in scene.c |
| Shotgun | 75 | ordinary selector, [pellet consumer](../src/diagnostic/scene_ai_shotgun.inc) |
| Sniper Rifle | 24 | ordinary selector, precision penetration in scene.c |
| rail_gun | 60 | ordinary selector, world-penetrating firearm dispatch in scene.c |
| Machine Pistol | 66 | [extra firearm selector](../src/diagnostic/scene_ai_extra_weapons.inc), finite ray in scene.c |
| heavy_machine_gun | 77 | extra firearm selector, finite ray in scene.c |
| scope_assault_rifle | 228 | extra firearm selector, finite ray in scene.c |
| Rocket Launcher | 23 | explicit resource-gated slot 4, [NPC rocket launch/flight](../src/diagnostic/scene_ai_rocket.inc) |
| Grenade | 15 | explicit resource-gated slot 5, [NPC grenade launch/flight](../src/diagnostic/scene_ai_grenade.inc) |
| Flamethrower | 5 | [exact selector](../src/diagnostic/scene_ai_flame_profile.inc), [finite fuel/volume consumer](../src/diagnostic/scene_ai_flame.inc) |
| shoulder_cannon | 16 | [exact selector](../src/diagnostic/scene_ai_fusion_profile.inc), [NPC Fusion launch](../src/diagnostic/scene_ai_fusion.inc), shared flight/contact; compilation pending |
| Tankbot Chaingun | 1 | [exact clipless selector](../src/diagnostic/scene_ai_tankbot_primary.inc), [actor penetration](../src/diagnostic/scene_ai_tankbot_penetration.inc) |
| Drone Smash | 1 | [exact selector and delayed melee](../src/diagnostic/scene_ai_drone_smash.inc) |
| Reeper Claw | 14 | [creature selector](../src/diagnostic/scene_ai_reeper_claw.inc), shared delayed melee owner |
| Baby Reeper Claw | 37 | creature selector, shared delayed melee owner |
| Mutant Attack 1 | 3 | creature selector, shared delayed melee owner |
| Mutant Attack 2 | 4 | creature selector, shared delayed melee owner |
| Laser | 11 | [exact selector](../src/diagnostic/scene_ai_laser_profile.inc), [Meca laser flight](../src/diagnostic/scene_ai_laser.inc) |
| TriBeam Laser | 7 | [exact selector](../src/diagnostic/scene_ai_tribeam_profile.inc), [retained Spike attack/flight](../src/diagnostic/scene_ai_tribeam.inc) |
| Capek Cane | 2 | [exact selector](../src/diagnostic/scene_ai_cane_profile.inc), [retained Cane attack/guidance](../src/diagnostic/scene_ai_cane.inc) |
| Sea Creature Sonar Attack | 7 | [exact selector](../src/diagnostic/scene_ai_sonar_profile.inc), [Sonar consumer](../src/diagnostic/scene_ai_sonar.inc) |
| Rock Snake Spit | 1 | [exact Small profile](../src/diagnostic/scene_ai_spit_profile.inc), [delayed Spit flight](../src/diagnostic/scene_ai_spit.inc) |
| Big Rock Snake Spit | 1 | exact Big profile, delayed Spit direct plus independent radial damage |
| Vauss | 13 | eight stationary owners via [turret combat](../src/diagnostic/scene_turret_combat.inc); Meca/eligible ordinary owners via [NPC Vauss selector](../src/diagnostic/scene_ai_vauss_primary.inc); Auto Turret heads below |
| HEAP | 3 | turret exact primary selection, [HEAP flight](../src/diagnostic/scene_turret_heap.inc) through [turret shot dispatch](../src/diagnostic/scene_turret_scene_combat.inc) |
| Drill | 1 | [vehicle loadout](../src/diagnostic/scene_vehicle_loadout.inc), [player-operated Drill](../src/diagnostic/scene_driller_runtime.inc); authored AI disabled |
| Jeep Gun | 7 | vehicle loadout, [Jeep/APC primary flight](../src/diagnostic/scene_apc_primary_runtime.inc); player gunner, including the NPC-driven Jeep |
| APC Minigun | 5 | vehicle loadout, [autonomous APC consumer](../src/diagnostic/scene_vehicle_ai.inc) and player primary owner |
| Torpedo | 10 | vehicle loadout, autonomous sub consumer and [player torpedo owner](../src/diagnostic/scene_submarine_weapon_runtime.inc) |
| Fighter Minigun | 22 | vehicle loadout, autonomous Fighter consumer, [scripted Attack](../src/diagnostic/scene_vehicle_attack.inc), and player Fighter owner |

## Secondary coverage

“Secondary” here means the entity's second weapon selection, not the alternate
fire mode of its selected primary.

| Actual effective secondary | Authored rows | Live admission / consumer |
| --- | ---: | --- |
| Drone Missile | 1 | L3S4 UID844, exact Drone Smash/Missile pair; [independent secondary acquisition/cooldown](../src/diagnostic/scene_ai_drone_secondary.inc) plus scene_ai_rocket.inc launch/flight |
| Tankbot Missile | 1 | exact Chaingun/Missile pair; [independent secondary owner](../src/diagnostic/scene_ai_tankbot_secondary.inc) plus scene_ai_rocket.inc launch/flight |
| Fighter Rocket | 19 | exact vehicle loadout, scene_vehicle_ai.inc secondary gates and physical missile owner; player Fighter alternate also implemented |
| APC Rocket | 3 | AI-disabled L15S1 UID3303, L15S2 UID9680 and L15S4 UID9707; [player mortar owner](../src/diagnostic/scene_apc_secondary_runtime.inc) |

The two AI-enabled APCs, L1S3 UID26/9627, explicitly select secondary `none`.
The three mortar-equipped APCs disable AI. All seven Jeeps and the single
Driller also disable authored AI. Thus a missing autonomous mortar, Jeep-gunner
or Drill selector is not evidence of an actual selected NPC combat actor
without its required attack in this scan. This does not claim new event-mode
combinations or every possible vehicle extension are covered.

## Exceptions that do not add a missing selected family

- Nineteen authored Auto Turret bases create real dependent Vauss heads through
  [scene_turret_generated.inc](../src/diagnostic/scene_turret_generated.inc).
  Sixteen bases select no primary, three explicitly say Vauss. The generated
  heads are not additional RFL records and are not folded into the 13 Vauss-row
  count. Their attack uses the stationary turret consumer.
- L10S3 Sea_Creature UID4310 spells its primary `Sea Creature Attack`, which is
  absent from the weapon catalog. Original/port unknown-name inheritance keeps
  its class-default `Sea Creature Sonar Attack`; it is included among seven
  Sonar owners, not reported as a new unsupported weapon.
- `camera1` (two rows) and `Bucket Bot` (one row) have no matching installed
  class declaration and no explicit selected weapon. They are unresolved class
  metadata, not assumed working actors or a missing weapon consumer.
- Eight of 44 catalog names are never an effective authored primary/secondary:
  Remote Charge, Remote Charge Detonator, Undercover 12mm handgun, Machine
  Pistol Special, riot shield, Rock Snake Smash, Big Rock Snake Smash and
  Tankbot Smash. The first five therefore create no new selected-NPC gap here;
  Undercover handgun already has an ordinary firearm selector, and player or
  defensive-item consumers are separate from this scan.
- The three unselected Smash names are actual **class-default melee metadata**
  for the two Snakes and Tankbot, rather than wholly unused content. Their
  selected primary/secondary attacks are covered above. Automatic close-range
  substitution to these additional melee weapons was not established by this
  selected-loadout sweep and is not claimed implemented or promoted into a new
  system task.

## Evidence and unchanged boundaries

The external evidence bundle contains all exact archive/level/UID/class/raw
and resolved weapon rows, record offsets, authored AI/seat fields, per-level
counts, extracted installed tables and a read-only extraction script.
Flamethrower reproduces the prior five-owner inventory; shoulder cannon
reproduces the prior 16-owner inventory. No engine executable or test harness
was run. No builds, tests, gameplay fixtures, campaign routes, emulator, images,
HDD operations, cleanup, Git writes or original-input writes were performed.
Only external evidence and this document were created; all game/save/memory
admission and existing pending-state refusal guards are untouched.
