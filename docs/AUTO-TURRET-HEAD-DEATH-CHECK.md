# Generated head firearm death check

Prepared for parent-owned serial Xbox execution, not yet run. The existing static turret fixture resolves authored UID rows, while the generated head has UID UINT_MAX and no RFL record. Authored event links therefore cannot name the head. This check adds an isolated diagnostic adapter resolving the actual generated key `(base1994, role1)`; it never creates a fake head or writes health.

At frame60 onward, one handgun-valued shot per frame uses the retained head model bound center and up to six axis-aligned candidate rays. Each candidate goes through `scene_driller_firearm_select` and normal level cover; only the first actual unobstructed head contact reaches `scene_driller_projectile_damage`. Failure to find a collision is reported rather than bypassed. Native input/ammo/cadence are outside this narrowly staged service check. Normal activation and head gunfire occur beforehand; neither AI action nor immunity is changed.

## Parent wiring

1. Include `scene_turret_generated_fixture.inc` immediately after `scene_turret_fixture.inc`. Call `scene_turret_generated_fixture(stream,frame)` beside the existing static fixture call and propagate errors.
2. Expose `extern uint32_t rf_scene_turret_generated_test_uid;` in the existing scene API used by Xbox main. Read optional `D:\\campaign-auto-head-test.bin` as exactly one nonzero uint32, using the existing campaign-turret-test.bin parser pattern. Default0 disables the adapter. Add that flag name to the native harness control list so unrelated checks isolate it.
3. Reset `rf_scene_turret_generated_damage_probe[10]` beside `rf_scene_turret_generated_death_probe` at new-level setup. The adapter now records the actual head-to-base request around `rf_scene_npc_damage`: count,target,amount,source,kind,health-before,health-after,applied,status,death-entry count. No request semantics changed.
4. Run `python tools/xemu_auto_turret_head_death.py` once after compiling. The harness writes only the opt-in flag and reuses the original base-only fixture, then restores controls/disc in finally.

## Expected evidence

The120-frame run samples after frame90. Real head contacts must reduce initial60 health to death, with one head-origin coupled dispatch and zero base-origin dispatches. The base begins at80 health and must receive exactly one actual1000 damage request with source-1/kind-1, successfully entering ordinary NPC death once. Head registration identity, detached link, death-effect transition count and both terminal probes must remain stable between the postdeath sample and final result. Head shots must stop at death; there must be no Slay, Remove_Object, duplicate handheld attacks, synthetic static fixture, recursion or errors. Stock64MiB must retain free pages and the bounded encounter must keep the player alive.

The head-hit probe is20 words: baseUID,initialized,basehandle,headhandle,rays,hits,covered,initialhead,initialbase,finalhead,finalbase,headdead,shotsatdeath,deathframe,errors,finalframe,damagekind,weaponamount,appliedsum,lastcontacthandle. No images, audio verification, desktop input, PC runtime or campaign route is used.

## Xbox result

The parent integrated the fixture and stock-64-MiB XEMU passed `artifacts/xemu/auto-turret-head-death-20260930-145229/report.json`: direct firearm contacts killed the actual generated head, the base received one 1000-damage coupled request, death/effect dispatch occurred once, and both stopped firing. Free pages: 3514. The check stages diagnostic rays; ordinary player aim and input remain outside this result.
