# Original HUD live composition

Source-written for the parent 20:00 batch on 2026-10-08. No helper builds,
tests, screenshots or image viewing. Runtime/art admission is unverified.

Dependencies: the asset worker's rf/hud_assets.h + core implementation and
the renderer worker's dedicated screen-space textured HUD sink. The existing
rf_scene_particle_sink signature is unchanged.

## Live state

HUD rendering now reads inventory.loaded/reserve and the selected definition
directly, including machine-pistol special selection. It never calls the
mutating campaign_ammo_publish routine. Numeric counts replace per-round bars
whose width became negative beyond66 rounds. No-clip items show their actual
reserve count; shield/unarmed states do not show a misleading magazine.
Mounted stationary HEAP/VAUSS turrets show their actual unlimited firing policy
instead of the holstered handheld inventory. Player health/armor numerals read
the damage owner; vehicle meters continue reading the selected chassis owner.

## Original resources and layout

The scene opens the bounded optional atlas once using ui.vpp, tables.vpp and
existing map archives. Temporary archives close immediately after admission.
The scene owns the atlas through every synchronous frame sink and closes it
only after rendering finishes. All admission failures leave the new numeric
procedural HUD available; they do not terminate gameplay.

hud.tbl's #640x480 rows0..18 position the health/suit sprites, smallfont health
and armor numerals, ammunition panels/signals/icons and bigfont ammo numerals.
Health frame follows clamp(int(health*.1),0,10); armor follows
clamp(int(armor/class_armor*10),0,10), with a guarded zero maximum. Original
logical sprite sizes and atlas UV subrectangles are used. Numeric strings are
centered within authored rectangles, scaling down only if they exceed bounds.

Reticles are centered from actual logical bitmap dimensions, following
43a41b rather than the stale hud.tbl reticle row. Sniper uses its scope reticle
while unzoomed and the generic reticle while zoomed; rockets/fusion and APCs
select their retained families. No reticle is emitted for unarmed, melee,
grenade, detonator, shield or dead-player presentation. Existing hit/surface
tints remain. A full sniper mask and lock-on animation are not invented.

Damage flash, subtitle/cutscene gates, scanner, death, blackout and endgame
ordering/lifetimes remain. Save/pickup/countdown notices move away from the new
top-corner panels only when original assets are admitted. Ordinary messages
retain their procedural font; numeric UI uses the recovered specialized fonts.
Vehicle labels remain the existing first-playable GUN/ROCKET names.

## Admission proof still required

rf_hud_assets_diagnostic[8] is exported by the scene:
0 attempts;1 signed open status bits;2 admitted owner;3 resident bytes;
4 peak bytes;5 successfully drawn original-art frames;6 fallback frames;
7 last original draw status bits. Admission/byte fields deliberately retain the
last scene result after teardown for the native harness. HUD_ASSETS logs the
open status and byte counts once. Require admitted=1, status=0, art frames>0,
fallback frames=0 and draw status=0 before claiming original art ran.

All new imagery/font memory is supplied by the bounded asset owner. No per-frame
allocation, archive read, inventory/vitals mutation or extra simulation step is
introduced. Stock64MiB admission, draw submission and actual presented FPS
remain the parent's 20:00 verification responsibility.
