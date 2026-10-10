# Sea Creature Sonar Attack: finite direct-flight first pass

Status: integrated after independent source review, awaiting the 05:00 Xbox build.
Scene selection, lifecycle/save wiring and shared direct-flight consumers are included.
No compilation, syntax check, test, fixture, emulator, PC build or campaign route
was run for this slice. Gameplay and presentation remain unverified.

## Actual missing consumer and authored contract

Read-only inspection of the installed original level records found these
selected, actually owned `Sea Creature Sonar Attack` sources:

- `L5S3`: UID 4645.
- `L10S2`: UID 6810.
- `L10S3`: UIDs 6795 and 6808; 6808 has friendliness 1.
- `L10S4`: UID 7013.

Existing placed-NPC loadout acquisition already admits the actual selected
weapon and fills its mapped reserve. This adapter neither grants a weapon nor
edits a level, affiliation or AI mode. Selection qualifies the real selected
owned weapon, rather than hardcoding a class or these UIDs. Ordinary awareness,
opposed-target, aim, range, LOS and action policy remain the parent's owners.

Installed `weapons.tbl`, beginning at line 2145, specifies:

- `SonarAttack.VFX`; `12mm`; maximum reserve 200/200; no magazine or reload.
- Energy damage kind 5; damage 80/80; fire wait 2.5 seconds; AI range 15/15.
- Collision radius 0.5; speed 20; lifetime 3 seconds; mass 5.
- Weapon flags `underwater`, `from_eye`, `torpedo`: 0x10600.
- Flags2 `undeviating`, `no_fire_through`: 0x28.
- No impact delay, homing, sticking, glow, radial damage or crater.
- No authored spread, AI-spread override, projectile count, burst or piercing.
- Empty launch/flight/default-impact sounds; flesh group `Impact Flesh`.

`scene_ai_sonar_profile.inc` reads this exact bounded direct-flight primary,
checks catalog/name agreement and the expected flags, and rejects unsupported
magazine, delay, spread, burst, piercing and alternate weapon-type additions.
It does not relax the conventional, explosive, Laser or TriBeam readers and
does not change their catalog or save layouts. The existing NPC default of
zero AI spread is retained; `undeviating` is not proof of original spread-zero
behavior, and exact original default aim/spread remains unverified.

## Admission and immediate owned flight

`scene_ai_sonar_launch(owner, target)` requires the selected owned Sonar,
generation-qualified live NPC source, zero loaded magazine and positive mapped
reserve. It uses current eye position as the authored `from_eye` source and
the ordinary caller's already selected target ray. Source full handle, fresh
AI-scaled damage, damage kind, weapon ID and liquid flags are captured before
publication. Current eye plus the admitted target ray remains the existing
NPC aim/muzzle approximation despite the authored `from_eye` bit; exact
original NPC firing-pose and aim construction are not reconstructed here.
It reuses direct-flight preparation, not explosive preparation or hitscan
damage. There is no delayed attack owner or second launch timer.

The independent pool has 64 slots. A full pool refuses before spread-RNG
publication, ammunition debit or accepted-shot presentation. After successful
callback-free preparation, `rf_weapon_consume_shot` debits one real reserve
round; no fallible operation or callback intervenes before flight and RNG
publication. Parent must exclude Sonar from the later generic ammo debit and
generic hitscan resolution while retaining normal cadence/onset presentation.

The finite policy is deliberate port gameplay scope. Original `4257c0`,
particularly `42580e..425827`, establishes clipless reserve debit and clamp at
zero. Original `425b89..425c0c` gates empty-ammo suppression through player or
controlled-player predicates. It does not prove that an ordinary single-player
NPC stops firing at zero. This adapter preserves the surrounding port's
positive-reserve readiness rather than reconstructing infinite NPC firing.

## Liquid flags are separate from swimming AI

Original `4c8c00` and `425ac2..425ada` use weapon bit 0x200 to permit submerged
firing. They do not require a wet muzzle. The original dry-muzzle gate at
`426587..426596` first checks `40a270`'s actor-class flag 0x400, then calls the
point-liquid predicate `4ce080`. This is a separate class flag word, not the
weapon's `from_eye` bit with the same numeric value.

Actual `Sea_Creature` authors movement mode `sub`, but its explicit flags
`swim`, `sentient`, `collide_corpse`, `custom_corpse`, `slippery` lack that class
`sub` flag. Original parser `41bbb9..41bbcf` stores movement mode at descriptor
+0x30, while `41bc25..41bc42` ORs explicit flags into +0x724. No movement-mode
to sub-flag inference was found. Therefore this actual owner has no invented
wet-only launch gate, player-swim dependency or cached body-room substitute.
Full creature swimming/navigation behavior is outside this combat adapter.
If a future separately supported source has the explicit class-sub gate, its
exact current muzzle query must follow `4ce080`'s liquid-presence test and
point-Y comparison against room minY plus depth, not a water-type-1-only test.

