# Rock Snake Spit: selected-owned delayed direct projectile

Status: integrated after independent source and full-hook review against b7546ce5, following the completed 07:00 batch; awaiting 08:00 compilation. No builds, syntax checks, tests, fixtures, emulator or PC runs, campaign traversal, forced events, grants or original-input changes were performed. This is an implementation candidate, not an action/runtime pass.

## Actual owner and missing consumer

Read-only v180 decoding of immutable `Installed_Game/levels2.vpp/L10S4.rfl` found Rock Snake UID5255 explicitly selecting `Rock Snake Spit` with secondary `none`. Its position is (399.5401001,-126.8851013,221.3800812), health2000, friendliness0 and creation flags0. Its authored AI bytes are1/0. It is a placed, visible, living original owner, not a class-default-only theory. No event directly links this owner and no Attack event words name it; no special activation event was invented. This is source evidence of a missing available attack, not proof of acquisition from neutral startup or any campaign route.

The original entry is archive offset7331840, size2742205, SHA25691c25e73b62cc11ca610428954af8a10fb0d7ee63d2987865c74636cba07bf87. RF.exe SHA256 is b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

The only other Snake placement found in the three campaign archives is Big Snake UID7007 in the same level, explicitly selecting `Big Rock Snake Spit`/none. Its health is5000, creation flags0 and friendliness0; When_Dead7137 references it. This independent weapon has 0.8-second delay, 2.5-second cadence and radial damage radius 3. It was excluded from the initial Small-only slice and is now separately integrated; see BIG-ROCK-SNAKE-SPIT-FIRST-PASS.md.

At source baseline b7546ce5, the ordinary and opposed selection chains lack Small Spit. Existing static turret admission is exact Stationary Turret/Stationary Turret_Plain/Auto Turret Head, not every class with turret movement. Rock Snake is a skeletal NPC. Existing startup acquisition plus `scene_npc_loadout_apply` already owns its real selected primary and fills mapped reserve; no new grant is needed.

## Exact original profile

Installed weapons.tbl2052–2080 defines:

- SpitAttack.vfx; clipless12mm with maximum reserve200/200.
- AP damage kind2, damage80, AI range50/50.
- Speed30, collision radius0.5, lifetime10, mass5.
- Fire wait3seconds and one primary Impact Delay1second.
- Weapon flags0; flags2 no_fire_through0x08. No undeviating flag, homing, sticking, glow, piercing, explicit spread or burst.
- Damage radius0. Acid splash impact Vclip radius5 is presentation size, not radial damage. AcidBlob.tga scorch size3 is presentation, not a GeoMod crater or acid damage kind.
- Launch RSnake Spit; empty flight sound; default Spit Hit; flesh Impact Flesh.

The bounded profile verifies the exact selected name, scalar values, supply and reset catalogs; it rejects unsupported magazine/delay/spread/burst/piercing/weapon-type additions. No alternate firing attack is fabricated. Exact class `Rock Snake` and its actual drools-slime flag are additionally required. The initial Small-only reader/selector has since been extended with a separately verified Big profile; see BIG-ROCK-SNAKE-SPIT-FIRST-PASS.md.

Zero AI spread/default scale1 and finite reserve exhaustion retain existing port policy. Original4257c0/42580e–425827 supports actual clipless debit, not proof that ordinary single-player NPCs refuse after exhaustion. Sixteen paired pending/flight slots are a bounded port allocation choice; a full pool refuses before charging or presentation.

## Authored action and delayed acceptance

Original426197–426273 starts action2, sets ordinary cadence actor+4b8 and delayed launch actor+4c0, plays Launch, then debits through4257c0. The adapter starts the existing selected fire presentation exactly once at acceptance, reserves a future flight slot and pays one real round. It owns3second cadence and1000ms windup. Due409340 checks and clears the timer at40939b/4093aa before calling factory4c77a0 at40956b; release has no second debit, refund, range/LOS gate or retry.

