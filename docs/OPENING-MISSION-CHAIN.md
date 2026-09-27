# Opening mission: authored waypoint blocker

Performance work is deferred after the final basic-testing pass. Follow_Waypoints and linked-trigger enabling are now connected. The staged entrance-to-Riot-Stick walking sequence passes on PC; full confrontation choreography and the rest of the campaign remain incomplete.

## Reproduced behavior

The local `artifacts/riot-trigger/entrance` replay crosses player trigger9028 without directly activating its events. It starts Goto8433/8434, both miners reach their destinations, and NPC8432 activates trigger9525. No Riot Stick grant occurs. That initial replay did not cross the upstream Follow_Waypoints trigger.

The authored chain is player trigger9029 -> Follow_Waypoints9646 -> actor8322 following `L1S1MinerBuddy` in `One way` mode. Player trigger9028 enables NPC-filtered trigger9027. Actor8322 entering9027 starts the confrontation, including Delay9871 (14.4seconds), which enables player trigger9869 (`give_riot_stick_trigger`); contact then calls Give_Item_To_Player9870.

## Installed level evidence

L1S1.rfl section0x10000 at offset2855626 has348 payload bytes. The payload parses exactly as a u32 record count (13), then records of u16 name length, name bytes, u32 node count, and u32 node indices. This layout is observed from installed data. The shared bounds-checked parser now passes all68 single-player path sections; the largest payload is510 bytes. A separate inventory scan also parses the multiplayer sections (94 total).

`L1S1MinerBuddy` contains indices18 and330. In the existing parsed navigation section0x20000:

- Index18 is UID8640, position(-80.531273,-5.681978,47.126621), tags[8322].
- Index330 is UID9673, position(-75.031525,-7.643442,27.972010), tags[].

The latter position lies within confrontation trigger9027. This supports interpreting the path entries as navigation-node indices; validate across other levels before generalizing the loader.

## Shared implementation and verification

The scene retains one bounded section payload (348 bytes in L1S1, hard cap64KiB) and each actor borrows its selected path. Node access decodes unaligned little-endian indices. Malformed lengths/counts, trailing data, embedded NULs and out-of-range indices fail validation without publishing output. Installed UINT32_MAX placeholder nodes are structurally accepted; requesting such an unresolved path returns NOT_FOUND without changing actor movement. Missing/empty paths do not start movement.

Event28 dispatches through the existing movement callback, including delayed activation and off/cancellation. One-way movement visits each authored destination through existing routing, ground support, collision and steering, then stops. Loop and Ping Pong have practical endpoint wrap/reversal behavior, but have not yet received an end-to-end authored replay. Exact original mode, speed, nearest-start and interruption semantics remain unverified. The normal actor-death/removal paths stop movement; new Goto/Follow commands replace the path.

`python tools/replay_waypoints.py` stages outside player trigger9029 and uses ordinary walking to activate9646. It supplies no direct setup/event dispatch. The2400-frame PC replay records one movement request,906 movement steps, both destination arrivals, zero blocked steps, and no active movement at completion. Miner8322 ends within0.25 horizontal units of node330 and contacts NPC trigger9672 near the first waypoint. All38 PC tests pass, including malformed/missing/placeholder paths and delayed on/off dispatch.

The earlier waypoint-only replay does not cross player trigger9028, so9027 remains disabled and the Riot Stick grant does not occur. The extended replay below covers that missing route. Play_Animation and Look_At have first-pass adapters, but full confrontation choreography remains incomplete; do not equate successful path traversal with retail mission parity.


Stock64MiB XEMU replay `replay-20260914-132101` passes2400 frames with exact PC movement, actor-position and route counters. NXDK build and normal-build restoration succeed. Reproduce with `python tools/xemu_replay_check.py artifacts/waypoints/inputs.bin --campaign-spawn --level L1S1.rfl --exit-start-uid 9646 --seconds 240`.


## Linked-trigger dispatch fix and opening handoff

