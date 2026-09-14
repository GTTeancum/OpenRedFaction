# Riot Stick first pass

The opening grant equips the baton after the authored weapon strip. RT (PC F)
bashes; LT (PC G) holds electrical alternate fire. Primary wins if both are held.
Primary uses authored60 damage and1-second cadence without consuming charge.
Held alternate fire drains a100-unit cell over150 active simulation ticks
(2.5seconds), including firing into empty space. Release stops the held animation
and preserves fractional drain, so repeatedly tapping cannot provide free charge.
Y/R replaces a partial battery when a spare is available; an empty battery starts
automatic reload on alternate fire. A depleted baton still permits primary bash.
The authored reload lasts2.7seconds and discards the old charge at1.3seconds.

The weapon item counts batteries: its Count1 now grants100 charge units. A repeat
grants100 reserve; ammunition-only Count100 items are not multiplied. Scripted
grants use this correction. World baton and battery pickup support now uses the same charge conversion;
other unsupported weapon pickups remain open. HUD displays charge and spare cells.
Fresh ammunition reset/respawn and reload clear fractional drain.

This is practical shared gameplay, not a completed retail firing dispatcher.
The held damage interpretation is120 damage per second, electrical type6;
precise original damage cadence and stun behavior remain unverified. The2.6-unit
reach borrows the authored AI range. Existing NPC bounds and world/mover rays
block attacks through surfaces. Contact plays impact sounds and flashes the
reticle; world contact uses an amber flash, electrical NPC contact blue.
Material-specific sound selection, an owned continuous sound with release stop,
electrical bolt/spark effects, exact delayed primary impact, retail hit volume,
fitted weapon/camera placement and final visual parity remain open.

The authored fp_riot_attack_taserB.rfa clip loops during held fire and returns to
idle on release. Loop states now enter through rf_motion_set_weight, correcting
previously inactive idle animation too. Riot Stick uses four clips, with1,022,928
resident and1,034,524 peak owner bytes inside its existing1MiB limit. Pistol and
rifle retain three clips; unused rifle alternate assets are not loaded. These
figures cover the weapon owner, not whole-game memory.

Evidence is the original tables.vpp weapons.tbl and items.tbl plus motions.vpp;
no installed assets are modified. Authored values: clip100, reserve900, drain2.5,
reload2.7, zero-drain1.3, primarywait1, damage60, altwait0.5 and altdamage120.

Validation (September14): focused player_weapon_resources passes, including
intermittent charge conservation, empty/invalid inputs, held/released animation
and the1MiB resource ceiling. tools/replay_riot_alternate.py passes five PC cases:
near held contact, release, exhaustion plus empty bash, battery replacement and
simultaneous buttons. Primary near/far and repeated-grant checks pass separately.
This is focused gameplay coverage, not a complete campaign acceptance run.

Stock64MiB XEMU run artifacts/xemu/render-20260914-161001 passes180 ticks with
exact selected PC body, combat, ammo, baton, animation and audio diagnostics.
Native framebuffer inspected;5763 free pages (22.51MiB). Reproduce:

```powershell
python tools/replay_riot_alternate.py
python tools/xemu_render_check.py --input artifacts/riot-alternate/held_near.bin --actor 8456 --setup-uid 9870 --seconds 240
```

Fractional charge below one unit is not serialized across section handoff;
integer charge and inventory use the existing campaign state. Natural campaign
traversal and additional weapons remain work to complete.

Stock64MiB XEMU run artifacts/xemu/render-20260914-161246 also passes360
ticks of release/reload with two scripted batteries and exact selected PC state.
Pistol/rifle short checks cover burst fire, held cycling, switching back, burst
cancellation, no-reserve reload and cycling without rifle ownership. Two old
expectations were corrected for the already-implemented opening inventory strip:
automatic baton selection and no surviving125-round starting pistol reserve.

## World pickups

Authored Riot Stick9463 in L1S1 now renders through the existing static-item
owner, grants100 charge on first collection and100 reserve if already owned.
It uses the same obstruction, contact distance, inventory capacity and persistent
retirement logic as the other pickups. The catalog now recognizes eight classes;
resources are loaded only for classes present in the section, within the existing
2MiB per-class model/material budget. The battery mesh/material check fits that
budget. The original items table declares a separate battery count100 in charge
units; it is not multiplied by100 again.

The63 installed SP sections contain baton placements9463 (L1S1),5707 (L2S2a)
and21203 (L17S1); no standalone riot_stick_battery placement was found. Its table,
mesh and materials are verified, but natural world battery collection remains
unverified. No original asset or level was edited to manufacture a placement.

PC tools/replay_riot_pickups.py passes four focused cases: visible uncollected
model, initial acquisition, duplicate-weapon ammunition and retired-item revisit.
The return case deliberately checks retirement separately from player inventory:
L1S1 currently repeats its startup strip on revisit and removes retained weapons.
This is a campaign event-persistence bug, recorded in TO-DO.MD, not successful
inventory preservation. Process-local placement and authored exit dispatch do
not prove the walking route or end-to-end campaign.

Stock64MiB native run artifacts/xemu/render-20260914-162114 passes150 ticks
of authored pickup9463 with exact PC pickup, ammo, combat, animation and body
diagnostics. Native framebuffer inspected;5777 available pages (22.57MiB).
The manual emulator remains on the previous tested combat build and is untouched.

```powershell
python tools/replay_riot_pickups.py
python tools/xemu_render_check.py --input artifacts/riot-pickups/collect.bin --item-uid 9463 --seconds 240
```
