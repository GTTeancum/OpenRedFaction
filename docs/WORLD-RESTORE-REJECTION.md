# Native ordinary-load rejection detail

Source-written 2026-10-09 after the parent 14:43 stock-64-MiB load. This change
adds read-only diagnostics, not a gameplay fix. It has not been compiled or
run; the parent owns the next scheduled batch.

## What the recorded failure establishes

The immutable 10:00 original L11S3 save is 40,844 bytes, generation 98, slot 1,
with SHA-256 `34c15d8bbc0a1475bcfc8be4bc8bbca9cd1b41413ed29f4b398d69fc327b81e0`.
The 14:43 fresh process fetched that native payload and reached the ordinary
loader's `world` stage, returning `RF_NOT_FOUND` (-3). The prior NPC codec and
resource-admission stage had succeeded. Its RFNC14/RFMC1/RFPC1 sections contain
19 NPC rows, 19 mover rows and 118 prop rows respectively. Original saved
bytes, assets and reports remain unchanged.

The previous report cannot establish the exact failed semantic guard:

- `rf_scene_world_load_reject` records only the enclosing `world` phase. Its
  zero byte count is the caller's accumulated cost, incremented only after
  the entire world preparation succeeds. It does not prove no private world
  allocation or NPC placement occurred.
- `rf_scene_npc_checkpoint_reject_state` belongs to capture admission. Row,
  placement and other restore-only failures need not update it.
- The existing world/NPC restore subphase messages were host-only optional
  printf probes. Native builds discarded those strings instead of retaining
  them in observed telemetry.

Saved rows alone cannot reconstruct every live registration, collision,
resource and staged-owner condition. No unobserved guard is named as the
cause, and no admission condition is bypassed to make the load succeed.

## New observations

`rf_scene_world_restore_reject[8]`, in `scene_world_restore.inc`:

1. FNV1a hash of failed subphase
2. Status cast to uint32
3. World stage's retained allocated-byte accounting before discard
4. NPC row count, once decoded by preflight
5. Mover row count, once decoded by preflight
6. Prop row count, once decoded by preflight
7. Last NPC placement UID, or UINT32_MAX before any placement
8. That placement's zero-based saved row index, or UINT32_MAX

Subphases distinguish envelope/identity, each component preflight, clutter
preparation/copy, mover decoding/preparation, control preparation/application,
and NPC decoding/preparation. All early returns now retain their phase.

`rf_scene_npc_restore_reject[6]`, in `scene_npc_checkpoint_restore.inc`:

1. FNV1a hash of failed detail
2. Row index, or UINT32_MAX for a non-row failure
3. UID, or UINT32_MAX for a non-row failure
4. Status cast to uint32
5. Number of admitted resident capture rows encountered
6. Number of supplied saved rows

For `resident_capture`, index is the resident actor index and UID is that
actor's authored UID. For row preparation, index is the sorted saved-row
index. `pair_fit` occurs after all rows and reports index equal to saved
count, with UID UINT32_MAX. Named details also distinguish count/storage,
row identity/vitals, movement, physics, placement, support, animation and eye
preparation. Existing `rf_scene_checkpoint_world_reject[9]` supplies finer
placement detail where its original recording sites apply.

Each new array resets when its preparation service begins. A zero phase hash
means no failure was recorded by that service; remaining fields may contain
successful progress. Neither array publishes actor state or changes any
guard, status, ownership, cleanup, allocation budget or checkpoint wire byte.
The parent harness must read these symbols from the new consumer's map while
continuing to use the original, hash-pinned saved HDD in snapshot mode.

## Verification boundary

Only source inspection and read-only inspection of existing reports and saved
bytes were performed for this diagnosis. No build, test, emulator, fixture,
new save, original-asset mutation, PC runtime, campaign traversal or Git
publication was performed by the helper. Exact native failure identification
and any subsequently justified correctness repair remain pending.