The extended route crossed9028 and started both Goto events, but9027 stayed disabled. The shared `rf_trigger_links_dispatch` admitted only events(kind6) and movers(kind8); it dropped linked triggers(kind5). Original4c0320 dispatches kind5 at4c0378 to4c0200, which clears bit16 at trigger+2b0. It enables the target without firing it or resetting its activation count/cooldown. The shared dispatch now forwards kind5 to the scene, which performs that same flag change.

The older `verify_trigger_links.py` compared only recorded event/controller calls. It executed the original enable branch but never observed it, so its passing result did not establish complete trigger-link behavior. The expanded verifier records trigger-enable calls, executes the original enable unchanged, and checks that only bit16 clears. All1024 kind/order/suppression combinations match PC/NXDK callback traces. Other original dispatch families, including kind4 object actions and ambient fallback, remain outside this implementation.

`python tools/replay_opening_handoff.py` starts once outside9029, walks into the hall through9028, and approaches9869. It sends no setup events, forced grants/slays, or later position changes. Three PC controls pass:

- Hall-only2400 frames: confrontation starts, but the player never enters the handoff volume; stays unarmed.
- Full approach1750 frames: player is in the handoff volume but the scripted delay has not finished; stays unarmed.
- Full approach2400 frames: exactly one authored Riot Stick grant, selected weaponID2, loaded charge1, reserve0; no player death. Two authored Slay_Object actions also execute.

The final player position is(-75.388870,-8.095128,27.786995). The NPC-trigger chain and delayed player contact now deliver the weapon naturally within this staged entrance route. This is not a start-to-finish mission proof: exact animation/gaze behavior, confrontation presentation, full actor combat and onward campaign traversal remain unfinished.

An image-free PC replay now starts at the actual L1S1 player spawn, with no
`RF_REPLAY_EXIT_START` or other staged pose. The older walking route needed a
small strafe adjustment and 52 further forward frames to enter the handoff
volume. `python tools/replay_opening_authored_spawn.py` passes 2380 frames:
player trigger9869 contacts, exactly one scripted Riot Stick grant leaves100
charge, one primary attack executes, and the player is alive. This closes the staged-spawn gap for the first
weapon handoff, but does not verify interactive controller input, visuals, or
the rest of the campaign.


Xbox handoff verification remains incomplete: the240-second attempt timed out while frames were advancing; the600-second retry (`replay-20260914-133827`) lost its QMP connection. These are terminal harness results, not a successful native handoff. The user then paused tests and redirected work to structural Xbox optimization. The PC controls and original/PC/NXDK dispatch verifier above passed before that pause.

Stock-64-MiB Xbox replay `replay-20260926-230828` completed the same 2380
real-spawn input frames without framebuffer capture. Saved guest memory matches
PC on the authored Riot Stick grant `[1,1,100,2,1,100,0,0]`; the broad report
had already matched selected weapon2, ammunition `[2,0,100]`, one attack,
and living player state. Recheck the saved evidence with
`python tools/check_opening_xemu_gameplay.py artifacts/xemu/replay-20260926-230828`.
That broad replay itself is **FAIL**: Xbox reports15 NPC draw submissions versus
PC9 at the final frame, with444880 total NPC vertices on both. The focused
gameplay result makes no visual-parity claim.

The NPC count check was subsequently corrected: PC counts actors only after CPU
clipping emits triangles, while Xbox counts retained models before GPU clipping.
The rerun `replay-20260926-231539` passed the bounded NPC count and exact
`NPC_RENDER_DISPATCH` comparison, then stopped at a stale L1S1 clutter memory
fixture. Both PC and Xbox reported `[168,149,173,97772,97772]`; the fixture
omitted one20-byte damage binding for each of170 authored records. The
independent `verify_clutter_scene_bodies.py` calculation now reproduces97772.
This fixes the fixture, not the broad replay result; rerun its remaining checks
before claiming full native parity.

The next text-only stock-memory run, `replay-20260926-232245`, advanced through
that clutter check and stopped at an outdated weapon-material limit in the
verifier. PC and Xbox matched `[4,4,4,525636,526372,524288,289886571,3371059565]`:
524288 bytes is pixel payload, while the measured 526372-byte peak includes
metadata and scratch. The scene loader's total budget is2MiB, so the broad
assertion now uses that actual limit. Remaining assertions have not yet run to
completion; this is still not a broad native PASS or visual-parity result.

