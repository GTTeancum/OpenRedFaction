# Meca Laser: finite NPC primary first pass

Status: source-integrated after the failed 04:00 batch, independently source-reviewed, not compiled or runtime-validated. The next scheduled 05:00 Xbox batch owns compilation and original startup/resource admission; attack and visual runtime remain unverified.

## Missing playable consumer

The existing ordinary and opposed NPC weapon selectors do not admit the exact selected `Laser`. The generic primary reader rejects this clipless, noncontinuous bullet family, while the explosive reader is unsuitable. `campaign_enemy_tick` therefore skips the selected owner before ordinary awareness, pursuit and firing. This is a missing authored combat consumer, not a new weapon grant.

Read-only decoding of installed original level entries found eleven selected-Laser `Meca Turret` owners:

- `levels3.vpp/L17S1.rfl`: 19950, 19952.
- `levels3.vpp/L17S2.rfl`: 18586, 18607.
- `levels3.vpp/L17S3.rfl`: 14435, 14436, 14437, 14438, 14446, 14447, 20885.

These rows have health 30, armor 0, friendliness 0, not initially hidden, raw authored AI mode byte 1 and attack-style byte 0. The raw mode byte is not the runtime AI enum. Meca's class uses `mft2.vcm`, robot/fly movement and use-none. It is not in the separate stationary-turret class list. Two L9S4 Meca owners, 964 and 965, explicitly select Vauss and are outside this exact-Laser adapter.

Existing startup and placed-NPC loadout code already acquire the actual selected Laser and its mapped power-cell reserve. No synthetic inventory, weapon substitution, level edit or grant is required. Selection intentionally qualifies the actual owned selected weapon rather than hardcoding the Meca class, so an authored or scripted owner of the same real weapon follows the same contract.

## Original weapon contract

The installed `weapons.tbl` Laser entry and existing supply/reset catalog decode establish:

- `laser01.vfx`; `power cell`; maximum reserve 200/200; no magazine or reload.
- Bullet damage kind 1; damage 80/80; fire wait 0.4 seconds; AI range 5/5.
- Collision radius 0.2; velocity 20; lifetime 20 seconds.
- Spread Degrees 2.5, with no authored AI-spread override.
- No homing, sticking or glow; damage radius zero; no crater.
- Reset flags 0 and 0x28 (`undeviating` and `no_fire_through`).
- Launch group `Laser fire`; flight loop `Laser loop.wav`, radius 8 and volume 0.9.
- Dry impact groups Laser Hit Default/Flesh/Metal/Rock/Water and named impact Vclips are separate presentation metadata.

`scene_ai_laser_profile.inc` requires this exact bounded family and independently reads its missing primary scalars. It does not relax the existing conventional/explosive parsers, catalogs or save layouts. It rejects unsupported delay, burst, projectile-count, piercing, magazine, explicit AI-spread/damage and Weapon Type additions instead of silently claiming those semantics.

The current NPC default AI spread remains zero when there is no authored AI override, matching the surrounding port policy. This is not justified by `undeviating`: original `RF.exe` 0x42d040 calls 0x4c8770, whose NPC-primary branch reads descriptor +0xf8 and does not inspect the undeviating flag. Original aim-spread default parity remains a separate uncertainty.

## Accepted launch and finite ownership

The dedicated launch path requires a live generation-qualified source, exact selected owned Laser, zero loaded magazine and positive actual mapped reserve. It prepares a flight using `rf_weapon_flight_launch`, snapshots the full source handle and fresh AI-scaled base damage, and consumes exactly one real reserve cell through `rf_weapon_consume_shot` before publishing the flight. There are no external callbacks between acceptance/debit/publication. Parent must skip generic reserve debit and generic hitscan resolution for an accepted Laser.

The pool holds 64 flights and never overwrites a live slot. Pool refusal precedes spread-RNG publication, ammo debit, accepted-shot cadence and presentation. The existing generic short retry may still run after refusal. At the authored 0.4-second cadence and 20-second lifetime, one unobstructed continuous owner can retain about 50 simultaneous shots, so multiple clear-space owners can reach the explicit pool bound. Refusal is preferable to inventing ammo loss or replacing live projectiles.

The first-pass muzzle is the owner's current eye position aimed at the already chosen target eye/point. Exact authored `mft2.vcm` muzzle attachment and weapon firing-pose reconstruction are not implemented. Flight speed, lifetime, straight motion, finite ammo and direct damage remain independent of that presentation approximation.

## Contact, identity and lifecycle

`scene_ai_laser.inc` reuses core finite-flight/liquid stepping, not the explosive terminal adapter. Each step composes existing static-world/liquid, mover, detached-piece, actor, vehicle/turret and clutter queries in nearest-contact order. World, body-sphere and retained environmental sweeps use radius 0.2. No radial damage, explosion, terrain crater, actor penetration, ricochet or homing is introduced.

