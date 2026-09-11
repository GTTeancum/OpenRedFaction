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

### Counter-gated voice cleanup verification

`python tools/verify_audio_voice_cleanup.py` now executes5060b0 together with
original reset callees505680/506140. Only hardware stop5442b0 and downstream
bulk-policy5439b0 are intercepted. Four fixtures cover audio disabled/enabled
and both forwarded keep arguments, with all30 positional and25 other voice
slots populated, sentinel sample indices and sample counters below/at/above2.
Exact voice-record bytes, stop ordering and unchanged sample metadata are
checked. Enabled cases each stop27 eligible voices; disabled cases stop none.
The keep argument does not alter this voice selection and is forwarded intact.

The first fixture attempt reversed the two families' sample/handle fields and
failed; correcting the fixture to the traced layout yields the results above.
Counter balancing remains unresolved. Record getters544700/544650 examined
during this trace do not establish where counters are incremented/decremented.
No persistent runtime ownership implementation follows from this check alone.

### Explicit bank PCM residency

Shared C now exposes rf_audio_bank_unload/reload. Unload frees retained file
bytes, clears the PCM view and deducts its allocation from the bank budget while
keeping the registered name/index/spatial parameters. Repeated unload is harmless.
Sample lookup returns NULL for an unloaded entry. Reload takes an explicitly open
archive, validates the file and budget before committing storage, and retains
metadata on failure. Duplicate registration still returns the same index; callers
use reload explicitly to restore unloaded PCM. No slot size or fixed memory cost
was added. All borrowing device and mixer voices must be stopped before unload.

The expanded PC bank check passes exact byte reclamation, preservation of the
other resident sample and registration parameters, closed-archive rejection,
one-byte-short reload rejection, exact-budget reload and repeated reload. PCM
after reopening then closing the archive reproduces hash3154186473 over the first
256 stereo frames. PC and NXDK builds pass, as does the180-frame PC door replay
with unchanged live-audio hash3527213817. Native runtime unload/reload is not yet
validated. This is explicit resource management, not automatic campaign eviction
or a claim that the unresolved counter policy is implemented.

### Native bank residency with device ownership

Stock64MiB XEMU apu-20260910-192456 passes the production bank/adapter residency
fixture. A small generated VPP contains the original hash-verified DoorOpen_07
sample; no full audio archive or original asset is tracked. Three playback cycles
close the archive, produce nonzero APU output, synchronously reset the backend
to release sample page locks, then unload PCM twice to check idempotence.
Each unload reduces bank-accounted bytes from57150 to136 while retaining the
index and spatial parameters. The two reloads first reject a one-byte-short
budget, then restore exact-budget residency using a reopened archive.

Full decoded PCM FNV hash3315163553 matches across all three cycles and an
independent Python wave decode. Native telemetry is[3,57150,136,3315163553,3].
Existing APU lifetime, allocation-failure, channel/gain and muted-start checks
also pass. This validates explicit bank unload/reload with the production Xbox
adapter, not automatic campaign residency, level transitions or real hardware.
The136-byte figure is retained bank/slot accounting, not whole-process physical
memory. Stop alone is asynchronous and does not release static buffer ownership;
this fixture uses complete backend reset before freeing bank storage.

### Release one Xbox voice without backend reset

The production adapter now exposes rf_xbox_audio_release_voice(handle). It
stops the matching slot, waits for stopped state, destroys the voice to release
static-buffer page locks, and clears that slot. Unknown/stale handles return
RF_NOT_FOUND. A stop failure or one-second state-wait timeout returns RF_IO
without destroying the slot; callers must retain PCM on failure. This API is
serialized with play/reset and is distinct from the asynchronous stop event.
All voices borrowing a shared sample must be released before bank unload.

Stock64MiB XEMU apu-20260910-192722 passes three selective-release cycles.
Each starts bank PCM on the left and independent calibration PCM on the right.
It releases the bank voice, checks repeated/stale handles, unloads the bank PCM,
and observes zero left samples plus nonzero right samples after150ms without
a backend reset. Reload cycles retain matching PCM and original settings.
The full Xbox game build also passes. These are sampled DSP observations, not
a continuous dropout recording. Timeout failure injection, automatic campaign
eviction and physical hardware validation remain open. The previous complete
backend-reset residency test remains historical evidence; this test now covers
individual release while another voice remains active.

