# Accepted-shot camera feedback

## Status and scope

Integrated and independently source-reviewed against
`994ed61791d517cb7a8ac750f789d0b0e57fb84d`, awaiting the scheduled 06:00
Xbox batch. No compile, parser test, syntax check, gameplay fixture, emulator run,
route replay or original-input mutation was performed for this slice. Runtime
behavior and full original frame ordering remain unverified.

The player-operated stationary Vauss extension is integrated and independently
source-reviewed against `cde3fb726b3e66628d7571f893a12bd17a36f2fa`, awaiting
the same batch. It has not been compiled or
runtime-validated; no additional checks, fixtures or original-input edits were run.

This bounded adapter covers the actual on-foot Assault Rifle, Machine Pistol and
Machine Pistol Special shot identities, plus the actual accepted primary of a
player-operated authored stationary Vauss emplacement. Handheld Vauss, HEAP,
autonomous/NPC turrets and other vehicle gun descriptors remain outside scope.
No persistent pitch/yaw recoil, gain, alternate RNG, first-person
mesh recoil state, save owner or checkpoint field is added.

## Original evidence

Fingerprint: installed `RF.exe` SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- Original completed-firing owner `425830` captures its descriptor at
  `4258e9..4258ff`. Autoswitch at `426c32` may change the current weapon, but the
  tail `426c3a..426c7e` reuses the captured descriptor at `426c54`, resolves the
  player/controlled host through `48aa30`/`48aa90`, and tests `+264` flag
  `0x20000000` before calling `40e0b0` with `+4e0` strength and `+4e4` duration.
  Feedback therefore belongs to the outgoing accepted shot. The port adapter
  deliberately restricts this shared tail to the current ordinary on-foot player
  or a qualified current player-operated stationary Vauss mount.
- Original loader `4c4503..4c4565` uses presence-only `$Camera Shake:`, followed
  by ordered `+Distance:` and `+Time:` numeric fields. Presence sets the flag and
  writes `+4e0`/`+4e4`; absence zeroes both numeric fields. It has no boolean
  token. Installed primary IDs are Assault Rifle 8 (`0.0005/0.04`), Machine
  Pistol 9 (`0.0003/0.04`), and Machine Pistol Special 10 (`0.0003/1.0`). The
  adapter resolves names rather than hardcoding those catalog indices.
- The installed exact `Vauss` declaration supplies strength `0.001` and duration
  `0.04`. The separate mounted profile reads that declaration through the same
  reader; `Vauss2 Fire`/`Vauss5 Fire` sound aliases are not weapon identities and
  do not select camera metadata. Values are passed through without rescaling.
- These values go unchanged to the existing resolved-player
  `rf_scene_player_feedback` / `rf_camera_effect_start` owner. The original
  `40e0b0` behavior replaces the current strength/duration/deadline. It is not an
  additive recoil accumulator. Existing `40db70` camera stepping, decay and
  random-orientation sampling remain the sole implementation.
- Original shot pose setup `41b0f8..41b183` obtains actor `+7d4`/`+7e0` eye pose.
  First-person camera setup `40d88c..40d8bf` copies it into the separate camera before
  `40db70` perturbs that camera. This supports separating shot aim from rendered
  camera shake; it does not establish every original update-order detail.

## Metadata admission

`rf_weapon_camera_shake_read/load` in `entity_assets` owns a separate small
metadata record. It does not change `rf_weapon_primary_definition`, reset flags,
weapon supply catalog, their hashes, or any save layout. The presence flag is
kept only in this cosmetic record, rather than adding the original camera bit to
an existing canonical descriptor digest.

The reader uses the existing bounded table lexer and decimal parser. It selects
the first matching named declaration, returns a disabled all-zero record when
that declaration has no camera block, and rejects duplicate camera blocks,
missing/out-of-order subordinate fields and nonfinite or negative numbers.
These are bounded port input guards, not claims about every original malformed
input case. Missing declarations and malformed input preserve the caller's entire output.
A loader owns at most one budgeted table scratch allocation and frees it on all
read outcomes.

At scene startup, three fixed optional on-foot records and one independent
stationary Vauss record bind exact declared names to actual supply-catalog IDs.
Each failed/missing/disabled record stays inert
independently; these cosmetic reads cannot reject gameplay startup. A duration
outside the existing timer owner's representable positive range is also left
inert. No per-shot archive access or allocation is introduced. Authored strength
and duration are not rescaled or replaced with port defaults.

## On-foot accepted-shot consumer

