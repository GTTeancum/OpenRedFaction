# Remote Charge automatic control handoff

Status: source-written and source-reviewed; compilation and runtime are
unverified. The parent owns shared scene wiring and the scheduled stock-64-MiB
Xbox batch. No helper build, test, fixture, emulator, image, commit or cleanup
was performed for this slice.

## Original evidence

The installed read-only RF.exe, weapons.tbl in tables.vpp and motion headers in
motions.vpp establish a normal charge-to-detonator-to-charge control loop.
It does not depend on the general Autoswitch Weapons preference.

- `4c65c3..4c65e6` resolves Remote Charge into `87210c` and Remote Charge
  Detonator into `85cce0`. Strings are at `5a32cc` and `5a32dc`.
- `4c90a0` identifies descriptor `+264` flag `0x40`, authored as
  `remote_charge`. `4a565a` uses this predicate before scheduling the charge
  follow-up.
- `4a56d3..4a571c` gets the primary or alternate first-person action duration,
  subtracts binary32 `0.1`, multiplies by 1000, adds 0.5 and truncates to
  milliseconds. It arms player timer `+b8` and records Detonator as the
  follow-up at `7c764c`.
- `4a539d` tests the `detonator` descriptor flag through `4c9070`.
  `4a53ae` calls `429c70`, which sets fuse `-1` on matching owner/class `0x40`
  projectiles. `4a53dc..4a5415` then schedules the full primary fire-action
  duration through the same millisecond conversion, with Charge as its target.
- `4a27c4..4a2813` consumes expired player `+b8`, clears it and requests the
  recorded selection. It uses `(defer=1, force=1)` for Detonator and `(0,0)`
  for Charge; this port retains its existing immediate, already-loaded
  first-person selection boundary instead of implementing the retail queue.
- `4a4abb..4a4b34` resolves a Charge selection back to Detonator if mapped
  reserve is nonpositive or a projectile has the exact charge type and owner,
  positive fuse and no object flag `2`. This block does not require attachment
  or positive life. The adapter uses that exact bounded scene-pool predicate
  rather than broadening the existing `rf_remote_charge_available` helper.
- `4acd50` clears player `+b8` when another selection is queued. The live
  adapter correspondingly cancels its follow-up at `campaign_select_primary`.
- `4ad9a0` resolves action names through the array at `5a0b00` (`fire` and
  `alt_fire` first), then calls `5033e0 -> 501c60 -> 51c2e0` for clip duration.
  Existing `rf_motion_duration` implements that duration conversion.

The separate original post-draw timer at player `+107c` also performs a
no-live-charge/positive-reserve return (`4aabbd..4aac59`). That generic draw
timer is not reconstructed here; the accepted action's `+b8` follow-up is
enough for the ordinary throw/detonate loop.

## Timing from retained resources

The adapter reads each already-loaded `rf_player_weapon` action envelope. It
does not open archives or advance playback to obtain a duration. Original
millisecond rounding is retained, then the port explicitly rounds upward to
whole 60-Hz ticks. This is fixed-rate integration, not exact subframe parity.

Owned motions.vpp entries have these start/end ticks:

- `fp_rmt_chrg_throw.rfa`: 160/3040, duration about 0.600 seconds. Subtracting
  0.1 gives 500 milliseconds and a 30-frame follow-up from action start.
- `fp_rmt_chrg_place.rfa`: 160/2400, duration about 0.466667 seconds. Subtracting
  0.1 gives 367 milliseconds and a 23-frame follow-up from action start.
- `fp_rmt_det_fire.rfa`: 160/2400, full duration rounds to 467 milliseconds,
  giving a 29-frame follow-up from accepted detonation input.

The existing charge publication delays remain 21 and 15 frames. The timing
reader rejects a charge follow-up that would expire at or before publication,
missing action payloads, malformed durations and more than 3600 follow-up
ticks. No unrelated fire cadence, release delay or 45-tick post-release
cooldown is repurposed as the selection timer.

## Live behavior

