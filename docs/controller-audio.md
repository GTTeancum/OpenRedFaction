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

## APU evaluation: first stock64MiB DSP output and clean shutdown

Run `python tools/build_apu_probe.py`, then `python tools/run_apu_probe.py`.
These build and boot an isolated image, leaving the campaign image and emulator
configuration untouched. Original DoorOpen_07.wav is extracted into the ignored
probe disc with its SHA256 checked. Dependencies, generated DSP code, binaries,
sample data and captures remain ignored. The game does not yet link this backend.

The builder pins nxdk-audio to fc2deca2cc1e434805ac03ca7c2f500b3b028f36 and
checks its tracked tree for modifications. DSP assembler v0.1.3 is downloaded
from mborgerson/dsp56300 and checked against SHA256
ff031c6daf89f4c2c78a5943920bf483b044e8ec805feaf1efe32c80f991b09c.
NXDK Clang 21.1.8 compiles the backend with C23; its DSP source assembles with
two interrupt-vector warnings, retained in the build output. No Rust install
or SDK modification is necessary. Build provenance includes the adapted source
and generated DSP hashes.

Two local adaptations are applied to a build-directory copy, preserving upstream
source/license notices and leaving the cloned source unchanged:

- Pin the ordinary image-data AC97 descriptor pages before physical-address
  lookup/DMA, then unpin after hardware shutdown. Unmodified initialization
  stopped in the debug kernel; pinning allowed voice creation and playback.
- Keep static sample pages locked until destruction/replacement. The upstream
  completion DPC unlocked them while retaining the buffer pointer, and destruction
  unlocked them again. Moving completion unlock inside the streaming branch
  allowed static voice destruction and complete backend shutdown.

An extra probe-only change exposes the DMA buffer for read-only observation.
Neither adaptation is yet a comprehensive audit of upstream streaming, replay,
thread safety or error cleanup. The static sample probe covers one mono PCM16
11025Hz voice and natural completion only.

Native run apu-20260910-175947 passes on stock64MiB XEMU: 28485 input frames,
2563ms to completion, 176160 requested hardware-buffer bytes and a measured
44-page (180224-byte) allocation. Available pages return exactly from 15784
to the 15828 baseline after shutdown. A retained 8192-byte snapshot of the
guest DSP output ring contains nonzero PCM; no output equality with Miles or
host-speaker audibility is claimed. Run 175846 also passed this scope.

