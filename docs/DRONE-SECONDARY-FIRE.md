# Authored automatic Drone Missile owner

Source written after the 2026-10-10 02:00 Xbox batch. The owner is in
`src/diagnostic/scene_ai_drone_secondary.inc`; parent scene integration and
projectile/resource admission are separate parts of this slice. No build,
syntax check, runtime fire, ammunition exhaustion, save/load or audio result
is claimed. Parent owns the next consolidated stock-64-MiB Xbox batch.

## Original consumer and authored values

Original `levels1.vpp/L3S4.rfl` Drone UID844 explicitly selects Drone Smash /
Drone Missile. It is a sentient non-Fighter actor. Ordinary acquired-target
combat `405af0` reaches `40628d–406299`, calling primary `401580` and then
secondary `406330`. The latter calls `426ca0` after `406390` admission.
`407310` enables firing following target acquisition and an optional ready
action. UID844 has direct When_Dead links, not a proven Shoot_Once consumer.

Installed `weapons.tbl:2599–2628` supplies Drone Missile: speed20, lifetime5,
collision radius0.15, mass2, explosive damage15, blast radius2, crater radius5,
AI attack range30 and Fire Wait4 seconds. There is no magazine, homing,
scheduled burst or authored impact delay. Startup already owns the secondary
and fills its mapped 15cm-rocket reserve to12; the identical authored secondary
override retains that supply. This adapter adds no ammunition, loadout, spawn,
event, fixture, route or asset changes.

The source-reconstructed `rf_weapon_ai_secondary_admit` in `weapon.c` retains
the `406330/406390` gates, approximate distance metric
`max(abs(delta)) + 0.375 * middle + 0.1875 * minimum`, blast/body-radius
clearance, normalized eye-to-target aim and body-forward dot threshold0.95.
The first-playable owner now supplies a real acquired actor pair to it.

## Combat ownership

- Selection requires the exact held and owned Drone Smash / Drone Missile
  pair, admitted resources, positive real mapped reserve and qualified full
  registry/entity handles. No unsupported secondary becomes a generic rocket.
- The existing `combat_alert` state is the first-playable combat-ready input.
  Ordinary mode0 addresses the already acquired player; explicit/reactive
  mode1/2 addresses its retained `combat_target`. The owner never creates an
  alert, target, Attack order or firing request from an authored movement order.
- Source/target health, hidden/dead flags, registry generations and allocated
  bodies are checked. Source AI mode, scripted animation, pain lock, holster,
  seat/turret control, point-fire and single-fire requests can suppress launch.
- This bounded adapter admits on-foot player/NPC targets only. A vehicle hull,
  mounted actor or point is not replaced with a fabricated player body. The
  shared missile still has its existing real swept collision consumers.
- Range uses the source body's actual position and the acquired target's eye
  point; minimum clearance is authored blast2 plus both actual body radii.
  Aim uses the source eye and full3D body orientation, not the primary's
  horizontal firing cone. World/mover and clutter LOS are required; the
  original unseen-fire flag is not synthesized. Target/order and geometry are
  requalified after the sight query.
- Dispatch is before primary ammunition, `combat_due` and Smash's range5
  checks. No primary deadline, pending strike, burst, selected weapon,
  animation or weapon-reset state is changed by missile acceptance. The caller
  continues its ordinary primary branch even when a missile was accepted.

The existing primary owner still controls acquisition, pursuit and Smash
cadence. The later bounded exact Drone fly11 XYZ pursuit patch preserves this
ownership and full-target steering; see `DRONE-XYZ-PURSUIT.md` for its staged,
runtime-unverified body-space policy. The secondary does not reconstruct the original optional
ready-action transition or reuse the primary's30-frame acquisition reaction
deadline; readiness is the current acquired/alert state plus the live gates.
Consequently a newly acquired, aligned, unobstructed target can admit its first
missile immediately. This is an explicit first-playable timing policy, not a
claim that the original primary/secondary presentation order is reproduced.

## Independent deadline and persistence

A fixed64-entry runtime bank retains source full handle, a monotonically
unique ticket and a wrapped millisecond deadline. A launch reservation is
published before entering the projectile launcher. The deadline is armed only
when that launcher returns `RF_OK` after real spawn and finite-ammo debit.
Pool/unavailable refusals do not consume a secondary cooldown. A full cooldown
bank refuses launch rather than firing without an owner.

