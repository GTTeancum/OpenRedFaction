# Vehicle continuous-fire voice ownership

Status (2026-10-09): source-written and source-reviewed only. The parent owns
shared scene integration and the hourly Xbox compile/runtime batch. This worker
ran no builds, tests, emulator sessions, gameplay fixtures, input mutation,
image capture, cleanup or commits. Audible playback and runtime transitions
remain unverified.

## Established gap and original evidence

Player Fighter primary, autonomous APC/Fighter primary and scripted Fighter
Attack each called `combat_sound("Vauss1 Fire", ...)` for every accepted round.
That helper discards its returned voice. The existing shared sound service
honors native loop metadata, so these were independently allocated looping
voices with no firing-episode owner or release boundary.
The player APC/Jeep primary scheduler had no launch-audio consumer at all.

Read-only inspection of installed original assets establishes:

- `tables.vpp/foley.tbl:2106–2107`: `Vauss1 Fire` selects the single
  `vauss_01.wav` sample, near distance 10, gain 0.9.
- `bluebeard.bty:17878–17884`: this sample is a looping sound with loop start 0.
- `weapons.tbl:2495–2524`: Fighter Minigun is continuous-fire and launches that
  group. Its existing 0.10-second scheduler warmup and 0.05-second cadence remain.
- `weapons.tbl:2385–2409`: APC Minigun is continuous-fire and names
  `Vauss2 Fire`, corrected here for autonomous APC and used by the newly wired
  player APC consumer. Foley lines 2109–2110 select `vauss_02.wav`, near 10, gain 0.9;
  `bluebeard.bty:17887–17893` declares it looping from 0.
- `weapons.tbl:2432–2466`: Jeep Gun is continuous-fire and names
  `Jeep Gun Loop`. Foley lines 2082–2083 select `Vauss_04l.wav`, near 12, gain 0.8;
  `bluebeard.bty:17912–17918` declares it looping from 0. Its existing 0.10-second
  warmup and 0.12-second cadence remain; no Attack/Release sound sequencing is
  added to this bounded launch-lifetime slice.

Read-only installed `RF.exe` disassembly establishes the shared original owner
and routing semantics:

- `41a9bd..41a9c3` skips another continuous launch while actor `+0x81c` holds
  the firing voice.
- `41aa3d..41aa95` selects the Launch group, passes actor `+0x7d4` through
  `48a9c0`, and retains the returned voice at `+0x81c`.
- `48a9eb..48a9fc` uses flat `505560` for the local-player branch;
  `48aa06..48aa21` uses positional `5056a0` for ordinary nonplayer owners.
- Existing reconstructed player-loop evidence records `41aee3..41aef7`
  stopping `+0x81c` before release sound, and `424fdc..424fef` closing it at
  actor teardown. See [the player-loop evidence](PLAYER-WEAPON-LOOP-AUDIO.md).

The reconstructed vehicle schedulers use compound static hulls rather than
those original skeletal actor structures. This adapter supplies equivalent
bounded audio ownership around their existing fire admission; it does not claim
to reconstruct every retail start/stop animation or sound transition.

## Bounded consumer

`scene_vehicle_loop_audio.inc` keeps 30 records, each 28 bytes: complete host
handle, controller kind, exact raw mixer-generation handle, seen marker,
authored Launch-group kind, public voice ID and selected sample. Its record
table is 840 bytes and the 12-word observation array adds 48 bytes, 888 static
tracking bytes total. There is no per-host heap allocation, second sound bank,
PCM cache or voice pool.

One active record per complete host handle is permitted across PLAYER, ATTACK
and AI controllers. A controller change closes the former record before any
new voice starts. Changing the authored Launch-group kind also closes the old
episode before starting another. The actual admitted profile maps 2 to APC
Vauss2, 3 to Jeep Gun Loop, and 5/6 to Fighter Vauss1. A dormant controller's stop
request matches both controller and host; it cannot stop a new controller's sound. Different vehicles keep
independent records and voices.

The first accepted shot starts an episode. Warmup alone cannot start it, and
accepted shots during the same held episode do not allocate more voices.
Admission comes from the existing post-tick scheduler held state plus remaining
ammo, so ordinary cooldown does not close/restart the sound. The accepted last
round is audible for that frame; the following no-ammo tick closes it. No new
tail duration, synthetic repeating one-shot or gameplay timing is introduced.

Foley resolution, authored gain, bounded PCM loading, the mixer and native
device playback all use existing services. Metadata must support a native
zero-start loop. The single-sample Foley chooser does not advance RNG. A failed
admitted start retains a silent episode until release rather than retrying or
loading on every frame. If the bounded owner table is full, that request is
silent and ordinary gameplay continues; occupied records are never stolen.

Both public voice ID and exact raw generation must match before stop or spatial
update. A stale/recycled slot is never treated as the former voice. Stop reaches
the native backend with that full handle, then clears only the matching spatial
record. Nonplayer AI/Attack use original-supported positional routing and
refresh from the current existing muzzle position each admitted tick. Player
Fighter retains its existing flat local-player route; player APC/Jeep use that
same original-supported local-player routing. Their new consumer is called
only after the unchanged primary scheduler and observes its actual held,
accepted-shot and reserve values, including the existing Jeep gunner gate.

## Lifecycle and integration

The parent-owned scene hooks are:

1. Include the helper after `combat_sound`/campaign audio services, before the
   four vehicle scheduler implementations. Forward-declare `close`, `reset`
   and `remove(uint32_t)` where earlier lifecycle consumers require them.
2. Call `scene_vehicle_loop_audio_begin()` immediately before the vehicle
   player synchronization/scheduler group in `actor_follow_view`; call
   `scene_vehicle_loop_audio_end()` immediately after `scene_vehicle_ai_tick`.
   The sweep closes untouched owners skipped by liveness, source admission,
   hiding/freezing, changed selection, or early-return controller gates.
3. Close before audio/spatial/ID resets, bank teardown and successful ordinary
   world-load publication. Reset counters at scene startup. Failed candidate
   validation must not affect an existing live voice.
4. Remove the exact host at admitted secondary removal, passive destruction,
   selected-host destruction and selected runtime teardown. These are
   idempotent audio-only closures; they cannot unregister or alter an owner.

The helper tick signature is `(host, controller, profile, held, fired, position)`.
The assigned scheduler files also close at AI/Attack cancellation, Attack
reissue and player takeover. Live AI/Attack list replacement closes only its
controller's records. The shared list-free functions distinguish the actual
live head pointer from a temporary checkpoint candidate, so disposing a
candidate cannot stop live audio.

No ammo, damage, flight, target selection, cooldown, warmup, reserve transfer,
ownership registration or wire-format fields are changed. Existing flights
continue after their firing episode ends. Restored held gameplay resumes audio
only on the next genuinely accepted shot; device identities are transient.

`rf_scene_vehicle_loop_audio[12]` exposes calls, successful starts, exact stops,
retained ticks, failures, stale identities, spatial refreshes, current episode
owners, peak owners, table-full requests, last start/error status and static
bytes. These observations never force input, firing, inventory or playback.

## Verification boundary

Source inspection covered controller qualification, live-versus-staged list
disposal, accepted-last-round handling, cooldown retention, full-generation
native stop, source updates and parent integration boundaries. A proportional
parent batch should compile the integrated source and, only where an ordinary
existing vehicle situation permits, observe a sustained burst retaining one
voice followed by release/owner closure. Do not fabricate a vehicle fixture,
alter inventory or expand a campaign route solely to obtain audio coverage.
