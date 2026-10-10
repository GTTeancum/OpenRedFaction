# Independent ordinary reload-selection review

Date: 2026-10-10. Baseline: `d2c1c9d32117f425e154d84c93a502edea658612`.

## Verdict

Approved for parent integration of the exact two-file candidate, source-only. No edits to the active repository, builds, syntax checks, tests, routes, game/emulator runs or fixtures were performed. The source is not runtime-validated by this review.

- `reload-selection.patch`: SHA256 `48bae5ad875db483118dc06b45fa662178198dbdb1c63ae68c6e1dbc92322f7c`
- staged `src/diagnostic/scene.c`: `317dbdd73357fe4a6c3659969966f4b29ecf503711c809caefab141ef578de88`
- staged `src/diagnostic/scene_armed_pickup_selection.inc`: `0a1f988c8e258e4fff20cf7d527191b44f4382ee245298571baba0768ee7cfd3`

## Original owner and request boundary

Original4a4a50 resolves its supplied player through4a3260. Independently reread4a3260 resolves player+14 with426fc0 and returns that actor's inventory at+2a0.4a4aa5 resolves the same player actor into EDI. The SP path calls425250 at4a4b77; an active reload and zero fourth argument return at4a4db4 before4acd50 selection staging.425250 reports FP action8 when the actor has a player owner, or actor+810 bit0x100. This is an already-started reload predicate, not raw reload input.

Manual cycle helpers select a menu candidate; the actual menu commit at4a62a8 independently shows both trailing arguments zero. The separate cycle-tail commit4a3fa5 also passes both zero. The pickup calls45a9fe and45aa93 pass force0 as well. Automatic pickup selection is therefore not synonymous with forced selection. A successful inventory grant may remain accepted while its ensuing ordinary selection request is refused.

Original42550f records the reload weapon at actor+13b0.419d27 compares that saved weapon with current primary; mismatch clears reload state/timers and returns before completion. This finding does not justify background reload completion after switching.

## Current source and minimal correction

The live manual cycle path is the only caller of `campaign_cycle_primary`. It calls the generic setter, then clears combat[6] after success. Armed pickup selection also calls the setter and `scene_player_auto_select_inputs`, which clears combat[6]. The generic reload completion later transfers reserve into `campaign_selected_weapon`; it does not retain a separate reload weapon identity.

The patch adds one read-only predicate for a positive existing gameplay reload countdown in ordinary selected slots0,1,3,4,6,7,13,14,15,16. The manual cycle checks it before candidate/selection commit. Its caller still samples `weapon_cycle_held` outside the refusal, so holding a rejected cycle input does not queue a delayed switch when reload completes. The armed pickup check precedes its first selection side effect; real grant, retirement, notice/audio and inventory publication remain intact because callers ignore a declined selection after the accepted grant.

The helper has valid declaration order: included `rf/scene_preview.h` declares `rf_scene_combat` before the new early helper. No new persistent state, allocation, codec field, timer, inventory grant, deferred request or shared-charge mutation is introduced. No conditional was added to the generic setter.

## Boundaries

This is the admitted ordinary-gun countdown projection only. It does not claim complete original425250 parity after gameplay completion while a renderer reload action remains. Raw reload input, residual visual clip state, custom-mode state and shared FP/f90 state do not block requests. Riot, Flame and Fusion special reload semantics, unarmed pickup, forced form changes, load/restore and other generic setter callers are unchanged.

A separate deferred current-source overlap was identified: `campaign_set_player_form` changes selection through the generic setter without clearing combat[6], while generic reload completion targets the then-selected weapon. The original saved-weapon mismatch cancellation proves that this must not be described as original background reload behavior. Whether an already-known authored form transition can overlap a live ordinary reload requires a separate bounded event/lifecycle qualification. It is not repaired or claimed covered by this two-file patch.
