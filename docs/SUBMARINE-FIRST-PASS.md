# Submarine first pass

Current scope is an enemy-free underwater DEV fixture, not campaign vehicle completion.

## Authored inputs

The installed `sub` class uses `Sub_Mini01.v3m`, `sub.vfx`, movement kind7, mass2500, health700, speed6, acceleration8, maximum rotation4 and rotation acceleration2. The actual hull has one radius2 collision sphere and14 attachment tags. Seven table override entries do not imply seven model spheres: missing names are ignored by the existing recovered override path.

The cockpit has19 mesh records. Disabled materialless `Sphere01` has26 vertices/48 faces and no-material face sentinels; it is fully parsed and retained with keys, without a render instance. The other18 meshes keep render instances. Hull, cockpit and torpedo resource test reports1,921,594 resident bytes and1,930,486 peak bytes on PC; these figures exclude allocator overhead and are not a full Xbox memory measurement.

Torpedo uses actual model `torpedo01.v3m`, damage200, speed7, lifetime10seconds, radius0.15, blast/crater radius5, reserve20 and3second cadence. Guidance now scans generation-bearing live hostile NPC handles with underwater aim points and actual collision LOS. The turn-time-as-full-revolution and full-cone interpretations are explicit first-pass policies; live enemy tracking remains unverified in the enemy-free fixture. Liquid boundary expiration is distinct from a solid-impact explosion.

## Implementation policies

Shared submarine core provides neutral buoyancy, world-up rise/yaw, local forward/strafe/pitch and bounded acceleration/rotation. Released input uses drag2/s. Unlike ground vehicles, it has no spring or gravity solver. Solid-sphere self-inertia is added for the single-sphere hull to avoid a singular point-mass tensor; ground vehicle inertia is unchanged.

Water admission checks actual posed hull spheres against wet rooms, their liquid surfaces and swept liquid faces. Ordinary world/mover collision resolves solids. Rotation uses the existing endpoint-center chord approximation. Boarding requires wet full-body transit; exit requires a clear submerged full-body destination outside the host, without requiring a floor. Missing water admission rejects sub boarding rather than allowing dry operation.

## Focused evidence

- Installed hull/cockpit/weapon resource checks pass, as does existing Jeep cockpit parsing.
- Core underwater controls, speed bounds and wet-path rejection pass.
- Actual L5S3 collision world at(-25,-15,0), room15, supports the radius2 hull, six2m axis sweeps, and30 ticks of water-admitted motion.
- Torpedo solid/liquid collision composition passes; a liquid hit expires without being treated as a wall explosion.
- Shared boarding/exit adapter rejects dry boarding and blocked/dry exits; a wet clear exit succeeds without a floor in the focused ownership test.
- Live240-frame PC replay boards, moves forward and rises, fires one torpedo, records a solid impact/detonation and retains19rounds. A300-frame continuation exits to swimming; its rendered external hull/on-foot weapon frame was inspected.
- Native240-frame run `artifacts/xemu/render-20260918-162349` passes with exact vehicle pose/velocity and damage words matching PC. Native cockpit/HUD frame inspected; ammo19 is visible. Free stock64MiB pages:5487 (21.43MiB). This first run did not yet collect the dedicated torpedo counters; the harness now includes them.
- PC and NXDK builds pass. Replay generation is retained in `tools/check_submarine_gameplay.py`; `--run` checks state, `--exit` appends return-to-swimming. Visual inspection is separate.

## Remaining

Native300-frame run `artifacts/xemu/render-20260918-162608` also passes: one boarding, one swimming exit, one torpedo launch/solid impact/detonation,19rounds remaining; all vehicle words and all eight torpedo counters match PC exactly. Native external hull/on-foot weapon frame inspected;21.43MiB free and harness disc restoration confirmed. Submarine live saves, enemy guidance verification/tuning, articulated/propulsion visuals, full audio and campaign placement remain open. The underwater fixture uses actual static world geometry; it does not yet install an editable terrain owner, so the torpedo terrain dispatch is wired but underwater GeoMod is not demonstrated. Live NPC torpedo damage is also unverified in this enemy-free fixture.

The dark cave and cockpit appearance are first-pass visuals. The generic vehicle HUD still uses ground-vehicle control hints; submarine controls are forward/strafe, look to rotate, jump/crouch to rise/dive, use to board/exit, primary fire for torpedoes.


## Save and guidance follow-up

RFVC4/profile4 stores common vehicle pose/motion/occupancy/700HP plus torpedo reserve and remaining cooldown. Shared RFCP3 transport and seated player pairing now admit this typed record; codec, cross-profile rejection and pure capture/restore staging checks pass. Safe restore staging requires fresh ownership plus separate hull and player admission callbacks, and rejects active shots or unsupported effects. This is not live submarine save/load: the live save path still depends on editable terrain and does not yet support the static underwater fixture. Its explicit frontend rejection remains in place.

Guidance performs allocation-free candidate scanning, source/driver exclusion, wet-target filtering, collision LOS and bounded speed-preserving steering before the liquid flight step. Focused candidate/LOS/wakeup/turn/error checks pass; the existing300-frame enemy-free PC drive/fire/exit replay also passes after integration. It does not demonstrate pursuit of a live enemy. Current parser ownership adds132 PC bytes to the earlier submarine resource aggregate (now1,921,726 resident/1,930,618 peak).
