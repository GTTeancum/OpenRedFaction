# Ordinary prop damage first pass

Player pistol, rifle, shotgun, conventional firearm and riot-stick hitscan now selects actual registered static prop model geometry alongside NPC/shield/vehicle candidates. The nearer candidate wins; world/mover and detached-terrain obstruction is checked before damage. Hidden and retired props are excluded, and the authored collide_weapon flag controls eligibility. Precision weapons now use their dedicated prop adapter described below; shared projectile prop contacts are now wired as described below.

Each loaded class has a76-byte damage/effect profile read from clutter.tbl, preserving class-specific life, protection and eleven damage factors even when multiple classes share a model. Each owner has a20-byte generation/class binding with a one-shot pending break flag. On creation health uses authored life; negative life gives protected100HP as in the existing class initializer. Damage uses the shared clutter receive logic, preserves the hit-event signal, and retires a destroyed prop once via flag2. Allocation remains registered until normal scene teardown; rendering, glow and model collision suppress it.

Destruction now has a first authored central-effect pass for yellboom lamps, described below; broader prop destruction presentation remains incomplete. Debris/corpse/explosion dispatch, source attribution, remaining special-weapon damage paths, physics response and persistence remain open. Authored immutable-clutter checkpoint validation still refuses changed props rather than silently losing their state.

## Verification

`rf_scene_clutter_damage_live_tests Installed_Game/tables.vpp` passes actual lantern80HP metadata plus typed scaling, protected/hidden behavior, stale ownership, retirement and no-revival cases. PC and NXDK builds pass.

`tools/check_clutter_damage.py` prepares an isolated CTF06 lamp fixture with the original UID13025, pose and lanternbox.V3D mesh. Both180-frame PC runs use identical aim; only the fire run requests shots. Inspected baseline shows the lamp, while the fire result shows its disappearance. Logs show two actual40-damage hits,80→40→0, and exactly one retirement. Later shots no longer hit the retired prop. No debris claim is made. Retained profiles/bindings report24,136/8,096 bytes; prop-owner budget rises from288 to320KiB with those allocations accounted. Stock64MiB native replay now completes180frames with the exact eight PC damage counters (4 queries,2 contacts,2 applications,1 retirement); its framebuffer confirms the lamp is gone. The run render-20260918-172650 stopped parity validation at SWITCH_DETAIL because clutter model pointers were incorrectly reported as type tags. That diagnostic is corrected to report stable kind4 and actual flags; this run is not claimed as an overall harness pass. Disc restoration passed.

The failed NPC scripted-attack test used unsupported weaponID0 without a supported definition or finite ammo. A second pickup fixture omitted its catalog count. Both fixtures are corrected and the complete rf_npc_residency_tests executable now passes. These are test setup changes, not a replacement for live gameplay evidence.

## Shared combat integration

Ordinary enemy hitscan and shotgun pellets now select a nearer prop before intended actor/shield/vehicle damage, preserving ties and checking world/rubble cover. The existing rail penetration policy remains explicit. Shared explosion dispatch now scans live ordinary props with authored center-distance falloff and CF5 world/mover cover before calling the same typed damage service. Hidden/dead objects are skipped; damage application revalidates registry generation. No additional retained allocations are introduced.

Focused enemy-fire ordering and blast falloff/cover tests pass, and the integrated PC build passes. These focused checks do not establish live NPC encounter or explosion presentation correctness; a real blast fixture is being prepared. Prop destruction still retires the object without debris or chain explosions.

## Precision weapons

Player sniper selection now merges exact prop-model contacts with the nearest actor, shield and vehicle. Detached fragments retain precedence at an equal distance, then world/mover cover is checked before typed prop damage. Rail visits each intersected registered prop once and continues along its existing authored piercing path, without consuming a new per-prop allocation or repeatedly damaging the same surviving prop. Hidden, retired and stale owners are excluded.

The integrated PC and NXDK builds pass, and the complete NPC residency regression executable passes. Live sniper/rail shots against the prop fixture remain unverified; builds and the NPC regression suite are not evidence of those visual outcomes. Material-specific sniper piercing remains a separate fidelity item.

## Flying projectiles

Shared player rocket, source-aware NPC projectile and vehicle-round sweeps now compose finite-radius contacts against retained prop meshes. Expanded bounds are only broadphase; actual model triangles decide contact. Local mesh contacts transform back to world space, preserve nearer world/liquid/actor/vehicle candidates and carry the prop generation separately from the slot tag. Direct damage is connected for player/NPC rockets and vehicle primary rounds. Grenades sharing the NPC sweep gain prop contact; their existing radial damage remains separate. Other specialized projectile adapters require coverage review.

The real scene/model collision test passes a quarter-unit projectile striking a plane at worldZ5 at fraction0.475, validates world contact/normal, nearer cover, hidden/source exclusion and stale damage suppression. PC and NXDK builds pass. Live projectile impacts and break effects against authored props remain unverified; this test is geometry and ownership evidence, not a gameplay screenshot claim.

## Lamp break presentation

The three current CTF06 lamp classes use authored yellboom, which references the rocket hit central explosion and light break Foley. Their parsed radius and transformed offset now drive a deferred one-shot health-break effect; ordinary positive-health GeoMod retirement does not fabricate it. The shared class timer enforces50ms cooldown. Eight effect instances reuse bounded particle/emitter pools; four textures require102,704 bytes under a128KiB material ceiling. Profiles/bindings now retain32,756/10,120 bytes for this level. Resource loading occurs only when a placed prop uses this effect.

PC replay breaks UID13025 at frame72 after80→40→0 health. The inspected frame73 image shows the authored flame burst at the removed lamp; frame76 and frame89 show it fading away. The effect is deliberately not a claim of complete break presentation: the vclip's60 sparks, genericlight debris, other vclip classes, corpse substitution and chained explosion damage remain open. Audio is dispatched but has not been auditioned. Native run render-20260918-204953 passes all harness comparisons at74frames on stock64MiB, with4,185 free pages (16.35MiB). The inspected Xbox framebuffer shows the same authored flame burst, exact damage/profile/binding counters match PC, and disc restoration passes. The earlier SWITCH_DETAIL diagnostic fix is also verified by this pass.
