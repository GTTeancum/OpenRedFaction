# Ordinary NPC reload completion independent of combat admission

Status: source-written against `568cb6b12285ef795d0bfbd6b24db83b25fd29a8`,
2026-10-10. Independently source-reviewed and parent-integrated after the 10:00 batch; compilation and runtime remain unverified, with compilation planned for the 11:00 parent-owned Xbox batch. The staged implementation and review performed no tests, fixtures, builds, gameplay or worktree creation.

## Gap and scope

The existing ordinary NPC ammo gate completes a retained reload only after
seat/operator suppression, single-fire FIFO handling, target liveness,
AI-mode/disguise admission, awareness/sight and secondary-action gates. A
reload already started can therefore wait past its deadline merely because
the owner no longer reaches ordinary firing logic.

This change services only completion of an existing ordinary finite-magazine
reload at the beginning of each `campaign_enemy_tick` owner iteration. It does
not start a reload, grant ammo, change selection, clear cadence, stop or restart
animation/audio, or change target ownership. The explicit whole-tick movement
fixture and outer developer-combat suppression remain unchanged; this is not a
new global timer service outside the existing gameplay tick.

## Original evidence and retained port policy

Authority: supplied read-only `Installed_Game/RF.exe`, 1.20 NA. Direct `objdump`
disassembly was read for this implementation. Companion raw text lives in the
staging package's `evidence/` directory; `input-sha256.txt` identifies the input.

- `41e4b0` is the ordinary entity update. Its `41e7e8..41e7ef` sequence calls
  `421060(actor)` and then `419bc0(actor)` without an AI target/order/alert gate.
- `419bc7..419bd8` qualifies the current magazine weapon via `4c86e0`.
- `419bed..419ca8` contains the separate empty-magazine automatic request path,
  ending in `425280(actor,1,0)`. This patch does not add that initiation behavior.
- `419cb0..419cbb` checks that reload is active through `425250`.
- `419cc1..419cde` calls `427020` and rejects a dead actor. `42702b..427033`
  reads entity `+810 & 1`. The port also retains its existing positive finite
  health and object-visibility eligibility checks.
- `419d27..419d60` compares selected weapon `+2a4` with saved reload weapon
  `+13b0`; completion is reached only when they match.
- `419d8c..419d9b` checks the existing finish timer `+139c`; `419da6..419dcc`
  commits staged magazine and reserve, then `419dd3..419de4` clears reload
  ownership/timer. There is no target/order/alert check in that completion tail.

The original stages ammunition at reload initiation and also owns separate
drain timers. This patch deliberately keeps the port's preexisting completion
policy: `rf_weapon_reload_transfer` moves
`min(magazine - loaded, current reserve)` when the retained frame deadline is
due. It does not claim exact original staged-ammunition/drain semantics.
Likewise, original mismatch/death handling can cancel reload state; the new
completion-only adapter leaves those cases untouched, preserving the port's
existing selection/cancellation owners.

## Implementation

`scene_ai_gameplay.inc` factors the original ammo validation into
`campaign_enemy_ammo_validate` and extracts `campaign_enemy_reload_complete`.
The latter requires a nonzero deadline, `frame >= deadline`, and the same saved
reload weapon; it validates the existing ordinary ammo contract, performs the
existing finite transfer, and clears only the deadline after successful
transfer. `combat_reload_weapon` remains retained, as before. Zero transfer is
also a successful completion.

The original `campaign_enemy_ammo_ready` signature and standalone behavior
remain intact. It still validates before mutation, cancels a mismatched local
deadline under its old selection behavior, can initiate an empty-magazine
reload, and returns events 0/1/2/3 with the same meaning. The extracted functions
are used from that helper, so isolated includes introduce no unused helpers.

`scene_ai_reload_completion.inc` adds `campaign_enemy_reload_service`. It
requires:

1. An already-due deadline belonging to the exact currently held weapon.
2. `registration.view == &owner->view`, and both the object registry and entity
   lookup returning that exact retained registration/view pair. The check does
   not require a physics allocation: inventory transfer does not use a body.
