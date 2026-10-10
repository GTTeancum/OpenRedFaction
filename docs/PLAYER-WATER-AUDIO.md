# Ordinary player water audio

Source-written candidate against231d8160785f541955fdc7e3504ab1f3ca192490.
Not compiled or runtime-tested. The frozen checkout was not modified. Parent
owns integration, the scheduled Xbox build and any bounded native checks.

## Concrete gap and implementation

The live campaign swimmer already commits body/eye wet flags, movement mode
and liquid damage. Neither `campaign_swim_update` nor the gameplay camera
consumer owned the authored surface-swimming or underwater loop, so ordinary
swimming remained silent even though its movement worked. This change adds
one small transient player-water audio owner with two generation-qualified
voice IDs, using the existing bounded sample bank and native audio adapter.

- A wet body with its eye above that body's liquid surface owns `swim_01.wav`.
  Native gain follows the actual body speed and class base speed. Submersion
  and dry exit stop it before starting the other loop.
- The actual gameplay eye in a liquid room owns `underwater_01.wav`; surfacing
  or leaving liquid stops it. Its room is queried separately from the body,
  through the existing exact body/camera room caches.
- Calls run after the gameplay listener pose, before diagnostic view overrides.
  A frame guard prevents duplicate starts if the same frame renders twice.
- Playback uses original sample names/defaults and the installed loop metadata.
  Both assets explicitly loop from0; missing/incompatible metadata does not
  silently become timer-driven one-shots. PCM loads only on demand through the
  existing idle-safe eviction path; no separate bank or budget increase.
- Only the registered, living, on-foot local player owns these voices. Boarding,
  a scripted camera, invalid ownership or missing room data releases them.
  This is a bounded ordinary-play scope, not complete vehicle/cinematic water
  audio reconstruction. No splash, gasp, oxygen or damage behavior is added.

`scene_player_water_audio.inc` owns implementation and telemetry. `scene.c`
has only declarations, one include, the gameplay-camera call and open/close
hooks. The existing `scene_player_impact_relocated` boundary gains one close
call, so accepted respawn, teleport, boarding/exit and checkpoint publication
cannot retain a loop from the old location. Save codecs and admission rules
are unchanged; no voice IDs or sample cursors are serialized. Failed staged
world loads never reach that existing relocation publication boundary.
A successful load reconstructs the necessary loop from live water membership
on its next ordinary camera update. Level teardown stops voices before the
mixer reset and PCM release. Same-slot replacement is protected by the existing
voice-ID/generation map; an inactive or expired owner is reacquired safely.

## Original evidence

Read-only RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

-429100 queries the actor's room and liquid membership.4292a2 checks the
 actor's eye against that room.4292d9..429307 stops global voice594594 when
 submerged;4293ea..429419 stops it on a dry existing room.
-42932a calls4895d0, which tests object flag8 for the local-player branch.
 429343..429356 starts sample17 via flat505560(volume1, pan0, category0),
 retaining its returned ID at594594. An existing voice is reused.
-42935b..42939d computes length(actor+144)/class+50, multiplies by binary32
 .4 (589508,0x3ecccccd), adds binary32 .6 (58946c,0x3f19999a), rounds to
 float, clamps to[0,1] with40a4c0, then calls505970(voice,gain,pan0).
 The source magnitude routine40a000 computes the full three-axis velocity.
-505970 updates requested gain and multiplies the voice category only before
 544390/522d30. It does not multiply the sample-default volume again. The new
 owner retains that initial-versus-refresh distinction: initial start uses
 the existing bank's.8 gain; subsequent surface refresh uses the raw speed
 gain (.6 at rest,1 at class base speed). Diagnostic software PCM remains
 unity, following the existing adapter; changing gain is sent to native output.
-431d1c..431d32 tests the final camera point against its own room's liquid.
 431dda..431e13 starts sample16 through505560 and owns ID59614c.
 431e14..431e47 stops it for an attached kind1 host;431ead..431ec7 stops
 it outside liquid. Predicate4290d0 resolves parent+200, then checks kind1
 through486c90. The candidate deliberately limits these paths to on-foot.

The installed `tables.vpp/sounds.tbl` has SHA256
`9e42163f04e24aaf65ce41d8fc3879253882cf425b715706c0643214adc33d5a`.
Rows16/17 are `underwater_01.wav` / `swim_01.wav`, both near10, volume.8,
rolloff1. `bluebeard.bty` lines16298 and17658 declare the matching files with
`+Looping Sound` and `+Loop Start: 0`.

Original archive payload inspection:
- `Swim_01.wav`:70,266 whole-file bytes,70,153 mono PCM8 frames at22,050Hz.
  SHA256 b5b509d6df485a2888a5334345c18d4ebabc6105bc9910c8335592fb41ffd6d5.
- `Underwater_01.wav`:83,776 whole-file bytes,41,866 mono PCM16 frames at11,025Hz.
  SHA2564778e16983eba6628749bd3364da5b165eee9b1bd5bf7bedca063d1603cedd23.
- Both files together add154,042 retained bytes if neither is evicted; ordinarily
  one voice is active. Bank/device eviction and total1,280KiB bank cap remain.

These are static disassembly, installed-data and live-source findings. No
original-code oracle, build, emulator execution, fixture, image or campaign
route was run for this candidate. Parent verification should establish native
loop output and cessation across a natural wet/surface/dry transition when an
authorized bounded opportunity is available. Do not claim audible output from
these source checks.

## Read-only telemetry

`rf_scene_player_water_audio[16]`: ticks, eligible ticks, surface starts,
underwater starts, stops, native gain updates, stale IDs, failures, last status,
body wet, camera wet, surface sample, underwater sample, surface voice,
underwater voice, last frame. `rf_scene_player_water_audio_values[2]` retains
actual speed and requested surface gain. No harness or fixture is introduced.
