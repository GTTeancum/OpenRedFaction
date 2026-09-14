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
grants use this correction. General world pickup support beyond the currently
supported pistol/rifle items remains open. HUD displays charge and spare cells.
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
