# DEV scene destruction checkpoint integration â€” 2026-09-15

The layout below was implemented in scene.c after this design review. PC fresh-scene tests now cover empty, repeated, shallow and reset states; native verification is in progress. This is a destruction checkpoint, not a complete game save.

Original design follows over `rf_geomod_terrain_history_size/encode/decode` (RGCH v1), based on the current scene owner. The design sections describe intended boundaries; current verification is recorded in TO-DO.MD and artifacts/geomod-checkpoint/report.json.

## Boundary and first usable result

Implement one bounded destruction blob for the currently supported DEV room. Capture at a completed frame boundary before `scene.c`'s `done:` teardown frees terrain resources. Restore into a **newly constructed, unpublished scene** just after `scene_terrain_open(stream,level)` succeeds. Failure discards that new scene through existing `done:` cleanup. Do not import into an already running scene in this first integration: RGCH publication is atomic only for the core terrain, not for subsequent draw subdivision, collision-overlay binding or atlas reconstruction.

Keep input/file transport outside the codec. PC can initially supply two explicit diagnostic paths (`RF_REPLAY_GEOMOD_CHECKPOINT_OUT` and `_IN`); Xbox can receive the same fixture blob during its ordinary native harness setup. This is process-local automation, not host input. Actual writable Xbox save storage is a later platform step; a fixture read from `D:` is not user save support.

The first restore proves destruction state after scene reconstruction. It does not restore live rockets, debris, weapon firing/reload clocks, actor state or the player body. For an immediately comparable capture, use a settled endpoint with no transient lights and a separately supplied fixed diagnostic viewpoint. Continuing ordinary player input requires the independently maintained player/body state to be restored before placement; do not infer this from terrain roundtrip success.

## Proposed RFDS v1 layout

All integers and IEEE-754 float words are explicit little endian. No C-structure dump, runtime pointers, padding, host-endian hash input or GPU handles. Reject unknown versions and flags. Hash fields below are content identities, not security authentication.

A fixed 288-byte header:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | `RFDS` magic |
| 4 | 4 | Version = 1 |
| 8 | 4 | Exact total byte length |
| 12 | 4 | Flags = 0; v1 is normal noise lighting, DEV template0/room0 |
| 16 | 64 | Zero-terminated canonical level entry name, remaining bytes zero |
| 80 | 32 | SHA-256 of canonical original DEV source geometry, source filters and ordered stable source-material identities |
| 112 | 32 | SHA-256 of the loaded RFCT template bytes |
| 144 | 32 | SHA-256 of canonical substrate asset identity plus decoded pixel content/dimensions |
| 176 | 32 | SHA-256 of effective GeoMod regions/default hardness after optional shallow fixture substitution |
| 208 | 8 | Substrate texture width, height |
| 216 | 24 | Three exact codec minima then maxima as float words |
| 240 | 4 | Admission count, 0..128 |
| 244 | 4 | Terrain orientation RNG value |
| 248 | 4 | Retained noise map count, 0..1024 |
| 252 | 4 | Embedded RGCH byte count |
| 256 | 4 | Noise RNG continuation value |
| 260 | 12 | Atlas packing cursor x, y, row |
| 272 | 4 | Final physical face count, 0..800 |
| 276 | 12 | Reserved zero words |

Body sections in order: RGCH blob; `admission_count` 48-byte records; `map_count` 88-byte records; `face_count` 16-bit map indices. Total length must equal the bounded section arithmetic exactly, with no trailing data. The maximum current record sizes require about 111 KiB: 288 + 12,380 + 128Ã—48 + 1024Ã—88 + 800Ã—2 = **110,524 bytes**. A single 112 KiB external blob cap is sufficient for v1. This is serialization storage, additional to existing live owners and RGCH decode scratch.

Admission record (48 bytes): adjusted center 12, two raw signed limit vectors 24, saved scale 4, three packed requested-center uint16 values 6, zero reserved uint16 2. Preserve all admissions including those whose later CSG failed. The exact codec bounds are part of the state because the port deliberately fixes them at terrain initialization. Restore raw vectors and adjusted centers directly; do not rerun region preparation, duplicate admission or shallow alignment. Continue the saved terrain RNG; do not derive it from successful cuts. No template/room keys are needed in v1 only because flags explicitly restrict the format to the current template0/room0 owner.

