# Player HMG loop ownership

Status (2026-10-09): ownership helper and parent scene wiring are integrated
and independently source-reviewed;21:00 Xbox compilation is pending. No build, test,
fixture, emulator, image or audible-output check was run for this slice.

## Concrete live gap and evidence

The accepted-player-shot branch in `campaign_combat_tick` sent HMG Launch1 or
Launch2 through `combat_sound` for every shot. That adapter discards the public
voice ID, while `campaign_sound_start` honors the installed loop metadata.
Neither trigger release nor reload owned those voices, so repeated shots could
retain obsolete loops in the bounded mixer/native voice pool.

Read-only installed-table inspection establishes:

- `weapons.tbl` line1440 marks `heavy_machine_gun` as both `continuous_fire`
  and `alt_continuous_fire`; lines1478–1479 give .10/.20-second shot cadence;
  lines1493–1494 assign HMG Launch1 and HMG Launch2.
- `foley.tbl` lines1939–1943 resolve those groups to `Heavymachinegun.wav`
  and `Heavymachinegun_02.wav`, both near distance10 and gain.9.
- `bluebeard.bty` lines5561–5578 mark both samples looping from0.
- Installed `RF.exe`41a9bd skips a new continuous launch while actor+81c is
  owned; the nonlocal branch41aa3d–41aa95 chooses primary/alternate Launch and
  retains the returned voice.41aee3–41aef7 stops and invalidates that owner.
  The delayed local-player branch41e642 instead selects primary descriptor160.
  The existing shared `scene_weapon_loop_audio` implements generation-qualified
  ownership; retaining the scene's existing two-mode sample mapping is a bounded
  adaptation, not a claim of exact original local-player crossover behavior.

This is source/installed-metadata evidence, not an executed original-game probe.

## Bounded consumer

`scene_hmg_audio.inc` retains only the HMG weapon ID and effective fire mode.
It delegates voice ownership, asset admission and generation-safe native stop
to the existing shared player loop owner. The first accepted shot opens an
episode; further accepted shots retain it, including a failed-start episode.
There is no second voice cache, per-frame asset request or retry allocation.

Each live tick consumes admission and the effective alternate bit from the
existing `scene_conventional_fire_select` result. Primary retains its current
priority when both triggers are held. Release, empty magazine, reload, lost
ownership, unarmed state, death or mounting closes the episode. A changed
effective mode also closes immediately, even during cooldown; the new mode
starts only when the existing gameplay trigger accepts its next shot. This
explicit boundary matters because the shared voice owner deduplicates by
weapon, not by Launch group.

The fix preserves the previous immediate accepted-shot sound onset. The table
also supplies SAssault Attack, .10-second Start Delay and SAssault Decay at
lines1485–1487; reconstructing that separate presentation sequence is deferred.
Cadence, damage, spread, ammunition, reload timing and save data are unchanged.

## Parent integration

- Include beside `scene_rifle_audio.inc`, after the shared loop helper.
- Forward-declare `close`/`reset` as needed. Close at accepted equipment
  selection, player death, player relocation, ordinary load and before audio
  teardown. Reset during ordinary scene startup. Use the same lifetime hooks
  as the already integrated rifle owner, including `scene_world_load.inc` and
  `scene_player_impact_gameplay.inc`; do not serialize this presentation state.
- At the existing per-frame rifle-audio boundary, select the conventional
  policy early for equipped slot14 using the existing arguments, guarded by
  the same on-foot, selected, armed and owned state that reaches the original
  late selection. Set an explicit ready flag only after successful selection;
  only that flag may skip the later call, preserving validation/error behavior
  for inhibited or unrelated paths. Pass admission gated by
  `conventional.held`, on-foot/ownership/living state, nonempty loaded
  magazine, no active reload and no accepted manual reload; also preserve the
  existing `rf_scene_enemy_combat[6]` trigger inhibition. Pass
  `conventional.alternate` as the second argument to `scene_hmg_audio_tick`.
- Replace only slot14's accepted-shot `combat_sound` branch with
  `scene_hmg_audio_shot(conventional.alternate, position)`. Keep all existing
  gameplay admission, inventory debit and other weapon consumers unchanged.

`rf_scene_hmg_audio[8]` reports accepted HMG shots, episodes, closed episodes,
effective-mode changes, retained shots, active state, alternate state and weapon
ID. Closes count presentation episodes; actual starts/stops/failures remain in
the shared loop-owner telemetry. Runtime and audible-output behavior remain
unverified until the parent-coordinated Xbox batch.
