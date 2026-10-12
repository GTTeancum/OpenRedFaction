# Ordinary NPC Use speech

Update: the parent stock64 Xbox batch compiled and linked this feature in source commit `7168ad0c4704c1f762c44f612302b37ebbe31a14` at 02:09:47 UTC on October 12, 2026. No runtime was performed; earlier source-only handoff wording below describes the pre-integration state.

Source-written 2026-10-12 against `7fdb1a826018517028340aadf7b7ef96091b0795`.
Not compiled or runtime-tested by the helper. The parent owns the combined
hourly stock-64-MiB Xbox build/check. No campaign route or new fixture was run.

## Operational gap and authored scope

`scene_use_dispatch.inc` already scores nearby Use-kind-9 actors by their
origin and consumes the input edge, but had no response callback. Miners,
technicians and administrative NPCs consequently remained silent on Use.

Read-only original table inspection found 12 entity classes declaring
`ai response`. Ten bind five real Use Foley groups containing 30 distinct
voice samples, all present in `audio.vpp`: `comp_tech`, `admin_male`,
`admin_male2`, `miner1`, `tech1`, `admin_fem`, `env_scientist`,
`ult_scientist`, `miner2`, `miner3`. `miner4` explicitly selects the empty
persona. `env_guard` selects `Grd Use`, which has no group in the installed
Foley table; it correctly remains silent rather than borrowing another voice.
Runtime eligibility still applies to each actor, independent of class binding.

## Original evidence

Installed RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- Use jump table `4a18fc`, entry9, selects `4a179b`. Single-player dispatch
  calls `409950` with the selected NPC AI and player, then returns consumed.
- `40995b..4099d0` refuses entity814 bit0x200, a missing player, non-human
  class (`40a150`, class724 bit0x20000), actions other than Waiting2 or
  Wandering4, hostile affiliation0, active actor speech (`429a50`), cutscene
  and multiplayer mode.
- `4099e5..4099f7` selects class6f4's persona through `4305a0`, whose
  persona Use field passes to `434da0`. The installed chain is entity.tbl
  `$Persona`, personas.tbl `$Use Key`, then a named foley.tbl group.
- `434da0` selects sample0 for a single entry; multiple entries consume one
  original CRT draw modulo the declared count. Existing `rf_audio_group_choose`
  already implements that selection, reused here through the Foley adapter.
- `4299b0` rejects missing samples/dead entities, replaces the per-actor1400
  voice and plays at entity7d4 with scale1. `429a50` polls that same voice.
- Original `409950` additionally resets model actions, selects the player as
  an AI target, turns and briefly changes Wandering4 timing. These gameplay
  side effects are deliberately outside this speech-only first-playable slice.

## Implementation and ownership

- `rf_entity_use_sound_group_read` transactionally resolves bounded class,
  persona and Foley names. Empty/unknown optional bindings yield -1; malformed
  or duplicate fields do not overwrite the output.
- Optional per-class bindings are loaded alongside existing entity sound
  metadata. No PCM is preloaded. Allocation/read/name failures stay silent
  and cannot reject scene startup. The group array costs four bytes per loaded
  class, capped at 2,560 bytes; personas.tbl temporary input is capped at64KiB
  (installed size5,463 bytes).
- A real Use9 edge calls `scene_npc_use_dialogue`. Existing range/input,
  player/cutscene/endgame/occupancy selection stays unchanged. The callback
  applies original humanoid, affiliation, action and speech gates. As an
  explicit port policy it also avoids active combat, scripted animation,
  scripted gaze and scripted shooting owners, since this slice cannot cancel
  those owners safely.
- Foley chooses an actual authored clip using a separate explicit seed1
  presentation RNG. This does not alter gameplay RNG or promise original
  interleaved global random order. Missing assets, busy voices, exhausted
  audio budgets or unexpected looping Use samples remain harmless silence.
- Use speech shares the existing actor-message voice array. A current scripted
  actor line suppresses Use; a later scripted actor Message replaces Use.
  Radio/flat messages remain independent. No second voice pool is allocated.
  New full actor-handle and origin fields add240 bytes for the30 shared slots.
- Actor speech follows the current eye position through existing listener
  refresh. Death, removal, stale generation or hidden Use speaker retires
  playback. Event OFF targets only event-origin speech, preventing an event
  UID collision from stopping a Use response. Shared native/software voice
  activity and PCM eviction rules stay in force.
- Scene close keeps existing device reset/voice cleanup. Successful committed
  world reload stops only Use-origin voices after storage close succeeds;
  rejected loads leave the outgoing speech untouched. Speech/its presentation
  RNG are not serialized or replayed, matching existing deferred presentation
  treatment. No save format, admission guard or gameplay row changes.

The first pass supplies audible authored responses when resources permit.
Facing, gestures/mouth animation, wandering pause, medic success/refusal lines,
automatic persona chatter and dialogue trees are not implemented by this slice.
No subtitle is invented for a Foley clip lacking authored message text.

## Parent integration and verification

The stage contains a unified patch and complete changed files. `scene.c` has
only declaration, optional load/reset/close and include hooks; merge those
with other current work. `scene_world_load.inc` has one success-only cleanup
call. No CMake/source-list entry is needed because the parser extends an
existing translation unit and the scene implementation is an included file.

Only static disassembly, source review and read-only asset/table inspection
were performed. All new C behavior, actual voice audibility, repeated Use,
scripted-speech replacement, disappearance and save/load behavior remain
unverified until the parent batch or ordinary tester feedback. No new gameplay
fixture or broad test suite is required by this handoff.
