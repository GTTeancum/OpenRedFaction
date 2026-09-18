# Fighter first playable integration

Fighter01 now has an enemy-free CTF06 DEV scene with boarding, hover flight, cockpit/HUD, finite minigun fire, rocket flight and exit on PC/Xbox. The initial attachment-axis bug is corrected: minigun and rocket aim use muzzle_1 forward while rockets retain secondary_1 launch position.

## Installed data

Fighter01 uses Fighter01.v3m and fighter01.vfx, use-kind1/radius5, movement9, mass1500, health900, speed20, acceleration10, maximum rotation4 and rotation acceleration4. Its model retains18 tags and two actual collision spheres. Tags include primary_1, secondary_1/2, interface_1 and muzzle_1; launch code must use the named authored tags.

The cockpit has21 visible mesh records and two DMMY animation markers, chaingun_1 and muzzle_1. The shared VFX loader now retains each marker's name, parent, flag, base pose and21 samples with strict byte/finite-value validation, memory accounting and cleanup. It does not evaluate an unsupported parent hierarchy; the admitted fighter meshes and markers are Scene Root children. Marker use for animated cockpit gun effects remains open.

Resource checks report1,894,308 resident/peak bytes on PC (hull863,700; cockpit1,030,552; aggregate metadata included). This excludes allocator overhead and is not full-scene Xbox memory acceptance.

## Working components

- Shared authored vehicle-body initialization admits Fighter01 with two real hull spheres and no suspension springs. Like the submarine, it includes solid-sphere self-inertia; ground vehicle tensors are unchanged.
- scene_fighter_motion reuses the bounded free-flight equations through the existing submersible backend interface. Its admission callback requires dry actual rooms, full sphere clearance above liquid surfaces and no liquid-boundary crossing. The legacy result.wet field means admitted flight space in this wrapper. Neutral hover, drag and rotation chords are explicit first-pass policies, not recovered retail aerodynamics.
- Actual ctf06 world/hull checks find a clear hover pose at(30,7,-160), verify six one-metre sweeps and20 motion ticks; the known L5S3 underwater fixture is rejected.
- Fighter Minigun reads900 reserve capacity,100 AP damage,0.05second cadence,275 speed and0.1startup. Its undeviating flag must suppress random spread despite the table's spread value.
- Fighter Rocket reads20 reserve capacity,200 damage,3second cadence,25 speed,5second lifetime,15 blast radius,8 crater radius and DrillMissile01.VFX. Four finite flights retain source/driver and use the ordinary rocket liquid policy. The authored rocket VFX is wired into the live scene; animation and appearance fidelity remain first-pass.
- Installed-data resource/weapon tests and actual-world movement tests pass on PC; the live integration evidence below is separate from those component checks.

## Live evidence

- `artifacts/fighter-muzzle`: corrected360-frame PC replay boards once, flies forward and rises, fires nine minigun rounds and one rocket, records nine minigun contacts and one rocket impact, then exits once. Reserves end891/19. The inspected final image shows the player safely on foot beneath the hovering fighter with full health.
- `artifacts/fighter-muzzle/missile.png`: inspected frame166 shows the actual missile effect forward/left of the cockpit; telemetry reports one live rocket, seven meshes and198 emitted vertices. The purple angular effect is visibly rough; material/effect fidelity remains deferred.
- Actual tag regression verifies both secondary tags point down and muzzle_1 forward follows the chassis, including rotated placement. Live resource selection now uses that same muzzle_1 tag. primary_1 is also a downward attachment and is not used as a firing direction.
- Earlier `fighter-gameplay`, `fighter-exit`, and `fighter-forward` runs used downward attachment axes and are superseded for aimed weapon acceptance. Their matching contact totals alone did not establish correct aim.
- Corrected stock64MiB XEMU run `artifacts/xemu/render-20260918-165531` passed360frames with all16vehicle words and all8fighter weapon words exactly matching PC,11.63671875MiB free and restored disc state. Its final native framebuffer was inspected: player on foot, full health, hovering fighter overhead. Missile-flight appearance was inspected on PC; the native final frame establishes exit presentation only. No accepted terrain cuts are demonstrated by this route; blast/terrain dispatch exists, but the impact does not establish GeoMod success.

## Save preparation

Pure fighter capture/restore staging now preserves airborne pose/momentum, health, occupancy, finite ammo and both weapon cooldowns. Focused tests pass for capture, admission and atomic failure. Active bullets/rockets and unsettled firing prevent capture. This is an in-memory record only: a disk profile, checkpoint transport and live publication remain open; frontends continue rejecting fighter save requests.

## First-pass limits and remaining work

Hover/drag, neutral unoccupied flight and endpoint-chord rotational collision are practical first-pass policies; retail aerodynamics are not reconstructed. The aircraft uses actual hull spheres, solid collision and dry-room/liquid-boundary admission rather than an ordinary player proxy.

Complete broader live damage/destruction/ejection coverage, campaign placement and the boss variant, durable saves, firing/engine audio, thruster/corona effects and cockpit marker-driven effects. Native and harness selectors intentionally reject fighter checkpoints. Full visual polish and broader collision/flight refinement remain deferred.
