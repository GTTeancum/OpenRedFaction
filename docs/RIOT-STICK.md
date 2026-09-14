# Riot Stick first pass

Riot Stick now occupies playable slot2 after pistol/rifle. Weapon cycling skips
unowned slots; ammo publication, reload ownership and campaign handoff use the
selected weapon ID instead of a pistol/rifle boolean. Authored primary settings
supply60 bash damage and1-second cadence. Primary strikes do not consume charge;
manual/automatic reload is bypassed for this first-pass melee action. A2.6-unit
segment against existing NPC bounds and world/mover obstruction limits reach;
this borrows the authored AI attack range, not verified retail player reach.
The named first-person mesh/draw/fire clips and Riot Attack sound are connected.
All37 PC tests pass; controlled grant/cycle/approach replays prove a near hit,
a withdrawn miss and unchanged one-unit charge. First-person attack vertices
render; fitted camera placement, precise delayed impact, melee hit volume,
impact effects and electrical alternate fire remain open. No GitHub image upload.
Evidence:artifacts/riot-stick. Reproduce:python tools/replay_riot_stick.py.
Stock64MiB XEMU replay `replay-20260914-125149` passes150 frames of the
close-strike case with exact PC combat, charge and weapon-animation state.
NXDK build/restoration succeeds. A PC L1S1->L1S2->L1S1 replay retains selected
Riot Stick ID2 and its charge. All seven existing pistol/rifle selection/reload/
respawn replay cases pass; their old audio assertions now include measured
ENEMY_FIRE sound requests alongside player shots/reloads. The prior failure
was3 player shots plus1 enemy sound compared against3 total sounds.


Each of the three playable weapons loads through the existing per-weapon1MiB resource budget. The PC Riot Stick replay reports1,015,728 resident bytes and1,027,324 peak bytes for its model owner. This is additional residency alongside pistol/rifle, not a whole-game memory figure.

The existing in-place respawn preserves Riot Stick ownership when restoring starting ammunition. Section handoff accepts its catalog ID while keeping the same catalog-hash validation. Inventory-only grants for other weapons still do not make those weapons selectable.

Native reproduction:

```powershell
python tools/xemu_replay_check.py artifacts/riot-stick/near.bin --campaign-spawn --level L1S1.rfl --actor-uid 8456 --setup-uid 9870 --seconds 240
```
