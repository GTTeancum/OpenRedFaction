# NPC marker-driven footstep integration

Source written 2026-10-09; compilation and runtime verification are pending the
parent's consolidated Xbox batch. No helper build, test, emulator, image capture
or gameplay fixture was run or added for this change.

## Source and runtime route

The read-only installed `RF.exe` confirms the retained reconstruction in
`NPC-MARKERS.md`: ordinary entity update calls `42f940` at `41e6e4`, after the
earlier main-model advance pass. `42f940` rejects a linked parent, consumes exact
`footstep_left` then `footstep_right` markers, chooses the actor's support-material
class group with default-material fallback, and uses the first/second half of
the declared sample list. The audio position is entity `+3c`, minus entity
`+180` on Y. The existing physics mapping identifies `+180` as `bounds.radius`.

`scene_npc_footsteps.inc` now connects these owners:

- A scene-update pass runs immediately after `campaign_npc_playback_tick`, never
  during rendering. It uses the registered, generation-qualified NPC and its
  resident motion catalog, preserving the linked-parent marker rejection.
- `rf_entity_plan_footsteps` consumes the actual motion's retained marker names
  and pending bits. A ten-entry stack view adapts the already-loaded class and
  Foley group bindings; there is no new group owner or per-actor allocation.
- `rf_audio_group_choose` selects from each authored left/right half using the
  existing scene Foley sound RNG. Counts of zero/one retain the original
  sample-zero selection if a sample exists in that declared group. A truly empty
  group cannot read through to another group's samples.
- `campaign_sound_start` provides the existing budgeted lazy PCM load, safe idle
  eviction, listener-relative gain, mixer and native-device voice lifetime.
  Failed sample or voice admission is counted without stopping gameplay,
  retrying a consumed marker or replacing the requested sound.

The first-person player is excluded. Original view-mode zero bypasses these
markers through `42fb20`; this change does not infer or synthesize that route.

## Bounds and remaining differences

The adapter retains 120 bytes of fixed counters/name/log state, plus bounded
stack requests and ten compact group entries. Existing audio-bank and device
voice budgets remain unchanged. NPC checkpoint playback already owns marker
bits; this change adds no saved actor state or separate PCM pointers.

An absent/out-of-range support material is a bounded-port safety case, not a new
claim about the original negative-index class read. The pass consumes only the
two actual named footstep events without playback, preventing a stale airborne
event from playing after landing. Unrelated markers remain untouched.

The scene's current support scheduling and separate sound RNG are unchanged;
complete original outer-frame support/RNG ordering is not claimed. This is a
first-playable integration of the existing verified marker, group and sound
owners, not complete retail frame reconstruction.

## Narrow validation telemetry

`rf_scene_npc_footsteps[13]` records: pending-marker polls, consumed named markers,
planned requests, successful starts, failures, unsupported-surface discards,
last request UID, material, original Foley group, sample, voice, last status and
sound RNG. `rf_scene_npc_footstep_sample[64]` retains the last requested original
audio filename. The first sixteen requests and up to four initial failures log
`NPC_FOOTSTEP`/`NPC_FOOTSTEP_FAILED`, including actual group and filename.

Use an unchanged original bounded scene with naturally moving NPCs. Positive
markers/starts and original filenames establish dispatch; actual native output
must be checked separately before claiming audible runtime success. If the
existing neutral scene has no natural walking markers, report runtime coverage
as pending rather than adding scripted movement or synthetic actors.
