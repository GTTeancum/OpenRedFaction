# Fighter implementation foundation

Fighter01 is the remaining major playable vehicle class. The current milestone provides working resources, motion and weapon components; it does not yet provide a playable fighter scene.

## Installed data

Fighter01 uses Fighter01.v3m and fighter01.vfx, use-kind1/radius5, movement9, mass1500, health900, speed20, acceleration10, maximum rotation4 and rotation acceleration4. Its model retains18 tags and two actual collision spheres. Tags include primary_1, secondary_1/2, interface_1 and muzzle_1; launch code must use the named authored tags.

The cockpit has21 visible mesh records and two DMMY animation markers, chaingun_1 and muzzle_1. The shared VFX loader now retains each marker's name, parent, flag, base pose and21 samples with strict byte/finite-value validation, memory accounting and cleanup. It does not evaluate an unsupported parent hierarchy; the admitted fighter meshes and markers are Scene Root children. Marker use for animated cockpit gun effects remains open.

Resource checks report1,894,308 resident/peak bytes on PC (hull863,700; cockpit1,030,552; aggregate metadata included). This excludes allocator overhead and is not full-scene Xbox memory acceptance.

## Working components

- Shared authored vehicle-body initialization admits Fighter01 with two real hull spheres and no suspension springs. Like the submarine, it includes solid-sphere self-inertia; ground vehicle tensors are unchanged.
- scene_fighter_motion reuses the bounded free-flight equations through the existing submersible backend interface. Its admission callback requires dry actual rooms, full sphere clearance above liquid surfaces and no liquid-boundary crossing. The legacy result.wet field means admitted flight space in this wrapper. Neutral hover, drag and rotation chords are explicit first-pass policies, not recovered retail aerodynamics.
- Actual ctf06 world/hull checks find a clear hover pose at(30,7,-160), verify six one-metre sweeps and20 motion ticks; the known L5S3 underwater fixture is rejected.
- Fighter Minigun reads900 reserve capacity,100 AP damage,0.05second cadence,275 speed and0.1startup. Its undeviating flag must suppress random spread despite the table's spread value.
- Fighter Rocket reads20 reserve capacity,200 damage,3second cadence,25 speed,5second lifetime,15 blast radius,8 crater radius and DrillMissile01.VFX. Four finite flights retain source/driver and use the ordinary rocket liquid policy. Actual model rendering remains to be integrated; no static replacement was fabricated.
- Installed-data resource/weapon tests and actual-world movement tests pass on PC. These are component checks, not live fighter flight/fire acceptance.

## Next integration

Wire class5 selection, DEV possession/exit policy, shared flight controls, cockpit/HUD, minigun dispatch and actual rocket VFX/impact/GeoMod into the scene. Then run one bounded PC/Xbox board/fly/fire/exit sequence and inspect its output. Campaign placement, boss variant, damage/ejection, saving, sound and propulsion visuals remain subsequent work.