Entity.tbl4458–4503 declares turret movement, speed0, sentient, collide_corpse, slippery, and flags2 collide_player/collide_entity/drools slime. `fire_stand` is rsnk_attack_spit.mvf with RSnake Spit Foley; the separate weapon-specific Smash overlay is rsnk_attack_bite.mvf. Existing action2/Launch services already load this selected mapping and positional sound. Foley1499–1500 resolves RSnake_Spit.wav at near10/gain0.9.

Hidden flag0x4000 defers the original update at41daf0→40a110. Pending deadline is retained and releases once after visibility returns. Source death/retirement, loss of exact ownership/selection, successful timeline replacement and explicit disarm cancel only pending work; released flights keep immutable source identity. Target death, a generic pain callback or AI-order change does not cancel another source's prepaid projectile. Normal holster428f00 clears original delayed timers, unlike authored Holster_Weapon4b9980's flag-only path; mirror the existing TriBeam explicit disarm/selection hooks rather than adding blanket pain/mode cancellation.

## Real primary_1 muzzle and drools aim

This is the meaningful difference from Spike TriBeam. Original40953a tests class+728 bit0x20; actual Rock Snake has that drools-slime flag. It therefore passes the result from41b040 at409543–409555, rather than substituting the current-eye basis at409557.

Original424955 pushes string595b58, whose bytes are `primary_`; the numbered lookup is appended into class+1bc at4249c7.41b350 reads that primary attachment list. This is distinct from the handheld `primary_weapon_` list at class+1d4. Unchanged `rf_scene_npc_muzzle` does not cover this creature branch.

Immutable meshes.vpp/rock_snake.v3c has16 bones and three first-LOD attachments: eye, mouth, primary_1. All three share the same local transform and parent bone0 (rsnk-bdbn-head). The primary_1 resource is present; no synthetic head offset or absent-resource fallback is needed.

The adapter resolves primary_1, calls `campaign_model_pose_ready` to evaluate the current generation without advancing animation, then places the tag through `campaign_file_tag_pose` with current skeleton/body transform. Missing tag refuses admission before debit; unexpected release-time resource failure is reported, never silently replaced with eye+0.3 or a center-of-body origin.

Original41b3e4 places the attachment;41b3f2–41b42c first forms a basis toward the current target's body for drools slime.41b4c0 then preserves that basis while pain animation locks aiming, otherwise refines toward current target eye only if normalized direction dot source eye-forward is greater than0.8 (double constant589540). Missing target falls back to source eye basis at41b589. The adapter composes that same order using the shared navigation basis and `rf_weapon_target_aim`, with separately qualified full player/NPC identities. Generic muzzle and generic NPC target-aim functions remain unchanged.

The player target eye is published directly after actor_listener_pose, before cutscene/APC/staged camera overrides. Parent invalidates it before every camera choice, publishes only on foot, and the consumer requires the same frame, full player handle, pool epoch and unlinked live player. Mounting never consumes an older eye. Missing current raw eye means absent target/eye-basis fallback, never a cinematic camera as target.

Current-target mapping is explicit port glue: current scripted/opposed actor orders use combat_target; ordinary acquired combat uses the player because this engine's ordinary sight path need not populate combat_target. This is not an onset-point snapshot or homing. Invalid/dead/hidden/nonactor target identities use eye-basis fallback while retaining the paid release. Only living actor-directed onset is supported; point/once/vehicle-target script admission remains deferred. Zero-length body direction keeps the existing eye basis as a finite-input policy. Shared math is used; bit-identical original normalization is not claimed.

## Independent direct flight and contacts

The adapter directly calls `rf_weapon_flight_launch` from the exact placed muzzle and computed basis. It does not temporarily mutate NPC eye/look state or use Laser's eye-only preparation. Shared `scene_ai_energy_tick` is origin-independent and already carries immutable AP kind2/damage/source/weapon, authored radius and liquid query flags0x1004.

