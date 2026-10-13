# Xbox audio level backend

Source implementation, 2026-10-12. Uncompiled and untested by this task;
the parent owns the hourly stock64 compile/startup smoke and UI/storage
integration. Testers own functional coverage. This is a port preference
feature, not a claim of recovered original menu or volume-curve behavior.

## Integration contract

`src/platform/xbox/audio.h` exports:

```c
typedef struct rf_xbox_audio_options {
    uint32_t master_percent, effects_percent, music_percent;
} rf_xbox_audio_options;

void rf_xbox_audio_get_options(rf_xbox_audio_options *options);
int rf_xbox_audio_set_options(const rf_xbox_audio_options *options);
```

`RF_XBOX_AUDIO_VOLUME_DEFAULT` and `RF_XBOX_AUDIO_VOLUME_MAX` are both 100.
The initial levels are 100/100/100. Every integer from 0 through 100 is valid.
Null get is a no-op. Null set or any out-of-range member returns `RF_RANGE`
before changing either settings or hardware. Valid settings are retained
across audio open, close, scene reset and End Session. They can be set before
the optional backend opens, returning `RF_OK` without hardware access.

The setter is serialized by the platform owner with play/poll/pause/reset,
just like the existing adapter entry points. It applies gain to every created
ordinary voice and the created music voice, including paused voices. It does
not issue Stop, Start, Pause, queue, decoder or borrower operations.

A native gain failure returns `RF_IO`; the validated requested settings stay
retained and are returned by get. Native gains may be partially applied.
Ordinary polling retries the desired gains, and another set call also retries
them. A gain failure increments the existing ordinary/music error diagnostics
without retiring a voice or releasing PCM. New voices use the retained
settings before playback. UI/storage callers should use get as the requested
settings state and must not interpret a gain failure as a successful close.

The parent implements both Options entry points, Reset Defaults, stored/dirty
notices, startup restore and explicit RFCS5 persistence. This commit does not
change menus, input options, settings storage, world saves or attack code.
Reset Defaults calls set with 100/100/100; startup load calls set after the
storage owner has successfully published a validated record. The approved
48-byte RFCS5 format and RFCS1-4 compatibility remain in the parent's lane.

## Gain composition and ownership

Ordinary voices retain the latest authored/spatial left and right gains.
Both initial playback and later attenuation/pan updates submit:

`native_channel_gain = authored_channel_gain * master_percent * effects_percent / 10000`

Muting does not discard those authored gains. Raising volume therefore uses
the current attenuation and pan, rather than multiplying an already scaled
value. Effects covers all thirty ordinary slots, including dialogue; the
current routing has no separate speech category.

The streamed stereo music voice uses `master_percent * music_percent / 10000`
on both channels. Authored crossfades/envelopes already applied by the music
mixer remain intact. Every music voice creation, including drained-queue
recreation, reapplies the current multiplier before queuing/starting. Muted
music continues its ordinary decoder/queue lifetime, so unmute does not
restart the track. No PCM samples, buffers, sample formats or callbacks change.

All gain writes use the existing `nxAudioVoiceSetChannelGain` API. Pause masks,
voice allocation, stream revision, descriptor borrowing and checked retirement
are unchanged. Initial music gain failure returns through the existing caller's
checked `music_release` path; it cannot abandon the created voice. Existing
failed-close/idle admission remains intact.

Storage is bounded: two floats per ordinary slot (240 bytes), three retained
percentages (12 bytes), and one retry flag (4 bytes), totaling 256 bytes of
additional fields before any compiler padding. There are no heap allocations, extra PCM buffers, tracks or worker
threads. The adapter byte diagnostic includes the added state.

## Baseline and handoff

The branch starts at published Git commit
`6d1bf09b54625cade292445139cd9da0779634d4`. Its original LF-normalized Git
blobs exactly match the parent-supplied current cloud files:

- `src/platform/xbox/audio.c`: SHA-256
  `2fb749de82f9bbf919d20ed1cf4716526bccd2c13529f3c8d57b3151a2531995`
- `src/platform/xbox/audio.h`: SHA-256
  `d2f1d4f2d308998640559f7c89c834d0ac8f72b57bf8a06b9d17bead3a6b5177`

The newer full Library snapshot could not materialize on Windows because its
unchanged transfer helper requires `os.setxattr`; no metadata bypass was used.
The parent authorized this independent backend slice against the matching Git
files. No unpublished parent baseline commits are included. The monitor branch
and user checkout are untouched. No builds, tests, runtime, images, cleanup,
or saved-HDD access occurred in this task.
