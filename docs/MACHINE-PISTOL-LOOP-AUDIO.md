# Player machine-pistol loop ownership

Status (2026-10-09): the bounded ownership helper and parent-owned scene hooks
are integrated and source-reviewed; scheduled Xbox compilation remains pending.
No build, test, fixture, emulator, capture or audible-output check was run for
this slice.

## Live gap and installed evidence

The accepted-player-shot branch of `campaign_combat_tick` sent slot13's Launch
group to `combat_sound` on every bullet. That adapter discards the returned
public voice ID, while `campaign_sound_start` honors original loop metadata.
Neither release nor reload then owns the accumulated machine-pistol loops.

Read-only inspection of `Installed_Game/tables.vpp` establishes:

- `weapons.tbl` lines 971–1059 define Machine Pistol with `continuous_fire`,
  `alt_custom_mode`, a 30-round 12mm magazine, .09-second fire wait and primary
  `Machine Pistol Launch` (line 1023).
- Lines 1064–1155 independently define Machine Pistol Special, also with
  `continuous_fire` and `alt_custom_mode`, a 20-round 5.65mm AP magazine,
  .12-second fire wait and primary `Machine Pistol Alt Launch` (line 1111).
  Its name does not make this an alternate-trigger shot. The scene's existing
  custom-mode transition selects the second definition under base ownership.
- `foley.tbl` lines 1913–1917 resolve these one-sample groups to
  `machine_loop.wav` (near 10, gain .8) and `machine_loop_02.wav` (near 10,
  gain .9). `bluebeard.bty` lines 12047–12063 mark both looping from 0.
- Neither machine-pistol definition declares a Start Sound, Start Delay or
  Stop Sound. The existing immediate accepted-shot onset is preserved; no
  guessed attack/decay sample or HMG/Assault presentation delay is added.

The installed `RF.exe` shares continuous firing ownership through actor+81c:

- `41a9bd..41a9c3` skips a duplicate continuous launch when the owner exists.
- `41aa3d..41aa95` resolves the selected Launch and retains its returned voice
  for the nonlocal branch. The local timer branch `41e642..41e688` uses the
  current weapon definition's primary Launch at+160 and stores its flat voice
  at the same actor+81c. This accommodates the two separate primary definitions.
- `41aee3..41aef7` stops and invalidates the owned voice before optional
  Stop Sound processing. `424fdc..424fef` also closes it at actor teardown.

These are static installed-asset/disassembly findings, not original runtime
execution or a claim that every custom-mode/reload crossover is reconstructed.

## Bounded consumer

`scene_machine_pistol_audio.inc` retains the admitted definition ID, completed
special-mode bit and player handle. It delegates audio allocation, sample
admission, one-attempt-per-episode behavior and generation-qualified stopping
to `scene_weapon_loop_audio.inc`. There is no new mixer, voice cache, looping
one-shot approximation, per-frame asset load, retry policy or serialized handle.

The first accepted shot begins an episode with the selected definition's
authored Launch. Further shots retain it, including a failed-start episode.
Each live tick closes an existing episode when admission is lost, the selected
definition/mode changes, or the player handle changes. An inactive tick never
opens a new voice. Only a later accepted shot can start the replacement episode.

The caller closes before a custom-mode transition starts, rather than leaving
the old loop running throughout the existing animation deadline. Pending
transitions also reject ordinary tick admission. Release, empty magazine,
active/accepted reload, deselection, unarmed state, lost base ownership, death,
mounting and existing enemy-combat trigger inhibition all end the episode.
The last accepted round still has its existing onset; the next normal combat
admission tick observes the now-empty magazine and closes the voice.

Lifecycle closure is silent. The helper neither changes nor replays switch or
reload sounds. It retains no independent ownership of the Special definition,
and never changes ammo, cadence, damage, spread, transition timing or saves.
This is a practical presentation policy at the scene's existing firing gates.

## Exact parent integration

1. Forward-declare `scene_machine_pistol_audio_close(void)` and
   `scene_machine_pistol_audio_reset(void)` beside the HMG declarations.
   Include `scene_machine_pistol_audio.inc` beside `scene_hmg_audio.inc`, after
   `scene_weapon_loop_audio.inc` and after the campaign IDs/objects are declared.
2. Add `scene_machine_pistol_audio_close()` alongside the existing HMG closure
   in accepted `campaign_select_primary`, `campaign_close_movers`, the player
   death transition, `scene_player_impact_relocated` in
   `scene_player_impact_gameplay.inc`, and the ordinary world-load boundary in
   `scene_world_load.inc`. Keep these before shared audio IDs/mixer teardown.
   Add `scene_machine_pistol_audio_reset()` beside the HMG scene-start reset.
3. In `scene_machine_mode_input`, insert
   `scene_machine_pistol_audio_close()` at the start of `if(event.started)`,
   before `scene_weapon_custom_actions_start`. The existing transition tick
   has already accepted and marked the pending transition at that point;
   close even if the following optional presentation request reports an error.
4. After enemy processing and beside the HMG/rifle per-frame audio tick, before
   any special-weapon/transition early returns, call
   `scene_machine_pistol_audio_tick(admitted, campaign_machine_mode.special)`.
   Use this short-circuit admission expression:

   ```c
   on_foot && campaign_equipped_slot==13 && !campaign_explicit_unarmed &&
   campaign_extra_ids[0]>=0 && campaign_extra_ids[0]<64 &&
   campaign_player_inventory.owned[campaign_extra_ids[0]] &&
   campaign_selected_weapon()>=0 && campaign_selected_weapon()<64 &&
   campaign_player_inventory.loaded[campaign_selected_weapon()]>0 &&
   campaign_player_damage.state.effects.health>0 &&
   !campaign_machine_mode.pending && !rf_scene_enemy_combat[6] &&
   !rf_scene_combat[6] &&
   !(player_input.reload && rf_scene_combat[5]<campaign_pistol.magazine &&
     rf_scene_player_ammo[1]) && player_input.fire
   ```

   Machine-pistol `scene_conventional_fire_select` sets `held=primary`; alternate
   requests a custom mode, never a firing mode. Reading `player_input.fire`
   therefore preserves that policy without moving or skipping its existing
   validation call. A simultaneous alternate rising edge is already blocked
   by the mode-input/pending check. Index checks precede every inventory read.
   Do not require `owned[campaign_machine_special_id]`; only base ownership
   authorizes either selected mode.
5. Inside the existing accepted `if(fire)` sound branch, add slot13's
   `scene_machine_pistol_audio_shot(campaign_machine_mode.special, position)`
   beside the rifle/HMG cases. Remove only slot13's former `combat_sound`
   choice. Leave trigger selection, reload transfers, shot debit, switch sound,
   custom-action calls and all other weapon sound choices unchanged.

`rf_scene_machine_pistol_audio[9]` reports accepted shots, episodes, closes,
unexpected live-mode mismatches, retained shots, active state, special bit,
admitted definition ID and player handle. Normal transitions close at start,
so their later completion does not increment the mismatch counter. Closes count
presentation episodes; shared telemetry records actual starts/stops/failures.
Source review and parent integration do not constitute compilation or runtime
validation. Those remain for the parent-coordinated hourly Xbox batch.