### Selective-release failed-observation recovery

Stock64MiB XEMU apu-20260910-192909 passes three injected failed stopped-state
observations followed by release retries. The isolated build modifies only its
copy of the adapter stopped helper to return failure under a test flag. The
production adapter and dependency checkout are unchanged. Each release returns
RF_IO with bank PCM still resident; disabling injection lets the same logical
handle release successfully, proving the slot was retained on failure. Only then
does the test unload PCM and confirm independent right-channel APU output.

This exercises the release error branch and recovery, not an actual one-second
hardware stall, PIO starvation, or the separate backend-shutdown fallback.
Native report release_failure_retries is3; existing selective release, reload,
PCM hash, gain and backend lifecycle checks also pass.

### PC selective borrower release

The PC adapter now exposes rf_pc_audio_release_voice(handle), matching the
Xbox operation's ownership purpose. It clears matching voice records and handle
mappings under the same critical section used by the refill worker. Completed
voice records can also be released; absent/stale handles return RF_NOT_FOUND.
Other voices continue. Already queued WaveOut buffers own their copied PCM and
may play after release, but they no longer depend on the original source pages.
Calls remain serialized with play/close; every borrower must release before
shared bank PCM is unloaded. No fixed backend storage was added.

The PC build and device check pass. The new fixture plays a zero-filled allocated
sample beside an initially muted independent sample, releases the first handle,
checks repeated/stale handles and makes its source pages PAGE_NOACCESS. It then
unmutes the other voice, runs the worker for100ms, releases/closes, and verifies
nonzero output with two successful plays and no device errors. Protected pages
remain inaccessible until after worker shutdown. This provides exercised-path
use-after-release evidence, not exhaustive race analysis or loopback capture.
Existing reopen and muted-start cases also pass. Campaign eviction remains
unconnected; this addition changes only the PC adapter and its device check.

### Register metadata before PCM residency

Shared rf_audio_bank_declare now reserves an existing archive name and spatial
parameters in a preallocated slot without reading or allocating its waveform.
The first declaration/registration wins on case-insensitive duplicates. It
checks directory presence and parameters; PCM validity is deferred until reload.
Registration count/index is independent of residency, and bank bytes do not
grow beyond already-budgeted slot storage. The existing eager register path
keeps its behavior; both share the parameter normalization helper.

The PC bank check passes two declarations in a metadata-only budget, duplicate
precedence, near normalization, missing-name preservation, refused PCM load
under that budget and selective loading after budget expansion. Native stock64MiB
XEMU apu-20260910-193306 starts its residency fixture with a136-byte bank,
declares the sample without PCM, rejects load under that budget, then expands
to57150 bytes and passes playback, release failure/retry, selective unload and
reload. PC and full Xbox builds pass. Global sounds.tbl parsing/registration
order and campaign residency policy are not yet integrated.

### Bounded shared sound-table reader

rf_sound_table_read reuses the existing table lexer and NXDK-compatible decimal
reader in entity_assets.c. It reads Sounds Start/End markers, quoted archive
names and three finite decimal fields, with nonnegative volume and positive
rolloff. It performs a validation/count pass before writing caller-owned rows,
allocates no memory, limits names to60 bytes and rows to2048, and preserves
rows/count on failure. A NULL row array with zero capacity queries required
count. Input/output must not overlap. This port adapter intentionally returns
errors instead of emulating original parser assertion behavior.

`python tools/verify_sound_table.py` passes15 fixtures on PC and compiled NXDK
code executed in Unicorn. All88 installed records match independent inventory
exactly, including binary32 values. Coverage includes one-row-short capacity,
count query, comments, malformed/truncated syntax, negative volume, invalid
rolloff, oversized names, decimal overflow, trailing input and2048/2049-row
boundaries. Output sentinels verify failed calls and unwritten capacity. Both
platform builds pass; XEMU table-file loading and campaign registration remain
to connect. No original parser execution equivalence is claimed.

### Bounded archive loading for sound declarations

rf_sound_table_load reads sounds.tbl from a borrowed open VPP using an explicit
scratch budget for the temporary text. It delegates validation to the shared
reader, releases text before returning and leaves only caller-owned declaration
rows. Caller row storage is separate from the scratch budget. Errors preserve
rows/count; count-only queries use the same validation path.