The shared runtime profile/payload now carries immutable `liquid_weapon_flags`.
Only Sonar initializes them to its admitted 0x10600. Existing zero-initialized
Laser/TriBeam profiles retain zero. The shared tick reads the accepted shot's
copy, never a mutable selected weapon or current profile. Per-flight query
flags still begin at 0x1004 and evolve independently through existing core
`rf_weapon_flight_step_liquid`.

Original `4c4ea1..4c4eb7` sets torpedo lifetime to zero on an admitted authored
liquid contact. Existing core flag-0x10000 handling emits terminal kind 2,
never direct damage or an explosion. The original-backed one-sided liquid
face sweep is unchanged: this does not promise removal at every geometric
water-plane crossing. There is no wet-to-dry endpoint synthesis, reversed
face or custom water-plane test. Existing fraction-1 endpoint and ordinary
solid-contact behavior are retained.

The shared generic water splash/ripple presentation remains a port
approximation. Original `4c4e30` supports a separate liquid effect descriptor,
but does not prove this exact `Medium Water Splash`/ripple combination. Sonar
adds no authored launch, loop or impact-audio consumer and no impact Vclip.

## Contacts and transient lifecycle

The shared nearest-contact composer retains static world/liquid, movers,
detached pieces, vehicles, clutter, actor bodies and current physical shields.
Bodies/world/retained environmental sweeps use authored radius 0.5. Shields
retain the existing directional centerline policy, not swept-sphere shield
parity. Parent prepares the player shield pose before this tick; NPC shield
poses are demand-prepared by the existing composer.

The accepted full source handle is retained after source removal. Contact
candidates retain full target handles, with current identity revalidation.
A selected physical shield consumes the contact, including breaking/stale
contacts. NPC Nano protection sees the exact Sonar weapon ID before ordinary
kind-5 damage. No actor penetration, radial damage, explosion, crater,
ricochet, homing, target reacquisition or additional ammunition debit is added.

Sonar owns independent pool state, counters, reset epoch and update guard.
The shared tick stages each shot locally, publishes terminal retirement before
callbacks and stops after a reset epoch changes. `scene_ai_sonar_pending()`
includes an active update even when its last flight is already terminal.
Save-only admission/capture must refuse pending Sonar, without changing shared
row capture used by loading. Successful loaded-state publication, world/life
replacement and teardown reset it. Failed load staging preserves live shots.
There is no new save layout and no Sonar row masquerading as an RFAP rocket.

## Narrow parent hooks

1. Add forward declarations for `campaign_enemy_sonar_open`,
   `scene_ai_sonar_reset` and `scene_ai_sonar_pending` alongside Laser/TriBeam.
2. Open the exact profile with existing tables/supply/reset catalogs. Include
   `scene_ai_sonar_profile.inc` after `scene_ai_laser_profile.inc`, before
   ordinary/opposed selection users.
3. Add `campaign_enemy_sonar_select` to both ordinary and opposed selection,
   retaining actual selected ownership. Dispatch `campaign_enemy_sonar_ready`
   before generic reserve-fed readiness.
4. Forward-declare and call `scene_ai_sonar_launch(campaign_npc_body *, const
   float *)` alongside immediate Laser launch after ordinary firing admission.
   Treat `RF_NOT_FOUND` as refused launch, before any generic acceptance.
5. Exclude exact Sonar from generic ammunition debit and the generic hitscan
   tail. Retain the single normal cadence, counters and onset presentation.
6. Include `scene_ai_sonar.inc` after `scene_ai_laser.inc`. Tick Sonar alongside
   Laser/TriBeam, after current player shield pose preparation.
7. Mirror Laser's save-only pending guards and successful-load/world/life/close
   resets. Do not add a pending rejection to load's shared source-row capture.

`rf_scene_ai_sonar[12]` uses the existing direct-flight diagnostic meanings:
launches, contacts, expiry, live, pool refusals, actor contacts, shield contacts,
Nano contacts, last source, last target, last frame and last status.

## Presentation and remaining verification

The installed `SonarAttack.VFX` is particle-only: zero meshes and one PART
`VParticle02` parented to `Scene Root`, with duration 25 effect frames. It cannot
be admitted by the existing mesh-only Laser/TriBeam geometry wrapper. This
helper deliberately supplies no substitute mesh or laser visual. Parent owns
any separately source-grounded particle integration and its resource scope.

These source changes alone do not establish actual launch, finite exhaustion,
liquid-face expiry, shield/Nano order, save/load behavior, visual admission or
Xbox runtime. Remaining swimming AI and optional presentation refinements must
not be described as implemented by this direct-flight adapter.

## Subsequent source integration

The original particle-only visual now has a separately reviewed bounded consumer and parent resource/render/lifecycle wiring (SEA-CREATURE-SONAR-PART.md), still awaiting compilation and visual runtime. The mechanics helper remains independent of rendering and supplies no substitute mesh.
