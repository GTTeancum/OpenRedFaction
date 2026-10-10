# Tankbot Chaingun burst-onset audio

Status: 2026-10-10 02:00 Xbox compilation passed at source 5c506c1a.
Build-only evidence: changed attack/audio/parser execution and save restoration
remain runtime-unverified. No fixture, grant, route, forced event or original
asset change was used. See HOURLY-20261010-0200.md for exact build proof.

## Owned source and asset evidence

Read-only inspection of installed `tables.vpp` establishes:

- `weapons.tbl:1771–1830` defines Tankbot Chaingun as continuous/from-eye, with
  primary burst count16 and delay0.1. Its `+Burst Launch Sound` at1792 is
  `tankbot_attack_guns.wav`, near distance7, gain0.9.
- `foley.tbl:1575–1576` independently defines `TBot attack guns` with the same
  waveform at near12/gain0.9. These are different authored declarations.
- `bluebeard.bty:16393–16397` contains the gun waveform with envelope/keyoff
  metadata and no loop declaration. The helper rejects loop-flagged metadata.

Read-only disassembly of owned `RF.exe` confirms:

- `4c3a21–4c3a38` loads the raw burst declaration through434620 into weapon44c.
- `43468f–4346be` reads near/gain and registers the waveform with rolloff1.
- `4269dc–4269e5` keeps a burst request out of the ordinary continuous-owner
  branch. `426a00–426a54` plays descriptor44c only when actor500 equals the
  authored full burst count. No persistent loop owner is created by that path.
- `48aa06–48aa21` routes NPC audio through positional5056a0.
- `5435e6–543619` searches registered names case-insensitively, and543708
  returns an existing sample before the new-record parameter writes. Therefore
  the original registration policy, like this port's bank, is first-name-wins.

The helper requests the actual burst declaration7/.9. Because this port loads
Foley first, the shared sample retains effective12/.9. It does not relabel the
Foley declaration as the burst declaration, overwrite shared parameters, create
an alias, or duplicate PCM. Exact original startup ordering and resulting
attenuation parity remain unverified; practical authored burst onset is the
implemented scope.

The installed `audio.vpp` entry `Tankbot_Attack_Guns.wav` starts at0xc35000 and
contains71114 bytes. Its RIFF extent matches the file. It has a16-byte `fmt `
chunk with PCM format1, mono,22050Hz,44100 bytes/second, alignment2 and16 bits;
the71070-byte data chunk contains35535 frames (about1.612 seconds). These fields
fit `rf_wave_pcm_parse` (`src/core/audio.c`) and the existing Xbox `play_mode`
PCM static-buffer path (`src/platform/xbox/audio.c`). No conversion, new decoder
or playback execution is claimed.

## Bounded implementation

`src/diagnostic/scene_tankbot_burst_audio.inc` optionally preloads one waveform
through the existing shared bank after mandatory and current impact/flight
admissions. It caps the file at96KiB, preserves256KiB of bank headroom, performs
no eviction, and pins accepted PCM against ordinary lazy eviction. Admission
failure remains optional and cannot fail level load or weapon admission.
This startup policy may retain the71114-byte sample on levels without a firing
Tankbot; there is no per-frame loading or demand retry.

Only an accepted, identity-qualified Chaingun primary shot calls the consumer.
Its pre-cadence remaining value follows the existing NPC rifle rule: zero or a
stale value at least the full count requests one sample; positive smaller
values are silent continuation rounds. A failed onset is never retried on a
continuation. Queued primary and no-animation requests still produce exactly
one gameplay shot each, with one burst sample each. This is a presentation
adaptation to current cadence, not reconstruction of the original actor500
state or a promise of exact retail burst scheduling.

The shot path checks resident PCM before ordinary positional native playback.
It retains no actor pointer, voice owner, timer or saved field. The returned
voice is telemetry only. Close restores the previous evictable flag without
freeing PCM still borrowed by normal/native one-shots; ordinary shared teardown
retires voices and frees the bank. No Start/Stop sound, loop fallback, extra
bullet, inventory change or impact-audio expansion is introduced.

## Narrow scene hooks

The scene owner supplies these hooks; this worker changes no existing file:

1. Forward-declare `scene_tankbot_burst_audio_load(rf_vpp *)` and
   `scene_tankbot_burst_audio_close(void)` before `campaign_audio_open`.
2. Include the helper beside `scene_npc_rifle_audio.inc`.
3. Close alongside flight audio at `campaign_audio_open` entry and before
   the mixer/native/bank teardown in `campaign_close_movers`.
4. Call `scene_tankbot_burst_audio_load(&archive)` immediately after optional
   flight admission in `campaign_audio_open`, before final bank statistics.
5. Reset beside NPC rifle audio during `scene_miner` initialization.
6. In accepted NPC fire presentation, use the new qualified Chaingun identity
   branch to add `scene_tankbot_burst_audio_shot(definition, burst_before,
   once, owner->eye_position)` to the existing sound-request counter. Its
   return value is a request count, never gameplay success/failure.

`rf_scene_tankbot_burst_audio[12]` records accepted shots, requests, starts,
suppressed continuations, failures, last status/sample/voice, pre-cadence
remaining, queued-single flag, retained file bytes and readiness. These are
read-only diagnostics; none forces firing, admission or inventory.