The expanded verify_sound_table.py passes20 cases. New PC archive checks cover
exact/one-byte-short text budgets, insufficient row capacity, absent sounds.tbl,
a malformed table in a generated VPP, and exact88-row data after archive close.
Existing PC/NXDK reader tests still pass. Both builds pass; native archive I/O
for this loader and campaign wiring remain unverified. Installed directory
inspection also confirms every one of the88 declared names exists in audio.vpp.
No original assets are added to tracked files.

### Campaign global declarations before controller PCM

Campaign scene startup now loads the global sound table with a64KiB text
scratch cap, declares its rows in order and requires each returned index to
match its table row. It then releases the temporary declarations/table archive
before registering controller sounds and loading only their PCM. Existing
global entries preserve their table parameters; newly referenced controller
sounds retain authored controller parameters. Persistent slot capacity includes
the global rows within the existing1MiB bank budget.

The ordering follows the successful original table-loader invariant4347f0:
row indices must equal registration indices. Combined with verified first-wins
deduplication, this requires table entries to precede other distinct controller
registrations. This is a startup-order inference, not a full original campaign
transition trace. The bank is still reconstructed per diagnostic scene; retaining
metadata across actual level transitions and automatic PCM eviction remain open.

PC and stock64MiB XEMU replay-20260910-194230 pass the180-frame door traversal,
with matching spatial telemetry[2,214,3428625177,216,209,536] and unchanged
unity PCM hash3527213817. New SOUND_BANK telemetry is[88,4,111904,12120]:
global declaration count, resident sample count, retained PCM file bytes and
metadata bytes. LIVE_AUDIO now reports92 registered records and124024 bytes;
it previously reported4 records and114168 bytes. The9856-byte difference is
88 additional112-byte slots, with no added resident waveform files.

`rf_audio_probe --global-bank Installed_Game/tables.vpp Installed_Game/audio.vpp`
checks all88 declarations without PCM, then the L14S3 Tram Door Right request
for Switch_01.wav. It resolves index37 with table near6/volume0.9 despite the
controller's near5/volume1 request; only that sample becomes resident. It does
not validate L14S3 gameplay. The native door test covers actual table-file I/O
and campaign wiring; it remains a staged L1S1 fixture.


### Bulk release order and device reference counts

verify_audio_bulk_lifetime.py executes the complete5439b0 ->544310 ->522d10
stop dispatch and543980 ->543930 ->522270 sample-release chain. Only final
voice destruction521930 and resource destruction521a60 are supplied.48 bulk
fixtures verify ordered callbacks, all2600 registry records, registration count,
device references and live-resource count across low-byte enabled/keep gates
and backend selection.28 additional522270 cases cover negative handles, positive
references and unsigned wrap edges in the original instructions. All pass.

When audio is enabled and backend1 is selected,522d10 invokes521930 for each
ordinary slot0..29 before any sample-release attempt. The keep argument does
not suppress these voice releases; only its low byte controls whether543980
runs afterward. This differs from retaining active playback.543930 still skips
retained samples (byte+63) and invalid device handles, as previously verified.

522270 decrements the device sample reference at18875d4+index*592 for every
nonnegative index. Only a decrement to exactly zero invokes521a60 and decrements
the live-resource count at1aed35c. Zero underflows without destruction in the
original; this is observed behavior, not permission to underflow port ownership.
This device reference is distinct from the registry retention counter at+56.

New raw521a60 output suggests cleanup of16 auxiliary records, allocated blocks
and DirectSound interfaces before clearing sample identity. Those destructor
internals and reference acquisition still require execution verification.
Automatic campaign eviction remains unimplemented; current retained PCM is
released only after the port's synchronous device reset. No runtime change or
full transition fidelity is claimed by these dispatch/reference tests.


### Device cache acquisition and identity

verify_audio_acquisition.py executes521c10 with original522210 lookup,5221e0
hash and57c130 comparison. Only resource loading521d30 is supplied.72 cases
check complete six-record cache images, returned index, load calls and count;
eight separate hash cases include null and signed high-byte input. All pass.

The592-byte device record starts at1887388.5221e0 rotates the hash left6 and
XORs each sign-extended character. Lookup checks that hash before ASCII-insensitive
name comparison, scanning only indices below1aed35c and returning the first
match. Consequently ordinary case-only changes can fail the hash gate despite
the case-insensitive comparator. Null hashes toFFFFFFFF; an empty name to0.

