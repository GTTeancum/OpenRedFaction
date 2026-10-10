# Capek Cane optional flight audio

Source written 2026-10-10 against `7a8d063e718ca80447c47bfdbfe60068964403ea`.
Not compiled, syntax-checked, run or heard. Independent source review found
no blocker after callback/timeline requalification. Parent owns scene
integration and the hourly stock-64-MiB Xbox batch.
No build, emulator, PC target, gameplay fixture, campaign route, inventory
grant, event, asset edit or image was introduced or executed.

## Original evidence

Read-only inspection of the installed original files established:

- `tables.vpp/weapons.tbl:1905-1935` defines the exact Capek Cane. Line 1928
  binds `$Fly Sound: "Capekcane_Fire_02.wav" 8 1.0`.
- `bluebeard.bty:2295-2301` names `CapekCane_Fire_02.wav`, explicitly declares
  `+Looping Sound`, and declares `+Loop Start: 0`. This is an authored loop,
  not an inferred one-shot or a new synthesized effect.
- The original `audio.vpp` directory entry starts at byte 241453056 and is
  66238 bytes. Its RIFF PCM format is mono, 22050 Hz, 16-bit, two-byte block
  alignment. The `data` payload is 66120 bytes at file offset 44; the whole
  file is retained by the existing audio bank, including bounded LIST data.
- Original projectile construction `4c7be2-4c7c05` requests descriptor 174
  Fly Sound and retains its voice at projectile 2a0. `4c70d5-4c70f9` updates
  the retained voice's position/velocity; `4c805a-4c806d` stops it on retirement.
  The port's shared spatial owner consumes position only, without new Doppler.
- The distinct Launch remains `Capek Cane Launch`, defined by
  `foley.tbl:2217-2218` as `CapekCane_Fire_01.wav` at near 8/gain .9. The
  existing accepted-windup presentation already owns it. This adapter never
  requests Launch, warmup, impact audio or impact visual effects.

## Accepted release and exact ownership

The gameplay pool retains the already monotonic, non-wrapping accepted
windup ticket in a parallel 16-entry `uint64_t` flight-ticket array. Publication
of the actual released flight and its ticket precedes optional audio. This
does not create a new gameplay acceptance gate or change the projectile/save
layout. Tickets are zeroed on pool reset, while the existing global acceptance
counter remains monotonic across timelines.

Each audio slot independently retains accepted ticket, full source handle,
weapon, nonnegative device ID, full mixer-generation handle and sample ID.
Acceptance qualifies the exact published Cane flight; duplicate requests for
the same retained ticket do not restart even a suppressed or failed episode.
Source death, source slot reuse, target changes, homing velocity and orientation
do not invalidate an independently flying projectile's presentation.

Movement supplies the copied slot/source/ticket, after publishing the new
flight and before terminal contact/damage callbacks. It can update or stop
only that episode. Terminal/inactive flights stop immediately. New accepted
slot use retires only its previous audio owner before attempting a new loop.
On a flight-tick error in the same timeline, all Cane loop episodes retire,
with no retry on the next tick. General Cane reset retires loops before
clearing flights/tickets; existing successful-load, frame-boundary and scene
retirement hooks inherit that behavior. Pending-only cancellation remains
unchanged and does not stop an already released flight.

Before stopping or moving, device-ID resolution, full mixer generation,
active state and spatial sample must all agree. A stolen/stale native slot
cannot stop an unrelated replacement. Audio-owner retirement occurs before
native stop callbacks. Acceptance requalifies canonical ticket/source/epoch
after native stop and native start, keeping the new device ID private until
then and retiring that exact voice if the timeline changed. Parent service
and tick also recheck epoch after audio callbacks before continuing.
No audio handles or tickets enter RFAP or any existing
save payload. Forgetting a timeline preserves the admitted PCM but never
resumes unsaved Cane flights.

## Bounded optional admission

Admission occurs once at the existing late load-stage scan of actual placed
NPC bodies. The existing `cane_visual_needed` predicate establishes a present,
nonretired, positive-health exact selected-owned Cane. It is demand evidence,
not a weapon grant, a runtime combat gate or proof of firing. The audio request
is independent of successful visual admission. No placed demand means no
declaration, PCM allocation or archive lookup for this helper.

At this stage the local `archive` is the mesh archive. The helper accepts only
the verified persistent `campaign_audio_archive`, transferred from the local
audio.vpp by `campaign_audio_open` and retained until normal audio teardown.
The bank's archive pointer must agree. It never opens an archive at release,
movement, impact or reset.

One file is bounded to 72 KiB, with 256 KiB bank headroom after any new load.
Existing resident PCM is reused. No eviction occurs. The helper rejects
unknown/nonlooping metadata and nonzero loop offsets. Reload parses bounded
PCM before publication; failure preserves prior bank rows, resident samples
and pin flags. A successful declaration may remain after failed optional
admission. No fallible step follows successful PCM reload before pin ownership
is published. Close restores only this helper's prior evictable flag and never
unloads PCM that another native/mixer borrower could still reference.

Up to four simultaneous Cane voices are allowed; this is independent of the
existing Rocket/grenade four-voice budget and is a bounded port policy rather
than original voice-priority fidelity. Refusal records the episode and remains
silent for its lifetime. Nothing returns an error to gameplay. The maximum
new installed PCM is 66238 bytes, with 16 fixed audio-owner rows plus 128 bytes
for retained gameplay tickets. Telemetry reports actual fixed bytes through
`sizeof`, without assuming a host/Xbox ABI size.

Every start checks residency before the generic starter, keeping its lazy
reload branch out of Cane gameplay. `campaign_sound_start_profile` already
copies the supplied near 8/gain 1/rolloff 1 parameters into the generation-owned
spatial row before native initial gain. Far distance uses `rf_audio_far_distance`.
Listener and projectile movement use that immutable copy. The helper neither
rewrites existing bank parameter rows nor mutates authored sound metadata.

## Diagnostics and validation limits

`rf_scene_cane_flight_audio[18]` contains accepted requests, starts, stops,
moves, stale voices, errors, voice-cap refusals, live and peak voices, retained
file bytes, readiness, latest status, slot, full source handle, sample, latest
device ID, fixed owner bytes and placed demand. Counters establish requests
and mixer ownership only, never audible native output. Late successful PCM
admission also updates the ordinary live-bank count/bytes and resident-PCM
accounting. The existing registered-metadata hash remains the startup snapshot.

Actual accepted release, positional attenuation, audible looping, terminal
closure, source removal, slot reuse, reset/load behavior and stock-memory
admission remain unverified until appropriate parent-owned validation. Exact
original scheduler/voice-priority behavior, Doppler, save persistence, warmup
and impact consumers are outside this bounded slice.