The subsequent text-only stock-64-MiB run `replay-20260926-232832` is a broad
**PASS** for all2380 frames. PC/Xbox match the real-spawn trigger9869 contact,
one Riot Stick grant with100 charge, one attack, living player state, authored
NPC dispatch, clutter state and the remaining campaign replay telemetry.
XEMU reports3831 available physical pages (15.67MiB) at completion. The
retained renderer's NPC submission count remains different by design after
CPU versus GPU clipping; this run did not capture or inspect visual output,
and does not prove PS2-level visual parity or full campaign completion.

The real-spawn PC replay now checks the confrontation's script work as well as
the weapon handoff:15 animation registrations produce8 starts (one loop and
seven actions), four Look_At commands produce2693 turn steps and170 published
pose changes, and two authored Slay actions execute. This is live event flow,
not exact choreography or visual fidelity. The broad Xbox verifier now reads
and compares `SCRIPT_ANIMATION` and `SCRIPT_LOOK_AT`; the earlier2380-frame
PASS predates those explicit comparisons.

The first expanded 2380-frame native attempt (`replay-20260926-233605`) ended
at guest frame1802 when XEMU's host OpenGL display fence asserted; no final
guest comparison was available. The contained 1800-frame prefix already
contains all eight scripted animation starts and four Look_At commands. A
second text-only stock-64-MiB run (`replay-20260926-234256`) **PASS**es the
expanded broad checks: `SCRIPT_ANIMATION` matches PC at
`[15,8,1,7,0,9493,8322,2892,5,0]`, and `SCRIPT_LOOK_AT` matches at
`[4,0,2495,0,0,9791,8432,8322,170]`. XEMU reports3847 free pages
(15.03MiB). This validates live scripted state through frame1800, before the
later slays and Riot Stick handoff. The prior2380-frame broad PASS covers
that later gameplay without the two newly added script comparisons. Neither
run inspected visual output.

Reproduce the shorter prefix after generating it with the PC route tool:
`python tools/replay_opening_authored_spawn.py`, then
`python tools/xemu_replay_check.py artifacts/opening-handoff/scripted-1800.bin --campaign-spawn --level L1S1.rfl --no-images --seconds 480`.

The full real-spawn route subsequently completed in stock-64-MiB XEMU as
`replay-20260926-235244`: all2380 frames **PASS** the expanded PC/Xbox
comparisons. Animation state is `[15,8,1,7,0,9493,8322,4118,5,1]`,
Look_At state is `[4,0,2693,544,0,9791,8432,8322,170]`, two scripted
slays match, and trigger9869 grants the Riot Stick before one player attack.
XEMU reports3831 free pages (14.96MiB) at completion. The earlier host
OpenGL assertion did not recur. The run produced no raster files and does
not establish visual choreography or traversal beyond the first handoff.
Reproduce with
`python tools/xemu_replay_check.py artifacts/opening-handoff/authored-spawn.bin --campaign-spawn --level L1S1.rfl --no-images --seconds 600`.

## First live encounter after the handoff

The stationary2380-frame handoff replay ends with23.2 player health. Extending
it without changing the route leaves guard8324 firing every30 frames and kills
the player at frame2386. The Riot Stick grant is already present by frame1805,
when health is100. This is a test-route exposure, not evidence that the player
should survive by standing in the handoff volume.

`tools/replay_opening_exit_walk.py` starts from the same real spawn, leaves at
frame1805, aims at guard8324 using ordinary bounded look commands, and starts
Riot Stick alternate fire near contact range. Holding alternate fire from the
handoff exhausts the battery before contact; the later start defeats the guard.
The player collects its drop, cycles to a16-round handgun and walks to the
stair approach near navpoint36. The tool first records the PC aim commands,
bakes them into an RFI6 input file, then replays that file without the PC aim
helper and requires exact matching PC gameplay/body diagnostics. No event,
inventory, position or damage is injected. The PC final player is alive at
(-53.719,-7.121,13.644) with23.2 health.

