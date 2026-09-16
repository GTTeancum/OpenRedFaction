# Live GeoMod save/load gap — 2026-09-15

## Finding

There is currently **no destruction save/load path**. `include/rf/campaign.h` explicitly calls its player state a first-pass section handoff, not an on-disk save format. `src/diagnostic/scene.c` retains campaign counters, pickup retirement, actor vitals/drops, switches and trigger state across section transitions, but closes the terrain, collision overlay, lightmap owner and admission history when the scene ends. The PC and Xbox entry points carry player state to the next section; neither carries destruction.

This is a source audit, not a runtime save test. No executable code, builds, emulator state or original assets were modified. Original behavior references are `CRATER-SAVE-REPLAY-20260915.md` and `CRATER-AUXILIARY-RELOAD-LIFETIME-20260915.md`; those reports have deliberately limited executable coverage.

## Existing owners and missing state

| Owner / function | State already retained during a scene | Save gap |
| --- | --- | --- |
| `rf_geomod_terrain` in `src/core/geomod.c` | Up to `RF_GEOMOD_CUT_LIMIT` (8) successful cutter meshes, kernels, star/convex mask, mapping dimensions, original geometry and collision policy | No public cutter-history export/import; `rf_geomod_terrain_get` returns the resulting mesh/tree, not the source cutters |
| `scene_stream.terrain_history[128]`, `terrain_requested`, fixed codec bounds | Adjusted centers, signed shallow vectors, saved scale, packed requested centers, admission count | Lost on close; does not retain basis, template identity, room identity or which admissions actually published geometry |
| `scene_stream.terrain_random` | Orientation RNG continuation, initialized to 1 when terrain opens | Lost on close; DEV terrain reset currently clears admission history but does **not** reset this RNG |
| `scene_terrain_noise_owner` | Ordered retained maps, per-map seed/plane/bounds/material/projection, atlas packing cursor, random continuation, bake/sample progress | Lost on close; no stable serialized mapping identity and current image indices are runtime handles |
| `scene_terrain_bind` and `terrain_bind` | Collision overlay, source IDs, generated-face bindings, draw mesh | These are derived resources and need rebuilding, not pointer serialization |
| `scene_terrain_input` | Successful prototype box edits and guarded DEV reset | Box edits do not enter rocket admission history, so a rocket-record-only save omits visible holes |

The admission count and successful cutter count are intentionally different. In `scene_rockets_tick`, history is written before `rf_geomod_random_basis`, debris preparation and the CSG call. A later cut can fail while admission history and consumed RNG remain changed. Such an entry still affects duplicate rejection and future shallow alignment. Serializing only successful geometry therefore changes future gameplay; serializing only the current admission records cannot reconstruct successful geometry.

`RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT` writes an RGM1 mesh dump for audits. It omits cutters, collision policy/tree, admissions, codec bounds, RNG, atlas allocation and seeds. It is not a savegame and must not be promoted to one by adding a load button.

## Smallest correct shared integration

Use a versioned **port-owned destruction checkpoint**, not an attempted original save-file decoder. Keep the current bounded owners and add export/rebuild boundaries; do not introduce a general entity serialization framework for this work.

1. Add explicit little-endian destruction encoding/decoding to shared C code, with a header containing format version, level identity and source geometry/template/material-content fingerprints. Validate counts, sizes, finite numbers, enum/range values and the complete byte span before publishing any state. Never dump pointer-bearing C structs or use current material/image indices as persistent asset identity.
2. Expose the opaque terrain's committed cutter records. Each record needs vertex/face counts, position/UV corners, face materials mapped through stable level asset identity, convex/star kind and kernel when applicable. Save only `t->count` committed slots; the slot at `count` may contain scratch from a failed cut. Retain mapping width/height and cavity/source-policy identity. Rebuild through the same validated preparation/publication code, using original source geometry and filters. A final mesh by itself is insufficient because the next blast recomputes CSG from original geometry and the full successful cutter history.
3. Save all current admission entries independently: exact adjusted center, raw signed vectors, scale, packed requested center, fixed encode/decode bounds and count. Save the orientation RNG continuation separately. The present DEV path has template0/room0 only; explicitly mark this scope in the version or records, and reject unsupported identities rather than silently reusing room0. Future generalized admission records need template/room keys before campaign destruction is enabled.
4. Preserve renderer map metadata and seeds in their current order, including maps no longer referenced by current faces. Recreate the atlas from base seeds, rebind its new image index and recompute the dynamic overlay from current lights. Save the packing cursor (`x`, `y`, `row`) and noise RNG continuation so later maps allocate and seed exactly as before. Plane, bounds, dimensions, projection and stable material identity suffice to reconstruct each retained map; runtime image handles, pixel hashes and cache generation numbers should be rebuilt. Use an explicit frame-boundary checkpoint at which pending base noise baking has been drained; otherwise partial bake/sample state must also be represented. An in-flight checkpoint cannot silently drop unbaked maps.
5. Wire capture before scene teardown and restore after original geometry, templates, materials and the terrain owner exist, before movement/collision or rendering consumes the scene. Natural scene integration points are the successful-transition block at `done:` and the `scene_terrain_open` / `scene_terrain_bind` initialization path. Initially prove same-room unload/reload in the DEV harness; campaign geometry ownership is not yet generalized beyond the DEV cavity.

