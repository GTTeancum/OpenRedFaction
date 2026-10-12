# Stationary turret loop retirement

Independent source correction based on published
`6d1bf09b54625cade292445139cd9da0779634d4`. Parent confirmed the same turret
defect in its current source. This branch contains no water/player/NPC-loop
changes or ancestry, and no vehicle or GeoMod changes. No builds, syntax
compilation, tests, runtime, images, haptics, saved-HDD access or cleanup were
performed. Parent owns integration and the hourly stock64 build.

## Concrete defect and existing protection

`scene_turret_audio_drop_voice()` previously cleared software playback, called
the void native stop callback, then cleared the retained full handle regardless
of the native result. `clear()` erased the whole sidecar afterward. Death,
firing cancellation, operator exit and model-tag failure can reach these paths.
The old resolver also discarded the native handle on a stale public-ID mapping.

`rf_xbox_audio_release_voice()` may return RF_IO after stop or its 1000 ms wait
fails while retaining the native voice. Losing the sidecar identity strands a
loop and its native slot until full teardown. The existing native allocator
does not recycle loops, and polling does not destroy them.

This correction does not claim ordinary PCM reuse was unsafe. Native idle
eviction rejects looping borrowers, and full audio reset shuts down hardware
before bank destruction. Those protections remain. Nonlooping turret sounds
such as Turret Detach retain their existing shared completion behavior.

Original continuous Launch ownership remains documented in
`TURRET-LOOP-AUDIO.md`: 41a9bd..41aa95 retains actor+81c, including the class+750
override; 41aee3..41aef7 stops it and 424fdc..424fef closes actor teardown.
This is a port ownership correction, without new original-fidelity claims.

## Logical episodes and retained devices

The existing `attempted` word now uses one bit for a logical episode and one
for pending native retirement. The 128 sidecars remain exactly 28 bytes each.
No second owner table or allocation is introduced.

- Host/controller/weapon describe the current logical firing episode. Ending
  it clears only those fields and its attempted-episode bit, then requests
  device retirement. Repeated old-host stops cannot close a new host episode.
- Full native handle/public voice/sample describe the retained device. Failed
  release preserves them and the matching software loop/spatial record, even
  after host/operator removal. Retirement never dereferences the old host,
  operator, model, registry entry or public-ID map.
- Native RF_OK/RF_NOT_FOUND alone permit generation-qualified software and
  spatial cleanup. The software loop remains occupied on failure, preventing
  ordinary slot reuse even across generation-counter wrap.
- Spatial refresh uses the retained full mixer handle, so a stale public-ID
  mapping does not erase a still-owned voice. A mismatched full mixer handle
  queues retirement without forgetting the native identity. No slot-only stop
  or later public-ID re-resolution is used.
- A new host/controller episode in the same sidecar records one attempted
  episode while preserving a pending predecessor's device fields. It cannot
  choose a Foley sample, load PCM, advance RNG or start playback while pending.
  Successful retirement does not clear its attempted bit, so no delayed start
  occurs within that held episode. The next ordinary episode may start.
- A model-tag failure or stale playback identity can retire the device while
  retaining the current attempted episode. Immediate post-start handle capture
  remains serialized; an unexpected capture failure preserves unresolved
  ownership instead of guessing from a later mapping.

Existing diagnostics retain their sizes. Active episode count excludes a
pending-only retired episode. Host/controller may describe a silent replacement
while voice/handle/sample still describe the predecessor being retired.

## Scheduling and lifecycle

Exactly **8 global transient bytes** are added: frame and rotating cursor, with
a cursor flag for the frame's spent attempt. Static assertions enforce both
the 28-byte sidecar and 8-byte scheduler. Records plus scheduler occupy 3592
bytes; including the existing 64-byte diagnostics, total tracking is 3656 bytes
(previously 3648). No public API, callback contract or save fields change.

Frame entry prepares the budget before input/restore or simulation callbacks;
that preparation performs no native work. A listener-refresh hook services
pending owners even after all turret hosts/operators are gone. Release calls
share that frame and rotating scheduler. At most **one turret native retirement
attempt per ordinary frame** is allowed, including first attempts and retries.
Re-presenting the same frame cannot replenish the budget. Scans are bounded by
128 sidecars; waits remain bounded by the existing native 1000 ms API. Several
cancellations may therefore retire over later frames; gameplay timing is
unchanged.

Explicit lifecycle drain ends all logical episodes, then processes at most 128
sidecars and returns at the first failure. It does not wait separately for every
failing voice. Pending/unvisited identities remain intact. Reset clears the
scheduler and diagnostics only after successful drain.

Checked drains precede both mixer-reset sites in `campaign_audio_open()` and
`campaign_close_movers()`. Scene entry checks reset before other owner resets;
session idle admission also checks these sidecars. Existing destructor order
and late close calls remain. Failure preserves the sticky session-retirement
status. Existing non-failing host/operator/restore close calls keep their
signatures and preserve pending devices without blocking gameplay publication.

Parent integration must combine the turret frame-budget, retry and checked
reset/drain/idle hooks with its already integrated water/player/NPC gates and
newer teardown/storage guards. Keep all existing gates. No turret targeting,
firing admission, shot cadence, animation, saved-world composition, vehicle
gameplay, GeoMod or global audio callback implementation is changed.
Compilation, audibility and native failure recovery remain unverified.
