# Ordinary NPC projectile combat

NPCs now request rocket/grenade definitions, world models, flights and impact effects from their living held/owned loadout before scene resources open. These requests do not grant player weapons or load extra first-person views. Known retired actors do not request combat resources; hidden living actors retain resources for script reveal. Existing fixed eight-flight pools, finite NPC ammunition, source-aware damage and updates after thrower death remain shared.

The former DEV-only attack gates now require prepared projectile resources. Authored per-instance weapon overrides are also parsed and applied after class defaults; see ENTITY-INSTANCE-WEAPONS.md for binary evidence and installed examples. This enables the actual level-data path rather than requiring a DEV gun assignment.

## Focused verification

- Level parser checks cover both weapon strings, empty/none, retained ownership and truncated records.
- Loadout checks cover preserving default alternatives, unknown/empty inheritance, category clearing, primary reserve fill, the Rocket Launcher three-reserve exception and secondary acquisition.
- Actual scene scheduler checks run with DEV disabled, reject missing resources without consuming ammunition, then launch real finite flights once resources are ready. Existing collision/source checks pass.
- Demand checks cover names independent of scene slot IDs, held ownership, empty ammo, hidden/dead/retired actors and no player inventory/view-mask mutation.
- PC replay tool: `python tools/check_ai_projectile_ordinary.py --run`. This creates a controlled encounter on original L1S1 geometry with guard8456, player9858 and per-instance projectile overrides; other actors/events are removed. Original tables and other assets remain read-only hardlinks. It is a synthetic encounter, not a claim of campaign progression.
- Both 650-frame PC replays passed: rockets `[3,3,0,0,0]`; grenades `[4,37,2,2,0]`. Neutral player never fires. Both final captures inspected: rocket encounter ends in player death; grenade encounter shows the throwing guard and player damage.

- Native grenade encounter `artifacts/xemu/render-20260922-180307`: PASS650frames, identical AI grenade `[4,37,2,2,0]`, blast and enemy combat counters,5213freepages (20.36MiB), stock64MiB. Native framebuffer inspected: guard mid-throw, level and player weapon visible. Original disc restored and emulator closed. Ordinary rocket native encounter remains unrun; prior DEV rocket coverage and current ordinary PC/focused checks apply.
- A subsequent source guard preserves model-less NPC animation startup when applying overrides; final PC/NXDK builds cover that guard, but it was added after the native test build. No additional native replay was run for that unaffected encounter path.

## Remaining first-pass work

Full campaign integration, secondary-weapon AI policy, general persistence of in-flight projectiles and authored grenade release timing remain open. Aggregate live logs do not prove exact remaining NPC rounds or exhaustion; finite debit is verified by focused implementation tests. Audio dispatch has not been auditioned. Broader encounters and presentation are deferred to integration/testing feedback.