Stock-64-MiB XEMU `replay-20260927-002215` **PASS**es all2254 baked input
frames. PC/Xbox match the Riot Stick grant, one combat death, the selected
handgun ID3 with16 rounds, player life/health and the broad campaign telemetry;
3815 physical pages (14.90MiB) remain. The broad verifier does not directly
read the weapon-drop diagnostic, so the native handgun acquisition is inferred
from matching selected weapon/ammo plus the authored drop route. No raster
files were produced or inspected. Reproduce with
`python tools/replay_opening_exit_walk.py` then
`python tools/xemu_replay_check.py artifacts/opening-exit/direct-guard.bin --campaign-spawn --level L1S1.rfl --no-images --seconds 600`.

The offline authored navigation graph has10 disconnected components. Handoff
and exit9019 lie in different components, with a7.443-unit nearest gap between
nodes134 and72 near x=-25. This graph does not prove a player obstruction or
GeoMod requirement; movement/collision and authored events must decide the
crossing. A PC-only extension toward navpoint37 exposes a second armed guard,
UID8326 at(-54.73,-3.34,31.67), whose shot kills the low-health player at
frame2348.

`tools/replay_opening_upper_guard.py` trims the first-guard route just after
the first guard's defeat, cycles to the collected handgun, and bakes ordinary look input
toward UID8326 before moving to a lower stair corner. The no-helper PC replay
ends at frame2300, position(-64.615,-8.563,13.294), alive with23.2 health and
16 rounds. Stock-64-MiB XEMU `replay-20260927-004601` **PASS**es all2300 frames
with matching broad PC/Xbox state and3815 free physical pages (14.90MiB).
No image was captured. Reproduce with `python tools/replay_opening_upper_guard.py`
then `python tools/xemu_replay_check.py artifacts/opening-exit/upper-walk.bin
--campaign-spawn --level L1S1.rfl --no-images --seconds 600`.

Further PC-only probing reached the stair crest near(-56.10,-7.30,13.70)
without additional damage. The lower corner's handgun ray selected UID8326
but was blocked by level geometry; from the crest, three actual hits reduced
the guard from75 to17.4 health. The stationary low-health player then died
before a fourth shot, so this is line-of-sight and damage evidence, not a
completed second encounter or Xbox combat validation. A route using cover or
an earlier firing position remains open, followed by the natural path to
exit9019.

## Moving duel at the upper stair

`tools/replay_opening_upper_duel.py` continues from the lower-corner replay,
keeps ordinary aim input on UID8326 while moving across the firing lane, and
fires four handgun rounds at45-frame intervals. The PC helper only supplies
bounded look inputs; the tool bakes those into RFI6 and confirms the same
player life, inventory, enemy combat and body state without the helper. All
four handgun rays select UID8326, whose health falls from75 to-15. The player
stays alive at23.2 health and finishes frame2550 near(-55.69,-7.27,14.97)
with12 rounds. This is ordinary recorded input, with no forced damage, health,
actor state or position.

Stock-64-MiB XEMU `replay-20260927-005357` **PASS**es the full2550-frame
route with matching broad PC/Xbox state, including two encounter deaths,
handgun ammo and living player state. It leaves3815 available physical pages
(14.90MiB). This checks gameplay state but does not inspect visual or audio
output. Reproduce with `python tools/replay_opening_upper_guard.py`,
`python tools/replay_opening_upper_duel.py`, then
`python tools/xemu_replay_check.py artifacts/opening-exit/upper-duel.bin
--campaign-spawn --level L1S1.rfl --no-images --seconds 600`.

`tools/replay_opening_upper_stairs.py` adds ordinary movement to the next
stair point near navpoint37. Its PC replay ends frame2645 at
(-50.55,-6.30,21.19), still alive with23.2 health and12 handgun rounds;
this extra segment has not been run through XEMU. A longer PC-only probe
toward navpoint130 exposed another armed actor: `ENEMY_SHOT_TRACE 2732 8625`
preceded player death at frame2733 near(-44.37,-4.94,27.61). UID8625 is
authored near(-43.55,43.96) in the horizontal plane. This direct walk exposes
the third encounter; the following route defeats that guard before advancing.

## Third guard and ammunition recovery