World, mover, detached-piece, vehicle, clutter, actor-body, physical-shield and Nano contacts retain shared nearest-hit/identity ordering. The selected physical shield consumes contact even when breaking/stale; shields keep the existing directional centerline policy while environmental/body sweeps use radius0.5. There is no rail-like world/actor penetration, radial damage, burn/acid status, crater, ricochet, gravity arc or class-based terrain bypass. Terrain stopping uses the real shared collider. No additional terrain/boss precondition was found in the exact owner/table/delayed-release scope; full original projectile hit processing is not newly reconstructed here.

Each pool has an epoch and its own presenting/servicing/updating guard. Terminal publication precedes contact callbacks. Save-only pending checks include pending windup, live flight and active service/presentation/update. Successful load/world/life publication clears transient state; failed preparation must preserve it. No RFNC/RFAP schema is changed.

## Parent integration checklist

1. Forward-declare campaign_enemy_spit_open, scene_ai_spit_reset, scene_ai_spit_cancel_source, scene_ai_spit_pending beside TriBeam. Open the exact profile with the other NPC profiles and real supply/reset catalogs.
2. Include scene_ai_spit_profile.inc after scene_ai_laser_profile.inc and before ordinary/opposed selection. Add campaign_enemy_spit_select(owner,...), preserving existing owners and exact inventory.
3. Include scene_ai_spit.inc after scene_ai_drone_secondary.inc (beside TriBeam). Service it alongside delayed attacks before per-owner mode/target/ammo/cadence early returns. Bypass generic ammo/fallback while scene_ai_spit_source_pending(handle).
4. Dispatch campaign_enemy_spit_ready before generic reserve-fed readiness. After ordinary range/aim/LOS admission, call scene_ai_spit_begin(i,intended_handle,frame,player_eye,&started), then continue. Exclude point_target and vehicle_victim as with TriBeam. This consumes the entire accepted-shot path; never also debit, advance cadence, present or hit via the generic tail.
5. In actor_follow_view, immediately before the camera-choice ternary, call scene_ai_spit_player_eye_invalidate(). Immediately after successful camera choice, and before apc_aim/cutscene overrides, publish scene_ai_spit_player_eye_publish(frame,position) only if !scene_turret_player_active() && !scene_driller_active(stream). This captures the actual actor_listener_pose return once without rerunning look.
6. Include scene_ai_spit_flight.inc after scene_ai_laser.inc and tick alongside TriBeam after current player-shield pose preparation.
7. Mirror every existing TriBeam pending save-only guard: scene_world_snapshot.inc, scene_player_checkpoint.inc save capture, scene_ai_projectile_checkpoint.inc save capture, scene_driller_checkpoint_live.inc save guard, and direct scene.c save paths. Do not add pending rejection to shared NPC-row load preparation.
8. Mirror successful TriBeam resets in scene/player/world/NPC checkpoint publication, world/life reset and teardown; retain storage-close/success conditions. Mirror pending-only source cancellation in explicit disarm, successful fallback/selection, death and unregister. Failed loads and unrelated pain/mode/target changes preserve pending shots.
9. Update open milestone and source status only after integration/review. These files were not compiled or executed.

## Presentation still outside this mechanics slice

SpitAttack.vfx is particle-only with two PART objects, not an admissible Laser/TriBeam mesh. No substitute mesh is supplied. The original flight PART consumer is now separately integrated after independent review; see SMALL-ROCK-SNAKE-SPIT-PART.md. Acid-splash Vclip, scorch and impact-sound rendering/playback remain separate consumers. Action motion and authored onset sound use the existing services; actual attack, presentation, exhaustion, current-pose release, hidden overdue behavior, contact ordering and save lifecycle remain runtime-unverified.

## Later source integration

Retained-contact impact audio is separately source-integrated in docs/SNAKE-SPIT-IMPACT-AUDIO.md. This supersedes any impact-audio exclusion above only for that bounded source slice; actual playback remains unverified.
