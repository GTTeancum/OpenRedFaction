# Authored Drone Missile shared projectile

Written October 10, 2026 after the 02:00 Xbox batch. This is source-only
implementation awaiting parent integration and the next consolidated batch;
no build, syntax check, gameplay fixture or runtime claim accompanies it.

## Original evidence and scope

Original levels1.vpp/L3S4.rfl UID844 selects Drone Smash / Drone Missile.
Installed tables.vpp/weapons.tbl:2599–2628 declares the exact secondary:

- DrillMissile01.VFX, shared with Tankbot Missile
- flickers/thruster flags0x18; undeviating/no_fire_through flags2=0x28
- speed20, lifetime5 seconds and collision radius0.15
- explosive damage15, damage radius2 and crater radius5
- fire wait4 seconds and AI range30
- capacity12 of finite `15cm rocket` reserve, with no magazine
- homing false, sticky false and glow false
- missile_loop.wav with near10/gain0.9

Original406330/406390–406543 supplies the already-reconstructed autonomous
secondary admission boundary, `rf_weapon_ai_secondary_admit`. The separate
Drone secondary owner consumes that gate; this projectile slice does not
invent autonomous targeting or use a player request as evidence of AI fire.

## Resource and launch admission

`scene_ai_projectile_resource_mask` now accepts Tankbot and Drone demand
outputs. A live authored owner selecting and owning Drone Missile admits
shared rocket resources even without a player rocket or Tankbot. Retired,
dead or unregistered owners remain excluded. Hidden live owners remain
eligible because an event can reveal them; empty ammo does not suppress
resource admission. No weapon, ammunition or first-person view is granted.

`scene_ai_drone_missile_resources_open` reads one bounded weapons.tbl scratch
buffer at startup. Existing explosive/primary parsers supply numeric values;
a small exact-profile reader also checks the otherwise-unretained ammo name,
projectile filename, no-homing and no-sticky declarations. Catalog identity,
flags, capacity, clipless shape and authored numeric values must match before
publishing the two definitions. There are no per-shot archive reads.

`scene_ai_drone_missile_launch(owner,target_eye)` requires the actual selected,
owned secondary, its mapped positive reserve, capacity12 and no loaded clip.
It uses the existing eight-slot NPC missile pool, swept world/body/liquid
collision, source attribution, damage/death, explosion and terrain owners.
It returns RF_OK only after one real reserve round is consumed and one flight
is published. Exhaustion, full pool or unavailable ownership returns
RF_NOT_FOUND without publishing a flight or debiting ammo. Invalid shared
launch inputs retain their original error status. Shared refusal telemetry
may still advance. No replacement inventory, reload, generic hitscan or grant is
used. The secondary owner controls its independent four-second deadline;
the shared primary combat_due and pending Smash are unchanged.

Each pooled missile now retains its actual weapon ID. Nano Shield contact
receives that ID instead of classifying every non-Tankbot flight as Rocket
Launcher. Existing RFAP Rocket/Tankbot restore reconstructs the matching ID
from its existing wire variant; the RFAP byte layout is unchanged.

## First-pass boundaries

Launch uses the existing shared eye-origin fallback and current target eye,
not a newly inferred muzzle attachment or prediction. The authored Drone
missile is non-homing, so its accepted direction remains fixed. Full retail
muzzle placement, authored trail and impact presentation parity remain
separate work. Shared impact/damage consumers are retained.

The later optional Drone audio adapter reuses the existing generation-owned
flight slot and resident PCM while copying the authored near10 profile into
its voice. Rocket retains near6; no shared bank parameter is rewritten. Drone
launch uses an independently admitted bounded one-shot sample. See
`DRONE-MISSILE-AUDIO.md`; compilation and runtime remain pending.

## Save and lifecycle boundaries

RFAP1–4 have only Rocket and Tankbot NPC missile variants. The save-only
`scene_ai_drone_missile_save_blocked` guard reports active Drone flights;
projectile capture refuses before touching the output rather than relabeling
or dropping them. Parent save/export gates use the same guard alongside the
separate secondary cooldown guard. No new save format is introduced.

Load preparation has no live-flight guard and does not clear the existing
pool. Successful existing pool replacement/reset retires the replaced
timeline through its normal owner. Failed saves and failed preparation do
not cancel a flight. Fired missiles retain normal shared projectile lifetime
semantics rather than being recalled merely because their source changes
weapon, order or visibility.

## Parent integration contract

1. Add `scene_drone_missile_resources`, `campaign_drone_missile` and
   `campaign_drone_missile_primary` beside the existing Tankbot globals.
2. Clear the Drone resource flag with the other projectile demand flags.
3. Call the demand helper with both resource-flag pointers; its bit4 uses the
   existing rocket model/effects load and render path, including DrillMissile01.
4. When Drone demand is present, call
   `scene_ai_drone_missile_resources_open(&tables)` after shared definitions
   and Tankbot admission. This replaces separate raw Drone definition loads.
5. Declare the launcher before the separate Drone secondary owner include.
   Declare the active-flight guard before early save/export callers and apply
   it only to save/export boundaries, never shared load-preflight row capture.

Only the three projectile includes and this document are owned by this slice.
No original inputs, grants, routes, assets, tests, emulator sessions, cleanup
or Git publication were changed or run. Xbox integration and behavior remain
unverified until the parent-owned consolidated batch.

Successful timeline publication also retires only old Drone flights. This
closes empty-RFAP and composed-player loads where a frame-zero launch could
otherwise survive while its unsaved secondary cooldown is reset. Failed
preparation is unchanged; Rocket/Tankbot entries are not cleared by this helper.
