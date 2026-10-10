# Ordinary, Fusion and Flamethrower empty-magazine automatic reload

The original conventional adapter passed the October 9, 2026 23:00 UTC Xbox
build and bounded original L3S1 ordinary sixteen-shot autonomous reload check.
That result does not establish every weapon or later extension.

The Fusion extension was integrated and independently source-reviewed on
2026-10-10 against `1d8f71b0`. Compilation is reserved for the scheduled
06:00 Xbox batch; its action runtime remains unverified. No extra build,
test, syntax check, fixture, route, emulator run or original-input edit was
performed for this extension.

The Flamethrower extension was integrated and independently source-reviewed on
2026-10-10 against `b3d5bcb8`. Compilation is reserved for the scheduled
06:00 Xbox batch; action runtime remains unverified.
No build, test, syntax check, fixture, route, emulator run or original-input
edit was performed for this extension.

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
  exception. It is the separately controlled Flamethrower;
  `4c6654..4c665e` resolves the string `flamethrower` at `5a335c` into that
  global. This outer exception permits the active reset before requesting
  reload; it does not bypass the reload entry's separate presentation checks.
- `419c65..419c77`: player pending selection `+f80` must be `-1`.
- `419c79..419ca8`: reset an active weapon effect if necessary, then call
  `425280(actor,1,0)`. The silent automatic request does not require a new
  trigger edge or consult the Autoswitch option.
- `425312..42532e`: the reload entry reads the current weapon's mapped
  reserve. The ordinary admitted path requires positive reserve and a
  magazine that is not already full.
- `42541f..42547e`: the reload entry independently rejects active `fire`,
  `alt_fire` and selected/queued `fire_fail`, without a Flamethrower exception.
  The outer reset/request sequence is not proof of immediate reload acceptance
  while the fire presentation remains active.
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
Rifle, the independently selected Undercover handgun, Fusion and Flamethrower.
Fusion and Flamethrower also require their actual `scene_fusion_resources` and
`scene_flame_resources` owners respectively. It excludes Riot Stick, Grenade,
both Remote controls and shield. Their
existing manual/trigger-driven behavior remains intact.

The helper requires:

- A living, armed on-foot player, outside a cutscene or active undercover
  form, with no holster `+810 & 0x800`, active reload, queued burst or delay.
- An admitted selectable slot with its actual base ownership. Machine
  Pistol keeps its completed mode and reads that mode's loaded/mapped reserve;
  no separate Special ownership is invented. Pending Machine Pistol or
  Undercover mode transitions decline the request. Flamethrower also declines
  underwater, during a pending canister throw or its alternate cooldown, and
  while its own `scene_flame_input.reload_left` is nonzero, independently of
  the shared published reload countdown.
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
   and before `campaign_combat_tick`. Fusion's earlier gameplay include now
   needs the exact static forward declaration described below. No new build-list
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

## Fusion extension (2026-10-10)

The separate Fusion input owner previously required primary or manual reload
when its loaded shell reached zero. Releasing primary after a successful shot
could therefore leave an empty launcher with real reserve indefinitely. The
original ordinary entity update at `41e7ef` calls `419bc0`; `419bed..419bfc`
admits an empty magazine, `419c15..419c77` waits for firing presentation and
pending selection, and `419ca8` requests `425280(actor,1,0)` without a new
trigger. `425312..42532e` admits real mapped reserve. This path has no Fusion
exclusion; the distinct presentation exception at `419c54..419c63` belongs to
Flamethrower. The installed `shoulder_cannon` has clip size 1, reserve capacity
5, Fire Wait 0.8 seconds, Reload Wait 1 second, and real fire/reload clips.

The helper now admits slot 12 only with actual Fusion resources and all its
existing owner, selection, finite inventory, retained definition, real idle,
view-slot and shot-stamp gates intact. It does not estimate presentation
completion from the 0.8-second cooldown, advance a pose, or alter input state.
The scene's selected primary already supplies Fusion's magazine and reload
ticks. Fusion never queues conventional burst/delay work; existing ordinary
selection paths retire that work before entering Fusion, so those conservative
shared gates remain unchanged.

Immediately before `scene_fusion_gameplay.inc`, scene.c declares:

`static uint32_t scene_weapon_auto_reload_ready(const scene_stream *,uint32_t);`

The wrapper ORs the qualified helper result into its existing manual reload
argument. The new request requires the captured `selected` predicate, current
`campaign_equipped_slot==12` and `scene_weapon_auto_reload_ready(s,1)`. The
explicit current-slot check occurs after projectile damage callbacks, because
the helper also admits conventional weapons; their readiness must never
request a Fusion reload after selection changes. The helper rechecks the
current on-foot/armed owner and the remaining read-only admission. Existing
manual/held-primary semantics are unchanged. The conventional combat path
still returns before slot 12 can reach its separate conventional consumer.