Each accepted shot owns exactly4000ms, equivalent to240 simulation frames at
60Hz. It does not read or write `combat_due`. Service before the enemy loop's
early-outs reclaims expired entries even while combat is suppressed. Target,
order, mode, selection, pain, holster and visibility changes do not reset the
timer. Old full handles cannot transfer their deadline to a replacement actor.

RFNC has no compatible independent secondary-deadline representation. Rather
than silently resetting cooldown on reload, `scene_ai_drone_secondary_save_pending`
rejects save/export while a launch is in progress or any deadline is pending.
It observes expiry without mutating the bank. The projectile owner separately
rejects unsupported active Drone flights at its save-only boundary. A valid
save therefore cannot omit either an active Drone missile or a remaining
secondary wait. Existing wire formats stay unchanged.

Do not add this gate to shared NPC row capture or load preparation. Failed
save/preparation must retain the live bank unchanged. Reset it only with the
existing successful NPC component commit, composed player checkpoint
publication, successful final world publication and normal teardown/restart.
Keep these resets alongside the equivalent pending-Smash timeline resets.
Do not add cooldown resets to Smash's per-source cancellation hooks. Tickets
prevent a callback which publishes a replacement timeline from rearming an
old launch reservation. Existing world loading is not otherwise made into a
general rollback transaction by this slice.

## Parent integration contract

The include follows `scene_ai_drone_smash.inc`, before `campaign_enemy_tick`.
Earlier scene lifecycle/save call sites need declarations for:

```c
static void scene_ai_drone_secondary_reset(void);
static uint32_t scene_ai_drone_secondary_save_pending(void);
```

At the start of `campaign_enemy_tick`, before its swim and per-owner exits:

```c
{int status=scene_ai_drone_secondary_service(frame);if(status)return status;}
```

After acquisition, actual target resolution and ordinary pursuit, but before
`if(selected.ammo)` and every primary cadence/range exit:

```c
if(!point_target) {
    uint32_t secondary_fired;
    status=scene_ai_drone_secondary_tick(stream,i,intended_handle,frame,
        player_eye,&secondary_fired);
    if(status)return status;
    /* Do not continue here: secondary and pending/next Smash are independent. */
}
```

The existing save-only `scene_ai_drone_smash_save_pending()` guards must also
check `scene_ai_drone_secondary_save_pending()` and the projectile flight guard.
The same successful timeline/reset sites clear the secondary bank. At fresh
scene startup also clear `rf_scene_drone_secondary` telemetry separately.

Projectile integration supplies `scene_drone_missile_resources`,
`campaign_drone_missile`, `campaign_drone_missile_primary` and
`scene_ai_drone_missile_launch(owner, target_eye)`. `RF_OK` means the shared
projectile pool accepted a real flight and debited the actual owned reserve;
`RF_NOT_FOUND` means no launch/debit. Neither return may reset or present the
primary attack. The owner includes the launcher forward declaration.

## Presentation and validation limits

Launch remains the existing shared Tankbot-style first-playable eye-origin,
straight target-directed flight. Original `426d73` hand rotation, `41b040`
secondary muzzle placement and `426e07` target alignment are not reconstructed
here. There is no invented homing, spread, predictive lead or altitude steering.

Authored Launch is `Missile Launch Drone` / `missile_02.wav` (near20, gain0.8);
flight is `missile_loop.wav` (near10, gain0.9). The separate optional audio adapter now adds those exact secondary launch
and flight profiles after actual projectile publication, using copied per-voice
parameters; see `DRONE-MISSILE-AUDIO.md`. It intentionally does not call
primary action2/Smash Foley just to obtain a sound. Shared rocket impact
presentation remains an explicit approximation of Drone's Small Explosion,
`big_charge_explode` radius7, missile trail and scorch authoring.

No neutral startup proves actual Drone fire: the original L3S4 player and
Drone authored origins are about70 units apart, beyond ordinary acquisition.
No campaign traversal or artificial attack was introduced to make a check.
The source should be reported as unverified until the parent hourly batch;
even compilation/startup admission cannot establish combat, timing or audio.

Successful timeline publication also retires only old Drone flights. This
closes empty-RFAP and composed-player loads where a frame-zero launch could
otherwise survive while its unsaved secondary cooldown is reset. Failed
preparation is unchanged; Rocket/Tankbot entries are not cleared by this helper.
