# Weapon stripping and unarmed state

Strip_Player_Weapons now clears shared weapon ownership/ammunition, cancels
reload/fire state and enters an unarmed presentation. Startup strips use the
same ordered queue as item grants, so inventory initialization cannot undo them.
Firing is blocked and weapon vertices are hidden while the selected weapon is
unowned; movement and interactions remain available. A later supported weapon
grant/pickup automatically selects an owned slot. Handoff represents unarmed as
UINT32_MAX without indexing inventory at that value. Unarmed respawn stays
unarmed instead of restoring the diagnostic pistol. All37 PC tests pass,
including unarmed handoff validation. Controlled strip-only, rearm, backtracking
and respawn cases pass; rearm grants Riot Stick and permits its primary hit.
This practical policy clears all ammo too; full original strip/holster visual
sequencing and natural campaign traversal remain open. Evidence:artifacts/weapon-strip.
Reproduce:python tools/replay_weapon_strip.py.
Stock64MiB XEMU replay `replay-20260914-125923` passes240 frames and both
section transitions with exact PC state:unarmed ID, zero ammo/shots and zero
weapon vertices. NXDK build/restoration succeeds. All five PC strip cases,
including mid-play stripping, pass; the existing armed rifle respawn replay
also passes, preserving its prior ammo/reload behavior.


Authored L1S1 event8366 is type56. Its on action uses a borrowed runtime service; off has no stripping effect, and delayed requests use the common scheduler. Queued startup operations preserve their order relative to Give_Item_To_Player. Inventory clear reuses the fixed grant queue with an internal negative weapon tag; it is not an external weapon index. No additional meshes or persistent allocations are required.

The existing unarmed state is not a fist weapon or a full holstering implementation. Unsupported inventory-only weapons do not become usable through automatic selection.

Native reproduction:

```powershell
python tools/xemu_replay_check.py artifacts/weapon-strip/return.bin --campaign-spawn --level L1S1.rfl --actor-uid 8456 --setup-uid 8366 --exit-uid 9019 --return-exit-uid 9346 --seconds 300
```
