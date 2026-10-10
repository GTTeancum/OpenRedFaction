# Authored player jump audio

Status: source-written on 2026-10-09 after frozen source `49a207b`.
No build, test, emulator run or audible-output claim. Parent integration and
the consolidated 05:00 Xbox batch are still required. No new gameplay fixture
or independent audio/PCM owner is introduced.

## Original evidence

- `docs/CAMERA.md`, "Original jump and fall transition", and
  `include/rf/player.h` retain the `4288b0` callback boundary: class `+0x120`
  reaches `434d00` after the accepted fall state and before the jump timestamp,
  followed by flat `505560` playback. The existing original-jump inspection
  records stack words `(sound, 0, 0, 0, 1.0f)` at that lookup boundary.
- Read-only installed-table inspection found `$JumpSnd: "Jump"` on all three
  actual player forms: `miner1`, `parker_suit` and `parker_sci`. Foley group
  `Jump` names `jump_04.wav`, near distance 8 and authored volume 0.5.
- Static inspection of `434d00..434d38` confirms that the function reads two
  arguments: group and sample index. It rejects invalid groups, negative
  indices and indices outside the group's count, then returns `samples[index]`
  at `434d2f`; `434d33` returns -1 on rejection. It has no RNG call. The jump
  lookup's explicit sample index is zero, so this adapter takes the first
  sample and leaves a zero-count group silent. The other stack words recorded
  by the existing jump inspection belong to the adjacent playback setup.
- `src/core/player.c` already calls the sound callback only for accepted jumps,
  after committing the fall state and before writing `jump_time`.

## New source

`rf_entity_jump_sound_group_read` reads the selected class's optional
`$JumpSnd:` and resolves its name through the existing Foley owner. Missing,
empty or unknown sound names return group -1. Invalid/duplicate fields leave
the caller's output unchanged. The reader allocates nothing and retains no
input pointers. Missing-tag and duplicate handling are explicitly bounded-port
parser policy, rather than a claim of full original parser equivalence.

The same bounded parser change supplies
`rf_entity_land_sound_groups_read(..., groups[10])` for the separate landing
adapter. Repeated `$LandSnd:` names bind their Foley material slots. Slots start
at -1, empty/unknown names remain absent, and duplicate resolved materials or
malformed input preserve the output with an error. The existing installed
player classes name Default, Metal, Solid, Water and Glass Land groups.

`scene_player_jump_audio.inc` caches all three real player-form bindings while
the existing `entity.tbl` bytes are resident. The actual published model-form
index selects the binding, including form changes and section reloads, with no
table read during a jump. The audio path uses `campaign_sound_start` with
volume 1, centered pan 0, category 0 and nonspatial routing. The existing bank
applies the authored sample gain once and handles budgeted lazy loading,
device-safe idle eviction and native voice submission.

The scene hook increments `rf_scene_player_jump[2]` unconditionally before
audio work. That counter remains the existing movement acceptance signal even
when a class is silent, a sample is missing, the bank budget is full or native
playback fails. Audio failures are diagnostic only and cannot undo a jump or
change the movement callback's return status. Rejected jumps never call the
audio adapter.

## Parent integration

The supplied `player-jump-scene-hooks.patch` changes only `scene.c`:

1. Forward-declare the isolated adapter's four entry points.
2. Load groups beside the existing player-footstep table admission.
3. Preserve the accepted callback counter, then forward sound and frame.
4. Supply the active authored class sound to `rf_player_jump` and pass the
   synchronous frame pointer as its callback context.
5. Include the isolated adapter beside the footstep adapters and reset it at
   scene initialization.

No changes to the frozen active checkout were made by this worker. Shared
scene wiring is deliberately supplied separately for the parent to apply.

## Compact batch evidence

`rf_scene_player_jump_audio[12]`:

| Slot | Meaning |
| --- | --- |
| 0 | Accepted callback/audio requests |
| 1 | Authored sample selections |
| 2 | Successful voice starts |
| 3 | Silent class or absent sample requests |
| 4 | Nonfatal audio failures |
| 5 | Last active model form |
| 6 | Last Foley group |
| 7 | Last bank sample |
| 8 | Last native/deterministic voice ID |
| 9 | Last status |
| 10 | Existing combat sound RNG snapshot, never advanced by this adapter |
| 11 | Class bindings ready |

`rf_scene_player_jump_sound_events[16][8]` retains frame, form, group, sample,
voice, status, accepted callback count and total starts. Missing signed IDs are
stored as `UINT32_MAX`. `rf_scene_player_jump_sample[64]` retains the last
resolved sample name. `PLAYER_JUMP_SOUND` logs are bounded to the first eight
requests plus the first four failures. Mutable bookkeeping is 644 bytes;
sample PCM remains inside the existing audio budget.

The parent can extend its existing ordinary neutral-spawn jump replay to
observe an accepted request, `jump_04.wav` selection and a successful voice
start. Playback submission alone does not prove audible output; native audio
capture is needed for that stronger claim. Silent-class/failure semantics are
source-reviewed only until exercised in the parent batch. No new fixture was
created for this slice.
