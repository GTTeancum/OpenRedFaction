# Pickup render room-cache cost reduction

The parent's15:00 original L1S1,240-frame,stock64MiB comparison measured
pickup drawing at5.491ms before and5.594ms after the first performance batch.
That phase was unchanged by the earlier work. This is emulator phase time,
not hardware FPS or input latency.

`scene_pickups_draw` already submits visible models through the retained GPU
backend. Its avoidable CPU work is re-running the complete collision-world
room locator for every untaken static authored item every frame. The written
cache retains only the query's room result and exact queried position. The
existing room visibility flag is still consulted each render; models, lighting,
collection policy, retirement and draw order are unchanged. NPC weapon drops
remain on their separate live path.

Cache entries invalidate when the accepted collision-world/views owner changes,
the shared terrain publication generation changes, or the one-shot campaign
wall/GeoMod owner changes state. Any item position change re-runs its exact
query. Successful whole-world publication clears the cache's world binding,
including same-generation restore cases. Unknown-room results preserve the
existing uncullable fallback. No cached decision bypasses current visibility.

Storage is optional20bytes per authored item, bounded by the existing1024-item
campaign limit. Allocation failure or a larger set retains the old lookup path;
there is no per-frame allocation and no new save format. The eight-word
`rf_scene_pickup_room_cache` telemetry reports frames, real queries, cache hits,
last visible/culled counts, bytes, invalidations and fallback.

Source-written after15:00; root16:00 owns the next comparison. No helper build,
test, new harness, image or gameplay-readiness claim. The next useful result is
actual query reduction plus pickup phase timing, not a coverage percentage.
