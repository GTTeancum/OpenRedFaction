# Ordinary empty-magazine automatic reload

Status: source-written, parent-integrated and independently source-reviewed
on 2026-10-09. Compilation and runtime verification await the scheduled
23:00 Xbox batch. No helper build, test, fixture, route, emulator, capture, commit or
cleanup was performed.

## Proven gap

The live conventional weapon path already reloads after a manual reload
request or an empty firing attempt. It completes through the real inventory's
`rf_weapon_reload_transfer`, with the existing authored duration, animation and
sound. Fusion and Flamethrower likewise have trigger-driven reload consumers.

However, conventional `campaign_combat_tick` previously required
`player_input.reload` or a `fire`/`alt` result to start a reload. Firing the final
round and releasing the trigger could leave an empty magazine indefinitely,
despite positive reserve. The original independently requests reload during
the ordinary entity update once the relevant weapon presentation is idle.

`rf_weapon_decide_empty` is separate: it considers exhausted loaded **plus**
reserve ammunition and may choose another weapon. It does not reload a
magazine and is not used by this adapter.

## Original evidence

Authority is the supplied read-only `Installed_Game/RF.exe`, version 1.20 NA.
The following addresses were inspected directly as x86 disassembly:

- `41e7e8..41e7ef`: entity update calls `421060`, then `419bc0` independently
  of a new player firing request.
- `419bc7..419bd8`: `4c86e0` requires a valid in-count current weapon with
  positive descriptor magazine at `+88`. Its actual predicate is
  `4c86e0..4c870b`.
- `419bed..419bfc`: positive loaded ammunition at entity `+32c[weapon]`
  skips the automatic request; nonpositive loaded reaches the empty path.
- `419c05..419c63`: the local-player path checks active first-person `fire`
  and `alt_fire` through `4ad8c0(player,0/1)`, and selected/queued
  `fire_fail` through `4a9520(player,2)`. Their names come from the action
  pointer table `5a0b00` and strings `5a0bb0/5a0bb8/5a0bc4`. Action 2 here
  is **fire_fail**, not reload. `4ad8c0` resolves the actual authored action
  resource and queries its current weight through `5033d0`.
- `419c54..419c63`: the weapon at global `87243c` has a presentation
  exception. It is the separately controlled Flamethrower and is outside
  this conventional subset. `4c6654..4c665e` resolves the string
  `flamethrower` at `5a335c` into that global.
- `419c65..419c77`: player pending selection `+f80` must be `-1`.
- `419c79..419ca8`: reset an active weapon effect if necessary, then call
  `425280(actor,1,0)`. The silent automatic request does not require a new
  trigger edge or consult the Autoswitch option.
- `425312..42532e`: the reload entry reads the current weapon's mapped
  reserve. The ordinary admitted path requires positive reserve and a
  magazine that is not already full.
- `425480..4254bb`: existing reload, death and linked-vehicle state reject
  starting another reload.
- `4254bc..425508`: stage the lesser of missing magazine space and reserve.
  `419d8c..419dde` later commits loaded/reserve at the existing deadline.
- `428e60..428e8a`: the existing holster predicate reads entity `+810` bit
  11 (`0x800`), with a linked-turret exemption. Turrets are already excluded
  by this adapter's actual controller owner, so the ordinary bit test is
  sufficient. The unrelated world-visibility `+7d0 & 0x200` is not used as
  a guessed first-person gate.

Neither the ordinary automatic-request path nor its local magazine/reserve
admission consults player `+f40/+f41`. Those profile bytes belong to automatic
replacement preferences, restored at `4a8bbc..4a8bd0`. No option value,
profile default, ammo grant or replacement ranking is introduced here.

## Read-only adapter

`src/diagnostic/scene_weapon_auto_reload.inc` provides:

`static uint32_t scene_weapon_auto_reload_ready(const scene_stream *stream,
uint32_t on_foot)`

