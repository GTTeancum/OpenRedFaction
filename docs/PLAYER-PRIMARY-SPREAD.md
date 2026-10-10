# Player primary firearm spread

Integrated October 10, 2026 after independent source review against exact
commit `40d8fe0109b6f3ff8bdbacd80b09cfeb051fcb5c`. No new build, syntax
check, test, emulator run, fixture, route, image, inventory grant or original-data
edit was performed. The next coordinated Xbox compilation and action runtime
are pending; the earlier 06:00 build at `28ad4190` does not validate this change.

## Original evidence and bounded policy

Read-only RF 1.20 NA `RF.exe` and installed `tables.vpp/weapons.tbl` inspection
establishes the missing live spread consumer. Input provenance is retained in
[PROVENANCE.md](PROVENANCE.md). Exact authored single-player primary values:

| Scene slot | Authored weapon | Spread degrees |
| --- | --- | ---: |
| 0 | `12mm handgun` | 0.5 |
| 1 | `Assault Rifle` | 0.8 |
| 16 | `Undercover 12mm handgun` | 0.5 |

- Loader `0x4c3811..0x4c385f` parses `$Spread Degrees` into descriptor `+0xd8`
  and the separate multiplayer value into `+0xdc`.
- The accepted firing function `0x425830` calls spread resolver `0x42d040` at
  `0x4262cf`. Resolver `0x42d040` calls player lookup `0x4a3740` at
  `0x42d07d`, converts its returned pointer to a boolean at `0x42d097`, then
  passes that boolean to `0x4c8770` at `0x42d09c`.
- Accessor `0x4c8770` returns selected player-primary spread from descriptor
  `+0xe0` at `0x4c87b5`, or player-alternate spread from `+0xec` at `0x4c87cc`.
  The NPC branches instead use `+0xf8` / `+0x104`.
- Positive spread at `0x4265ef..0x426647` is converted from degrees using
  constant `0x589428` (`0.01745329238474369`), cone-sampled, then rotated into
  the shot basis before projectile creation.
- Neither this accessor nor the shot-spread block reads the Flags2
  `undeviating` flag. All three authored weapons declare that flag; it does
  not justify bypassing this bounded positive-spread consumer. No suppressor
  state is read in these spread paths.

This does not establish the loader-to-effective-field initializer linkage:
`+0xe0` is not claimed to be the raw `$Spread Degrees` storage. The port retains
its existing fixed authored-angle policy. Original resolver `0x42d040` also
has separate zero-spread exceptions for player `+0xf94` through `0x4ace90`,
entity `+0x814` mask `0x4` and class `+0x728` mask `0x100`, plus entity `+0x13b4`
scaling for the named-special-weapon predicate `0x4c9920`. Reconstructing those
dynamic exceptions is outside this slice; none is an `undeviating` test.

The shared primary parser already retains validated SP `spread_degrees`
(`src/core/entity_assets.c:660..664`), independently of AI/MP values.
`campaign_select_primary` assigns the actual selected slot's definition to
`campaign_pistol`; the variable name does not mean slot-0 data is reused for
other weapons. No new angle constants, metadata fields or parser changes are
introduced by this consumer.

## Admission, mode and contact ordering

The new branch is in the existing pellet loop in `campaign_combat_tick`, after
successful ordinary ammo consumption, ammo publication, launch feedback and
hearing, and before the shared handheld penetration or ordinary contact path.
It admits only slot 0, slot 16, or slot 1 with effective `alt == 0`.

Existing guards remain in control: selected ownership, explicit unarmed state,
mounted/special-controller exclusions, Undercover pending transition, reload,
combat inhibit, trigger/cooldown, accepted-fire and real loaded-ammo handling.
Idle, dry, rejected and inhibited attempts return before the new sampler.
Undercover alternate remains the existing suppressor toggle, not a new attack;
pending attach/detach blocks fire, and either settled suppressor state uses the
same authored primary spread without changing launch sound, hearing or visuals.

Every accepted ordinary Pistol/Undercover shot samples once. Every emitted AR
primary burst round samples once, including queued rounds emitted after trigger
release. No sampling is moved to the trigger edge or added to the burst queue.
The sampled 100-unit eye-forward ray reaches all existing shield, actor, prop,
vehicle, fragment, world and rubble contact selection. When the accepted AR shot
has authored piercing enabled, that same sampled ray enters
`scene_handheld_penetration_fire`; continuing through actors does not draw again.

The new slot/mode condition is disjoint from all existing spread consumers:
Shotgun retains its per-pellet `campaign_shotgun_random`; AR alternate retains
`campaign_rifle_alt_random`; Machine Pistol, HMG and scoped rifle retain their
existing mode policy and conventional-stream sampler. Riot, precision,
projectile, mounted and NPC paths do not gain a draw. No damage, cadence, burst,
reload, ammo, sound, camera, contact or penetration implementation is changed.

## Random ownership and saves

The new calls reuse `campaign_conventional_random`. There is no new RNG,
initializer, persistent allocation, timer, telemetry word or save ABI field.
This intentionally means newly admitted Pistol/AR-primary/Undercover shots
advance the sequence subsequently seen by existing conventional firearms.
Their sampling algorithm and per-shot draw count are unchanged; their future
sample values can differ after those newly sampled shots. Shotgun and AR
alternate streams remain independent.

`rf_weapon_spread_ray` (`src/core/weapon.c:1079`) retains the existing uniform
solid-angle sampler with a deterministic state, ray-length preservation and a
no-draw zero-angle branch. A successful positive-angle call invokes the existing
cone sampler once, which advances the RNG twice. Validation/sample failure
preserves the helper's output and RNG; its error propagates. Already-consumed
ammo and earlier shot feedback are not rolled back, matching the existing
nontransactional spread/contact path.

The frame-zero conventional seed remains 1 before normal checkpoint import.
RFWM capture and assign already retain this stream
(`scene_weapon_modes_checkpoint_live.inc:56,80`); the existing 32-byte codec
stores it at byte `+24` (`src/core/weapon_modes_checkpoint.c:19,29`). Ordinary
world snapshot writes that RFWM component at `scene_world_snapshot.inc:174..175`
and ordinary world publication restores it via the same assign helper at
`scene_world_load.inc:312`. Settled-action admission, legacy component defaults,
catalog hashes and restore ordering remain untouched.

The original uses its own random/shot-basis path. This change makes no claim of
retail RNG, x87 constant, distribution, basis or sampling-order parity. Full
projectile-flight and dynamic spread modifiers remain separate work.

## Historical evidence and pending verification

Older Pistol/AR-primary rays were unspread. Prior hit/miss, exact-ray, dependent
damage, RNG and resave outcomes must therefore not be reused as validation of
this consumer. In particular:

- The original grate-check note's eye-forward premise is historical; after
  sampling, its retained eye ring alone cannot prove the actual shot ray.
- Earlier rifle primary/alternate-transition contact outcomes predate primary
  spread, even though alternate sampling itself is unchanged.
- The older RFWM Undercover value 1 and associated resave hashes precede any
  Undercover spread draw. Existing bytes still decode; those historical values
  are not fresh sequence-continuation evidence for the new behavior.

The related notes now mark these boundaries. Historical scripts, fixtures,
expected counts and runtime reports are preserved. No expectations were edited
to force a pass. Parent-coordinated Xbox compilation, actual Pistol/AR-primary/
Undercover firing, settled suppressor-mode firing and saved-stream continuation
remain unverified. A neutral startup establishes resource/build admission only,
not these action-specific outcomes; no new fixture or route is bundled.
