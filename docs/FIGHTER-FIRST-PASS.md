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

## Live saves

RFVC5 is a160-byte fighter record carried by the existing RFCP3 save envelope. It retains pose, linear/angular motion,900HP health state, occupancy,900/20 finite ammunition, both cooldowns and lifetime shot counters. Reserved bytes, checksum, finite values and authored bounds are validated before publication. Active projectiles, held triggers and unsettled warmup defer capture.

The live restore stages the actual airborne rigid body, checks the full hull and occupied player seat against restored geometry, then publishes health/motion/ammo and restores possession through the normal vehicle owner. It does not require ground support and never routes fighter fields through APC/Jeep interpretation. The existing160-byte vehicle transport capacity is unchanged.

`tools/check_fighter_checkpoint.py --run` passes three PC processes:360-frame seated capture after flight/fire,60-frame seated continuation, then60-frame restored exit. Health900, ammo891/19 and shots9/1 survive; residual velocity continues decaying. Resumed cockpit and on-foot exit images were inspected. Codec corruption/atomicity and composed parked/seated transport tests pass. Stock64MiB XEMU `artifacts/xemu/render-20260918-170229` restored the saved seated fighter, exited once and saved again: all vehicle/weapon/player checkpoint counters match PC and the1228-byte RFCP files have identical SHA256 c419ad3c68901f351667329c4038adf1c6c57aeccba58287c01409e80e1ad640. Endpoint headroom is11.56640625MiB. Native final framebuffer was inspected and shows the healthy player on foot beneath the hovering fighter.

## Authored campaign host and route

L13S3 contains one `Fighter01` entity, UID8955 at (24.44,-26.29,20.15). The ordinary campaign vehicle loader now registers that source UID and pose. The existing `Follow_Waypoints` event UID8644 links to the fighter and names the one-node `fighterpath`; node41/UID8642 is at (-82.29,-49.27,110.44). Trigger UID9820 links to the event. It is a spatial trigger volume, despite its authored name `Trigger Auto`, so the fighter remains parked until that volume activates.

The vehicle event adapter already accepted the waypoint order, but free-flight ignored its rigid ground-vehicle command. The new fighter route adapter steers yaw, forward throttle and world-up rise toward authored 3D nodes through the existing dry-hull solver. Seat occupancy gives controls to the player; the unoccupied route may resume afterward. This is direct first-pass steering with collision admission, not obstacle-aware pathfinding or reconstructed retail pilot AI.

An 80-frame stock-64-MiB Xbox check in `artifacts/xemu/fighter-campaign-l13s3-20260927-173205` staged the player beside UID8955, boarded once and fired four minigun rounds with a held press, ending with896 primary rounds and20 rockets. It had3,785 free pages (14.79MiB); disc flags were restored. A separate 180-frame check in `artifacts/xemu/fighter-campaign-l13s3-20260927-172844` staged the player inside trigger UID9820 without directly firing the event. Normal trigger dispatch started UID8644, produced179 steering ticks, moved the unoccupied fighter38.88m and reduced distance to the authored node from141.68m to112.93m, with3,799 free pages (14.84MiB). This verifies one authored host and trigger-to-flight path without a campaign route or PC gameplay run. Natural player travel, other fighter instances, combat while scripted, route completion and ordinary campaign save/load remain open.

L18S2 now registers its sole `Fighter01`, UID10066. Its `big_charge_explode` resource needed more than the former 128 KiB per-effect texture allowance; the 512 KiB aggregate scripted-effect cap remains in force. The bounded stock-64-MiB Xbox load check `artifacts/xemu/fighter-campaign-l18s2-20260929-153954` completed120 frames with that fighter registered,3,065 free pages (11.97MiB), a living player and restored disc flags. No PC gameplay was run.

The shared scripted `Attack` path can now resolve either the boardable host or a registered passive vehicle target, aim toward its live pose, route firearm hits through vehicle damage, and preserve the target UID in ordinary NPC-order saves. A focused stock-64-MiB L20S2 Xbox fixture directly issued an Attack order from existing armed NPC UID4726 to passive Fighter UID4801 after a player sniper hit and blast. The order remained active, sampled live hull health, and recorded19 pursuit ticks in the first20 frames; the distant NPC did not fire in that interval. This verifies target binding and live pursuit, not a successful NPC projectile hit or active-order save/reload. L18S2's authored `Attack` UID10635 links to the fighter but names attacker UID10614, which is absent from the level's25 entity records. Its linked trigger UID10634 is authored disabled; the missing L18S2 entity is not silently substituted.

## First-pass limits and remaining work

Hover/drag, neutral unoccupied flight and endpoint-chord rotational collision are practical first-pass policies; retail aerodynamics are not reconstructed. The aircraft uses actual hull spheres, solid collision and dry-room/liquid-boundary admission rather than an ordinary player proxy.

Complete broader live damage/destruction/ejection coverage, verify NPC fire against a living fighter target, other campaign placements and the boss variant, broader campaign saves, firing/engine audio, thruster/corona effects and cockpit marker-driven effects. Full visual polish and broader collision/flight refinement remain deferred.

Deferred restore refinement: add dry-liquid full-hull admission before publication for externally modified, checksum-valid submerged fighter poses. Normal captures originate from dry-admitted flight; current restore validates solid hull/seat clearance but does not independently repeat the liquid restriction.
