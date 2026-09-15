# First-pass enemy awareness and combat

The shared scene now checks unalerted, visible, alive armed actors every30
simulation frames, staggered by actor index. Authored affiliation0 can acquire
the player within20 world units and a120-degree forward cone when the retained
static/moving geometry ray is clear. Acquisition waits30 frames before firing;
alerted actors then attack every60 frames within the supported weapon
range (pistol20, rifle30, Riot Stick2.6), with a fresh obstruction check.
Unsupported weapons retain the provisional40-unit attack limit. Damaging an armed actor also provokes retaliation regardless of affiliation.

These distances, cone and timing are practical port policy, not reconstructed
original AI. The affiliation values are corroborated by Dash Faction's
[ObjFriendliness declaration](https://github.com/rafalh/dashfaction/blob/master/game_patch/rf/object.h):
0 unfriendly,1 neutral,2 friendly,3 outcast. No community implementation code
was copied. Original loader field evidence is in entity-loader-fields-verification;
Set_Friendliness original action evidence is in door-event-actions-verification.
Runtime Set_Friendliness now updates this awareness owner as described below.

The existing two words per NPC retain alert/timing; global awareness counters
add32 bytes. There is no per-frame allocation for perception. No patrol, pursuit,
squad behavior, hearing, disguise, shooting animation or weapon-specific AI is
implemented here. Alerts persist until death/respawn; original alert decay is open.

`python tools/replay_enemy_awareness.py` checks240 frames without player fire:
staging near hostile8456 acquires two guards and receives8 hits (health61.6);
staging near neutral miner8431 acquires none and retains100 health. Nonhostile
and range rejections are observed. Facing/obstruction rejection branches are
not separately demonstrated by these routes. Neutral guard8326 has another
hostile nearby; its encounter must not be treated as an isolated neutrality test.

`tools/replay_enemy_combat.py` covers provocation, killing one of two attackers,
and fatal damage. `tools/replay_player_life.py` holds Use before death and checks
fresh-press recovery, restored vitals/ammo and real movement after respawn.

Stock64MiB XEMU240-frame PASS: awareness and combat state match PC, with
2 acquired guards,8 hits,health61.6 and matching final HUD samples.7115
available pages at completion (not peak memory):
artifacts/xemu/replay-20260914-010332/report.json. Both builds and26 CTests pass.

## Scripted allegiance

Set_Friendliness (type30, original action4bc280, off no-op4b9f80) now
dispatches resolved registered links through the scene service, including
delayed event ticks. The value comes from authored words[0]. NPC and player
damage affiliations are updated; the practical NPC combat alert and due time
are cleared so the new affiliation takes effect on subsequent awareness ticks.
Non-entity objects remain unsupported by the scene service. This does not
claim the original object+560 targeting side effects or all original AI state.

The npc_motion_residency test exercises the actual scene callback through
registered runtime events: absent backend, stale generation, duplicate links,
immediate friendly change clearing an active alert, and delayed hostile change.
Both PC/NXDK builds and26 CTests pass. Eight bytes of borrowed callback/context
are added to the32-bit trigger owner; no event-time allocation. End-to-end
authored mission sequences in XEMU remain to be verified.

## Authored attack range (2026-09-14)

The primary-weapon reader now accepts the optional `$AI attack range:` pair.
It retains the first value for this single-player implementation and validates
both finite positive values. Missing range stays zero so callers can retain
their explicit fallback. Duplicate, truncated and invalid pairs preserve output.
Installed pistol/rifle/Riot Stick pairs are respectively20/20,30/30 and2.6/2.6;
all supported pairs agree. The table comment describes this as the range the
AI attempts to stay within while attacking. No claim is made that the original
uses the same hard firing cutoff or pursuit hysteresis as this first pass.

Supported firearms pursue outside their range and stop at80 percent of range.
Riot Stick keeps its2.2-unit stopping threshold within the authored2.6 reach.
The same definition supplies damage kind. Unsupported classes retain the
existing20/16 pursuit thresholds and40-unit firing fallback.

The weapon-resource test verifies installed values, distinct paired values,
missing/default fields and malformed/duplicate preservation. PC full-spawn
3,000-frame rescue/cell-exit passes. PC and NXDK builds pass.

`tools/replay_cover_combat.py` extends the generated3,000-frame spawn replay
to3,600 frames: advance, aim, nine semiautomatic shots, retreat and reload.
Four shots hit and kill guard8490; the player returns to the doorway at
(24.540,-4.118,10.210), reloads to16 rounds and survives with damage taken.
A standing exploratory variant dies at frame3587. This is one fought encounter,
not a cleared corridor or completed section. Generate the prefix first with
`python tools/replay_area2_spawn.py`, then run the combat script.

Stock64MiB XEMU `render-20260914-205728` completes3,600 frames and all28
selected PC/native comparisons. COMBAT is `[9,4,1,42664586,...]`: nine shots,
four hits, one guard kill. Ammunition matches16 loaded,116 reserve and one
reload. Player health matches47.19998. Free memory is4,304 pages (16.8125MiB).
The harness closes its own process; no RF emulator remains. Reproduce with:

```
python tools/replay_area2_spawn.py
python tools/replay_cover_combat.py
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --input artifacts/cover-combat-replay/input.bin --seconds 540
```

This proves the connected spawn/rescue/guard-kill/retreat/reload sequence. It
does not establish complete encounter balance or a cleared path to the exit.

## Two guards and medical recovery (2026-09-14)

The read-only `rf_scene_npc_combat_row` snapshot exposes stable IDs, weapon,
combat orders, body/eye positions and health while owners are alive. PC replay
tracing emits it at the beginning of the last simulation frame, before teardown;
it is not a post-final-frame snapshot and does not alter NPC state.

`python tools/replay_cover_combat.py --second-guard` passes4,000 PC frames:
guards8490 and5677 die from13 player shots/eight hits; two reloads leave16 loaded
and112 reserve. The player retreats alive. The snapshot verifies both deaths.

`--medical-crate` extends this same uninterrupted spawn route to4,500 frames.
The player walks back into the room, holds Use near trigger8549, opens the
little crate via key8552, and collects medical kit8553. The pickup restores25
health; the player ends at(25.543,-4.118,4.214) with48.2 health. No actors, events,
health or player placement are injected. Remaining guards and the exit are open.

The4,000-frame native attempt render-20260914-211422 was manually closed by the
user during loading and is not gameplay evidence. QMP disconnect cleanup now
releases its socket without throwing on a reset, so disc restoration can run.
New render checks persist original disc settings in disc-restore.json before
mutation. That interrupted older run had no persistent backup: its exact replay
was removed; the existing spawn/level selection and controller flag were retained.

A same-route PC control with crate Use omitted ends at the same position,
leaves the crate closed, takes no kit8553 and restores zero health (23.2 health
remaining). Reproduce with `--crate-closed-control`; merely walking into pickup
range through a closed lid does not collect the kit.

Stock64MiB XEMU `render-20260914-212459` now passes the full4,500-frame medical
route and all28 selected final PC/native state comparisons. Two kills/eight hits,
13 shots, two reloads, kit8553 collection and crate key8552 agree. Final health
is48.19999; free memory is4,304 pages (16.8125MiB). The pickup CPU-vertex count
is backend-specific and excluded as before; this is selected final-state parity,
not a comparison of every frame or every subsystem. Disc restoration was checked
against the persisted backup and no project XEMU remains after owned cleanup.

```
python tools/replay_cover_combat.py --medical-crate
python tools/replay_cover_combat.py --crate-closed-control
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --input artifacts/cover-combat-medical-replay/input.bin --seconds 600
```

## Four kills and onward corridor (2026-09-14)

`tools/replay_area2_corridor.py` preserves the medical replay through frame4199,
then turns to face the two pursuing Riot Stick guards immediately. A delayed
turn at4500 instead kills only one guard and the player dies at4814; this is
route timing evidence, not a reason to weaken damage or grant health.

The earlier turn kills5676 and5678 as well as the two prior guards. PC completes
5,350 frames alive with48.19999 health,29 shots,18 hits,four kills and three
reloads (16 loaded,96 reserve). It then walks out of the rescue room, through
the hall and the northern doorway to(27.092,-4.145,28.662). No placement, event,
health or damage injection is used. NPC snapshots verify all four dead IDs.
Native verification of this extension is pending; the4,500-frame native medical
route remains the latest passing Xbox evidence. This does not clear every guard.

Further PC exploration reaches the western room and rear oval room. Guard5029
still threatens that route; medical kit8047 can be collected, but lingering in
the rear room kills the player. The attempted direct north approach stops near
(4.376,-5.118,49.437). Static geometry slices at y=-5.1 and y=-3.8 show a solid
northern wall on this floor and an offset upper exit passage connecting from
the east near x7.5,z51.5. The rear room is not a direct entrance to that passage.
The eastern upper approach, its elevation changes and uninterrupted section exit
remain unverified; do not substitute a teleport or forced exit for this traversal.

Reproduce the verified continuation after generating its medical prefix:
```
python tools/replay_cover_combat.py --medical-crate
python tools/replay_area2_corridor.py
```

## Native corridor and eastern firefight (2026-09-14)

Stock64MiB run `render-20260914-214750` passes5,350 frames and all28 selected
final PC/native comparisons for the four-kill corridor route. Free memory is
4,255 pages (16.621MiB). Original disc settings match the saved restoration
manifest afterward and no project XEMU remains.

`tools/replay_area2_east.py` extends the authored-spawn route to6,250 PC frames.
It takes the far eastern ramp at x50, approaches door triggers7816/8058 centered
near(46.981,-4.771,38.495), and fights from(47.108,-5.118,38.624). Four pistol
shots each kill8071 and5683. The full route records37 shots,26 hits,six player
kills and four reloads;16 rounds remain loaded with88 reserve and15 health.
The six dead IDs are checked in the live NPC snapshot. The initial higher aim
missed both guards; correcting pitch produces eight hits from eight new shots.
This extension is PC evidence only; native remains the5,350-frame corridor run.

Exploratory ordinary-input continuations cross the chamber alive, approach
trigger7827 at(18.481,-4.771,46.495), and enter the room before the exit ramp.
The lower floor contains a long central fixture (approximately x12.8..15.1,
z47.6..59.1) absent from the earlier higher slice. The exit ramp near x7.75..9.625,
z50.5..52.5 rises from y-5.0625 to-3.875 (static faces7040/7041). Standing
movement/jump attempts remain obstructed. Crouch clearance, breakable geometry,
and precise collision behavior there have not yet been distinguished. No full
section-exit claim or forced exit is made. Full six-kill native verification and
resolving this last approach are open.

## Uninterrupted Area2 exit (2026-09-14)

The obstruction is resolved by the route: a centered approach at x11.5,z51.6,
a jump at frame6810 and crouch from6822 allow entry over the raised static ramp
and into the low passage. Crouched movement reaches the authored exit normally.
No collision bypass, removed geometry, injected event, forced placement or
health grant is used. Previous standing/poorly centered attempts did not prove
an engine collision defect; crouch plus alignment succeeds with existing code.

`tools/replay_area2_exit.py` continues the verified6,250-frame six-kill prefix.
The PC run passes7,450 frames and transitions L2S2a.rfl to L2S3.rfl via5150 at
frame7275. Player health15, loaded16/reserve88 pistol rounds and the consumed
kit8553 persist. Combat counters reset for the new section, so the six-kill
assertions belong to the separately checked prefix, not the destination snapshot.
The test checks one natural exit and retained player state; it does not establish
retail parity, every optional encounter, revisit state or the entire campaign.

The native render harness bound is raised to9,000 frames/900 seconds to cover
this uninterrupted section and loading its successor. Xbox still reads one
replay record at a time; the change does not allocate the whole replay in guest
RAM or change the stock64MiB target. Native verification is pending.

```
python tools/replay_area2_east.py
python tools/replay_area2_exit.py
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --input artifacts/area2-exit-replay/input.bin --seconds 900
```

Native confirmation: stock64MiB `render-20260914-220704` passes all7,450 frames
and28 selected final-state comparisons. Xbox takes the same5150 exit at global
frame7275 into L2S3.rfl. Health15 and pistol88 reserve/16 loaded match PC after
175 destination frames. Destination free memory is4,679 pages (18.277MiB).
The input SHA256 equals the reproducible PC exit fixture. Disc restoration is
verified byte-for-byte against its manifest and the owned XEMU is closed.

This validates uninterrupted traversal and retained player state across the
natural section exit. Destination combat counters reset, so this final snapshot
does not independently count the six native kills before transition; the six
kills are directly asserted by the PC prefix and the four-kill native prefix
was separately checked. Full single-player completion remains open. Next focus:
L2S3 encounters and onward progression. Rough project estimate advances to~46%.

## L2S3 entry and natural return (2026-09-14)

`tools/replay_area2_return.py` extends the7,450-frame exit prefix and follows
the real return passage east. PC completes8,000 frames with transitions5150
at7275 into L2S3 and5151 at7688 back into L2S2a. The final live snapshot omits
retired8490,5677,5676,5678,8071,5683 and5458; ACTOR_RETIREMENT reports seven
retirements. Kit8553 stays consumed, health remains15, and pistol ammunition
remains88 reserve/16 loaded. This checks those concrete persistent states, not
all mission timers, movers, injured living actors or full revisit fidelity.
Native verification of this return fixture remains open.

The onward path instead runs west to x58.1, then north out of the low passage
toward z78.6. It activates the point-of-no-return door area near trigger8206.
Pistol guard2020 waits around(57.339,-1.107,78.765). An unarmed approach dies
after two enemy hits; the aimed attempt lands three shots, leaves him at30
health, and the15-health player still dies. These are PC observations, not a
proof that the encounter is impossible or that damage should be reduced.

The installed weapons.tbl gives the12mm handgun `$AI Spread Degrees: 3.0 4.0`
and the Assault Rifle2.0/2.0. The current enemy loop aims at its chosen target
and applies damage after range/facing/visibility checks without a spread ray.
This is an existing first-pass accuracy gap. The meaning of the paired spread
values and their original sampling still needs confirmation; do not assume they
are SP/MP values merely because some other table settings use that convention.
AI damage scaling and cadence remain open as already documented. Next work
should address these combat systems rather than indefinitely tuning a route
against provisional perfect aim. Rough estimate remains~46%.

## Authored firearm spread first pass (2026-09-14)

Local Dash Faction reference commit b2d61d9f66623b188907aae749c4c47c9e40ca25,
`game_patch/rf/weapon.h` WeaponInfo, labels the paired AI spread fields as
single/multiplayer plus a selected runtime value. This resolves the earlier
pair ambiguity: primary handgun SP spread is3 degrees, rifle2. The port now
parses the SP value and validates both finite values in0..90, rejects duplicates,
and preserves outputs on malformed input. Missing spread defaults to zero.
No third-party implementation was copied.

`rf_weapon_spread_ray` normalizes the aim axis, uses the existing deterministic
cone sampler, and preserves ray length. The cone is uniform in solid angle and
the table value is treated as its half-angle as an explicit first-pass policy;
original weapon sampling, difficulty scaling and RNG ordering are not claimed.
A dedicated RNG starts at1 for each section and does not consume audio RNG.
Zero spread consumes no RNG and returns the original ray exactly.

After a firearm attempt passes range/facing/visibility and emits presentation,
the sampled ray must intersect the intended target bounds and pass the existing
bullet-occlusion query before damage is delivered. Misses still count as shots,
including watched script Attack attempts. Melee is unchanged. Unsupported
weapons retain zero-spread fallback and are counted. Intercepting other actors,
impact effects, complete original accuracy, cadence and damage scales remain
open; damage is still the provisional10. This is not a tuned survival guarantee.

ENEMY_SPREAD reports firearm attempts,samples,target hits,misses,world blocks,
RNG state,unsupported fallback count,status. The exposed guard-recovery PC
fixture passes with27 sampled firearm attempts,24 target hits and3 misses;
no fallback or errors. The unopposed player still dies. Full-spawn3,000-frame
rescue also passes (eight firearm hits/eight samples). Older long-route health
and exact timing evidence predates this change and must be revalidated before
being claimed for the new build.

Tests cover2048 deterministic samples over horizontal, vertical and oblique
axes, cone bounds, ray length, zero-spread RNG preservation, malformed parser
output preservation and installed handgun/rifle values. PC and NXDK builds pass;
player_weapon_resources passes. Native exposed-encounter verification is pending.

Final-branch PC regressions: the exposed2,400-frame encounter passes, the
six-kill6,250-frame fixture passes, the natural7,450-frame exit passes, and the
8,000-frame return passes. The long route now carries48.1999855 health rather
than15, with ammunition unchanged. Exit/return checks compare health against
the checked incoming fixture instead of retaining the obsolete15-health value;
they still require positive damaged health, exact exits, ammunition and retired
actors. Cone tests pass. A miss-path bookkeeping branch caught in review was
corrected before the final native rerun; misses reach the final scripted-shot
accounting instead of returning through the pursuit/cooldown check.

Final native confirmation: `render-20260914-223924` passes2,400 frames and all29
selected PC/native comparisons on stock64MiB. ENEMY_SPREAD is
`[27,27,24,3,0,2146787367,0,0]` on both targets. Free memory is4,377 pages
(17.098MiB). This final run includes the reviewed miss-accounting correction.
The disc restoration manifest matches afterward and no RF XEMU remains.
The complete updated7,450/8,000-frame route is PC-verified; its new native
revalidation remains open rather than being inferred from the short encounter.
Rough project estimate remains~46%; next gameplay focus is L2S3 combat with the
new carried health and accuracy behavior.

## L2S3 first encounters after spread (2026-09-14)

`tools/replay_area3_entry.py` extends the complete Area2 exit replay to8,000 PC
frames. From the carried entrance position, west then north clears the low
passage, followed by ordinary aim/fire/reload input. Guard2020 dies from four
hits among seven shots; player health is38.599987, pistol16 loaded/81 reserve.
The outgoing Area2 transition remains5150 at7275. Native full-route validation
is pending; this continuation uses the accuracy implementation above.

`tools/replay_area3_maintenance.py` extends that sequence to9,000 PC frames.
It walks through maintenance trigger1103/door1100, then approaches1099 and kills
guard2047. Two L2S3 kills,14 shots/eight hits/two reloads are checked, with25
health remaining. The first crossing attempt stopped short; extending the
centered movement crosses the door using existing behavior. No forced trigger,
placement, invulnerability or health changes are injected.

Bounded inspection of original L2S3 entity records (level.c read layout) confirms
2020/2047 are guard1 affiliation0,2061 is miner1 affiliation2, and1762 is tech1
affiliation1. The nearby armed miners are not interchangeable hostile targets.
The maintained fixture checks2061 remains at100 health. Trigger1099 also links
the authored delayed Slay_Object2082/2083 events, which kill miners2058/2043
at2/2.5 seconds; their deaths are checked separately from the player's two kills.
Further guards, invulnerability event2084, scripted encounter completion and
the eventual L2S3 exit remain open. Rough estimate remains~46%.

Native entry confirmation: stock64MiB `render-20260914-224708` completes8,000
frames and all29 selected PC/native comparisons. The transition remains5150
at7275. The first L2S3 guard kill matches `[7,4,1,9371790,...]`, with38.599987
health and16 loaded/81 reserve. Free memory is4,678 pages (18.273MiB). This
revalidates the full updated Area2-to-L2S3 entry route with firearm spread;
pre-transition kill counters are still not inferred from destination counters.
Disc restoration is checked and the owned XEMU is closed. The9,000-frame second
guard/maintenance continuation remains PC-only evidence. Its final input and
live snapshot also pass the separate checks for dead2043/2058 and healthy2061.

Reproduce after generating the Area2 exit prefix:
```
python tools/replay_area3_entry.py
python tools/replay_area3_maintenance.py
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --input artifacts/area3-entry-replay/input.bin --seconds 900
```


## L2S3 third guard: uninterrupted hall encounter (2026-09-14)

`tools/replay_area3_hall.py` extends the maintenance fixture to 9,600 frames.
It walks east while turning south, then fires on guard 1751 from the hall.
There is no placement, forced event, health grant or state reset: the full
Area2 rescue/combat/medical/exit prefix remains intact. PC completes with
three L2S3 kills, 19 shots/13 hits, 5 health and 16 loaded/70 reserve. The final
position is (98.539818,-0.848180,84.116463). Actor snapshots separately require
2020/2047/1751 dead, authored Slay victims2043/2058 dead, and friendly 2061 at 100.
A movement-only exploratory control died in the same hall; this is evidence
that the current low-health encounter can be survived with aimed fire, not
that combat balance or the full campaign is complete.

The installed weapons.tbl identifies weapon 8 as Assault Rifle, already one
of the three supported primary definitions. The earlier working assumption
that guard 2114 required another weapon implementation was incorrect. Further
combat, the elevator shaft climb, scripted arrivals and the L2S3 exit remain
open. The native harness now accepts up to 12,000 streamed input frames (the
Xbox player already supports 60,000); its 900-second wall-time cap is unchanged.

Reproduce after the maintenance prefix:
```
python tools/replay_area3_hall.py
python tools/xemu_render_check.py --spawn --level L2S2a.rfl --input artifacts/area3-hall-replay/input.bin --seconds 900
```

Next-route asset evidence: L2S3 item 2115 is a Shotgun at (100.591,-1.704,74.312);
medical kit 1931 is at (108.702,7.131,60.701). These are authored item-section
positions, not verified reachable pickups in this replay. Shotgun gameplay
is outside the current three-slot player implementation and is now a TO-DO.

Shotgun table baseline for the next implementation: four projectiles, eight
shells, two-second reload, 1.50-second primary and 0.225-second alternate wait;
SP primary/alternate spread is 3/6 degrees, with damage 40. These values
are parsed directly from installed weapons.tbl; spread sampling and complete
primary/alternate behavior still require implementation and verification.

Xbox confirmation: `render-20260914-230916` passes all 9,600 frames and all
29 selected PC/native comparisons on stock 64 MiB. COMBAT is
`[19,13,3,9109642,3240099836,16,0,0]`, health is 5, and ammo is 16/70.
The natural transition remains UID5150 at frame7275. Free memory is
4,548 pages (17.765625 MiB). Per-NPC health assertions remain PC-side; the
native comparisons verify aggregate combat and shared player state. All 19
disc override files match the saved restoration manifest, and the owned
XEMU process is closed. Rough project estimate remains approximately46%.