3. Finite positive health, no existing object flags `2 | 0x4000`, and no entity
   dead flag `flags_810 & 1`.
4. A valid owned weapon ID and a catalog count bounded to 1..64, containing it.
5. A supported ordinary magazine profile: Pistol, Assault Rifle, Shotgun,
   Sniper, Rail, the four installed extra ordinary guns, or the existing
   resource-admitted Rocket Launcher path. Riot/melee, grenades and all
   reserve-fed/special creature, energy and mounted weapons are excluded.
6. Positive matching primary/supply magazines, nonnegative supply capacity,
   and the existing ammo-type, loaded/reserve and reload-duration validity
   checks. The existing transfer's nonnegative reserve policy is retained;
   this patch does not add a reserve clamp or new replenishment rule.

Idle, early, mismatched, unsupported or ineligible owners are no-ops. Reached
catalog/inventory errors return without changing ammo/deadline/selection.
The due predicate is evaluated before these validations so this adapter does
not introduce errors for unrelated owners with no due reload.

The scene calls the service before generated/operator/seat suppression,
FIFO expiry, secondary-once dispatch, opposed acquisition, mode/disguise,
target, sight, pursuit and ordinary firing gates. The service owns the actor's
ordinary inventory only. It does not grant use of a mounted weapon; existing
turret-operator binding already refuses pending `combat_reload_due`.

## Frame-local completion receipt

Each owner iteration keeps one `int32_t reload_completed_weapon`, initialized
to -1 by the service and set to the completed weapon only on successful
transfer. When the ordinary ammo branch is eventually reached in that same
iteration, a matching receipt supplies event 2 and readiness from the current
loaded count. It does not call the initiation/exhaustion gate again.

This matters when a due reload transfers zero because the reserve has become
empty. The old gate returned event 2 / ready 0 in that case; a second gate call
after early completion would incorrectly produce exhaustion event 3 and change
weapons in the same tick. The receipt keeps event 2 / ready 0. Exhaustion and
fallback may run on a later admitted owner tick, as before.

If a same-weapon due timer remains without a receipt, the later ordinary branch
holds with event 0 / ready 0 instead of allowing the standalone helper's weaker
context to bypass exact scene-owner qualification. It still runs the ordinary
ammo validation, so reached malformed/unowned ammo state retains its existing
error result rather than silently stalling. This hold applies only to
same-weapon overdue state; mismatch cancellation and future deadlines keep
their existing behavior. Special/reserve-fed readiness dispatch remains ahead
of these ordinary branches and is unchanged.

The receipt is stack-local, not a save field, pool or cross-frame event bank.
No additional completion presentation is started. Existing completion traces
remain at the ordinary ammo consumer and therefore are absent when a later
combat gate skips that consumer; the transfer itself has already completed.

## Save and deferred work

No checkpoint code, layouts or guard were changed. In particular,
`scene_npc_checkpoint_capture.inc` still rejects pending reloads outside its
supported combat modes, and action 39 still requires its existing alert/script
presentation ownership. Completing ammunition does not mean the authored
reload clip has ended or is representable without that ownership.

The separate `scripted-target-release-next` candidate is not integrated and
remains blocked on that independent animation/save concern. This completion
service must not be used as evidence that target release is now safe.

## Source review and verification limits

Source inspection covered the ordinary/extra/Rocket selector boundaries,
finite transfer's all-validation-before-mutation contract, per-owner early-out
ordering, legacy ammo helper behavior, and unchanged checkpoint gates. Existing
registered synthetic owners without physics allocations remain eligible when
their ordinary catalog/health/state are valid.

Parent verification should establish compilation and, when an appropriate
existing bounded check is available, due-versus-early transfer, no target or
seat suppression stalling an accepted reload, zero-reserve completion delaying
exhaustion until a later admitted tick, and identity/mismatch/error preservation.
No such runtime result is claimed here. Do not add campaign route playthroughs
or per-slice gameplay fixtures to validate this patch.
