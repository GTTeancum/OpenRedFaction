# Static turret combat first pass

`scene_turret_combat.inc` supplies autonomous targeting, bounded rotation and
Vauss firing cadence for the static owners in `scene_turret_owner.inc`.
It is integrated into the Xbox scene; bounded native evidence and remaining limits are recorded below.
No PC execution, gameplay run or image check was performed for this slice.

The helper retains each turret's authored mounting basis and applies relative
pitch/heading to that basis. It uses parsed class FOV, relative eye limits,
maximum rotational velocity and acceleration. The shared reconstructed
`rf_angular_velocity_step` implements the rate response (49f485..49f565);
acquisition, five-degree firing alignment and overshoot clamping are practical
port policies, not a reconstruction of the original turret scheduler.

Installed `entity.tbl` evidence, retained in
`artifacts/entity-table-inspection.txt`:

| Class | FOV | Pitch range | Maximum rotation / acceleration |
|---|---:|---:|---:|
| Stationary Turret | 60 degrees | -25 to 25 degrees | 3 / 1.1 |
| Stationary Turret_Plain | 60 degrees | -25 to 25 degrees | 1 / 1 |
| Auto Turret Head | 360 degrees | -360 to 360 degrees | 6 / 10 |

All three permit heading -360 to 360 degrees and declare Vauss as their
default primary. PHB declarations are decoded to radians by
`rf_entity_eye_limits_read`; FOV cosine is decoded from original class764 by
`rf_entity_vitals_config_read` (41bcba). The physical look representation uses
pitch within +/-90 degrees; a target direction never needs a wider pitch.

Installed `weapons.tbl` Vauss declaration, retained in
`artifacts/weapons-inspect.tbl`, has `continuous_fire`, `from_eye`, fire wait
0.08 seconds, damage100, normal AI damage scale0.2, AI range30 and AI spread
1.5 degrees. It declares no clip or reload fields. The primary parser now
allows a wholly absent clip pair when continuous firing is explicitly
authored; partial clip pairs and missing required fire/damage fields still
reject. Existing `player_weapon_tests.c` contains a bounded positive/negative
fixture for this parser change; it has not been executed in this slice.

The scheduler shares `campaign_enemy_cadence`, producing five simulation
ticks between ordinary Vauss shots at60Hz. It never replays a backlog of
shots after blocked aim or cover. The parent shot callback owns spread,
nearest-body selection, shields, world/mover/clutter obstruction, damage and
feedback. Authored `from_eye` means the physical ray begins at the eye;
the separate muzzle position is supplied for presentation. Both eye and
muzzle sight lines must be clear before dispatch. Actual spread rays still
need the existing firearm cover check inside the parent callback.

Catatonic action1, dead/hidden owners and linked occupants inhibit auto-fire.
Waiting2 permits acquisition, matching existing NPC behavior. A mode change
clears the previous target/burst; motion-detect11 requires movement for a
fresh target. Parent candidates supply liveness and hostility relative to
the shooter. An already acquired target is retained outside the initial FOV
while visible and within range; loss of cover/range releases it. No idle
search sweep, blind pursuit, possession or authored scripted-fire mode is
implemented here. Emplaced firing currently has no finite-ammo owner.

Integration order:

1. Include after turret ownership and `scene_ai_gameplay.inc`.
2. After opening owners, call `scene_turret_combat_open(&tables, vauss_id)`
   with the actual weapon catalog ID. It loads primary metadata once.
3. Call `scene_turret_combat_tick(frame, &backend)` once per60Hz simulation
   frame after current world/actor transforms are published. Backend fields
   are `count`, `target`, `blocked`, `shot`, `context`. Candidate enumeration
   is bounded to1024. Zero candidates releases acquired targets.
4. Render and collide using the owner's updated `basis`. The sidecar `mount`
   remains the authored attachment frame; a future moving-parent binding
   must update it explicitly instead of overwriting aimed `basis` alone.
5. Call `scene_turret_combat_close` before releasing owners on level teardown.

The sidecar stores handle, last processed frame, burst, last AI action,
mounting basis, relative angles and angular state. Save integration should
restore matching `action` and set `last_frame=UINT32_MAX`, preserving the
target's stable identity and rebasing `owner.fire_due` to the loaded frame.
The RFTU1 world-save wrapper now persists these fields; its native evidence is recorded in STATIONARY-TURRETS.md.

## Scene firearm adapter

`scene_turret_scene_combat.inc` now implements the concrete backend; include
it after `campaign_enemy_tick` and the combat helper, then call
`scene_turret_scene_tick(stream, frame)` inside the ordinary once-per-frame
combat update. The actor-follow callback has already published the listener
position before this point; the adapter reuses it without advancing player
look or animation again. Player shield animation must retain its existing
pre-enemy-fire update order.

Candidate enumeration validates live generation-bearing registrations and
uses the existing NPC rule that affiliation0 may acquire the player. Player
damage affiliation currently initializes to0, so equal values are not a
general team-ID comparison. NPC candidates use opposed authored0/2
affiliations; neutral1 and outcast3 automatic targeting are excluded. An
uncompromised disguise prevents fresh player acquisition. Incidental actors
may still intercept fired rays regardless of target affiliation.

Each shot uses shared AI spread RNG and `combat_enemy_primary_damage`,
then selects the nearest actual player/NPC body or held shield. The existing
vehicle/turret contact helper competes at that distance; live clutter,
GeoMod fragments, static geometry and movers resolve before actor damage.
Shield damage, Nano protection, player/NPC damage, pain feedback and NPC
death entry all use the ordinary services. Authored Vauss5 Fire is played
for stationary turrets and the declared Vauss2 Fire override for Auto Turret
Head. No temporary skeletal owner or direct health subtraction is used.

The adapter emits one shot per scheduler callback and does not invent a
reserve quantity from the player's170-round capacity. Emplaced reserve
remains unlimited by explicit first-pass policy; determining turret reserve
semantics, possession and inventory is separate follow-up work. Visual
muzzle effects, cinematic-player aiming and physical linked-base ownership
are not established by this adapter. `rf_scene_turret_shots[8]` records shots,
damaging hits, shield interceptions, vehicle/turret contacts, cover stops,
misses, last victim and errors for the parent's focused Xbox check.

## Native autofire evidence (2026-09-30)

`tools/xemu_turret_fire.py` ran120 neutral frames on stock64MiB Xbox with the
same authored UID5547 in CTF06, facing the player and without the catatonic or
scripted-hit fixture. It acquired once, turned, emitted two shots through the
shared firearm adapter, and delivered two attributed damaging hits. Player
health fell100 to80.800003; no health was restored. The scene retained3,397 free
pages (~13.27MiB), and no combat, damage, render or guest errors were recorded.

The original `artifacts/xemu/turret-fire-20260930-125404/report.json` remains FAIL
because the first harness incorrectly required no armor pickup. CTF06's retained
item granted10.400002 armor; this does not explain the health loss or attributed
shots. `--validate-existing` checked the same captured outputs and wrote
`validation.json` with PASS_TURRET_AUTOFIRE, explicitly retaining that caveat.
No native rerun or alteration of the archived fixture was used to obtain it.
Appearance, sound output, allied-NPC target choice and moving-base aiming remain
unverified; this is not a campaign encounter or weapon-ammunition test.