521c10 reuses a valid matched entry by incrementing its reference at+588, without
calling the loader or changing the global count. Otherwise it writes the name,
path, offset and parameter into the entry at the global count and calls521d30.
Failure clears the first name/path bytes and returns-1 without count/reference
increments. Success stores the hash and increments both reference and count.
Loader mode is forwarded unchanged; its interpretation remains inside521d30.
The harness supplies no loader mutations, so these tests prove wrapper changes,
not the initialized reference state of a newly loaded real resource.

Together with522270 this identifies reference acquisition/release, but arbitrary
cache-hole reuse is not established: acquisition uses the current count while
release decrements it. Selective-release ordering, loading/destruction internals
and registry retention scheduling must be reconciled before transplanting this
policy into the bounded port bank. No automatic eviction is added by this test.


### Loader dispatch and stream cleanup

verify_audio_loader_dispatch.py executes521d30 and522130 with resource
open/read/close and format-specific loaders supplied.720 cases check exact
arguments and ordered calls, complete cache records, enabled/negative-index
gates, low-byte modes, format tags1/2/unsupported and each failure stage.
All pass. The supplied open callback writes only the format pointer; its
allocation and initialization effects are outside this proof.

522130 checks the enabled low byte and nonnegative index, calls563370 to open
resource metadata, then5635d0 to read data metadata. Either reported failure
closes the stream through5636e0 and returns-1. After success,521d30 returns
immediately without stream closure when the mode low byte equals1. Other
modes dispatch format tag1 to521db0 or tag2 to521f90, then close the stream;
unsupported tags fail and also close it. Mode1 is therefore a distinct stream
lifetime, not merely another boolean variant of immediate buffer creation.

The tested orchestration itself does not reset the device reference at+588.
New raw exports show format1 creates/fills a DirectSound buffer and optionally
queries3D state, while format2 uses a conversion buffer and codec context.
Their actual failure cleanup and global cache initialization remain unverified;
raw decompilation is not taken as proof of reference initialization. Automatic
PCM eviction and level-transition retention remain open.


### Cache reset and count-based lookup limitation

verify_audio_cache_reset.py executes original5215f0 against three patterned
4096-record images, checking every byte plus boundary canaries. It clears
record offsets512,524,528,536,544,588, marks hash+516 asFFFFFFFF and clears
the first name/path bytes at0/256. Other bytes and global count1aed35c are
preserved. This establishes zero initial references after reset; the reset
is not a general memory wipe or a device-resource destructor.

Two lifecycle sequences run original acquisition and522270/521a60 release
together, supplying only successful521d30 with no actual resources. Duplicate
acquisition raises one record to2 references; two releases clear identity and
return the count to0; reacquisition uses slot0 with one reference. External
destructor branches remain excluded because supplied records own no resources.

The second sequence loads A/B/C into0/1/2, then releases A. The count becomes2
while C remains at2, outside the lookup range. Acquiring C invokes loading again
at2 and increments that record's existing reference to2. This directly verifies
that count-based lookup plus selective release is not a general hole-aware cache.
It does not prove this sequence occurs in original gameplay or that original
transition scheduling is faulty. It prevents treating the isolated mechanism
as permission for arbitrary live PCM eviction in the port.

Next ownership work must reconcile actual release scheduling and device borrower
completion. The bounded Xbox bank must preserve stable registration identities
and avoid freeing referenced PCM; no automatic eviction follows from these tests.


### Release completed device borrowers by sample

The PC and Xbox adapters now expose rf_*_audio_release_idle_sample(samples).
This is a port ownership operation, not reconstruction of an original eviction
policy. It preflights all records matching the bank PCM base pointer. Any
active or looping record returns RF_RANGE without changing any borrowers.
Otherwise it clears/destroys every matching completed record and returns RF_OK.
Absent borrowers (including a closed backend) also succeed; null is rejected.
PC holds the refill lock for both passes; Xbox calls must be serialized with
play/reset and require NX_STOPPED before destruction. Other voices continue.

This exact-pointer contract assumes each bank sample uses a consistent base
pointer, with no overlapping subrange aliases. A successful device operation
only releases device borrowers; the campaign must separately clear its logical
mixer references before bank unload. Automatic eviction is not yet connected.

