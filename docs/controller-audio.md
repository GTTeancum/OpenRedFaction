# Controller audio integration

Current work provides a bounded PCM resource adapter, not an original audio
engine or audible playback. rf_wave_pcm_parse borrows a RIFF/WAVE buffer and
returns PCM data, frame count and format. It supports PCM8/16 mono/stereo,
checks RIFF/chunk extents and frame alignment, handles odd chunk padding and
metadata, and rejects duplicate fmt/data chunks. No allocation or conversion;
errors preserve the output view. The caller owns resource retention/budget.

verify_wave_pcm.py passes81 PC/NXDK execution cases. Python wave independently
checks the authored format and sample bytes; malformed fixtures cover every
truncated prefix of a small valid file, duplicate/missing chunks, unsupported
encoding/bit width, channel/block alignment, and incomplete frames. This does
not compare the original decoder or exercise the live Xbox audio device.

Installed audio.vpp resources:
- DoorOpen_07.wav:11025Hz mono PCM16,28485 frames,56970 sample bytes.
- DoorEnd_07.wav:11025Hz mono PCM16,18027 frames,36054 sample bytes; a LIST
  metadata chunk follows the audio.
- Combined PCM93024 bytes (about91KiB), file payload93186 bytes.
- Exact authored DoorLoop_2.5.wav is absent from the installed VPP inventory.
  Do not silently replace it with another DoorLoop file. Name resolution and
  any original fallback remain to be established.

Original evidence:469250 loads four authored strings through5054b0, then
5054d0 when a valid sample index is returned.5054b0 calls543580;543580 performs
name lookup/registration and calls544680.5054d0 calls543760 for loading.
46a120 chooses initial/reverse sound and optional moving sound.46a0d0 stops
handle320 when present, clears it, then requests sample2e0 and retains324.
These are source addresses in original RF.exe SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

Local NXDK evidence:lib/hal/audio.h supports16-bit stereo output and recommends
SDL2 for applications. samples/xaudio/main.c demonstrates48kHz output and warns
about FPU use in DPC callbacks. The project already links NXDK SDL. Next work:
resolve sample-name semantics, retain bounded source PCM, mix/resample with
shared state, connect controller start/stop requests, then add platform output
and native audio capture verification. No generated replacement sound assets.

## Shared output mixer

rf_audio_mixer is an Xbox/PC platform adapter rather than original Miles
mixer recovery. It retains16 borrowed PCM voices in836 bytes on x86. Caller
serialization and PCM lifetime are explicit. Input remains at its source rate;
integer linear interpolation produces48kHz signed16 stereo without pre-expanded
resources, heap allocation or floating-point work. Per-channel gain uses
0..32768; summation saturates after all voices. Loop/one-shot progression uses
an exact integer rate remainder. Handles retain generation/slot identity and
stale stops cannot stop a replacement voice (generation wraps after65534).

verify_audio_mixer.py passes144 cases and37008 stereo frames against an
independent integer reference, comparing PC17-frame chunks to single-call
NXDK output. Covers PCM8/16, mono/stereo, six source rates,1/2/16 voices,
negative interpolation, clipping, gain, loop endpoints and completion. PC
also checks full-pool rejection and stale stop handles. Both builds, the81
PCM-parser cases and six CTests pass. This does not prove audible hardware
output, spatial attenuation or concurrent producer/callback ownership.

Further lookup evidence:544680 calls523ce0 with the requested filename.
543580 registers only when that existence/open gate succeeds; otherwise it
returns-1.56baa0 looks up metadata by name; its fallback metadata pointer does
not replace the requested filename before544680.543760 also reports a missing
file and returns-1 when its later open fails. No alternate DoorLoop file is
selected in these paths. This is code inspection, not full original filesystem
execution. Live sample registry/loading and output remain the next integration.

## Bounded sample ownership

rf_audio_bank owns a caller-sized slot array and loaded file buffers under
one explicit budget. It is scoped to one borrowed archive and one level,
deduplicates names case-insensitively, keeps stable PCM pointers, and performs
no eviction. Missing, malformed or over-budget loads preserve index/count
and existing storage. Capacity is bounded by the original2600 registration
ceiling; callers should size it for actual level references. Whole retained
WAV files are charged, including metadata and slot/bank bytes, excluding
allocator overhead. Stop all borrowing voices before closing the bank.

verify_audio_bank.py verifies PC ownership with the two authored door files:
93402 retained bytes, exact-budget success, one-byte-short rejection, duplicate
lookup, missing loop rejection, sample access after archive closure and repeat
close. First256 mixed stereo frames after archive close match an independent
Python wave/integer-resampling hash3154186473. Both platforms build; existing
144 PC/NXDK mixer cases and six CTests pass. Ownership has not yet run in live
XEMU; scene loading, controller start/stop and device output remain open.

