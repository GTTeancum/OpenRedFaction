# Drone white/yellow screen effect and NPC-first player contact

Written October 25, 2026. Source only: not compiled, not run on the console.
Everything below is unverified until the parent's stock64 console build.

RF.exe SHA-256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`,
Ghidra project `local/ghidra` (read-only headless decompilation). Tables read
from `Installed_Game/tables.vpp` (read-only).

## 1. White then yellow screen near a Drone

### Every original screen-flash writer

`416450(player, r, g, b, alpha)` is the only setter of the player flash
(`player+0x10d4` alpha, colour through `50cc40`). Its four callers:

| Caller | Colour | Trigger |
|---|---|---|
| `4a7520` | 255,0,0 alpha 0x80 | player damage (ours: `rf_scene_player_damage_flash`) |
| `414860` (at `414c5c`) | glare `$Light Color` | corona glare within 12 m, skipped when the glare has an owner at `+0x2cc` (muzzle glares) |
| `4c4b50` (at `4c4e02`) | 255,255,192, alpha `0x20-…` | projectile impact, **only** when weapon type is global `0087244c` = `shoulder cannon` (resolved in `4c6570`, `4c659f.c.txt:42`); also camera shake `40e0b0` |
| `47c280` | 255,255,255 alpha 1 | end-of-game camera path, not combat |

So no Drone attack writes the screen flash in the original. The Drone Smash
(`weapons.tbl`: bash 1600, `$Impact Vclips ("")`) has no visual. The
`DroneChainGun` muzzle glare has light colour 255,255,128 (yellow), but
`effects.tbl` states muzzle glares never flash the screen, `414860` skips
owned glares, and the port does not create NPC muzzle glares at all.

### What does fill the screen

`weapons.tbl` Drone Missile: `$Impact Vclips ("big_charge_explode")`,
`$Impact Vclips Radius (7.0)`. `vclip.tbl`: `big_charge_explode` is
`code_explode` with `$Explosion Name "shoulder mounted geomod"`, the same
explosion the Fusion `shoulder_cannon_explode` names. `explosion.tbl` gives it
three central emitters; scaled by radius 7:

- `explosion flare 2`: `explosionflare01.tga`, colour 255,255,255,255, glow,
  radius 1.3 (about 9 m), life 0.09 s: a white full-view flash.
- `explosion main part 2`: `Obj_EXP01.tga` fireball, glow, radius 0.6-0.8
  (about 4-6 m): the following yellow/orange flash.
- `explosion boom 2`: `JustSmoke.tga`.

A Drone missile striking at or next to the player therefore whites out and
then flashes yellow: the effect the user saw before dying. Our earlier flash
telemetry only watched the screen-flash overlay, which this never touches.

### Port difference and change

The port played the Rocket Launcher's `rocket hit` recipe for every NPC
missile, scaled to the Drone's radius 7 (`scene_ai_rocket.inc`). That is a
different emitter set (white flare, large flamethrower fire, smoke) at the
right size. Now:

- `scene_fusion_effects_open_clip` (in `scene_fusion_effects_resources.inc`)
  opens the explosion owner from any `code_explode` vclip and keeps its
  explosion name; the Fusion entry still passes `shoulder_cannon_explode`.
- New `scene_drone_missile_impact.inc`: when Drone Missile resources are
  demanded, open that owner from the Drone Missile's own `$Impact Vclips`
  entry, or confirm an already-open Fusion owner names the same explosion.
  Drone Missile impacts then start that recipe at the authored radius.
- If the open is refused, a `DRONE_MISSILE_IMPACT_ERROR` line is printed and
  the previous rocket recipe stays in use; the level still loads.

Memory: the owner's materials are capped at 512 KiB (same cap as Fusion
levels already use), loaded only in levels with a living Drone holding the
missile. No other allocation.

Not changed: Fusion's own `4c4b50` 255,255,192 flash and camera shake are
still missing for the player Fusion weapon (outside Drone scope). The
explosion's sparks and trail emitters are still dropped by the shared owner,
as for Fusion. Impact sound stays the shared rocket one (authored
`Small Explosion` is separate work).

## 2. Drone moving into the player (NPC-first contact)

Existing: `scene_npc_player_contact.inc` already finds the player as the
nearest contact for a Drone's full-3D sweep (`scene_drone_sphere_sweep`).
But the Drone pursuit step (`campaign_script_step`, `xyz_pursuit`) treated
that hit like a wall: it discarded the whole step and ended locomotion.

Original response, per the existing reconstructions:

- `49ffd2..4a007c` (`rf_physics_contact_advance`): advance to the hit
  fraction less a 0.05-unit separation margin.
- `49d7e0` dynamic route `49dcf6..49ddef` (`rf_physics_dynamic_contact`):
  for an NPC against the player (`49dd0b..49dd16`) zero impact, velocity and
  normal unchanged, no push on either body.

Re-sweeping the same direction meets the player again inside that margin,
so the Drone comes to rest just short of the player and carries on with its
pursuit. The new block in `campaign_script_step` does exactly that for a
Drone whose winning hit is the player body (no solid/room/face, player
handle): advance with `rf_physics_contact_advance`, commit that position,
keep the pursuit order. World hits, wall/ceiling stops, non-Drone NPCs, the
player-first direction and the shared admission/pair filters are unchanged.
Bodies with flag `0x4000` (a different original translation path) keep the
old stop. Counters: `rf_scene_drone_player_response[4]` (responses, last
fraction, last advanced ratio, zero-length advances).

## Unverified

- Compilation, console start and both behaviours.
- That the user's sighting was a missile impact, not another effect; the
  earlier reproduction died to the Smash with zero missiles launched.
- Original projectile-particle near-plane handling when the camera is inside
  a 9 m flare; the port uses its existing particle path.
- Whether the original actor loop re-sweeps along a changed direction after
  the zero-impact response (`49d7e0` callers were not re-decompiled here).
