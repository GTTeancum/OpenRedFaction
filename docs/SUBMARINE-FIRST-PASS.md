# Submarine first pass

Current scope includes an underwater DEV fixture that defaults to enemy-free, an opt-in submerged guard for torpedo targeting, and board/fire checks in three authored campaign sections. Wider campaign vehicle completion remains open.

## Authored inputs

The installed `sub` class uses `Sub_Mini01.v3m`, `sub.vfx`, movement kind7, mass2500, health700, speed6, acceleration8, maximum rotation4 and rotation acceleration2. The actual hull has one radius2 collision sphere and14 attachment tags. Seven table override entries do not imply seven model spheres: missing names are ignored by the existing recovered override path.

The cockpit has19 mesh records. Disabled materialless `Sphere01` has26 vertices/48 faces and no-material face sentinels; it is fully parsed and retained with keys, without a render instance. The other18 meshes keep render instances. Hull, cockpit and torpedo resource test reports1,921,594 resident bytes and1,930,486 peak bytes on PC; these figures exclude allocator overhead and are not a full Xbox memory measurement.

Torpedo uses actual model `torpedo01.v3m`, damage200, speed7, lifetime10seconds, radius0.15, blast/crater radius5, reserve20 and3second cadence. Guidance scans generation-bearing live hostile NPC handles with underwater aim points and actual collision LOS. The turn-time-as-full-revolution and full-cone interpretations are explicit first-pass policies; a single opt-in hostile target now verifies live native tracking below. Liquid boundary expiration is distinct from a solid-impact explosion.

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

Native300-frame run `artifacts/xemu/render-20260918-162608` also passes: one boarding, one swimming exit, one torpedo launch/solid impact/detonation,19rounds remaining; all vehicle words and all eight torpedo counters match PC exactly. Native external hull/on-foot weapon frame inspected;21.43MiB free and harness disc restoration confirmed. Ordinary live saves and one native submerged-target check are covered below; wider pursuit, articulated/propulsion visuals, full audio and campaign placement remain open. The underwater fixture uses actual static world geometry; it does not yet install an editable terrain owner, so the torpedo terrain dispatch is wired but underwater GeoMod is not demonstrated.

The dark cave and cockpit appearance are first-pass visuals. The generic vehicle HUD still uses ground-vehicle control hints; submarine controls are forward/strafe, look to rotate, jump/crouch to rise/dive, use to board/exit, primary fire for torpedoes.


## Save and guidance follow-up

RFVC4/profile4 stores common vehicle pose/motion/occupancy/700HP plus torpedo reserve and remaining cooldown. Shared RFCP3 transport and seated player pairing admit this typed record; codec, cross-profile rejection and pure capture/restore staging checks pass. The live ordinary world-save path now captures and restores the seated L5S3 DEV submarine with full-hull and player wet/solid admission. Active torpedoes and unsupported effects still reject capture; the separate DEV GeoMod player-checkpoint frontend remains disabled for this profile.

The stock-64-MiB Xbox save/reload check at `artifacts/xemu/submarine-save-20260927-155359` passed with an isolated test HDD and process-contained input. After boarding, forward motion and ascent, the save wrote10,768bytes; the fresh load accepted the same payload, restored occupancy and20 torpedoes, and continued80 neutral frames with5,362 free pages (20.95MiB). The test disc flags were restored. This verifies a bounded native functional flow, not campaign vehicle placement or interactive controller feel. The L5S3 `Headlamp_State` and `Detach` event types now have ordinary event checkpoint records; their gameplay effects remain separate implementation work.

Guidance performs allocation-free candidate scanning, source/driver exclusion, wet-target filtering, collision LOS and bounded speed-preserving steering before the liquid flight step. Focused candidate/LOS/wakeup/turn/error checks pass. A stock-64-MiB Xbox run in `artifacts/xemu/submarine-target-20260927-162038` with opt-in `dev-npc.flag=7` spawned one submerged hostile guard in L5S3: the torpedo acquired it for25 steps, changed heading25 times, detonated once, and reduced guard health from50 to12.57;19 torpedoes remained and5,127 pages (20.03MiB) were free. The test disc flags were restored. This proves one bounded native acquisition/damage flow, while wider pursuit and campaign encounters remain open. Current parser ownership adds132 PC bytes to the earlier submarine resource aggregate (now1,921,726 resident/1,930,618 peak).