PC checks prove busy refusal preserves completed handles, then idle release
removes all matching handles. PAGE_NOACCESS on released source pages does not
interrupt unrelated looping playback. Stock64MiB APU run20260911-013355 passes
the corresponding completed-plus-looping borrower case, stale-handle checks,
unrelated nonzero guest output and exact available-page restoration on reset.
Existing APU overlap, loop, gain, lifetime and failure cases also pass. Production
PC/NXDK builds and all eight CTests pass. Native tests do not claim host listening
or arbitrary alias-safe reclamation.


### Campaign ambient PCM pressure retry

rf_audio_bank_release_idle now bridges shared and device ownership. It rejects
active/looping logical borrowers before touching the device; device failure
preserves shared voices and bank PCM. Device success permits clearing completed
logical references and unloading the waveform while retaining registration.
rf_audio_bank_reload_idle first tries the ordinary load, then retries range
failures after reclaiming caller-eligible idle samples in registration order.
Missing archives and individually oversized requests do not trigger eviction.
Reclaimed counts/bytes remain reported if a later load fails. This is a bounded
port policy, not a claim about original automatic eviction order.

Campaign ambient reload now uses that helper and the device event callback.
A2600-byte static eligibility map marks only samples lazily loaded by ambient
playback; existing controller/rejection/jump preloads remain pinned because
their current paths still expect residency. The map resets with the bank.
Resident sample/byte telemetry decreases on eviction, while ambient lazy-load
bytes remain cumulative. The audio bank retains its1MiB cap; eligibility storage
is separate fixed scene state. Legacy devices without release certification
continue ordinary loading without eviction.

The PC bank probe exercises actual waveform allocations: active-borrower denial,
device-failure preservation, completed-reference removal, exact freed bytes,
idempotence and pressure-triggered replacement under a one-byte-short budget.
Four campaign startup checks, PC/NXDK builds and all eight CTests pass. Native
campaign startup validation is recorded below; startup does not exhaust the
bank. A native moving-listener pressure/reload run remains required before
claiming end-to-end eviction coverage or audible parity.

Final native L1S3 run20260911-013910 passes31 frames with PC state parity and
nonzero guest DSP output under64MiB. The earlier013726 pass preceded moving
the retry loop into the tested shared helper. Neither run forces eviction.


### Native budget-pressure reclamation

Stock64MiB APU run20260911-014139 now executes the same shared pressure-retry
helper with the production Xbox release callback and actual allocated bank PCM.
The isolated generated pressure.vpp contains two distinct registrations backed
by identical original DoorOpen_07 waveform bytes. Its bank fits only metadata
and one waveform, forcing a real replacement without decoder differences.

The first retry is refused while the shared voice is active. After the shared
mixer consumes its entire waveform, the next retry is still refused because
the Xbox voice is playing. Both refusals preserve resident bytes and report
zero reclamation. Once the Xbox voice naturally finishes, the retry releases
its device record, clears the completed shared borrower, frees57014 bytes and
loads the second registration within the unchanged budget. The old device
handle is absent, registration count stays2 and only the replacement is resident.

Starting the replacement produces left-channel guest output while an independent
right-channel loop continues through reclamation. Device reset restores15796
available pages, exactly matching the pre-open measurement. The native report
records budget_pressure=[1,1,1,57014,1,15796,15796]; all earlier APU tests pass.
This closes native shared-helper/device reclamation coverage, not full campaign
moving-listener traversal, repeated eviction cycling or host-listening parity.


## Original game voice stop control

rf_audio_control_voice models the original44-byte table at1753c38, separate
from the port PCM mixer's handle format. rf_audio_control_stop reconstructs
505a40 and505680: the low8 handle bits select one of30 slots, with arithmetic
signed generation in the upper24 bits. Enabled uses its low byte. Negative
device values and generation mismatches do nothing; handle zero is valid.
The supplied device stop runs before reset. Reset clears device/sample to-1,
category, fields10/14 and volume to zero; generation, position and positional
marking survive. The retained positional byte must not be silently cleared.