## Live controller integration (supersedes the earlier ownership status)

The campaign loads controller-authored names from the sibling audio.vpp once,
deduplicates them in a 1 MiB bank, closes the archive, and retains PCM until
level teardown. Mixer voices are cleared before the bank is freed. Four L1S1
resources occupy 113848 bytes including slots; five nonempty names are missing,
with no malformed or over-budget resources. Empty names remain sample -1.
The Xbox build script stages the local original archive in the ignored disc
directory; its 246208512 disk bytes are never allocated wholesale in RAM.

Live activation and translation sound requests now call the existing 46a120
selection helper. Arrival follows the inspected 46a0d0 stop-moving-handle then
play-arrival-slot sequence. Alert/wakeup work remains pending independently.
The output adapter currently plays at unity gain without spatial attenuation
or metadata looping. Those parameters remain unreconstructed; this is not
original Miles output equivalence or audible device playback.

Each controller tick mixes 800 stereo frames into a reused 3200-byte buffer.
LIVE_AUDIO reports loaded count, bank bytes, missing names, rejected resources,
successful voice starts, unavailable requests, mixed stereo frames and a rolling
FNV-1a PCM byte hash. All eight words match PC and stock64MiB XEMU in the
180-frame traversal replay (replay-20260910-173237). Two valid voices and two
unavailable requests produce 143200 frames with hash 3527213817.
verify_live_door_audio.py independently decodes the original WAVs and computes
that entire output using the original-verified arrival tick 145. It confirms
143116 nonzero frames. This proves live event-to-PCM wiring, not device output.
The 420-frame closing/reversal replay also matches PC/XEMU, including three
valid voice starts, five unavailable requests, 335200 stereo frames and hash
4013480469 (replay-20260910-173344). Door traversal and both reversals still pass.

Outstanding: original metadata/volume/range/attenuation, looping where authored,
device queue and underrun handling, audible Xbox/PC output, and broader effects.

## Device probe: reproducible XEMU failure, not enabled in normal play

An optional synchronous scene PCM sink now feeds an Xbox SDL device probe.
The application queue is fixed at 3200 stereo frames (12800 bytes), protected
by SDL's device lock, and performs no allocation in its producer or callback.
Full-queue submissions are dropped and counted; empty callback frames become
counted silence. The installed NXDK SDL driver uses two 4096-byte contiguous
device buffers, in addition to SDL's own bookkeeping/work storage.

Only the explicit audio-output.flag enables this probe. Normal interactive
and replay runs retain deterministic PCM mixing without opening the device.
Use xemu_replay_check.py INPUT --door --audio-capture for the failing diagnostic.
It saves/restores the flag, selects QEMU's WAV backend, reads queue and shutdown
telemetry through QMP, and rejects a silent capture or incomplete consumption.
The harness still checks all existing movement/rendering evidence.

XEMU 0.8.136 (fc24584ce88f0915ad7f04775bb7712c2e3f49ee), stock64MiB,
replay-20260910-174126: all 180 scene frames submitted, only 3200 audio frames
queued, zero consumed, 140000 dropped, zero callbacks/nonzero samples. Teardown
phase 2 identifies SDL_CloseAudioDevice as the wait. The earlier 173803 run
likewise timed out and produced a silent WAV. Processes were reaped and disc
flags restored; neither run is a playback success. Queue correctness under a
working callback and actual hardware playback are not yet verified.

The symptom agrees with the open [XEMU XAudio callback report #1529](https://github.com/xemu-project/xemu/issues/1529).
Installed SDL_xboxaudio.c waits on a semaphore posted by the XAudio DPC, and
SDL_CloseAudioDevice waits for that thread to exit. This explains the observed
wait but does not prove the underlying emulator defect's precise cause.
The [QEMU WAV backend](https://www.qemu.org/docs/master/system/invocation.html)
captures the emulator's device output without desktop or host input.

Next backend candidate: [Ryzee119/nxdk-audio](https://github.com/Ryzee119/nxdk-audio),
inspected locally at fc2deca2cc1e434805ac03ca7c2f500b3b028f36. It uses the APU
voice/DSP path and declares MIT in its source; its HRTF attribution includes
CC BY 4.0 material. No code from it has been incorporated. Evaluate bounded
memory, build dependencies, callback lifetime and provenance before integration.
Device playback must also use an independent audio clock: consuming 800 frames
per simulation tick alone would leave gaps when rendering cannot sustain 60Hz.