`tools/replay_opening_guard_8625.py` holds the safe navpoint37 stair position,
aims at UID8625 with ordinary look input and fires eight handgun rounds at
45-frame intervals. Its PC helper-baked and plain RFI6 replays match player
life, ammo, enemy combat and body state. The first shot damages the guard;
as he moves, some later selected rays hit intervening world geometry. The
remaining shots defeat him by frame2990. At frame3040 the player is alive at
23.2 health with four rounds loaded. No damage, health, position or actor
state is injected. Stock-64-MiB XEMU `replay-20260927-010636` **PASS**es all
3040 frames with matching broad PC/Xbox combat state and3671 physical pages
(14.34MiB) free. Reproduce after the preceding route tools with
`python tools/replay_opening_guard_8625.py` and
`python tools/xemu_replay_check.py artifacts/opening-exit/guard-8625.bin
--campaign-spawn --level L1S1.rfl --no-images --seconds 600`.

`tools/replay_opening_third_drop.py` then walks toward navpoint130 and detours
to UID8625's dropped handgun near(-47.4,-2.3,41.0). The PC replay reaches
frame3345 at(-47.42,-2.33,41.80), alive at23.2 health, with four loaded and
16 reserve rounds. Its weapon-drop diagnostic reports two collected drops,
32 rounds collected in total and UID8625 as the last source. Stock-64-MiB
XEMU `replay-20260927-011215` **PASS**es all3345 frames; selected handgun,
four loaded and16 reserve rounds match PC, with3671 pages (14.34MiB) free.
The broad native verifier does not directly compare the drop diagnostic, so
the Xbox pickup is evidenced by matching ammo after the same walked route.
No images or audio were captured or inspected. Reproduce with
`python tools/replay_opening_third_drop.py` then
`python tools/xemu_replay_check.py artifacts/opening-exit/third-drop.bin
--campaign-spawn --level L1S1.rfl --no-images --seconds 650`.

The later route and exit9019 remain open. The cut through the navigation gap
and the upper-bridge walk are documented below; neither establishes full
campaign or visual parity.

## First walked GeoMod progression wall

The no-image route `tools/replay_opening_wall_blocker.py` extends the real
spawn through the graph's near side. The player passes NPC UID9382 without
an attack, reaches navpoint134 near(-25.35,-1.62,29.59), and presses into
the wall. Forward movement stops near(-22.36,-0.90,28.19); a jump is
accepted at frame3650 but also cannot cross. This establishes an actual
player collision blocker for this route, beyond the disconnected nav graph.
Compiled room40 faces3460/3462/3463 belong to solid editor brush8715; the
opposing room27 faces3501/3502 belong to brush8747. The near face at
x=-21.75 is about0.61 units from the stopped player center. No mover is
authored at this wall.

The same recorded inputs collect authored Remote Charges UID8866 and8867
beside the wall, then Medical Kit UID9789. The kit raises player health
from23.2 to48.2; the later approach and retreat take some combat damage but
leave the player alive. The route cycles to charge slot8, faces the wall,
launches a charge, retreats, cycles to detonator slot9 and fires. PC trace
records attachment at frame3964 and detonation at frame4056 at
(-21.800,-0.742,29.564). `REMOTE` ends `[1,1,1,1,0,0,0,0]`, but both
ordinary `GEOMOD` and `CAMPAIGN_GEOMOD` stay zero: the wall is not cut.
The current `scene_terrain_open` wires weapon-driven edits only for the
developer terrain, while ordinary L1S1 creates a separate scripted cutter
fixed to room28. This explains the missing weapon cut; it does not establish
that the player's chosen route is the only retail route.

Stock-64-MiB XEMU `replay-20260927-013736` **PASS**es all4095 frames with
matching PC/Xbox player life, pickup UIDs8866/8867/9789, weapon
selection, Remote Charge start/launch/attachment/detonation counts and zero
campaign cuts. The native verifier now compares the remote counter directly.
There are3666 physical pages (14.32MiB) free at completion. No image or
audio was captured or inspected. Reproduce after the preceding route tools
with `python tools/replay_opening_wall_blocker.py`, then
`python tools/xemu_replay_check.py artifacts/opening-exit/wall-blocker.bin
--campaign-spawn --level L1S1.rfl --no-images --seconds 750`.

