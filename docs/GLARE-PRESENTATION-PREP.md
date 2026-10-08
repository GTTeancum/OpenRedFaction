# Particle/corona owner preparation

Written after the17:00 Xbox batch, based on29a8660. No helper build, test,
fixture or XEMU run was performed. The parent measured4.201ms in the combined
particle/corona command stage with four emitted fans; that timing includes
backend commands and does not isolate the preparation changed here.

## Removed duplicate work

Both particle/volume drawing and corona drawing eagerly allocated a complete
owner snapshot, populated every mover/NPC/clutter view and fact, then freed it.
Population called handle APIs that scanned owner arrays even though the caller
already had the exact index. Per-room glare collection likewise rescanned the
whole glare array for each already-known instance.

Live presentation now prepares the snapshot only when a real parent visibility
or occlusion query needs it. Particles and coronas borrow the same snapshot
within one synchronous scene sink call. Payload storage retains the existing
128KiB maximum plus a small owner header; the first required use allocates it,
ordinary frames reuse it, and scene teardown frees it before owner teardown.
Capacity growth discards old storage before allocating, avoiding two retained
banks. No valid snapshot survives sink exit, error, or the next sink entry.
This is not an across-frame visibility cache.

Indexed constructors preserve exact generation-qualified registry identity,
object kind, model ownership, actor pose availability and live field reads.
Public handle APIs retain their ordinary lookup semantics and share those
constructors. Snapshot rows and candidate ordering remain movers, live NPCs,
existing clutter, then selected player. Exact array membership replaces the
fact-pointer scan; arbitrary handles still use the existing qualified search.
The glare room shortcut also checks the actual registered instance.

NPC visibility snapshot hashes honor the existing live diagnostic switch.
Diagnostic visibility passes and enabled checksum/volume fixtures keep eager,
independently owned snapshots and their normal counters/queries. The explicit
fixture code is not used to provide live gameplay behavior.

## Lifetime and effects

The existing Xbox sink calls particle drawing then corona drawing synchronously;
physics, events, attachment motion and retirement follow after it returns.
These callbacks can update glare samples, glare visibility caches, particle
presentation state and screen flash, but do not mutate the mover/NPC/clutter
views represented in the borrowed snapshot. Headlamp parent visibility retains
its real vehicle-owner path and can avoid building the general snapshot.

No occlusion result, room decision, glare sample, billboard, queue ordering,
visibility cadence, beam dimensions, random draw or emitted fan is cached or
suppressed. `rf_glare_visibility_search`, its current geometry/model consumers,
and the original alternating-frame refresh continue unchanged. Gameplay save
formats are unchanged; the retained buffer is disposable presentation scratch.

`rf_scene_glare_snapshot_cache[6]` reports fills, second-consumer shares,
allocations, retained bytes, populated objects and the last preparation error.
The18:00 parent batch must establish timing and functional behavior. No FPS
improvement is claimed from source inspection.