Capture correction: in this XEMU version, the APU monitor feeds SDL directly,
so QEMU's AC97 WAV backend can stay silent while APU voices run. The SDL disk
driver is not compiled into the installed emulator. The probe therefore uses
the SDL dummy output and explicitly enables `audio.use_dsp=true`, then verifies
guest DSP DMA output. Without DSP emulation enabled, that output ring stays
zero even when the voice reaches its end. This follows inspection of the exact
[XEMU gp_ep.c](https://github.com/xemu-project/xemu/blob/fc24584ce88f0915ad7f04775bb7712c2e3f49ee/hw/xbox/mcpx/apu/dsp/gp_ep.c)
and [monitor.c](https://github.com/xemu-project/xemu/blob/fc24584ce88f0915ad7f04775bb7712c2e3f49ee/hw/xbox/mcpx/apu/monitor.c).
The snapshot is an unordered ring observation, not a full sound recording.

Next: audit repeated/static voice lifecycle, connect campaign start/stop/reset
with sample ownership, validate DSP output in the full memory-constrained scene,
and provide PC device playback using an independent audio clock. Real hardware
and a linear recording of final device output remain unverified.

### Static voice lifecycle follow-up

Run apu-20260910-180147 extends the same stock64MiB probe: replay the retained
buffer after natural completion, stop/destroy/recreate eight times, shut down,
initialize the backend again, create/play/stop/destroy once more, then shut down.
All checks pass with available pages restored to 15828 after both shutdowns.
The original first-play DSP snapshot remains nonzero. No additional backend
adaptations were needed for these cases. The harness requires the replay to
last 2400-3200ms and every requested stop to finish within 1000ms; failures
remain explicit. This covers the static voice lifetime needed for door effects,
but not simultaneous voices, streaming, queue exhaustion or campaign integration.

### Live campaign APU integration

The campaign now links the adapted pinned backend described above. Prepare it
with `python tools/build_apu_probe.py --backend-only` before the Xbox build;
this writes ignored `build/nxaudio` sources, DSP data and provenance. Invoke
NXDK builds through MSYS2 Bash, not the extensionless nxdk-cc script directly
from PowerShell. Upstream license notices remain in the prepared source.

Shared controller sound start/stop events carry the deterministic mixer's logical
handles to sixteen Xbox static-voice slots. Hardware advances each sample with
its own audio clock. The shared simulation mixer still generates the previous
PCM hashes for regression checks. Reset synchronously stops/destroys voices and
shuts down the backend before the campaign frees its owned sample bank. Stopped
slots are reused. Normal interactive Xbox input enables this path automatically;
bounded native replays enable it with `--audio-capture`.

Stock64MiB XEMU replay-20260910-180755 passes the 180-frame door sequence with
two successful device plays. Replay-20260910-180905 passes the 420-frame closing/
reversal sequence with three plays. Both have zero rejected plays, one completed
reset, no forced-shutdown fault, and identical PC/native gameplay and shared PCM
results. All six CTests and PC/NXDK builds pass. The backend initialization costs
44 pages; adapter slots plus retained diagnostic snapshot occupy 11712 bytes.
Whole-scene after-close page counts cannot be compared with pre-init counts
because other scene allocations occur between those measurements.

Guest DSP ring snapshots contain 4094 and 4090 nonzero int16 samples respectively
out of 4096. Their SHA256 values are
`2cb1593b282bae94a666d038d14026da6bd186222c6717d357b14ad424086f95`
and `546f9de34959bb0c88f780f59fdd5b0a68f9e7f4dad12bab4a69c47badf0ebf4`.
These prove device-side output, not a linear recording, host audibility or Miles
output equivalence. Each replay peaked at one active hardware voice.

Remaining work includes spatial/gain/loop metadata, overlapping voices and pool
exhaustion, PC device playback, full backend failure-path auditing, and listening
on actual hardware. The timeout fallback stops hardware before unlocking retained
PCM pages but has not been fault-injection tested. Missing authored sound names
remain unavailable rather than being silently replaced.

### Production adapter capacity and shared-page ownership

Stock64MiB XEMU run apu-20260910-181512 links the actual Xbox adapter into
the isolated probe. Sixteen simultaneous static voices borrow the same original
DoorOpen_07 PCM storage. After 50ms all sixteen remain active and guest DSP output
is nonzero. A seventeenth request increments the rejection counter without an
additional successful play. Stopping the first logical handle allows a replacement
request (seventeenth successful play); stopping that old handle again does not
increment the stop counter or address the replacement slot.

Reset while the voices remain active completes without the forced-shutdown path,
and available pages return exactly to the 15825 pre-initialization baseline.
The probe includes the previous natural completion, replay, eight recreation and
reinitialization checks. Its new adapter telemetry is [16,1,17,15825,0]. The extra
adapter storage explains the lower image baseline versus earlier isolated tests.
This checks shared-page lock balancing and capacity behavior, not individual
contributions to the mixed DSP waveform, spatial fidelity or failure injection.

### Partial initialization allocation failures

Stock64MiB XEMU apu-20260910-181638 passes fault injection at each of the ten
contiguous-allocation calls in nxAudioInit. The isolated builder wraps only its
copied allocator expression; `--backend-only` production preparation exits before
this instrumentation. For failure positions 1 through 10, the production adapter
returns RF_IO, close is safe, and available pages equal the 15825 baseline.
With injection disabled, the same process then passes the sixteen-voice capacity,
reuse and shutdown checks. No production backend fix was needed for these paths.
This validates partial initialization cleanup, not actual fragmentation behavior,
interrupt failures or the voice-stop timeout fallback.

### Spatial dispatch recovery, pending execution comparison

Further inspection of the fingerprinted original locates attenuation in 505740,
called by positional dispatch 5056a0. Ghidra parameter types here are unreliable;
the second argument is a position pointer, not a scalar float. Disassembly confirms
sample records at 01cd3ba8 with stride 0x40: offsets 0x24, 0x28 and 0x2c supply
the near distance, far cutoff and attenuation factor. Constants at 5893f8 and
5893e0 are binary32 1 and 0. Beyond the far cutoff both output values are zero.
Otherwise attenuation uses volume / ((distance / near - 1) * factor + 1), with
special branches inside the near distance and for a zero denominator, followed
by the original clamp helper. Original x87 intermediate precision matters.
Listener/vector helpers and their calling conventions still require verification
before reconstructing the complete positional calculation.

505560 indexes the volume array at 01753c18 with its second argument. Thus the
callback argument currently named flags represents a volume-group index on this
path, not a looping bit mask. The sample-record byte at offset 0x3e selects
543a80 versus 5439d0. Those wrappers pass 1 versus 0 respectively as the fourth
argument to 522530. This is evidence for a sample-authored playback-mode branch;
confirm 522530 and the metadata parser before assigning final loop semantics.
Do not infer looping from the filename or from the controller callback's zero.
The current unity/nonspatial adapter remains explicitly provisional.

Next verification should execute original 505740 against bounded distance/range/
factor cases, retaining original vector helpers and x87 rounding, then compare
shared C output before connecting listener updates and per-sample metadata.

### Original positional arithmetic executable check

`python tools/verify_positional_audio_original.py` executes original 505740 and
its vector subtraction, magnitude, normalization, dot-product and clamp helpers
unchanged in Unicorn, with x87 control 0x027f and the checked RF.exe fingerprint.
2592 cases pass: three near radii, two far cutoffs, four attenuation factors,
three input gains, each signed coordinate axis, and zero/inside/at-near/beyond-near/
at-far/outside-far distances. Listener position is zero and its right vector is
(1,0,0). Exact returned binary32 gain and pan match the independent expression.
Output digest: 954a17afa0d7536584ed8d8b968ce2aa449478a00bb1e0fd93bf8eb89d2c63a1.

The far boundary remains audible; only strictly greater distances mute. Zero
separation returns centered pan. The pan is the normalized source-minus-listener
vector dotted with the vector at 01753c28. This is an original-code arithmetic
check, not yet a shared C/NXDK comparison; general directions and listener motion
remain necessary before spatial integration.

### Shared positional C implementation

`rf_audio_position` in src/core/audio.c now implements the recovered position,
distance, attenuation and pan calculation. It accepts listener position/right
axis and sample near/far/factor values explicitly, preserving binary32 stores
around double arithmetic. The existing executable harness compares all 2592
cases byte-for-byte with PC output and NXDK-compiled code executed in Unicorn;
both pass with the same original output digest. PC and Xbox game builds and
all six CTests pass. This is compiled-Xbox arithmetic evidence, not a new native
XEMU playback check. The function is not yet connected to live controller audio.
General-vector rounding, sample metadata and listener updates remain open.

### General directions and normalization precision

The positional harness now adds 4096 deterministic random cases (seed 0x505740)
with translated listeners, arbitrary source directions, normalized listener right
axes, and varied near/far/factor/gain values. The first run found a pan mismatch
at random case 5, despite the earlier axis tests passing. Disassembly of 4faaf0
shows its reciprocal remains in x87 precision until each normalized component
is stored; 40a0b0 sums z, then y, then x. The shared C now retains a double
reciprocal and follows that dot-product order.

All 6688 cases match original returned bytes exactly in PC and NXDK-compiled
code with x87 control 0x027f. Output digest:
6c84804d4f1a30bda0e2dd0de1a5de32d9a6bac64eb1bfc6639ba44e2743c150.
Both game builds and six CTests pass. This establishes the tested finite input
range, not exhaustive floating-point equivalence. Sample metadata, playback gain
conversion and live listener updates remain separate integration work.

### Registration parameters and far cutoff provenance

Original 469250 registration was checked against machine instructions because
Ghidra folds the pending stack arguments into the intervening 4ff480 string
accessor call. At 46944c..46945b it pushes rolloff 1, the authored value through
EBX, near distance from the local (5 normally, 10 after the L14S2 comparison),
and finally the returned filename before calling 5054b0. The wrapper passes
these to 543580 with category 0. The deduplication branch in 543580 returns an
existing record before applying new registration parameters, so first registration
wins and scene loading order matters.

543580 stores default volume at record +0x20, positive near distance at +0x24
(substituting 1 when the requested near value is nonpositive), rolloff at +0x2c,
and category at +0x34. Far distance (+0x28) comes from 544960:
(1 - 1/rolloff) * near + (near * default_volume) / (rolloff * threshold),
where threshold at 5894f4 is binary32 0.05 (0.05000000074505806).
543a60 later multiplies the category gain at 01cd3b94, default volume, and
requested gain. These formulas still need an executable comparison before use.

The installed tables.vpp also contains sounds.tbl, whose entries specify filename,
near distance, default volume and rolloff. Its comments describe rolloff direction
opposite to the recovered attenuation formula; executable behavior is authoritative.
Controller registration must not blindly replace its own parameters with table
rows, especially given first-registration deduplication. Metadata lookup 56baa0
returns a separate 180-byte filesystem record; its +0xa8 bit30 and +0xac bit29
feed sample playback fields. Their source and loop semantics remain to be traced.

### Shared far cutoff calculation

`rf_audio_far_distance` reconstructs 544960 with the original binary32 threshold
0.05. `python tools/verify_audio_range.py` executes the unchanged original and
NXDK code, stores their x87 returns to binary32 as registration does, and compares
the PC probe output. All4221 cases pass:125 parameter-grid cases plus4096 seeded
random cases with positive near/rolloff and nonnegative volume. Output SHA256:
99506b24e061c1bed56603e236901c21b36e8631c79e6cb43815d42019d194fc.
The6688 positional cases still pass, as do both builds and six CTests.
Registration normalization, deduplication ordering and category gain are excluded
from this arithmetic check; the function is not yet used by live sound loading.

### Campaign registration metadata ownership

The owned sound bank now retains near distance, derived far cutoff, default volume
and rolloff per sample. `rf_audio_bank_register` preserves parameters on duplicate
filenames, normalizes nonpositive near distance to one, and rejects nonfinite or
unsupported parameters before allocating. The older load-only adapter registers
unity defaults. The current campaign registers each controller's authored volume,
near5 (10 for L14S2.rfl), and rolloff1. Earlier global table registration and
filesystem loop flags remain absent, so global registration order is not yet
faithfully reproduced. Stored parameters are not yet applied to playback gain.

The PC bank check verifies retained first-registration parameters when a duplicate
requests different settings. Its two-sample budget grows from93402 to93434 bytes;
the L1S1 scene reserves20 slots and grows from113848 to114168 bytes (320 extra).
The180-frame PC door replay and independent full PCM reference remain unchanged.

Stock64MiB XEMU replay-20260910-182835 also passes the180-frame door replay
with APU output enabled, matching PC state and PCM and clean device shutdown.

### Native running-voice channel updates

Stock64MiB XEMU apu-20260910-183015 extends the isolated APU probe with channel
updates on one running original door sample. It sets front-left only, front-right
only, then both muted through nxAudioVoiceSetChannelGain, waiting150ms after each
change (over three8192-byte stereo ring durations). Each unmuted side has nonzero
DSP samples, each opposite side is entirely zero, and the final ring is entirely
zero. Voice state remains active throughout; no restart occurs between updates.
The prior lifecycle, initialization-failure and sixteen-voice checks still pass,
with available pages restored to15824 for this image.

This verifies channel routing and mute on native emulated APU output. It does not
establish intermediate gain calibration, original DirectSound pan conversion,
host audibility or real hardware. The production scene still needs listener-driven
updates wired through its event adapter after those semantics are settled.

### Device volume quantization and tables

`python tools/verify_audio_device_volume.py` executes original521680 table
initialization and522420 lookup, including original ftol, without intercepting
arithmetic or calling a sound device. All202 entries and17584 lookups pass
independent formulas at x87 control0x027f. Inputs include each half-step boundary
and4096 seeded values spanning -1 to2; all four selection-flag combinations run.

The volume index is trunc(volume*100+0.5), clamped0..100. The default table has
entry0=-10000; remaining entries are trunc(1000*log2(i*q)+0.5), with q equal to
the original binary32 0.01. Only when both bytes01aed340 and01aed360 are nonzero
does lookup select trunc(0.5-(1-i*q)*10000). This is not the usual log10 amplitude
formula. The meanings of those mode flags still need recovery. Generated tables
and the report remain ignored under artifacts/audio-device-volume.json.

Disassembly52261c..522641 also confirms pan is multiplied by1000 then converted
by ftol before the device vtable+0x40 call. Volume goes to vtable+0x3c. This
establishes the original integer conversion; the device interpretation and mapping
to Xbox gains remain separate work. No host audibility or native gain-equivalence
claim follows from this executable arithmetic check.

### Shared device-volume conversion

`rf_audio_device_volume` now reproduces522420 in shared C. Its default curve
uses a101-entry int16 table (202 bytes) generated from the verified521680
expression; the alternative linear curve is computed directly. Neither path
allocates or evaluates logarithms per call. The caller explicitly selects the
mode; interpretation of the original two global flags remains open.

The updated executable harness compares the compiled PC probe and NXDK code
against all17584 original lookups, including half-step boundaries and out-of-unit
inputs within -1..2. All pass exactly. Both game builds and six CTests pass.
The result remains an integer device attenuation value, not a linear PCM gain;
this helper is not yet connected to the campaign's playback adapter.

### Device attenuation to channel amplitude

`rf_audio_device_gains` converts integer device volume/pan into linear L/R
amplitude, adding pan attenuation only to the quieter side. Amplitude is
10^(hundredths_dB/2000); no artificial zero cutoff is added. It accepts volume
-10000..0 and pan-10000..10000, rejecting invalid values without changing output.
This is a platform adapter based on documented DirectSound semantics, not a
reconstructed RF function. The original pan multiplier1000 therefore produces
at most10dB side attenuation for a normalized positional pan of magnitude1.

Microsoft documentation confirms cumulative volume/pan attenuation and the
hundredths-of-dB units:
https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708938(v=vs.85)
https://learn.microsoft.com/en-us/previous-versions/windows/desktop/mt708939(v=vs.85)

`python tools/verify_audio_device_gains.py` passes76 boundary/directional/error
cases against independent amplitude expressions in PC and NXDK-compiled code,
using2e-7 relative tolerance for math-library rounding. Both game builds pass.
The adapter is not yet wired into playback; native intermediate-gain calibration
and original-device listening equivalence remain unverified.

### Native intermediate-gain calibration

Stock64MiB XEMU apu-20260910-183743 passes the added gain-calibration phase.
A4096-sample synthetic periodic square wave (PCM16 mono48kHz, amplitude8192,
period64 samples) loops through a static APU voice. Shared rf_audio_device_gains
sets unity, -6dB centered, +10dB pan, -10dB pan, and -20dB centered, each followed
by150ms settling. Absolute sample sums over the4096-int16 interleaved DSP ring
are normalized against each channel's unity sum. All measured ratios are within
3% of the requested amplitudes. The periodic waveform and full-period ring span
avoid comparing different parts of an authored sound. No voice restart occurs
between changes. Sample storage is probe-only, not added to the game.

The previous original-sample, channel-mute, lifecycle, overlap and allocation
failure checks still pass. After calibration, available pages equal the15820
image baseline. This verifies the mathematical gain adapter through emulated
APU output for the tested levels, not original DirectSound output, host speakers,
real hardware, arbitrary waveforms or live campaign listener integration.

### Logical-handle gain updates

The scene device-event interface now supports gain updates addressed by logical
voice handle. The Xbox adapter resolves the handle and updates the existing APU
voice channels. Invalid gains are rejected; unknown handles do not affect another
voice. Shared rf_audio_voice_gain similarly changes only Q15 left/right gains,
rejecting stale handles and invalid ranges without changing mixer state.
The PC bank probe verifies phase/frame preservation and subsequent muted-left,
half-amplitude-right samples, including invalid-update state preservation.

Native apu-20260910-184000 moves calibration through the production adapter's
play/gain/reset path and passes all five gain settings with one successful play,
no rejection or forced-shutdown fault, and restored15799-page image baseline.
The synthetic probe sample is now one second (96000 bytes, probe only) to allow
all five150ms settling intervals without restarting. An earlier short-sample
attempt183920 failed gain ratios and was replaced. Both game builds and the
PC bank check pass. Campaign listener calculations still need to call this new
interface; normal gameplay audio is not yet spatialized.

### Listener pass and positional voice refresh

Listener writer505ec0 copies the supplied position into01754160 and the first
orientation axis into01753c28, then traverses30 positional voice records. For
registered positional entries it calls5058c0 with the stored source position,
stored requested volume and generation/slot handle. This confirms that the
listener pass refreshes existing voices; controller movement does not by itself
replace a sound's copied source position. Calls to505ec0 occur at433624,480eef
and50606f; the normal frame caller433624 supplies stack position/velocity/basis,
with an alternate player-state adjustment immediately before it.

5058c0 checks slot<30, generation equality and positional marking, copies the
source and requested volume, computes505740, multiplies by the voice's volume
group, then calls544390(volume) and544450(pan). Initial5056a0 instead passes
attenuation multiplied by requested volume through505560. Thus initial and
refresh paths must not be assumed identical for non-unity requested volume;
544390 needs tracing to establish where sample default gain is applied.

Current scene.c executes controller/audio ticks before the render callback
calculates its first-person camera pose. Wiring a cached camera directly there
would use the previous render's listener pose. Preserve/update pose ownership
explicitly and verify the original frame ordering before connecting spatial
refresh. Camera inspection overrides should not silently move the gameplay
listener. No runtime audio behavior changed during this investigation.

### Executed refresh gain path

`python tools/verify_audio_refresh.py` runs80 combinations through original5058c0,
544390/544450 and522d30/522d80, retaining original spatial math, clamp, volume
tables and ftol. Only device-handle resolution and final vtable setters are
intercepted. Both final setter arguments match. Cases vary position, requested
volume (.25,.5,1,2), group gain and sample default volume independently.

Confirmed: refresh applies requested-volume attenuation and volume-group gain,
clamps gain0..1, then uses522420. Changing sample default volume does not change
those refreshed setter values (the test holds far cutoff fixed). Pan clamps to
-1..1 then truncates pan*1000. No543a60/default-sample gain step occurs in refresh.
That differs from initial playback, which does apply sample/category gain.
This behavior must be preserved rather than normalized to one common gain formula.
The hardware handle resolver is stubbed, so this check does not validate device
handle lifetime or the still-unconnected campaign listener pass.

### Listener pose separated from projection

scene.c now factors gameplay eye/look calculation into actor_listener_pose.
It contains the existing eye offsets, stance transition, look update and body
orientation/tensor update and is invoked once at the same frame point as before.
World projection, visibility and particle-inspection camera overrides remain in
the following view preparation. This exposes the current gameplay pose for an
audio refresh before diagnostic camera changes, without computing look twice.
Both builds and the PC180-frame door replay pass; this refactor does not yet
change audio timing or apply spatial gains.

Stock64MiB XEMU replay-20260910-184648 passes with APU enabled after the
pose refactor, including the PC/native camera and gameplay comparisons.

### Live Xbox listener-driven gains

Campaign source positions/requested volumes are now retained per logical voice.
After actor_listener_pose, before inspection-camera overrides, the scene refreshes
spatial gains using the current gameplay eye position and right axis. Initial
playback uses the previous listener and sample default volume; refresh follows
the separately verified path without reapplying that default. Changed integer
volume/pan settings are converted to channel amplitudes and sent to the Xbox
adapter without restarting the voice. Fixed tracking costs536 bytes.

Stock64MiB XEMU replay-20260910-184932 passes the180-frame door replay and
matches PC spatial telemetry exactly:2 initial updates,214 refresh updates,
integer settings hash3428625177,216 noncenter updates and209 changed settings.
APU output is nonzero with no rejected voice requests or shutdown fault. Camera,
physics and other replay state still match; six CTests pass. The PC compile error
from an initially missing telemetry declaration was fixed before this replay.

The deterministic shared PCM diagnostic deliberately remains unity/nonspatial;
its unchanged independent reference does NOT validate spatial device samples.
PC computes the same spatial settings but still lacks a device backend. Original
volume-group/category controls currently remain unity, the default device-volume
curve is selected, global registration precedence and loop flags remain unfinished.
Natural device completion and logical mixer completion have independent clocks;
tracking slots may continue to refresh a completed hardware voice until reused.
Original whole-frame timing and an independent full spatial output recording
remain unverified. This is live Xbox spatial gain wiring, not full audio parity.

### Closing/reversal spatial replay

Stock64MiB XEMU replay-20260910-185100 passes the420-frame closing/reversal
sequence with spatial APU output enabled. PC/native spatial telemetry matches:
3 initial updates,694 refresh updates, integer gain/pan hash2739072757,
697 noncenter updates,390 changed settings,536 fixed tracking bytes. Gameplay
still reports two closing reversals and six controller arrivals. Device plays,
DSP output and shutdown checks pass. This broadens live integration evidence to
voice reuse and changing listener position; it does not resolve the remaining
independent-clock ownership, sound-group/loop or PC-device work noted above.

### PC output backend

The Windows campaign frontend now installs a WaveOut event backend for interactive
--campaign mode. Four512-frame stereo48kHz buffers are refilled by a worker when
the device returns them. Each PCM voice runs through the shared integer mixer on
this audio clock, using the same sample bank and logical play/stop/gain events as
Xbox. A critical section serializes voice updates and mixing. Reset wakes and
joins the worker before releasing bank references, resets/unprepares the device
buffers, closes handles and clears mixer state. Headless replays do not open audio.

Fixed backend headers, samples, mixer and handle map occupy9220 bytes; Windows
thread/device allocations are additional host-platform overhead. The nominal
queued sample capacity is2048 frames (42.7ms). There is no allocation per refill.
If no output device opens, the visual diagnostic remains available without audio.

`rf_pc_audio_check` passes two open/play/gain/stop/reset cycles with a quiet
synthetic PCM sample:10752 generated frames and21504 nonzero channel samples per
cycle, no reported device API failure. This observes generated PCM and successful
WaveOut calls, not loopback capture or listening fidelity. The full PC build,
six device-independent CTests and headless180-frame door replay pass. The Xbox
backend/source path was not changed by this PC-only addition. Real-time underrun
measurement, complete campaign listening and wider failure-path testing remain.

### Initial gain before device playback

The platform play event now carries left/right amplitudes. The scene computes
initial spatial settings before dispatch; Xbox sets channel gains before starting
the APU voice, and PC installs the gains under the same lock as voice creation.
This removes the previous unity-gain interval between separate play/gain calls.
Listener refresh behavior and integer spatial telemetry are unchanged.

Both targets build successfully. The PC device check passes two reopen cycles
and a muted-start case with at least2400 generated frames, no nonzero samples
and no device error. Stock64MiB APU run apu-20260910-185745 passes the production
adapter muted-start snapshot and restored-page checks, alongside its existing
voice lifecycle, overflow, allocation-failure and gain calibration checks. The
Xbox snapshot observes silence after150ms; it is not a continuous recording of
the startup interval. Ordering is established by the source change.

Integrated stock64MiB XEMU replay-20260910-190219 passes the180-frame staged
door traversal with APU output and cleanup checks. Spatial telemetry remains
[2,214,3428625177,216,209,536], matching PC. This is not authored-spawn traversal,
physical Xbox validation or a claim of complete campaign audio parity.

### Sound table registration trace

Ghidra identifies434720 as the sounds.tbl loader: after successful open it
requires the Sounds Start marker and reads entries until Sounds End or2048 rows.
The per-row function4347f0 reads one quoted filename and three floating values,
then calls5054b0 and asserts that the returned sound index equals the row index.
The5054b0 wrapper forwards the values to543580 with category0. Its caller4346f0
conditionally invokes the loader after506270, then calls434880 and505480.
The meaning of506270 and ordering relative to campaign controller construction
remain unverified; do not yet replace controller parameters with table rows.

`python tools/verify_audio_registration.py` executes original5054b0/543580,
including original case-insensitive comparison and far-distance arithmetic.
Filesystem presence and metadata are supplied at56baa0/544680. Three ordered
pairs confirm that reversed registration order changes the retained parameters,
nonpositive near distance becomes1, wrapper category is0, and a differently
cased duplicate with changed parameters/category preserves all64 record bytes
and the registry count. This verifies original precedence, not table parsing,
initialization order, metadata flag semantics or a new runtime implementation.

### Table scope and startup gate

`python tools/inspect_sound_table.py` inventories all88 installed sound rows and
compares names against controller records across68 levels. Exactly one overlaps:
L14S3.rfl, Tram Door Right, slot0, Switch_01.wav. Its controller volume is1; table
row37 specifies near6, volume0.9, rolloff1. L1S1 controller sounds have no overlap.
The extracted table SHA256 is
9e42163f04e24aaf65ce41d8fc3879253882cf425b715706c0643214adc33d5a.
The tool reads archived data without checking original assets into Git; its
strict inventory parser is not claimed to reproduce the original parser.

Additional original-code trace:506270 returns byte017543d8, which506170 sets
to1 after successful543310 audio initialization.434880 loads foley.tbl;505480
is empty. The sound-table initializer4346f0 is called at4b22f4 in the case1
path of4b1e70. Controller construction469250 is called at463bf3 in463820,
whose direct caller is460f9e. Connecting those startup/level paths and checking
registry resets remains necessary before claiming precedence during level loads.
Do not preload all table PCM just to retain defaults: metadata precedence and
resident sample ownership must remain separate under the64MiB budget.

### Registered metadata versus resident sample release

Original543930 releases a sample only when audio is enabled, the index is not-1,
record byte+63 is zero and device handle+48 is nonnegative. Backend1 calls522270
with that handle; then the routine stores handle-1 and clears byte+61. It leaves
the name, spatial parameters, category and registration count intact. Bulk543980
invokes this for all2600 slots when audio is enabled. Full reset543450 instead
invokes543730 (release plus name clearing) and zeros the registration count; its
identified direct caller543410 is the audio backend shutdown routine.

`python tools/verify_audio_release.py` passes48 branch combinations against the
unchanged original543930, intercepting only final device release522270. It checks
all64 record bytes and count. A bulk543980 case additionally checks every byte of
all2600 slots, including a live last slot beyond count88 and a retained slot88;
only eligible handles are released and the registry count stays88. Actual device
free, level-transition call scheduling and a C/NXDK ownership implementation are
not covered. This supports separating metadata lifetime from PCM residency; it
does not yet prove that metadata survives every campaign transition.

The level reader460820 handles section0x3000 via463820, which constructs
controllers through469250. Startup-to-level scheduling and bulk-release callers
remain the next trace boundary before changing campaign registration behavior.

### Level-entry counters and cleanup chain

The level-entry path4356d0 calls45c540;45c540 calls506080 before460820 reads
the new level.506080 checks audio-enabled017543d8, visits2600 records, and
increments positive signed counters at+56 only when byte+60 is zero.
`python tools/verify_audio_retention.py` executes that routine unchanged over
full2600-slot fixtures with audio disabled/enabled, negative/zero/positive counters
and both flag states. Every record byte and the registration count are checked:
zero changes when disabled and780 eligible increments when enabled.

The bulk-release caller is a tail jump at5439bd:5439b0 first calls544310,
then invokes543980 only when its argument byte is zero. Its caller5060b0
examines positional and other voice slots, stops/resets those whose sample
counter is below2, then calls5439b0 with its argument. These latter branches
are decompiler traces, not yet execution-verified. Their scheduling and counter
balancing still need recovery before implementing a persistent residency policy.
No runtime ownership change is justified solely by the verified increment.