Physical shield candidates are queried across the complete step before global nearest-cover/body arbitration, including protruding shields outside body spheres. NPC shield poses are demand-prepared before querying. Player shield pose must be advanced by the parent before Laser tick. Selected candidates retain full handles and original query endpoints; only the winning candidate is committed. Breaking or stale selected physical shields consume the contact without leaking damage to a body. This uses the existing directional centerline shield policy, not radius-0.2 swept-shield parity. Coarse actor body spheres likewise remain an explicit port limitation.

NPC body contacts run `scene_nano_weapon_contact` with the exact accepted Laser ID before ordinary kind-1 damage; consumed includes the Nano-shield-breaking contact. Direct NPC/player damage propagates feedback status. Full source identity is captured at launch and full target identity before callbacks; removed sources do not become newly reused actors. The separate query-kind field disambiguates the existing shared turret/clutter tag alias.

Each update works on a local shot copy, publishes terminal retirement before callbacks and checks a reset epoch before continuing. `scene_ai_laser_pending()` includes an active update even if its last flight has just become terminal. Integrated guards cover only save admission/capture paths, following the existing Drone transient pattern; shared row capture used by load must not reject pending Laser flights. Successful publication of a loaded state, life/world replacement and teardown call reset. Failed load staging must leave the live state intact. No active Laser flight may be mislabeled as an RFAP Rocket/Tankbot row, and no new save format is required for this transient first pass.

## Original projectile visual evidence

The installed `meshes.vpp/laser01.vfx` is 1600 bytes, version 0x40006. It contains two four-vertex/two-face SFXO planes (`Plane02`, `Plane01`) and two material records; both planes name parent `Dummy23`. There is no in-file DMMY object, particle system or warp object. Each plane's decoded animation has rate 15, start 0, end approximately 0.13333334 seconds and three samples at frames 0, 1 and 2. The original definition's +0x84 duration (`values[1]` in the retained decode) is 2 effect frames.

Static original executable evidence for looping:

- Kind-3 model setup at 0x489fe0, including 0x48a0c4–0x48a0d2, supplies loop argument 1 to 0x503390.
- 0x503390 forwards to 0x501af0; 0x501b22–0x501b34 clears pause, sets instance loop flag 2 and zeroes age/frame.
- 0x54cce0 converts seconds to effect frames using 15 at 0x589854 and compares against definition +0x84.
- 0x54cd37–0x54cd78 wraps by subtracting duration multiplied by the original 1/15 float at 0x58a268 until age is below one cycle, then recomputes the effect frame.

Thus the parent draw's `fmodf((20 - remaining) * 15, 2)` has the authored two-frame looping period. It is a practical flight-age clock, not bit-exact emulation of the original repeated float subtraction, clock storage or pause scheduler. The current flight basis and decoded mesh-local transforms are retained.

`Dummy23` has no definition in this VFX file. The existing Fusion/ShellTest loader also admits that parent token; extending its identity-parent policy to exact `laser01.vfx` is a bounded approximation, not evidence that the original external parent transform is identity. External parent transform parity remains unverified.

The two retained materials name static original TGA entries:

- `Shell01.tga` in `maps3.vpp`, 16428 bytes, 64×64, 32 bits per pixel.
- `Light_LilRedFlare01.tga` in `maps4.vpp`, 3116 bytes, 32×32, 24 bits per pixel.

This source pass inspected archive entries and headers, not runtime image-loader output. Parent's existing strict one-frame material admission and image/mesh byte caps remain authoritative, and transferred-image ownership follows the existing Fusion path. The integrated visual is demanded only by actual living owned Laser NPC loadouts. As with the existing Fusion pipeline, demanded asset admission is strict: a missing, malformed or over-budget resource stops startup. It does not substitute explosive resources or invent success.

The integrated source includes `Laser fire` in existing optional NPC launch audio. Flight-loop/impact audio, impact Vclips, precise muzzle, exact external-parent transform and visual/runtime parity remain deferred. There is no runtime evidence for launch, contact, ammo exhaustion, shield ordering, save refusal, texture admission or drawing yet.

## Integration and validation status

Shared scene wiring, exact finite-reserve readiness, transient save-only guards,
successful-load retirement, lifecycle resets and player-shield/tick ordering are
integrated with the dedicated includes. Original laser01.vfx uses the existing
bounded material ownership and mesh emission path, with its authored two-frame
loop and the explicitly limited external-parent policy above.

Independent source review accepted the complete integration after correcting
current NPC shield-pose preparation and player damage callback-status propagation.
The unused legacy melee include remains removed from production after the 04:00
compile correction, while its separate test helper/API is intact.

No new compilation, runtime launch, fixture, original asset mutation or campaign
traversal was performed. The existing neutral startup admission helper now accepts
original hash-pinned levels3.vpp/L17S1.rfl for the next hourly batch, preserving
its unchanged authored spawn and 120 zero-input frames. This can establish startup
and demanded-resource admission only; it does not establish an attack, audio output,
precise muzzle/parent placement, shield ordering, save restoration or FPS result.