This established the need for a weapon-driven, authored-geometry cut across
rooms40/27, separate from the existing room28 scripted cutter.

A text-only CSG probe now loads the authored Remote Charge crater radius (5.0)
from `tables.vpp` and uses the recorded detonation point. Brush8715 owns36
compiled faces in room40; brush8747 owns12 in room27. The grouped admission
check finds zero foreign solid faces and zero touching external portals at
radius5.0. A single-room check instead counts20/6 foreign solid faces,
respectively, because it sees the opposite wall side. The bounded candidate
build produces85 new wall faces and320 vertices. Room40 stages587 faces/2506
vertices with a98,612-byte tree; room27 stages662 faces/2765 vertices with a
111,212-byte tree. This proves each side fits the current per-room capacities,
but does not yet publish either side, connect the weapon blast, or establish a
through passage. Reproduce with `rf_opening_wall_probe` using `levels1.vpp`,
`build/data/geomod-template.bin`, and `tables.vpp` in that order.

The live ordinary Remote Charge now uses that grouped admission and stages
both rooms' replacement meshes and collision trees before publishing. The
recorded charge commits one cut: room40 has533 faces/2304 vertices, room27
has618 faces/2604 vertices, and the two trees total193,360 bytes. Extending
the replay with150 forward inputs leaves the living player at
(-18.745,-0.240,29.982), beyond the old x=-21.75 wall plane. The text-only
stock64MiB XEMU run `replay-20260927-015923` **PASS**es all4245 frames
against PC state, including the new wall telemetry, with3610 physical pages
(14.10MiB) free at completion. Reproduce via
`python tools/replay_opening_wall_cross.py` and
`python tools/xemu_replay_check.py artifacts/opening-exit/wall-cross.bin
--campaign-spawn --level L1S1.rfl --no-images --seconds 850`.

The replacement meshes enter the shared rendering path, but their visual
appearance has not been inspected because of the no-images instruction.
Both room collision trees retain the new cavity faces, while rendering submits
those identical wall polygons once to avoid coplanar overlap.
Wall-only and combined wall/script-cut save encoding and restore have been
added, but a natural quick-save at frame4300 still fails before destruction
serialization: active NPC UID8625 has `combat_alert=1`, and other opening
actors also retain unsupported navigation, animation or combat state. The
save milestone remains open until that broader NPC checkpoint admission is
implemented and a live save/reload passes on PC and Xbox.

## Far-side corridor and upper-bridge combat

`tools/replay_opening_bridge_cover.py` continues the recorded charge cut with
ordinary forward/strafe inputs. The player follows the second navigation
component around the first right bend, up the long ramp, and to upper-bridge
cover near(41.45,11.92,81.00) at frame5452, alive. At the first bend, guard
UID8462 fires repeatedly; the route's movement reaches cover with14.6 health.
The upper bridge exposes two more env_guards: UID9404 begins at
(60.94,13.36,74.77), UID9406 at(63.14,13.36,77.69). A direct approach dies
to UID9404's shot at frame5441. The northern cover detour avoids that lane,
but advancing from it to about(49.1,11.9,81.6) dies to UID9406 at frame5542.
A normal handgun ray from nearby cover at frame5487 selects and damages
UID9406 from50 to30.8 health, but standing still for the next pistol shot
allows its return fire to kill the player first. This is a live combat and
route blocker, not evidence that AI or hit detection is missing. More robust
cover/weapon play or a health route is needed before exit9019. No visual
content was captured or inspected.

The same5452-frame bridge-cover replay passed on Xbox in XEMU with stock
64 MiB memory (`artifacts/xemu/replay-20260927-025039/report.json`). The
guest final camera position and player-life/cut telemetry agree with PC;
3543 physical pages (about13.84 MiB) remained available. That replay used
the executable built before the duplicate-wall-draw correction above; the
runner restored and rebuilt the current Xbox executable afterward. The
correction passed the PC bridge-cover replay and Xbox compilation, but its
visual appearance remains unverified under the no-images instruction.