verify_audio_control_stop.py matches2,500 original/PC/NXDK scenarios, including
270 device stops, signed generation extremes, invalid slots, stale generations,
negative devices, zero handles and callback mutations. NXDK compares all30
records, checking unrelated entries remain untouched. Original5442b0 is supplied;
the original reset executes. Both builds and eight CTests pass. Evidence:
artifacts/audio-control-stop.json. Allocation/refresh must populate this original
control layer and translate to the existing device/mixer adapters before burn
voice lifetime can be connected end to end; this is not native audio evidence.


## Original game voice allocation

rf_audio_control_start reconstructs505560 over the original30 control slots.
Only prepare_sample result-1 rejects; other results continue. It queries every
device using the playing low byte, stops/resets each nonplaying slot, and only
then scans for the first negative sample field. Category gain and sample loop
byte remain borrowed until this cleanup finishes. Failed device playback stores
its negative result without completing assignment. Success stores sample,
category, requested volume and pan, clears only the positional low byte, then
increments the full32-bit generation and returns its low24 bits shifted by8
with the slot. Full generation is intentionally not masked in storage, matching
the original even beyond representable handle generations. The formerly named
fields10/14 are now identified as requested_volume/pan. Control reset is shared
with the previously verified stop helper; metadata/PCM/device work is supplied.

verify_audio_control_start.py compares1,024 full original/PC/NXDK scenarios:
357 playback requests,102 exhausted control tables, all30 cleanup calls and
exact callback arguments, complete table bytes and returned handles. It covers
low-byte gates, preparation/playback failures, signed/overflow generations,
first/last free slots and callbacks that mutate device, gain, loop byte or
selected generation before subsequent original reads. Both full builds,
2,500 control-stop regression cases and eight CTests pass. Evidence:
artifacts/audio-control-start.json. Positional5056a0/5058c0 wrappers and real
sample/device adapters still need connection before live burn-audio proof.


## Positional control start and refresh

rf_audio_control_start_position connects original5056a0 to shared505740 spatial
math and505560 control allocation. It multiplies spatial gain by requested
volume again before allocation. Only a nonnegative returned game handle receives
positional volume, late-copied source position and a low-byte positional flag1.
Callbacks may change the borrowed source before this copy; upper flag bytes
survive. The unused original fourth argument is omitted.

rf_audio_control_refresh reconstructs5058c0 with signed generation and positional
low-byte gates, but no global enabled or device-positive gate. It commits source
and volume first, computes shared spatial gain multiplied once by category gain,
then calls volume followed by pan, re-reading the device after the volume callback.
Velocity is unused. Callers resolve matching sample/category metadata and retain
it across callbacks. Setters are still the downstream544390/544450 boundary.

Original/PC/NXDK evidence:1,024 positional-start cases through actual505740 and
505560, including source changes during playback;2,048 refresh cases with304
accepted updates, source aliasing, negative devices and device changes between
setters. Complete state and ordered float arguments match exactly. Existing
1,024 control-start and2,500 control-stop regressions pass. Both builds and eight
CTests pass. Reports: artifacts/audio-control-position.json and
artifacts/audio-control-refresh.json. No live sample loading, final device
conversion or native gameplay is claimed by these CPU tests. Connect sample/
device adapters and persistent owner scheduling next.


## Sample preparation and playback dispatch

rf_audio_sample_prepare reconstructs5054d0: disabled low byte returns-1; any
nonzero prepared byte returns0; otherwise543760(sample,0,0) runs, and only a
nonnegative result stamps prepared=1. Loader mutations survive a failed call.
rf_audio_sample_start reconstructs5439d0/543a80 through the522530 boundary.
Disabled/sample-1 rejects before load; after load only-1 rejects. One-shot
bypass accepts any nonzero low byte, while looping bypass requires exactly1.
Otherwise543a60 applies the sample's separate category and default volume.
Device mode must equal1, checked after loading and gain calculation. The
sample buffer, category/default gain and mode remain live across load callbacks.
This category at1cd3b94 is distinct from the game voice category at1753c18.

verify_audio_sample_start.py matches5,400 playback-dispatch scenarios (2,160
loads,1,152 playback calls) and600 prepare scenarios (90 loads) on original,
PC and NXDK. Original543a60 executes; only543760 loading and522530 device start
are supplied. Low-byte differences, load return distinctions, mode failure and
loader mutations are covered with exact state and ordered arguments. Both full
builds and eight CTests pass. Evidence: artifacts/audio-sample-start.json.
Archive loading/decoding and actual device allocation still need adapters.
Do not cast the port mixer's uint32 handles to original signed device handles:
its65534-generation cycle can set the sign bit. Any shared control/device bridge
must preserve separate handle ownership and account for native playback status.


