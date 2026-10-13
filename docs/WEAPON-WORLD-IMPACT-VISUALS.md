# Conventional dry-world impact visuals

Source-written against `ec1d7390d981b528852b4ac7bd2996fd4c7af495` on
2026-10-12. No helper compilation, executable verification, gameplay, fixture,
image, HDD access or campaign traversal was performed. The parent owns the
next hourly Xbox compile/package rollup and startup-only smoke.

## Actual missing consumer and scope

The existing accepted conventional player/NPC shot paths delivered material
impact audio but did not consume `$Impact Vclips`. The only prior use of the
weapon-owned impact list was the Rocket definition. This slice supplies the
common gun-hit flash, spark and smoke feedback at actual dry world contacts.

The exact installed profiles are read from `weapons.tbl`, using current weapon
catalog IDs and owned metadata. Clip order and radius are kept together:

- 12mm handgun, Undercover 12mm handgun and Assault Rifle: FGatE, radius .3
- Shotgun: FGatE then gatsmoke, both .3
- Machine Pistol: FGatE_lilspark, .3
- Machine Pistol Special: FGatE_lilspark then gatsmoke, both .3
- Sniper Rifle, heavy_machine_gun, Vauss and Tankbot Chaingun:
  FGatE_bigspark then gatsmoke, both .3
- scope_assault_rifle: FGatE_bigspark then gatsmoke, both .25

Some authored one-clip lists have two radii. The reader validates those
surplus values but iterates only the actual clip count. Unknown effect
families or malformed selected profiles fail this optional bank admission;
no replacement texture or invented effect is used.

Player ordinary, penetrating handheld and Sniper world stops use the new
shared feedback helper. NPC conventional/shotgun/actor-penetrating world stops
reach it through the existing NPC impact adapter, including Vauss/Tankbot.
Existing NPC Sniper/rail exclusions remain. No world traversal, penetration,
shot target selection, body contact, ammo, cooldown, damage or audio selection
is changed. No effect is requested on a miss. Actor, clutter, fragment,
physical/Nano shield and vehicle impacts, rail VFX, bullet decals, traces and
wet presentation remain outside this slice. Existing audio on those paths
is retained.

## Original evidence

Read-only installed `RF.exe` SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `4c5820..4c59df` world presentation separately handles decals, audio and
  impact Vclips. `4c54b0` is the decal branch, not the Vclip enumerator.
- `4c8a10..4c8a76` walks the descriptor's impact count and ordered IDs, loads
  each paired radius, and invokes `4c16e0` once per entry.
- `4c1a02..4c1a97` forwards a code-explosion normal/scale to `48e640`.
  Existing reconstructed evidence in
  `research/CODE-EXPLOSION-IMPACT-DIRECTION-20260915.md` establishes the normal
  copy; central creation normalizes it. The new consumer uses the real hit
  normal and preserves the exact collision hit point.
- `48e7c5..48e879` central creation scales velocity, radius and life. Existing
  `rf_explosion_central_prepare` and emitter creation implement those rules.
- `48e5a0..48e63a` builds the shared spark packet with one radius/lifetime.
  `48e9b7..48ea88` emits exactly 10/5/15 sparks for the three actual FGatE
  recipes, into global particle pool0; radius and per-spark speed scale by
  explosion size, while life does not.
- `4c1b8c..4c1cec` compares integer births with particle_count * supplied size.
  Five gatsmoke particles at .3 or .25 therefore emit two births. Its missing
  radius_in_multiples flag preserves authored radius, speed and life. Its
  no_z_check flag is preserved in the ordinary particle renderer.

The installed `explosion.tbl` recipes each contain one immediate central
emitter with process_per_frame=no and a .2-second owner lifetime. The three
HEAP centers differ in authored size. Their spark emitters resolve normally,
so this does not reuse the Rocket recipe's known missing optional emitter.

## Contact and lifecycle ownership

The existing ordered world feedback query is moved intact from the audio-only
helper into `scene_weapon_impact_world`. It uses the same accepted post-spread
ray, target limit, `0x26` query and retained collision `rf_collision_solid_hit`.
One returned hit supplies both existing material audio and the new visual.
The wrapper does not return contact decisions to gameplay. Visual readiness
is independent of optional PCM admission. The original query's retained-
fraction ordering is preserved, rather than replaced by a generic ray.

An outward .001 room-lookup probe avoids ambiguous surface ownership; particle
births retain the exact hit point. Missing room ownership and wet impact
points omit the optional effect. Liquid qualification is independent of audio
and uses retained room/liquid data; existing audio behavior is unchanged.
Launch-wet and water-crossing parity are not reconstructed by this dry-point
first pass.

32 fixed explosion owners retain their admitted emitter slots with a unique
source tag. Original recipe clocks release the central emitter after .2s,
detaching surviving particles into the existing pool. Sparks/smoke use that
same fixed pool directly. Pool/owner exhaustion drops optional births and
cannot fail a shot. No allocation, archive read or registration occurs per
hit. A separate visual RNG does not consume gameplay/spread or shared ambient
particle RNG. Admission requires exact particle flags0x202 for all centers,
0x4006 for normal/small sparks,0x400e for big sparks and0x4c for smoke, with
zero secondary flags. Center/spark emitter flags must be the authored
immediate/continuous/on0x16; smoke requires on0x10. Thus no swirl, wind or
damaging particle can enter the common simulation through this optional bank.
Finite bounded scalar admission and supported .25/.3 weapon radii protect
the simulator from altered invalid cosmetic lifetimes/overflow. Whole-engine
original RNG ordering remains a port policy.

On an ordinary successful world load, after final storage close and successful
publication, the new owner releases only its tagged emitters and particles.
Failed loads leave them alone. These cosmetic effects are not serialized and
create no new save refusal or schema. Teardown releases tagged emitters before
particle-pool close; animations remain owned until after all particle records
close. Texture tags use a distinct `0x60000000` range, resolved through the
existing particle sink and depth/blend logic.

## Bounded resources

The deduplicated bank retains original HEAP.vbm (all three frames),
Flame01_02.tga and smokedark2.tga. Their decoded pixel storage is respectively
196,608, 262,144 and 16,384 bytes: 475,136 bytes total. The current VBM decoder
retains RGBA8; the smaller encoded1555 file size is not used as a RAM estimate.

A 512KiB total admission cap includes the fixed owner/profiles/32 instances,
shared material metadata, temporary combined definition, table scratch and a
16KiB conservative fixed loader/render-stack reserve. Peak scratch for table
reading is not concurrent with decoded texture residency. No measured native layout is claimed here:
`rf_scene_weapon_impact_visual[9]` records actual sizeof-based retained bytes,
and `[10]` records accounted peak bytes, including the fixed reserve. The
loader enforces the cap before publishing. Native startup additionally
requires that full cap plus 256KiB free-page headroom before admission. Missing
assets, malformed tables or insufficient memory omit the entire optional bank
without changing combat admission. There is no texture reduction or duplicate
per-recipe copy.

Other telemetry records accepted contacts, central owners, spark/smoke births,
pool drops, wet/room omissions, errors, active emitters, reset count and the
shared world query/match counts. This is source instrumentation, not evidence
that Xbox rendered the feature. Parent compilation and tester observations
remain outstanding.
