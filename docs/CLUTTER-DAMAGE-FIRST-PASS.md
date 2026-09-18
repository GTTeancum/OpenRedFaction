# Ordinary prop damage first pass

Player pistol, rifle, shotgun, conventional firearm and riot-stick hitscan now selects actual registered static prop model geometry alongside NPC/shield/vehicle candidates. The nearer candidate wins; world/mover and detached-terrain obstruction is checked before damage. Hidden and retired props are excluded, and the authored collide_weapon flag controls eligibility. Precision weapons and physical projectiles use separate paths and are not included yet.

Each loaded class has a56-byte damage profile read from clutter.tbl, preserving class-specific life, protection and eleven damage factors even when multiple classes share a model. Each owner has a16-byte generation/class binding. On creation health uses authored life; negative life gives protected100HP as in the existing class initializer. Damage uses the shared clutter receive logic, preserves the hit-event signal, and retires a destroyed prop once via flag2. Allocation remains registered until normal scene teardown; rendering, glow and model collision suppress it.

This is disappearance on destruction, not completed prop destruction presentation. Debris/corpse/explosion dispatch, source attribution, NPC/precision/projectile/blast damage, physics response and persistence remain open. Authored immutable-clutter checkpoint validation still refuses changed props rather than silently losing their state.

## Verification

`rf_scene_clutter_damage_live_tests Installed_Game/tables.vpp` passes actual lantern80HP metadata plus typed scaling, protected/hidden behavior, stale ownership, retirement and no-revival cases. PC and NXDK builds pass.

`tools/check_clutter_damage.py` prepares an isolated CTF06 lamp fixture with the original UID13025, pose and lanternbox.V3D mesh. Both180-frame PC runs use identical aim; only the fire run requests shots. Inspected baseline shows the lamp, while the fire result shows its disappearance. Logs show two actual40-damage hits,80→40→0, and exactly one retirement. Later shots no longer hit the retired prop. No debris claim is made. Retained profiles/bindings report24,136/8,096 bytes; prop-owner budget rises from288 to320KiB with those allocations accounted. Full-scene stock64MiB runtime headroom has not been remeasured for this change.

The failed NPC scripted-attack test used unsupported weaponID0 without a supported definition or finite ammo. A second pickup fixture omitted its catalog count. Both fixtures are corrected and the complete rf_npc_residency_tests executable now passes. These are test setup changes, not a replacement for live gameplay evidence.