Device-source status adapter (2026-09-11)
---------------------------------------

The optional rf_scene_audio_events.playing callback queries a logical unsigned
handle against the platform's actual source owner. Xbox reads nxAudioVoiceGetState
from the matching created slot. PC reads the device-refill mixer under its critical
section; it does not inspect the separate deterministic campaign mixer. Unknown,
released, naturally completed and closed-device sources return zero. Muted running
sources return one. Queries neither release nor modify source ownership.

The PC source can finish while copied output remains in waveOut's bounded queue;
this callback describes source consumption, not speaker completion. Neither platform
uses a false result as proof that PCM has been released: the existing explicit
release_idle_sample/reset contracts remain necessary. The callback is optional and
must be checked before use by a future control adapter. Platform lifecycle operations
remain serialized on the owner thread; the PC refill worker is synchronized by lock.

The existing real-device probes now distinguish an expired short one-shot from a
muted looping source and an unrelated live loop, reject unknown handles, retain the
expired borrower for explicit release, and return zero after device close. PC
rf_pc_audio_check passes, including protected-source release checks. Native stock64MiB
APU report artifacts/xemu/apu-20260911-054025/report.json passes the same checks and
the existing allocation/lifecycle/bank pressure suite. PC/NXDK builds and eight CTests
pass. This adds the device-status boundary required for original control-table
integration; signed device-handle mapping and sample loader wiring remain open.


Ambient device identities (2026-09-11)
-------------------------------------

Live ambient starts no longer cast the unsigned PCM mixer handle to int32_t.
The mixer's generation32768 sets the sign bit and the old code incorrectly
rejected a successfully allocated voice. A fixed244-byte rf_audio_voice_ids map
now retains independent nonnegative identities and the complete unsigned source
handle. Ambient stop, gain refresh and active-source telemetry resolve through
this map. Zero is a valid identity; -1 remains failure. A replaced/released mixer
source fails resolution by its full generation, including when a controller
reuses a previously ambient slot. Natural completion alone does not invalidate
the mapping, so explicit release can still resolve the retained borrower.

The identity counter follows actual original522683..5226a6 after successful
522530 playback: store current1aed358 in slot+10, increment, replace a negative
increment with zero. For a valid nonnegative counter this cycles0..INT32_MAX.
verify_audio_device_identity.py executes that exact original success block and
NXDK-linked binding for720 combinations of all30 slots, counter boundaries and
mixer generations spanning the sign bit. Device creation and full522530 are not
executed by this verifier. The map/unsigned source binding is a port adapter,
not a reconstruction of the complete original device table. Its current identity
stream belongs to ambient playback; controller game handles remain separate.

The audio_device_identity CTest starts the actual shared mixer at generation32767
and verifies the resulting high-bit handle remains usable through identity0,
natural completion, release, unbound slot reuse and subsequent binding. It also
checks identity wrap and rejected-input preservation. All nine CTests and both
full builds pass. Four campaign registration fixtures still pass within the1MiB
bank budget. The L1S1 walk/restart PC replay preserves starts/stops, refresh counts,
active voice counts, positions and resident sample bytes. The frame780 diagnostic
hash changes2828698778 ->1873249434 because stop hashing now records the signed
identity rather than the encoded mixer source; the regression fixture is updated.

This mapping does not prove device completion from deterministic mixer time. It
neither releases native borrowers nor changes the existing release/reset contract.
Original game-control integration still needs a common device owner using native
source status where appropriate, loader wiring and category ownership. Persistent
NPC burn ownership remains open. No new visuals accompany this internal change.

Native L1S1 replay20260911-054619 passes1170 frames with audio enabled on
QMP-verified64MiB. All checked guest state matches PC, including ambient restart
telemetry; DSP output is nonzero and checked device errors are zero. The harness
restores the normal disc configuration and rebuilds its ISO. This covers the live
ambient mapping under ordinary handles; the sign/wrap boundaries are covered by
the explicit CTest and original/NXDK CPU verifier described above.
