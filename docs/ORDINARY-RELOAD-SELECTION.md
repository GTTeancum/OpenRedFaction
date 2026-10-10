# Ordinary reload selection admission

Baseline: d2c1c9d32117f425e154d84c93a502edea658612, 2026-10-10. Staged outside the frozen repository for integration after the parent 19:00 batch. Independently source-approved; no build, syntax check, test, fixture or game execution was performed.

## Concrete finding

An already-started ordinary reload should refuse an ordinary weapon-selection request. Current manual cycling changes the selected weapon and clears `rf_scene_combat[6]`. The armed newly-acquired-weapon selector likewise selects and calls `scene_player_auto_select_inputs`, clearing that countdown. This permits interrupting the existing finite reload through request paths that the original refuses.

This is a request-admission correction, not background reload continuation. The exact original reload predicate also admits a retained first-person reload action after the entity reload bit clears. That residual presentation-only tail is outside this bounded gameplay slice.

## Original evidence

All disassembly comes from the supplied read-only RF.exe, SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

- `4a4a50` resolves the player's actor, then `4a4b76..4a4b89` calls `425250(actor)`. If it reports active reload and the fourth argument (force) is zero, execution returns at `4a4db4`. This precedes selection staging at `4a4d85 -> 4acd50`.
- `425250..42527e` first queries player FP action8 through `4ad8c0`; otherwise it returns entity `+810` bit8 (`0x100`). The staged predicate projects only the established positive gameplay reload countdown.
- Manual menu commit at `4a62a8` and direct numbered selection at `4a674a` pass both trailing arguments zero. The preceding cycle helpers `4a3770/4a3be0` choose a menu candidate; they must not be described as directly changing the equipped weapon. The port already collapses menu selection into a cycle edge; the new gate refuses that commit without retaining a request.
- Armed pickup `45aa93` calls `4a4a50(player, acquired, 0, 0)`. The grant has already succeeded. Unarmed pickup `45a9fe` also passes force0, but no ordinary active reload is admitted while genuinely unarmed; no stale unarmed veto is added here.
- `42550f` stores the reload's selected weapon at entity `+13b0`. `419d27..419d5f` cancels mismatch before completion, clearing reload ownership and timers. `419d8c..419de4` commits staged ammo only on matching selected weapon and due timer. This does not justify retaining an old reload through forced selection.

Raw evidence files: original-select-request.txt, original-reload-entry.txt, original-reload-service.txt, original-pickup-select.txt, original-cycle-one.txt, original-cycle-other.txt. Manual commit excerpt is copied as original-manual-selection-commit.txt. The selected-setter and FP-action excerpts retain immediate setter versus request/queued publication context.

## Minimal implementation

Two modified source files; no new runtime/save state or resource allocation:

1. `scene.c` adds `campaign_reload_blocks_ordinary_selection`. It returns true only for nonzero `rf_scene_combat[6]` while selected slot is 0,1,3,4,6,7,13,14,15 or16: Pistol, Rifle, Shotgun, Rocket, Sniper, Rail, either completed MP mode, HMG, Scoped Assault Rifle or Undercover handgun.
2. `campaign_cycle_primary` refuses before candidate selection. Its caller still updates `weapon_cycle_held`, so holding a refused edge cannot select later when the reload completes. The accepted-selection clearing path is never reached for this refusal.
3. `scene_armed_pickup_select` refuses immediately before its first selection side effect. Its real grant, item retirement and feedback remain accepted. It does not invoke `scene_player_auto_select_inputs`, cancel contacts, invalidate pose, change selection telemetry or queue a retry on refusal.

The helper does not consult a raw reload button, FP renderer current action, custom-mode state, shared charge tuple, reload sound or ammo quantity. Existing reload completion continues normally for the same selected weapon, using the port's existing finite `rf_weapon_reload_transfer` completion policy. Original initiation-staged ammo semantics are not introduced here. Independent projectiles, flight/contact owners and shared f90 are untouched.

## Current caller qualification

- Manual cycle: sole production caller of `campaign_cycle_primary`, in on-foot input; now gated at request commit. Existing ordinary cycle edge sampling is unchanged.
- Armed pickup: actual accepted world-item/drop/Give_Item consumers; now gated after the grant, before selection. Existing force0 proof applies. The current unarmed path remains unchanged for the reason above.
- Conventional and Machine Pistol depletion selectors already reject `rf_scene_combat[6]` before selecting. No duplicate change is needed.
- MP custom-mode input already inhibits on the reload count; Undercover manual mode toggle already requires no reload. No mode owner is changed.
- Grenade and Remote automatic selection are separately owned non-ordinary source paths. Their independent projectiles are preserved, and no gate is added.
- Flame, Fusion and Riot special reload semantics are deliberately outside this ordinary-gun subset, even though some publish the shared diagnostic countdown. The whitelist prevents this bounded slice from claiming those owners.
- `campaign_select_primary` remains an unconditional committed-selection primitive. Resource initialization, accepted load/import/respawn, forced player form, actual equipment stow and diagnostic-only callers are not ordinary force0 requests and are not globally gated. Existing forced-form reload mismatch is separately deferred; no background transfer or guessed cancellation is added.

## Review and integration

Exact source hashes and patch are in SHA256SUMS and reload-selection.patch. Independent review approved the exact two-file patch; its signed-off source verdict is in INDEPENDENT-REVIEW.md. Parent owns integration, top-level milestone update and scheduled executable validation. No test or runtime result is claimed.