This boolean helper reads existing owners only. It allocates no memory,
retains no state, performs no I/O and mutates no input, animation, sound,
inventory, selection, timer or save record. Invalid or unavailable inputs
decline the optional request rather than manufacturing an owner or supply.

The bounded subset admits Pistol, Assault Rifle, Shotgun, Rocket Launcher,
Sniper Rifle, Rail Gun, Machine Pistol, Heavy Machine Gun, Scoped Assault
Rifle and the independently selected Undercover handgun. It excludes Riot
Stick, Grenade, both Remote controls, Flamethrower, shield and Fusion. Their
existing manual/trigger-driven behavior remains intact.

The helper requires:

- A living, armed on-foot player, outside a cutscene or active undercover
  form, with no holster `+810 & 0x800`, active reload, queued burst or delay.
- An admitted selectable slot with its actual base ownership. Machine
  Pistol keeps its completed mode and reads that mode's loaded/mapped reserve;
  no separate Special ownership is invented. Pending Machine Pistol or
  Undercover mode transitions decline the request.
- Positive authored magazine and capacity, loaded count exactly zero,
  and actual mapped reserve in `(0,capacity]`. Negative/inconsistent port
  inventory is rejected rather than following the original's unchecked
  nonpositive branch. The retained firing definition must agree on magazine
  size, and existing reload ticks must be positive and bounded.
- The matching first-person owner must already be initialized and in its
  real idle clip, with a loaded reload clip. The scene's view slot must match
  current selection and its shot counter must reflect the last actual shot.
  An invalidated or newly selected view therefore first reaches the ordinary
  draw/idle boundary. Relocation itself adds no new reload/animation reset.

`rf_player_weapon_step` returns a completed action to idle; existing continuous
fire presentation returns to idle on release/depletion. This adapter observes
that transition. It does not advance the pose twice, impose a guessed wait,
derive a fire duration from cooldown or reset the cooldown. The existing
trigger step continues aging cooldown even when reload inhibition is active.

## Parent integration contract

1. Include `scene_weapon_auto_reload.inc` after `scene_vehicle_equipment.inc`
   and before `campaign_combat_tick`. No forward declaration, new build-list
   entry, lifecycle reset, persistent field or save-format change is needed.
2. Add one frame-local `uint32_t auto_reload=0` to `campaign_combat_tick`.
3. After special-controller, pending-mode, unarmed and ownership returns,
   and before constructing conventional trigger rules, evaluate
   `auto_reload=scene_weapon_auto_reload_ready(stream,on_foot)` exactly once.
4. In the conventional trigger-inhibit expression, replace the manual-only
   `player_input.reload` test with `(player_input.reload || auto_reload)`,
   retaining the current magazine/reserve conditions.
5. In the existing manual reload-start branch, make the same replacement.
   Keep its authored timer assignment, audio selection, return and the
   existing completion/transfer path unchanged. Do not synthesize a physical
   reload input or alter the later empty-attempt branch.

The helper's idle/pending-work gates ensure the new request cannot cancel an
unspent burst. Starting automatic reload through the shared inhibition path
consumes a held trigger with the same established semi-automatic edge policy
as a manual reload. A continuous weapon resumes only through its existing
post-reload trigger logic.

## Deliberate boundaries

This is a source-grounded first-playable adapter, not the complete retail
reload/presentation dispatcher. The port does not currently retain a separate
first-person `fire_fail` action queue. Requiring the actual idle owner is a
conservative projection of the represented actions; no fake action or timer
is introduced. Newly added draw/holster or fire-fail actions must join this
gate before being allowed to overlap automatic reload.

The Riot Stick's held charge and the Flamethrower's original presentation
exception need their own controller review. Active forms and custom transition
actions are deliberately outside this new automatic path. Source-exact NPC
reload allocation, multiplayer branches, first-person blend equivalence and
broader saved animation fidelity are separate. The parent scheduled build can
establish compilation only; natural final-shot/release/reload and cancellation
runtime remain unverified until actually observed.