The existing call-local `scene_weapon_empty_request` already captures source
handle, actual weapon ID, logical slot and `SCENE_WEAPON_EMPTY_SHOT` immediately
after the successful conventional/Machine Pistol ammo debit. Its name does not
mean ammo must be empty: it records every accepted shot, including ones with
rounds remaining. The same receipt is reused; no retained queue/counter is added.

After combat and the applicable conventional/paired empty-selection consumers
succeed, the camera consumer runs once, including after a selection change.
It requires the same full-handle registered living player, body ownership,
on-foot nonstaged context, actual resource view and name-qualified profile.
Active forms, cinematics, APC aim overrides, mounted/vehicle ownership and
unsupported weapon IDs are outside this adapter. Dry attempts, reloads, MP mode
requests, Flame fuel debits, held input and individual pellets do not qualify.
A repeated render of one combat frame receives no new shot receipt.

The captured outgoing ID controls the camera values. There is no post-selection
current-weapon, current-mode, ammo or inventory-ownership check: MP Special is
intentionally not a separately owned weapon, and final-shot autoswitch must not
replace the outgoing descriptor. Optional camera failure cannot convert the
already accepted combat operation into failure.

## Stationary Vauss accepted-shot consumer

`scene_turret_player_tick` captures the full player handle, full host handle and
actual outgoing weapon ID immediately before `scene_turret_scene_shot`, and
passes those same host/weapon values to the shot backend. Only its `RF_OK`
return admits feedback: `RF_NOT_FOUND` and errors bypass it. After the existing
successful hearing block, one optional void consumer receives those captured
identities. Held input, counters, individual penetrating contacts, current
selection and sound names are never used as an acceptance receipt. The existing
same-frame guard remains the only mounted update/cadence owner.

After shot/hearing callbacks, the consumer rechecks
`scene_turret_player_controls(captured_host)`, captured/current mount player,
player object/view/damage handles, both registries, physical body admission and
finite positive player health. It freshly resolves the captured host, requires
an authored use-kind4 stationary owner with matching view/damage identities and
finite positive health, and excludes terminal/hidden/dead, generated or
NPC-operated hosts. Cinematic, APC-aim and active vehicle conflicts are inert.
The captured weapon must match both `scene_turret_vauss_id` and the independently
name-qualified enabled `Vauss` camera profile. The post-shot selected descriptor
does not replace it.

The consumer starts only the existing resolved-player camera-effect owner;
optional failure cannot reject the accepted shot or core startup. There is no
new queue, retained shot state, timer, save/checkpoint field, RNG or cadence.
Existing unlimited stationary emplacement ammunition is unchanged: this cosmetic
adapter does not require carried inventory or reserve. The separate finite
ordinary NPC Vauss reserve and its combat path are untouched.

## Aim separation and retained timing

A call-local eye/basis snapshot is taken immediately before the existing camera
shake apply, after the existing player/vehicle/APC/cutscene camera pose choices.
Only ordinary on-foot combat with no staged shot, turret, active vehicle, APC aim
or cutscene override receives that raw snapshot. Existing vehicle/Nano staging
APIs still receive their previous camera orientation, and staged/mounted combat
keeps its previous arguments. Existing rendered-camera/listener wiring and the
shared camera-effect RNG owner remain in place. Newly active effects naturally
change the rendered pose and consume the existing stream; no independent RNG
or seed is introduced.

This also corrects the prior coupling in which damage/Force_Shake camera effects
rotated ordinary on-foot gameplay aim. It does not write back camera perturbation
into actor look, or create a new persistent aim kick. Snapshot placement leaves
existing camera/cutscene positional choices intact.

The current scene applies camera shake once before ordinary on-foot combat.
On-foot accepted-shot feedback is started after that combat, so it is first
sampled on the next camera/render pass, normally the next simulation tick.
Mounted firing already runs before the existing camera pose/apply in
`actor_follow_view`: its successful-shot feedback is therefore sampled in the
same view pass, after the unchanged turret eye/basis shot dispatch. The mounted
control, aim basis and single camera-apply ordering are not moved to imitate the
on-foot delay. The existing camera apply runs on every
view invocation, including a repeated frame; this slice does not change that
scheduler. There is no additional same-pass apply, decay or random draw, and no
attempt to claim full original timing parity. Existing active camera samples
retain their two draws and replacement semantics; this slice adds no RNG owner
or seed.

## Remaining work

- Parent integration, scheduled Xbox compilation and bounded runtime observation
  are pending; authored values and source review alone are not a runtime pass.
- Full camera/update-order parity, mesh recoil integration, handheld Vauss,
  HEAP, autonomous/NPC turret and other vehicle camera consumers remain deferred.
