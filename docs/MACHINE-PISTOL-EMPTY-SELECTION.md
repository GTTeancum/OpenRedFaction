# Machine Pistol depletion through its existing custom mode

Integrated on 2026-10-10 after `b88f3d6f`. Uncompiled and runtime-unverified; no test, fixture, emulator,
PC-build, image, route or save-format work was performed for this slice.

## Original evidence and bounded decision

Read-only original `Installed_Game/RF.exe` and `tables.vpp` establish:

- `4a7016..4a703e`: depleted Machine Pistol tests the Special counterpart only
  with global byte `64ecb9 == 0`. This is the multiplayer/override byte already
  identified by the existing SP reconstruction, not the selected MP mode.
- `4a7040..4a7058`: depleted Special tests the base counterpart without that
  override gate. In ordinary SP both directions are admitted. Actual loaded
  plus mapped reserve is tested; positive counterpart reserve is sufficient
  even if its magazine is empty. Nothing makes its ammunition infinite.
- `4a7063..4a7074`: positive counterpart ammunition requests
  `4a4e80(player,1,1)`. When both sides are empty, `4a7061` reaches ranked
  replacement at `4a70b8`. The ordinary Autoswitch and descriptor no-switch
  gates precede these branches and remain enforced by `rf_weapon_decide_empty`.
- Both authored weapons have `alt_custom_mode`, `alt_fire`, and primary
  `continuous_fire`; neither has `alt_continuous_fire`. Predicate `4c9290`
  reads descriptor `+264` bit 19. `4a520c..4a5225` reaches the custom branch
  `4a52fe..4a5369` after its continuous-primary activity check, before any
  ordinary shot or empty-fire path. Thus this particular counterpart request
  is a custom action, not a second shot or a direct ranked weapon selection.
- `4a530b` tests current custom state with `42a6b0`, then dispatches
  `42a7c0` to enter or `42a6e0` to leave. `42a86a..42a8d3` enters base to
  Special; `42a76f..42a7af` leaves Special to base. `4adb00` requests action
  10, and `4adb30` requests action 11. The original internal Special acquisition
  at `42a8a5` is not reproduced: the port already owns both mode resources and
  intentionally authorizes Special through base ownership without new grants.

### Why the automatic pair starts only on a real dry attempt

The generic dispatcher still applies action admission. Its owner `+4b8`
clock checks are at `4a50fd..4a5146`. The completed real shot publishes that
clock at `426b52..426b5d`, before the empty callback at `426c32`; that call
cannot immediately start the paired custom action. We therefore record SHOT
separately from DRY and decline PAIR for SHOT, preserving the actual cooldown.
There is no fabricated deferred request. Releasing after the last real shot
can leave that mode selected until another actual fire attempt is accepted.

The dry path first calls reset `41ae70` at `4a55ff`. That reset clears the
weapon's active-fire byte at `41af81` and invokes the local player's empty
handler at `41b02c`, before the later dry backoff at `4a561d` and final empty
call at `4a564a`. This earlier accepted-dry boundary supplies the narrow
source-backed opportunity. In the port, only actual `fire == 2` qualifies;
`rf_weapon_trigger_step` already required the prior cooldown to become due.
The adapter also declines nonzero cooldown, queued burst/delay, or reload
input. The existing MP audio tick closes the dry loop owner before this point;
starting the existing custom action takes over its first-person presentation.
An alternate button, idle zero ammo, manual selection, restore, counters, or
inventory comparison is never used to infer this request.

## Port integration and ownership

The existing operation request grows from 12 to 16 call-local bytes with an
`operation` field. It remains fresh for each successful combat invocation and
contains full player handle, slot, actual mode weapon ID, and SHOT/DRY kind.
The conventional consumer still excludes outgoing slot 13. A separate
`scene_machine_empty_after_combat` handles it only after that combat operation
returns success, preserving outgoing contact, impact, hearing, and audio work.

The MP consumer requires the actual registry-qualified living player body,
matching captured mode, on-foot state, no form/cutscene/reload/holster/death
block, owned base, unowned Special, admitted views and custom owners, and no
existing pending transition. It tests exact-zero current loaded and mapped
reserve and rejects negative counterpart counts. Both ammo types must be valid
and distinct. The actual descriptor bits and factory Autoswitch values are
used; `override_mode = 0` is explicit SP policy. No ammo or ownership cell is
written by this adapter.

PAIR enters `scene_machine_pistol_mode_begin` on a candidate copy and then
uses the same `scene_machine_mode_apply` boundary as manual alternate input.
This primitive does not synthesize an input edge or alter the existing held
state. Its source custom action, sound, target, pending flag, and authored due
time match the existing controller. Automatic input release uses
`scene_player_auto_select_inputs`, which preserves real cooldown. It cannot
fire the counterpart in the same tick, and does not refill a reserve-only
counterpart. After the custom action completes, ordinary mode commit selects
the matching definition and normal autonomous reload can use its actual reserve.

The original changes selection before its target presentation callback. This
port deliberately preserves its existing practical source-view animation and
commit-after-duration behavior. It does not claim source-exact model dispatch,
blending, all generic admission gates, or complete original dry-backoff parity.

When both modes are empty, SELECT uses the same actual ownership, admitted
resource, retained-mode, and authored-rank filters as the conventional consumer.
No rank is added or borrowed. This fallback may occur after the successful last
shot or accepted dry attempt and uses the existing input-release/view boundary.

## Failure and save behavior

Mode state is staged while the custom view starts or detaches. Successful
presentation publishes the candidate; an error returns through the existing
combat boundary without changing completed mode or either ammo pool. A start
failure attempts the existing detach routine. If detach also fails, its payload
remains with the existing custom owner, the error remains fatal, and the current
custom-active save guard still rejects the unsettled view. This is not a claim
of total pose-byte rollback after a malformed animation fails evaluation.

No new persistent owner or queue is added. Existing RFWM admission continues
to reject pending mode transitions and attached custom actions. Successful
restore still assigns completed mode only after load admission; failed loads
cannot apply a new mode or activate the automatic action. Existing campaign
carry retains completed mode and real separate loaded/reserve counts. The
completed-mode and held-input save layouts are unchanged.

## Parent wiring

The parent scene now includes the shared mode input/presentation adapter and
separate Machine Pistol depletion consumer. The frame-local operation starts
empty before all early returns; actual successful debit records SHOT and accepted
`fire==2` records DRY. The outer caller invokes the paired consumer only after
combat and conventional selection succeed without changing the weapon.

The existing mode helper now exposes a begin primitive used by both real manual
alternate edges and qualified dry counterpart requests. State layout and save
ownership are unchanged. No fabricated controller input, inventory or gameplay
fixture is introduced.

Source integration and independent review are complete. Compilation, actual mode
handoff, both-empty fallback and save/runtime behavior remain unverified pending
the scheduled Xbox batch and a proportional ordinary runtime opportunity.