Map record (88 bytes): plane[4] 16, minimum[3] 12, maximum[3] 12, material token 4, atlas x/y/width/height 16, base_seed 4, two projection axes 8, projection scale[2] 8, projection offset[2] 8. Keep every retained map, even when it has no current face reference. Save no image index or pixel hash.

Physical face binding record: uint16 map index, or `0xffff` for authored faces. This explicitly restores the exact retained map selected before save; it avoids relying on a later containment search choosing the same candidate. Require each generated face to reference a valid compatible map. Reconstruct `terrain_bindings[f]` from that map's projection and the new registered atlas image index. The binding array addresses physical face order; draw subdivision keeps each face's index while adding vertices, so do not save a separate render-vertex binding table.

## Stable material identity with the existing RGCH layer

The shared RGCH codec intentionally stores caller-owned uint32 material IDs; a scene wrapper must normalize them. Current DEV box/rocket cutters all use the single level-authored GeoMod substrate (`settings.texture`, loaded immediately before `scene_terrain_open`). On export, require every committed cutter face and retained noise map to use `s->terrain_material`, and rewrite each to stable token0 in the wrapper's private blob. On import, require token0, verify the substrate content identity, then rewrite it to the newly loaded `s->terrain_material` before calling RGCH decode. Reject additional materials in v1 rather than silently mapping them to the substrate.

Hash source geometry using ordered source-face IDs, positions/UVs, collision policy fields and source texture names/content identities, **not** transient material slots. Source faces themselves remain the newly loaded original source and are not imported from RGCH. The current original geometry filters are supplied by `scene_terrain_open`; verify their identity before core import. Use explicit canonical fields for region hashing, including substituted shallow fixture values; never hash padding or pointers.

A source-content hash match must not depend on PC/Xbox renderer allocation order. If existing preparation tooling already has content hashes, carry them into the runtime manifest; otherwise use a shared streaming hash implementation or verified build-time fingerprints. Do not replace semantic identities with a hash of entire `scene_stream` or `rf_material` memory.

## Capture and restore ordering

Capture:

1. Require a healthy DEV scene using the ordinary retained-noise path (`terrain_shadow_reference == 0`). Run after the frame's `scene_terrain_lighting` work and before the `done:` cleanup. Do not emit a checkpoint after a scene error.
2. Require all current maps fully baked: `owner->bake == owner->count` and `owner->sample == 0`. Drain pending base work through a deliberately factored base-only helper if necessary, or return an explicit not-ready status. Avoid calling gameplay/render callbacks repeatedly just to finish baking. Dynamic pixel overlays are not serialized.
3. Query/export RGCH, capture admissions/RNG/bounds, maps and final face bindings. Validate every expected generated-face map reference. Normalize the one material token and emit the exact-sized blob.
4. Hash or compare the completed blob for diagnostic reporting. Any file write must use a staging file and checked close before replacement; do not publish a partially written save.

Restore into a new scene:

1. Parse header, lengths, fingerprints and all metadata into bounded scratch before mutating owners. Reject unsupported mode, material, region identity and inconsistent geometry bounds. The effective shallow fixture must already have been constructed by `scene_terrain_open`.
2. Remap token0 in a private RGCH buffer, call `rf_geomod_terrain_history_decode`, then `rf_geomod_terrain_get`. Validate final face count and every saved face-to-map association against the rebuilt geometry. Run `scene_terrain_bind` to rebuild draw subdivision, source IDs and collision overlay. Any failure discards the new scene; no visible half-restore is published.
3. Copy admissions, bounds and orientation RNG. Populate the existing noise owner from validated maps and cursors. Regenerate the **base** atlas rectangle for each map using its saved seed; do not advance the saved continuation RNG while regenerating. Recompute each map's pixel hash from restored base pixels. Set `bake=count`, `sample=0`, `cuts=view.cuts` and importantly **`generation=view.mesh.generation`**. Saving/restoring an old generation number or setting zero makes `scene_terrain_lighting` reset or reinterpret this state.
4. Invalidate `terrain_light_cache`; it must compare against newly constructed live lights. Mark the full atlas dirty. Dynamic overlays are recomputed normally from whatever the broader scene has restored; matching an active rocket glow is not promised unless that rocket is also restored.
5. Defer image binding completion to `rf_scene_prepare_lightmaps`. It registers `s->terrain_atlas` after `scene_terrain_open`, via `particle_draw_stream`, and only then assigns `terrain_atlas_index`. Rebind every map and current face to this new index before the first `scene_terrain_lighting`/draw. Do not persist an old image index or allow `UINT32_MAX` to reach rendering. `rf_scene_update_lightmaps` then performs the normal staged pixel upload.
6. Only after restore and registration succeed may normal movement, collision and rendering consume the scene. Use the restored collision overlay for player placement; the original room bounds may reject a player legitimately standing in an excavated opening unless the broader placement logic supports that state.

Extracting base-map regeneration and binding-finalization into small helpers is sufficient; a general save manager or second `scene_stream` architecture is not needed. The RGCH codec's scratch budget does not cover the RFDS blob or metadata staging: include these explicitly in scene peak accounting and free the blob before gameplay once ownership has moved.

## Validation rules beyond RGCH

- Count and byte arithmetic use wide intermediates; header length is exact; reserved bytes are zero. Admission scales are finite and positive; centers/vectors finite; codec min/max ordered and match initial level bounds. Zero RNG state is legal for the CRT LCG and must not be rejected arbitrarily.
- Map normals/planes, bounds and projection values are finite; axes are distinct and 0..2; width/height fit the existing 64Ã—64 tile limit and valid lightmap grid rules; atlas rectangles fit 512Ã—512 without overlap. Replay the deterministic row packing over ordered map dimensions and require exact x/y/row/cursor agreement. Recompute expected projection from stored bounds/dimensions rather than accepting arbitrary UVs.
- Recompute the ordered seed continuation from initial noise RNG1, consuming each map's widthÃ—height pixels through the verified noise generator; require each base_seed and final RNG to match. This is appropriate only for v1's current isolated noise stream and must be versioned if that ownership changes.
- Every generated physical face has a map with the expected material, compatible plane and enclosing bounds under the existing `scene_terrain_noise_plane` / containment tolerance. Authored source faces use the sentinel. Retain unreferenced maps; do not compact them as a save optimization.
- Capacity failure or malformed metadata must fail before the new scene becomes available. An in-place restore would need additional atomic ownership staging and is expressly outside this first begin/end integration.

## Process-local acceptance fixture

Use the existing three-shot settled recording and a shallow-overlap recording. First scene runs ordinary inputs and emits RFDS at its normal end. A second invocation in the same test process reloads the identical DEV level and imports RFDS at scene begin, then renders a fixed diagnostic camera through a short neutral interval. Compare committed RGCH bytes after normalizing material tokens, admissions/bounds/RNG, all retained maps/seeds/cursors, exact physical geometry and representative collision rays/sweeps. Compare the native framebuffer for the settled crater, explicitly inspecting actual content. Repeat in a fresh process to eliminate accidental static-state dependencies.

For next-edit continuity, drive the same explicit prepared cut and admission request against an uninterrupted control and the reloaded owner, with no gameplay effects; compare resulting history, geometry, collision and new map seeds. This isolates destruction continuation without falsely claiming full actor/input checkpoint coverage. A later ordinary rocket-input continuation must also restore player body/weapon state and clocks, or first arrange an identical diagnostic spawn and firing state in both branches.

Required edge fixtures: no cuts; eight cuts; mixed boxes/rockets; one/two shallow limits; an admitted-but-rejected cut; duplicate rejection; terrain reset followed by later blast; unreferenced retained maps; altered material runtime slot; wrong region/template/source fingerprint; malformed map rectangle/projection/seed; output-face capacity failure. Run the same RFDS bytes on PC/Xbox and measure peak free pages during restore, not just the settled endpoint.
