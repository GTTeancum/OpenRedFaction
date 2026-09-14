# Scripted item grants

Give_Item_To_Player now resolves named items.tbl weapon/ammo definitions and
applies the existing SP pickup grant to the shared inventory. Requests during
startup wait in a fixed32-entry queue (384 bytes) until starting/imported
inventory is ready, so frame0 initialization cannot erase the grant. Repeated
requests add allowed ammo without reacquiring an owned weapon. The live9870
regression grants Riot Stick ID2 with one loaded unit; a second request adds
one reserve unit. This does not equip Riot Stick: usable selection, melee,
first-person presentation and authored Strip_Player_Weapons remain critical
open work. Non-weapon item grants are unsupported. All37 PC tests and both
startup/repeated-grant replays pass. Original4bb690 creates a temporary item
through459100 before pickup dispatch; this practical path reuses decoded item
benefits without creating that object. Reproduce:python tools/replay_script_grants.py.
Evidence:artifacts/script-grants.
Stock64MiB XEMU replay `replay-20260914-124350` passes90 frames with exact
PC grant/inventory diagnostics:two requests, one acquisition, one loaded and
one reserve unit. NXDK build and restoration succeed.


The original type19 factory case calls constructor4be720 and installs vtable589a1c; its on action is4bb690. The owned event string is resolved against items.tbl by the port. Its declared weapon name, SP count and gives-weapon flag feed the existing inventory grant. Item definition scratch is bounded to128KiB and freed after each request; no item mesh or transient world actor is allocated. Missing/unsupported definitions remain explicit.

Native reproduction:

```powershell
python tools/xemu_replay_check.py artifacts/script-grants/inputs.bin --campaign-spawn --level L1S1.rfl --setup-uid 9870 9870 --seconds 180
```