`src/diagnostic/scene_remote_selection.inc` retains exactly two 32-bit words:
`scene_remote_followup_target` (0 none, 1 Charge, 2 Detonator) and
`scene_remote_followup_ticks`. It is included inside `scene_remote_gameplay.inc`
after the existing pool and scheduler declarations.

The follow-up requires real Remote Charge ownership, both admitted first-person
views, existing remote gameplay resources, a living on-foot armed player and
no active player-form override. Missing admission leaves the existing remote
action usable without automatic selection. It never invents ownership, grants
ammunition, loads an absent view or changes another weapon family.

An accepted charge action arms the duration before its existing delayed
publication. The first decrement occurs on the next combat frame. Actual
publication failure, including a full projectile pool or unavailable reserve,
cancels the follow-up. A successful release keeps the deadline and its
independent launched charge. This success-gated policy deliberately avoids
claiming the original allocation-failure branch has been reconstructed.

An accepted detonation request with a real matching projectile arms the
29-frame return. Repeated trigger edges during the pending handoff do not
restart it. Empty detonator input retains its previous no-op behavior; this
slice does not add its retail dry animation. At return time, exhausted reserve
or a still-ready own charge leaves Detonator selected. Otherwise Charge is
selected for the next throw.

Selection is applied only after all special projectile and world ticks for
the frame. The old by-value remote mode and trigger arguments are never reused
after a change. `campaign_select_primary` publishes the selected definition;
the existing shared `scene_player_auto_select_inputs` consumes held fire,
alternate and reload until release. First-person pose/shot/reload caches start
at an idle selection boundary, and ammunition/counters are republished.
The remaining handheld-input path then ends for that frame. The shared input
reader clears the conventional held edge only on actual fire/alternate release,
including special slots that never call the conventional trigger consumer.
This prevents a stale settled-save veto while retaining the held-input save
guard until physical release.

Manual selection, scene reset, death, explicit unarmed state, vehicle/turret
control and an active player-form override cancel a pending handoff. Launched
charges, host attachment, fuse requests, blast/damage, terrain effects and the
old cooldown remain independent. Manual cycling remains available.

## Checkpoint contract

The existing remote checkpoint component owns the two new fields
`followup_target` and `followup_ticks`. RFRM2 has a 56-byte header, retaining the
existing fields through offset 43 and reserved zero word at 44, with target
at 48 and remaining ticks at 52. Each active-charge record remains 228 bytes;
the maximum component grows by eight bytes to 7352. The enclosing save formats
already carry and validate the component length.

Persistent invariants are:

- Target 0 requires ticks 0; targets 1/2 require ticks in 1..3600.
- Target 2 requires selected Charge mode 1. If a throw is pending, follow-up
  ticks must exceed its remaining publication delay.
- Target 1 requires selected Detonator mode 2 and no pending throw.
- Genuine RFRM1 and absent components restore no follow-up. Neither cooldown
  nor live charges are used to fabricate a timer in an old save.

The normal countdown temporarily reaches zero inside combat, but the same
frame's post-world helper consumes or cancels it before ordinary save capture.
An unexpected mid-combat capture of that transient state is invalid rather
than silently clamped. Restore assigns the staged pair only after restoring
selected mode through `campaign_select_primary`, because that common selection
boundary intentionally clears any old live follow-up.

## Parent integration

1. Forward-declare `scene_remote_selection_cancel(void)` before
   `campaign_select_primary`, and call it at that common selection boundary.
2. Forward-declare
   `scene_remote_selection_after_world(scene_stream *, uint32_t frame,
   uint32_t on_foot, uint32_t *changed)` before `campaign_combat_tick`.
3. Do not add another helper include: `scene_remote_gameplay.inc` includes it
   internally, before the already-following checkpoint implementation.
4. After the flamethrower/canister/world block, immediately before depleted
   grenade selection and the remaining special/conventional return guards,
   call the post-world helper, propagate errors and return `RF_OK` if changed.
5. Keep the existing remote frame-zero reset/restore ordering. Remote reset
   itself clears the new state; the versioned checkpoint owner restores it.

No general autoswitch option, preference order, armed pickup policy, new
controller binding, campaign route, resource family or per-slice fixture is
added. Native behavior and compilation await the parent's scheduled batch.
