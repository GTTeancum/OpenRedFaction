# Handheld Rocket Launcher alternate first pass

Staged against babf35f1 on 2026-10-11. Source-reviewed only; parent integration,
Xbox compilation and action runtime are pending. No build/test, emulator, fixture,
image, asset extraction output, original-input modification or Git write.

## Playable mode

- Select an owned Rocket Launcher. RT keeps ordinary straight fire.
- Hold LT while aiming at a living, visible actor or supported vehicle/turret.
  The most centered eligible target within approximately 100 world units and a
  30-degree half cone is retained. Continuous acquisition takes 2.75 seconds.
- HUD shows AIM AT TARGET, LOCKING percentage, then LOCKED - RELEASE LT; the
  existing locked reticle is reused. There is no automatic shot on lock.
- Release LT to request one ordinary Rocket shot. A completed, still valid lock
  adds guidance; an early/no-target release fires straight. Existing cooldown,
  finite magazine/reserve, reload, empty feedback and pool capacity still gate it.
- Guided shots wake after approximately one third of a second and steer toward
  the actual target body using the existing Cane/source velocity blend. Full
  generation handles are re-resolved each tick. Lost/dead/hidden targets leave
  the paid shot ballistic permanently; there is no fabricated hit or reacquire.
- Selection/possession/death/teleport/reload/swimming and primary takeover cancel
  pending acquisition. Previously fired rockets continue in the ordinary pool.

## Original evidence

The installed weapons.tbl Rocket Launcher record was read narrowly, in memory,
through the existing bounded VPP entry reader, without extraction output:

- Flags include thruster, player_wep, infrared, alt_fire, alt_lock; Flags2 has
  no_fire_through and damage_self. Generic Homing is explicitly false.
- Authored 15cm rocket ammo: 6 loaded, 18 reserve, reload 1.7 seconds, fire wait
  1.25 seconds; speed 20, lifetime 15, collision radius .051, damage 400, blast
  radius 5 and crater radius 5. Existing consumers retain these values.
- Normal and alternate Launch both name Rocket Launcher; no separate alternate
  action is authored. Reticles are reticle_rocket.tga and reticle_rocketlock.tga.
- RF.exe 4a63f9/4ad470 arms SP alternate input; 4acec0..4ad0ff scans while held
  and shoots on release. Unlocked release uses ordinary primary admission.
- 4ad110/4ad240 chooses the highest-alignment living visible target with eye LOS
  and body fallback, approximate range 100 and cosine(pi/6) cone. No faction
  restriction is added. 4ad01b..4ad0ff rescans after 75 ms and locks after 2750 ms.
- Locked release 4acfa5..4acfff binds the target and writes turn 5, cone 45,
  scan range 150 and wakeup 0x3eaaaa3b (about 1/3 second) into its descriptor.
  4c6aa8..4c6b5f supplies the separate alt_lock target branch; generic Homing=false
  therefore must not disable this player-controlled alternate.
- 4c6fca..4c7064 guidance preserves speed after normalized velocity-plus-target
  acceleration; the existing Cane owner already implements that mathematical join.

## Practical boundaries

75 ms rescans round up to five 60 Hz simulation ticks. Acquisition uses the
existing swept world/body/prop/rubble occlusion path and includes the scene's
supported vehicle/turret owners. Release rechecks current target visibility,
range and cone. Primary input takes priority and cancels acquisition.

Guidance retains only the acquired full handle; configured homing cone/range do
not introduce new scans. Exact predictive lead, thrust acceleration, lock beeps,
near-miss self-destruction and retail target/HUD policies remain refinement work.
The existing swept projectile still collides with intervening geometry/objects
and dispatches its real direct damage, radial damage and terrain effects.

No new asset or allocation is needed: 28 bytes of input state, 50 four-byte target
handles and a 48-byte diagnostic vector. Existing projectile/render/audio pools
are unchanged. No per-frame retained pointer and no new save payload.

RFCP/RFAP cannot represent this new target state. Capture and in-session load
explicitly refuse pending acquisition or an active guided rocket; save after
canceling the hold and waiting for guided shots to retire. The world refusal is
named player_rocket_lock. Successful ordinary load resets empty transient state;
preparation/rejected loads never erase it. This is an intentional compatibility
limit, not silent restoration as unguided shots.

## Integration and verification

Source wiring covers once-per-frame input, the existing shared trigger, successful
flight publication, per-flight guidance/orientation, HUD, save/load refusal and
lifecycle cancellation. The ordinary primary flight and damage paths are reused.

The parent-owned 20:00 batch may compile this and check fresh live boot. That is
not evidence of lock acquisition, release, homing, ammo debit or hit damage. Those
remain action-runtime questions for a later bounded check/tester play. Do not
reopen broad parity/codec/resource proof work merely to rename this staged slice.
