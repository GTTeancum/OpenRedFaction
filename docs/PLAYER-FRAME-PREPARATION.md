# Player frame preparation throughput

Status, 2026-10-08: source-written from aa7d550; no helper compile, tests,
benchmark, emulator, screenshots, host input or generated runtime assets.
Parent owns the 16:00 consolidated stock-64-MiB Xbox batch. Source inspection
establishes removed work, not a measured speedup or the 30-FPS goal.

## Preserved work

Every frame retains player input, stance/climb/swim/jump transitions, controller
advancement, playback advancement, the first full bone evaluation, eye transform,
body and class initialization, model publication, camera, visibility, collision,
rendering and scene callbacks. No simulation tick or visible geometry is omitted.

## Exact room-query reuse

campaign_swim_update formerly located the body in the entire collision world
every frame. actor_follow_view separately did the same for the final camera.
The two positions differ and therefore have separate small inline caches.

Each cache retains the full room/face/retry result only for bit-identical XYZ.
Its key also includes collision-world identity, view-array identity, terrain
owner, publication serial (or actual legacy terrain mesh generation), and the
one-shot campaign Geomod/wall publication bits. World checkpoint commit clears
both caches explicitly, covering restores that reuse generation numbers. A new
scene owns zero-initialized entries. There is no cross-scene static cache.

Moved queries still use rf_geometry_collision_world_locate. World edits or
restoration cause a fresh query even if the player stays in place. Wetness,
eye height, stance decisions, camera setup and portal visibility run every
frame with the same returned room. Dynamic movers never participated in the
original static room-locate query and are not approximated by this cache.

The inline cache costs 96 bytes on the Xbox 32-bit ABI: two 48-byte entries.
rf_scene_player_room_cache reports body misses/hits, camera misses/hits,
topology invalidations and sizeof the two entries. It resets per scene.

## Animation diagnostic work

The existing scene checksum-mode pointer is now passed into the animation
placement. NULL keeps standalone and legacy callers fully diagnostic. Enabled
mode preserves the previous hash ordering, redundant-evaluation cache probe,
per-vertex collision skin scan, physics hashes and skin preparation.

Disabled mode omits only these diagnostic hashes and probes. In particular:

- The second evaluate_playback deliberately changes root displacement X to 1,
  verifies that the current generation skips it, hashes the unchanged pose,
  then clears X. It is a cache-idempotence test, not motion. The first evaluation
  still consumes the real displacement and publishes the same bone matrices.
- Every model vertex was transformed with rf_model_collision_vertex solely to
  hash its temporary position. Those local positions have no collision or
  rendering consumer. This scan previously ran even for a suppressed hidden
  first-person body. It now runs only with checksums enabled.
- Prepared skinning matrices are still made for every rendered mesh. They are
  unnecessary when both the mesh and diagnostic scan are suppressed; the
  published model points at the ordinary bone matrices, not that skin buffer.
- Scalar frame, body, allocation and animation timing telemetry remains live.
  Disabled hash fields must not be presented as parity evidence.

rf_animation_diagnostic_work reports current enabled mode, fast-mode frames,
skipped cache probes and skipped vertex transforms. No new fast flag or
benchmark-only mode was added: this follows normal live play's existing mode.

## Timing boundaries and pending parent validation

The reported 6.754 ms pre-camera phase contains body room lookup, controller
work, ordinary pose evaluation, pose hashes and the redundant cached evaluation.
The full discarded vertex scan runs after prepare_view and before scene_frame,
so its removal belongs to the subsequent actor-preparation phase. Camera room
query reuse belongs to rf_scene_world_profile[3]. These changes must not all be
attributed to a single phase or converted to an estimated FPS improvement.

Pending: compile in the scheduled Xbox batch, compare the unchanged original
L1S1 neutral spawn on stock 64 MiB, inspect actual FPS and relevant profile/count
symbols, and preserve existing movement/stance/room-edit/checkpoint behavior.
No new gameplay fixture or PC runtime test is introduced.
