# Independent exact staged-patch review

Result: PASS, with no blocking source findings. Reviewed on 2026-10-11 UTC.
This permits parent consideration after the frozen 02:00 batch outcome; it
does not authorize early integration or claim compilation/runtime proof.

## Bound identity

- Baseline commit: `c30cd51a5306e2ae691774297d8e213b5d8ca74e`.
- Patch: `outside-difficulty.patch`.
- Patch SHA256:
  `9271b7037b88262819682e661521a7c7b492d548b08a8e8ac9abb474f8e7b320`.
- Staged `src/diagnostic/scene.c` SHA256:
  `0f2ac841fbd754f870bcb460f11a1cd2893f5141be6fd612ffc00dc62ade91cb`.
- Staged `src/diagnostic/scene_player_outside_damage.inc` SHA256:
  `f786e3a704522a0631a5f433b5beab1f0a9372cfe9b3aa8500c7be8c02dfcc89`.

The actual patch and both full staged files were independently hashed and
read. Full staged-versus-checkout diffs match the patch. The checkout still
reported the baseline commit and a clean working tree at final inspection.
See `INDEPENDENT-SOURCE-REVIEW.md` for independently verified original-input,
baseline source identities and ownership findings.

## Change review

1. The single constant table is extracted from the c30 NPC helper into
   `scene_player_difficulty_multiplier(uint32_t,float *)`, before the outside
   include. It has no campaign access, callback, mutation beyond its output,
   allocation or retained owner. Its four exact binary32 constants are unchanged.
   Null output returns RF_RANGE; an index >= 4 returns RF_FORMAT; output is
   written only on success.
2. `combat_enemy_player_difficulty` keeps its pointer validation, default 1,
   force/kind exemptions, full current identity checks and all original bounded
   scope gates. Only the table/index tail delegates to the pure helper. For
   invalid qualified rows its output remains 1 and error remains RF_FORMAT,
   matching the previous helper. The existing NPC consumer is unchanged.
3. The outside adapter adds one local scalar and reads the existing saved
   campaign difficulty only after all unchanged exposure admission. The prior
   accepted full handle remains the dispatch target; no identity recapture,
   bypass, class reinterpretation, armor change or extra exposure gate is added.
4. The raw request is unchanged. The existing audio API receives the selected
   scalar instead of 1, and the core remains the sole difficulty multiplier.
   The retained class factor remains its separate downstream multiplier.
5. The lookup occurs before request telemetry and dispatch. A new invalid-row
   error follows the existing error path and does not claim a request occurred.
   Normal successful-state telemetry order and all later effect handling are
   unchanged. This guarded invalid-state behavior is a disclosed port policy.
6. Neither shared damage/public APIs nor scheduler, liquid, audio, NPC state,
   save/history, room, physics, original inputs, oxygen or death owners change.
   Existing liquid -> outside damage -> independent gasp ordering is preserved.

## Verification limits and handoff

This review is fixed source/binary inspection, hashing and textual comparison
only. No compile, syntax check, test, fixture, emulator, source execution,
campaign route/scan, runtime exposure, original-input write or cleanup was run.
No image or audio was produced. The patch and c30 NPC change remain uncompiled
at review time. Outside damage, audio, death/save continuation and actual saved
difficulty in a running session remain unverified.

Parent owns the frozen checkout, serial 02:00 build outcome and any later
integration or scheduled validation. Do not manufacture an exposure recipe or
broaden this two-file correction to other damage sources.
