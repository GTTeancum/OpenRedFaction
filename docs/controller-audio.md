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
