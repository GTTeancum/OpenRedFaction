# Ordinary NPC pistol location-directed pain hint

Source-only staged extension of the NPC pistol body-location consumer, against baseline `f4980b975aca21856f9fc0faf8b8e916126ed9f4`. Independent static review accepted the exact two-file patch; integration and Xbox validation remain parent-owned. No compile, syntax test, runtime, fixture or save-continuation result is claimed.

Final patch SHA256: `9b856d58b1f4a09bc36f7d4c353b12450df2c4a21e4912b8d256342cc0804b4d`.

Independent exact-code review SHA256: `59a42e8262b0eb202c12a1124e1326476c186263c7931e1e9c1363c73c888b42`.

Design SHA256: `0b4a482dc7fb4f9d03ddcbcca498a5640e4d58a043ba679d1c4cd6a5c41ff4ea`. Implementation note SHA256: `ac203e0d6b1f3d0f3f2423ab36e38aad568022d8d16d37e294f749f03cfb65c4`.

## Source qualification

Original `42ce00` returns both the factor and selected CSPH index. The12mm descriptor bound at `4c65fa/4c6604` to `872114` is distinct from the exceptional descriptors at `4c5d95..4c5dcb`. After the humanoid `40a150` and actor810-bit1 `427020` gates at `4c5d6e..4c5d89`, this ordinary NPC-owned pistol reaches the normal reaction block without a player-source requirement.

`4c5e93..4c5eaa` transforms the contact through the actor's published3c/48 frame. Index1 uses the forward-side sign and writes828=24/25 at `4c5f48..4c5f79`; index0 uses right-side sign and writes27/26 at `4c5f85..4c5fbd`. Strictly positive chooses24 or27; zero/negative chooses25 or26. Index2 leaves828 unchanged. `4c600f` subsequently calls penetration4c9b20, followed downstream by generic4892c0 damage. This slice reconstructs only828, not the separate824/death/head effects.

## Bounded integration

The existing ordinary guard1 finite-pistol source and guard1/guard2/medic1 target qualification remain unchanged. No new owner, weapon or target560 interpretation is added. Unsupported/fallback contacts and index2 retain the target's prior hint.

The AI receipt reuses existing published-basis and reaction fields. Preparation snapshots the real published basis and preserves point-minus-published-position float stores, Z/Y/X dot grouping, final float side store and strict sign test. Physicale4/fc still owns collision/scalar geometry; it is not substituted for published3c/48. Receipt liveness additionally rejects a changed published basis after callbacks.

Shields/Nano continue consuming raw damage first. Once scalar amount validation succeeds, the AI-specific full receipt guard runs immediately before storing only24–27 into the same target's pain.selected_action and entering generic NPC damage, with no intervening callback. Suppressed/lethal damage does not roll the hint back. Existing generic pain owns motion selection, cooldown, animation lock and sound; this patch adds no motion start, timer, RNG, PLAYER helper or persistence state.

Existing RFNC24 target-state capability already covers hints24–27 and residual22–27 through exact class/model/actual base-or-weapon selection, including the genuinely unarmed medic and existing RFAP burn ownership. No checkpoint implementation, codec version, save layout or replay behavior changes.

## Validation limit

Only `scene_ai_pistol_location.inc` and its immediate `scene.c` damage join change. Receipt size is unchanged. Conservative additional preparation scratch is64 bytes, bringing the source-accounted declared-object budget to at most2660 bytes under the3072 cap. Compiled frame/spill sizes, nested callback/pose stack, stock64-MiB fit, animation and save continuation remain unverified until the scheduled parent-owned Xbox batch.
