# Startup terminal observer: independent source review

Disposition: approved for parent integration, source-only, 2026-10-10 20:45 UTC.

The complete staged-file diff changes only the `SYMBOLS` mapping and adjacent comments in the existing original-startup admission helper. It adds six existing exported uint32 arrays, totaling 44 words / 176 bytes. No new runtime symbol, gameplay behavior, fixture, input, allocation policy or acceptance predicate is introduced.

Independently confirmed declarations:

- `rf_scene_late_load[8]`: `src/diagnostic/scene_late_load_diagnostic.inc:5`.
- `rf_scene_profile_stage[2]`: `src/diagnostic/scene.c:373`.
- `rf_scene_script_movement[8]`: `scene.c:5687`.
- `rf_scene_script_routes[8]`: `scene.c:18716`.
- `rf_scene_npc_idle_ground[6]`: `scene.c:18956`.
- `rf_scene_medic_syringe[12]`: `src/diagnostic/scene_medic_syringe_owner.inc:11`.

Each exact token appears once in the actual 19:00 tested linker map. The existing exact-address resolver refuses missing/ambiguous tokens; the existing word reader requires the requested word count. The native runner reads `extra_symbols` only after it observes terminal success/error and issues its existing stop. It persists raw terminal results before the ordinary acceptance checks. The additional mapping therefore uses an existing read-only terminal observation path; it does not add a live probe or pause/resume cycle during gameplay.

The 120-frame neutral recipe, original-asset pins, clean source and consumer XBE/map checks, stock 64 MiB assertion, session exclusion, snapshot-only base-HDD use, no-save behavior, packing/restoration logic, success predicates and failure exit are byte-unchanged. No previous failure is converted into a pass and no retry is added.

These are retained terminal diagnostics, not proof of an exact cause before the new run. `rf_scene_late_load[6]` preserves the profile stage at scene exit, which is useful alongside the live profile array. Some counters can reflect teardown. Syringe words 9–11 have declared parent/child/state labels but no baseline population; their zero values must not be presented as a captured live identity. The actual 19:00 RF_FORMAT failure remains unchanged evidence.

## Receipt

- Reviewed baseline commit: `65490a235229cafdff24c94c9a40ddf2bfefffa5`.
- Baseline helper SHA-256: `1ff37a9a6bea8089e9735289556f82783541831c474fa5b2ecfb9c82b4fe0b48`.
- Staged helper SHA-256: `79be1db965d4779f1c4f36778a0b38ec6f1a86012b728ed2dedf7228039c24ad`.
- Patch SHA-256: `129ce4b33e4edcb62344fe3f08767cb5608176f936db7b47b8f7497e2fed70f3`.
- Patch: `startup-observer.patch` in this directory.

No active-tree edit, syntax check, build, test, helper execution or emulator run was performed. Parent owns integration and the next requested batch.