The earlier fixture's detonation was a world contact whose blast damaged the guard, rather than a direct projectile hit. Impact ownership is now reported explicitly by `rf_scene_submarine_impact_state`. With the opt-in guard centered on the measured torpedo corridor, the stock-64-MiB Xbox run in `artifacts/xemu/submarine-target-20260927-163421` records one direct NPC contact, zero world contacts, one detonation and guard death (health -100 after overkill). The torpedo struck at (-24.56,-15.06,4.89), 19 torpedoes remained, 5,127 pages (20.03MiB) were free, and test disc flags were restored. This verifies direct-hit dispatch in one bounded scenario; pursuit across other levels and targets remains open.

## Authored campaign placement

Direct installed RFL inventory (`tools/inventory_campaign_vehicles.py`) finds nine `sub` entities in L5S3, L5S4, L10S2, L10S3 and L10S4. The closest L5S3 entity to the player start, UID3977 at (105.14,65.54,10.02), loaded natively but rejected a staged Use press at its full-body water gate (`artifacts/xemu/submarine-campaign-20260927-164720`). Its authored links include `Turn_Off_Physics`, `Make_Invulnerable` and `Detach`; proximity alone does not establish it as a boardable host. UID3963 at (30.74,-16.49,8.31) instead has authored `Headlamp_State` and `Goto_Player` links. A bounded 80-frame stock-64-MiB Xbox run in `artifacts/xemu/submarine-campaign-20260927-165038` staged the player three units away through the existing process-local actor-start hook, boarded once, launched one torpedo, retained 19 rounds and had 5,895 free pages (23.03MiB); disc flags were restored.

The same one-host adapter now selects L5S4 UID3955 and L10S3 UID6794 from their actual entity records. Matching 80-frame stock-64-MiB Xbox checks in `artifacts/xemu/submarine-campaign-l5s4-20260927-165437` and `artifacts/xemu/submarine-campaign-l10s3-20260927-165612` each staged beside that host, boarded once and launched one torpedo, retaining 19 rounds. They had 5,122 pages (20.01MiB) and 6,049 pages (23.63MiB) free respectively, and restored test disc flags. These checks verify three authored level/host combinations, not natural travel from each player start. The six remaining submarine entities include an L5S4 `UnHide` target and an L10S3 `Turn_Off_Physics` target; two others lie outside the serialized liquid-room bounding boxes at their initial poses. Their scripted motion, visibility and simultaneous ownership remain open. Current scene ownership can represent only one vehicle host at a time.

## Authored movement order

L5S3 `Goto_Player` event UID4647 links to the selected underwater submarine UID3963. The campaign event adapter now routes `Goto` and `Goto_Player` orders for that submarine to the existing wet-hull motion solver when no player occupies it. Steering uses the authored fixed point or the current player body position, bounded yaw/throttle/rise, and the existing swept water and solid-collision gates. A player in the seat retains manual control. Active orders use the ordinary vehicle checkpoint trailer (version 2), which reconstructs the order from the authored event and validates its link on load; the earlier waypoint trailer remains version 1.

A 180-frame stock-64-MiB Xbox check in `artifacts/xemu/submarine-campaign-l5s3-20260927-170300` explicitly fired UID4647 through the process-local event hook. It issued 179 steering ticks and moved the submarine 4.67 m, with 5,779 free pages (22.57 MiB). A second bounded check in `artifacts/xemu/submarine-campaign-l5s3-20260927-170733` quick-saved at frame 60, reloaded at frame 100, resumed the active order for 78 scene frames and ended 4.54 m from the initial position. Its checkpoint status was successful, 5,580 pages (21.80 MiB) remained, and disc flags were restored. These are functional Xbox checks of event dispatch, movement and persistence. Natural event activation, collision-aware pathfinding, other submarine hosts and multi-host ownership remain open.

L5S4's boardable UID3955 is linked to fixed-point `Goto` event UID3958 at (-12.32,-22.44,-18.18). A 180-frame stock-64-MiB Xbox check in `artifacts/xemu/submarine-campaign-l5s4-20260927-171205` explicitly fired that event. The submarine moved 14.12 m and closed its distance to the authored point from 15.80 m to 1.70 m, with 5,139 free pages (20.07 MiB); disc flags were restored. This verifies the fixed-target path through the native wet-hull solver. It does not establish natural event timing or obstacle-aware routes, and the hull had not yet reached the 1.5 m arrival threshold by frame 180.
