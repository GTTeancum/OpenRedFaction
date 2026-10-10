# Big Rock Snake Spit: selected-owned delayed AP and radial flight

Status: integrated after independent source review against `aee1950a`, following the completed 07:00 batch; awaiting 08:00 compilation. No compile, syntax check, test, fixture, PC/Xbox execution, image, grant, forced event or campaign route was performed. Parent owns integration and the scheduled 08:00 Xbox batch. This is not runtime evidence or a gameplay completion claim.

## Actual original owner and exact profile

Immutable `levels2.vpp/L10S4.rfl` contains Big Snake UID7007, named `Big Snake`, explicitly selecting `Big Rock Snake Spit` / `none`. Position is (430.794708,-101.963516,217.384659), health5000, armor0, friendliness0, creation flags0 and AI bytes1/0. Record offset2062790, size215, SHA256 `319d53c3c280ff90c50bdd32d2a77cf6b6f71af05dd016fed45c588768d8de93`; When_Dead7137 references this owner. The whole level entry is archive offset7331840, size2742205, SHA256 `91c25e73b62cc11ca610428954af8a10fb0d7ee63d2987865c74636cba07bf87`. This proves placement and selected ownership, not neutral-startup target acquisition or campaign reachability. No activation event is fabricated.

`tables.vpp/weapons.tbl` at archive offset1013760, size108890, lines2114–2142 independently defines:

- `SpitAttack.vfx`, clipless `12mm`, maximum reserve200/200.
- Direct damage kind `armor piercing bullet` (2), damage80, AI attack range50/50.
- Speed30, collision radius0.5, lifetime10, mass5.
- Fire wait2.5 seconds, primary Impact Delay0.8 seconds.
- Damage radius3.0. The separate acid-splash Vclip radius3.0 is presentation size.
- Flags0; flags2 `no_fire_through` (0x08). No homing, sticking, glow, piercing, explicit spread or burst.
- Launch `BSnake Spit`, empty Fly Sound, default `Spit Hit`, flesh `Impact Flesh`, `AcidBlob.tga` scorch size3.

Small Spit's separately read lines2052–2080 happen to have the same flight/AP/supply/VFX values, but fire wait3 seconds, delay1 second, damage radius0 and Launch `RSnake Spit`. The paired bounded reader verifies each complete supported profile against its own contract and exact supply/reset catalog entry. The class/weapon pairing cannot cross-admit Small on Big or Big on Small. Both readers reject unsupported magazine, alternate-delay, spread, burst, piercing and weapon-type additions. No conventional/explosive reader is widened.

## Big class, real muzzle, aim and onset

`entity.tbl`4678–4722 names `Big Snake`, model `Big_Snake.vcm`, turret movement, speed0, mass5000, life5000, flags sentient/collide_corpse and flags2 collide_player/drools slime. Its AP damage factor1.5 is a received-damage rule, not a multiplier for outgoing Spit. Allowed weapons are its own Spit/Smash, default primary is Big Spit, and `fire_stand` selects `bsnk_attack_spit.mvf` with `RSnake Spit` Foley. Only the separate Big Smash overlay selects the bite motion. Weapon Launch `BSnake Spit` and action Foley `RSnake Spit` are distinct authored names; foley.tbl1484–1485,1493–1494 and1499–1500 resolve them to the same `RSnake_Spit.wav`, near10, gain0.9. Existing selected action2/Launch services retain these mappings.

`meshes.vpp/Big_Snake.v3c`, archive offset2301952, size23380, SHA256 `073d237c80a362060bc802ad9c8368c0e02e78561cf70de7e752bb96adcd85cc`, has16 bones and one LOD. Its three100-byte attachments start at file offset21474: eye, mouth, `Primary_1`. All use parent bone0 (`rsnk-bdbn-head`) and local position(-0.0121288495,0.0510853641,2.8516931534), quaternion(0.499999881,-0.500000179,-0.499999791,-0.499999791). These are Big's own resource values, not scaled Small offsets. Existing case-folded exact-length tag lookup resolves `primary_1` to the authored capitalized tag. Missing tag refuses before debit; no synthetic head/eye fallback is introduced.

Original40953a tests class+728 bit0x20 and passes the creature result into factory4c77a0. Both actual Snake classes carry that flag. Original41b350 selects the class+1bc primary attachment;41b3e4 places it on the current skeletal/body pose;41b3f2–41b42c forms a basis toward the current target body.41b4c0 retains that basis during pain aim-lock, otherwise refines toward the target eye when its normalized direction dots source eye-forward above0.8. Missing target retains the source eye basis. Shared Spit code already implements this branch using each owner's real posed tag and same-frame qualified raw player eye. No ordinary muzzle, camera, target selection or global aiming policy changes.

Original426197–426273 owns action2, cadence actor+4b8, delayed timer+4c0, Launch and4257c0 debit;409340 consumes the due timer before40956b calls the factory. The shared16-slot paired pending/flight pool now uses the selected profile's2.5-second cadence and800-ms delay for Big, preserving Small's3-second/1000-ms values. One accepted real reserve round is prepaid, presentation happens once, hidden owners defer the same overdue deadline, and release never rechecks range/LOS, charges again or refunds. Exact source/weapon loss, disarm, death and successful timeline replacement cancel pending work only. Released flights retain accepted source handle, weapon, damage, direct kind, blast radius/kind and trajectory even if their source later switches or dies.