The smallest standalone core API can be a bounded cutter-history export/import plus validation; admission and lightmap records can use explicit shared value types even while orchestration remains in `scene.c`. Avoid making a general save subsystem depend on all of `scene_stream`.

A fresh scene rebuild can deserialize the small checkpoint, release the departing scene and recreate its resources within the existing budgets. For an in-place DEV reload, keep the current terrain valid until a replacement terrain and matching bindings validate, or use a separately proven rollback path. Do not reset the live terrain and then incrementally apply untrusted records: failure halfway through would destroy the current room. Account any replacement owner against the peak memory budget rather than assuming existing free memory permits it.

### Why not replay only impact requests?

The current source does not retain successful bases or outcomes. Re-running collision, hardness/admission, effects and RNG from impact requests would introduce dependencies on transient gameplay. Replaying only decoded packed centers also changes the original exact adjusted centers through quantization. Exporting the existing committed cutter meshes avoids both problems and covers prototype boxes, rockets and mixed histories without rerunning damage, sound, debris or ammunition use.

Alternatively, a future explicit successful-operation log can retain basis, exact adjusted center, scale, effective limits and successful outcome. That is smaller than cutter meshes but requires new ownership now, must handle every edit type, and is unnecessary for the current eight-cut owner.

Rebuilding noise from only the final visible faces is also incorrect: old maps remain retained and consume allocation space and RNG even when later cuts remove their last face. The local audit finds 121 retained maps but only 114 referenced maps in the examined three-cut state. A deterministic operation replay would have to reproduce each intermediate lighting-binding update, including multiple edits in one frame, to substitute for saving map metadata. Persisting that metadata is the smaller correctness change.

### Original save behavior versus the port policy

Original `4674b0` replays 32-byte records in order, preserving saved scale by calling `45cff0` with `applyHardness=0`. It does not rerun admission/duplicate gates, does not reduce history count while replaying, and can inspect stale future auxiliary records. Identified constructor/reset/restore routines do not clear those auxiliary bytes. This is not a desirable dependency to recreate in the port.

The proposed port checkpoint preserves the current exact auxiliary history and resulting committed cutters. It deliberately avoids recomputing shallow alignment through stale-memory behavior and avoids applying hardness twice. This is a robust port-owned state restore, not a claim of byte-compatible original save replay.

## Stock 64 MiB bound

The existing eight committed cutters require at most about 12.3 KiB of explicit fields (8 × [60 × 20-byte corners + 20 × 16-byte faces + counts/kind/kernel]). Current 128 admission entries require approximately 6 KiB, plus codec bounds and a few counters/RNG words. Current retained-map capacity is 1024; an explicit pointer-free record containing the needed fields can remain below 128 bytes each, or at most 128 KiB, without copying a second 512×512×2 atlas into the checkpoint.

These are design upper bounds, not measured implementation peaks. Preserve the existing terrain 1 MiB budget and external atlas budget; measure serialization scratch, temporary replacement owner and allocator overhead on stock-memory Xbox. Do not allocate 128 levels × full checkpoint capacity in RAM. A bounded per-level store or eventual disk-backed store must report exhaustion explicitly; silent hole eviction would block campaign traversal. Xbox writable save storage and crash-safe file replacement remain platform integration work—`D:` here is the game disc and existing diagnostic reads are not a save destination.

## Required verification before calling this complete

- Zero cuts, one rocket, the existing three-cut state, all eight successful cuts, box-only and mixed box/rocket: restore exact physical mesh positions/UVs/materials/source IDs and collision ray/sweep results.
- One and two shallow limits, orthogonal/oblique overlap: preserve adjusted/raw admission state and subsequent alignment. Save after a duplicate rejection and after a CSG rejection; distinguish admitted history from published cuts.
- Continue with the same next input after reload and compare the next cutter and RNG continuation against uninterrupted play, including after DEV reset (whose current orientation stream continues).
- Preserve all retained atlas metadata, including unreferenced maps, base seed sequence and regenerated pixels; next-map allocation and seed must match. Invalidate dynamic caches and test a light entering/leaving after restore. Compare actual crater output, not only hashes or draw counts.
- Fresh-process load and same-process repeated load must agree without stale auxiliary influence. Wrong level/template/material fingerprint, truncation, bad counts, invalid geometry, nonfinite values and capacity failure must leave the prior usable state unchanged or fail before scene publication.
- PC-written data accepted on Xbox and vice versa; compare physical geometry, collision, admission and atlas states, then inspect native framebuffer content. Measure peak free pages during restore, not only the settled scene.
- Revisit/load terrain before placing the player; confirm a saved traversable hole remains traversable. A destruction checkpoint alone does not provide full player/AI/mover/projectile/debris save coverage, so do not label the initial feature a complete game save.