`scene_fusion_input_tick` remains byte-for-byte unchanged: its active-reload
branch owns admission/completion, `ceilf(reload_seconds*60)` owns the deadline,
and `rf_weapon_reload_transfer` owns the single transfer. The input owner
also prevents a duplicate reload if a restored reload is live before the HUD
countdown is republished. Existing manual and held-primary requests retain
their original behavior, including their earlier admission without the new
idle-only gate. A qualified autonomous request still emits the existing
`reload_started` event, Shoulder Reload sound and published countdown; the
ordinary presentation consumer starts the existing reload clip. Projectile
ownership, fire cooldown, completion/deselection rules and RFAP remain intact.

Natural final-shot/release/autonomous refill, cancellation and restored-reload
runtime are unverified. The port's absence of a separate fire-fail action queue
and full original draw/blend dispatcher remains the same conservative boundary
described below. No authored ammo grant, new fixture or gameplay route is added.

## Flamethrower extension (2026-10-10)

The existing Flamethrower input owner starts reload on manual input or held
primary with an empty tank. Exhausting actual primary fuel and releasing
primary could therefore leave loaded fuel at zero indefinitely despite real
reserve. Automatic replacement remains separate: the fuel-debit consumer in
`FLAME-EMPTY-SELECTION.md` requires loaded and reserve both zero, so it does not
replace a Flamethrower with fuel available for reload.

The original outer automatic path has a Flame presentation exception at
`419c54..419c63`, retains the pending-selection guard at `419c65..419c77`, and
resets an active weapon at `419c79..419c9b` before the silent reload request at
`419ca8`. The reload entry itself rechecks fire, alternate and fire-fail
presentation at `42541f..42547e` with no Flame exception. This adapter does not
claim a same-call reset/reload sequence or exact retail blend scheduling. It
conservatively waits for the existing, represented real idle boundary.

That boundary already progresses without another fire press: after primary
fuel exhaustion and release, `scene_flame_input_tick` leaves its output active
flag at zero and calls `scene_flame_input_stop` through its existing stop
branch. The normal `scene_player_weapon_advance` later observes inactive Flame
and requests idle 0 from fire 1 when no reload or alternate request owns the
presentation. A subsequent combat tick may therefore request reload. The new
helper never admits `current==1`, resets the active controller, advances the
view twice, estimates a presentation deadline or changes release timing.

Slot 10 joins the existing read-only helper only with its actual loaded
resources, no swimming inhibit, no canister pending/cooldown and no internal
Flame reload in progress. Every shared selection, ownership, health/controller,
finite inventory, retained definition, real idle, view-slot and shot-stamp gate
remains intact. The internal reload guard also declines a restored reload
before its countdown is republished. Busy alternate state is observed after
the existing canister tick has advanced flights, applied callbacks and updated
its pending/cooldown owner; the adapter does not cancel or reorder it.

At the existing `scene_flame_input_tick` call in scene.c, the qualified result
is ORed into the manual reload argument only while the current equipped slot
is still 10. This explicit current-slot recheck follows canister callbacks and
prevents readiness of a newly selected conventional or Fusion weapon from
requesting a Flame reload. The helper receives the existing on-foot predicate
and independently checks the actual Driller/turret/form/armed admission. No new
forward declaration, frame field, input ABI or retained state is required.

`scene_flame_input.inc`, canister logic, fuel accounting and presentation are
unchanged. Manual and held-primary reload keep their existing earlier timing;
the real-idle restriction applies only to the added autonomous request. The
installed single-player tank has magazine 100, reserve capacity 1000, primary
drain 2.0 seconds, reload wait 2.6 seconds and reload-zero drain 1.2 seconds.
The existing input owner still derives its deadlines from the authored
primary definition, discards loaded fuel only at its existing drain boundary,
and commits exactly the existing `rf_weapon_reload_transfer` on completion.
The same start branch publishes the countdown and plays `Flame Reload`; the
ordinary view consumer starts its existing reload clip. Fractional fuel,
ignition/release sound, continuous-loop ownership, damage cadence, alternate
history, independent canisters/burns and RFAP layouts are untouched.

Natural final-fuel/release/autonomous refill, cancellation and restored reload
runtime remain unverified. Full original action reset/blend timing, a separate
fire-fail queue and any alternate/dry selection work remain outside this slice.

## Deliberate boundaries

This is a source-grounded first-playable adapter, not the complete retail
reload/presentation dispatcher. The port does not currently retain a separate
first-person `fire_fail` action queue. Requiring the actual idle owner is a
conservative projection of the represented actions; no fake action or timer
is introduced. Newly added draw/holster or fire-fail actions must join this
gate before being allowed to overlap automatic reload.

The Riot Stick's held charge still needs its own controller review. Flame uses
the conservative represented-idle projection above; the original full active
reset/presentation dispatcher remains outside this slice. Active forms and
custom transition actions are deliberately outside this new automatic path. Source-exact NPC
reload allocation, multiplayer branches, first-person blend equivalence and
broader saved animation fidelity are separate. The parent scheduled build can
establish compilation only; natural final-shot/release/reload and cancellation
runtime remain unverified until actually observed.