## Direct versus radial evidence and shared terminal consumer

RF.exe SHA256 is `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- Object contact4c611c loads descriptor+52c and4c6132 delivers direct damage: Big is AP kind2.
- After direct damage,4c62b5–4c62f5 independently obtains damage through4c8b10, loads radius descriptor+208 and source projectile+30, and invokes488dc0. Its kind is3 plus `(weapon == global87243c)`;4c6654–4c665e binds that global to `Flamethrower`. Big therefore uses ordinary explosive radial kind3, not AP2 or acid damage. World contact4c5378–4c53ad uses the same rule.
- Both branches pass collision contact point projectile+1b4 directly:4c62ee and4c53a7. The adapter uses retained `contact.hit.point`, not projectile-center time-of-impact or the rocket prototype's normal*0.01 bias. Original field mapping is also retained in `include/rf/collision.h` and `src/core/collision.c` gather/scatter.
- Direct victim is not excluded from488dc0's independent populations. Surviving direct victims can also receive radial falloff.
- Physical shield path4c4f3f→4e36a0 returns at4c5023 before radial. Nano4c5a90–4c5b5d also consumes the whole contact, including the breaking hit. Unresolved stored object4c5a35–4c5a51 returns before direct/radial; this boundary alone does not establish projectile retirement. Consuming that finished port sweep remains shared port policy.
- Ordinary lifetime handling4c6ba9–4c6bcd skips detonation for flags0 and reaches4c6d55 retirement; Big is not a timed grenade. Expiry and ordinary liquid passage do not blast.

The shared energy step retains its exact nearest world/mover/piece/vehicle/clutter/body/shield sweep, liquid policy, terminal publication, epoch and updating guard. Its original API remains a direct-only wrapper for Laser/TriBeam/Sonar. A bounded terminal callback lets Spit call the direct dispatcher exactly once, observe shield/Nano/stale-actor consumption, check its pool epoch, then call the existing radial service only when the immutable blast radius is positive. Small's radius0 still produces no radial call. No Nano prepass, second debit, hitscan, penetration, crater or persistent acid effect is added.

The radial service gains an optional pool-epoch guard; existing callers retain the NULL wrapper. Big checks after mutating population stages, rebinds actor full handles after damage and before death handling, and stops after successful timeline replacement. Current clutter damage defers break processing, active vehicle destruction defers eject, and combat event publication is bounded telemetry; their inner implementations are unchanged. The shared service's ordinary488dc0/489010 policy remains one CF5 cover ray to victim physics position, positive linear `damage*(1-distance/radius)` falloff and existing damage owners.

## Exact shared flight presentation and bounded policy

Both independent weapons name the identical original `SpitAttack.vfx`: meshes.vpp offset2381824, size2424, SHA256 `86b83983d92e026134c9b8486eb0d9f48634c14d29728eac40d966170d28a53d`. Factory4c77a0–4c7ca3 supplies descriptor-driven resource/radius, with no observed Big-specific visual scale branch. Big reuses the already-integrated two-PART optional consumer and native AcidBlob/AcidBlob02 textures at authored size. No replacement mesh, enlarged spray, recolor or second resource bank is introduced. The visual's copied-owner comparison now includes blast radius/kind; its ticket, sorted queue, optional failures and reset remain unchanged. See `SMALL-ROCK-SNAKE-SPIT-PART.md` for exact PART evidence and scheduler limits.

This extends existing Spit admission/hooks rather than creating another attack pool, player-eye cache, update pass, save schema or visual allocation. The shared flight payload gains eight bytes per retained copy; existing sizeof-derived resource budgets account for copied visual owners. Both Snake profiles share the same16 pending/flight slots. All Small save-only guards and successful world/life/load reset hooks consequently cover Big too. Failed load preparation still preserves pending/flight state.

Deferred policies are explicit: finite SP NPC reserve exhaustion, zero AI spread/default scale1, actor-directed onset only, existing current-target interpretation, current physical-shield centerline policy, shared collider/cover ordering and population admission, and exact original global scheduling/RNG/x87 parity. Nano-specific radial attenuation/debit at489073–48910b is now separately source-written in [Shared radial Nano shield damage](RADIAL-NANO-SHIELD.md), integrated after the completed09:00 batch and awaiting10:00 compilation, with positive-only falloff and nonnegative armor retained; direct Nano interception still suppresses the whole radial pulse. The existing radial service's vehicle/source policies are unchanged, not newly claimed exact. Separate acid-splash impact Vclip, scorch, mouth/windup visual and comprehensive original object/projectile explosion populations remain outside this slice. The actual attack, mixed direct/radial damage, visual playback, neutral acquisition and stock64MiB residency remain runtime-unverified.

## Integration handoff

Apply the staged patch against `aee1950a` after independent source review. It changes the existing seven source files and adds this document; no new scene startup or lifecycle wiring is required. Parent should replace the old Small-only exclusion wording and top milestone when integrating. Existing counters cover both selected profiles; accepted source/weapon identities distinguish them. Parent owns TO-DO.MD, integration commit, serial hourly compilation and any separately authorized focused Xbox observation.

## Later source integration

Retained-contact impact audio is separately source-integrated in docs/SNAKE-SPIT-IMPACT-AUDIO.md, including optional bounded residency and explicit liquid/default-owner limitations. Compilation and playback remain unverified until the scheduled batch; startup alone cannot validate playback.
