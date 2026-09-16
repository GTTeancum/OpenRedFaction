# RFDS pure selection validation: minimum extraction

Read-only source audit, 2026-09-16. No source changes, builds or emulator runs. The transport's pure callback must reject an unusable newer RFDS without changing live scene state. This document identifies the narrow extraction needed; it does not replace the writable-storage plan.

## Implementation update (2026-09-16)

The synchronous core `rf_geomod_terrain_history_check` is now implemented and its focused rollback/next-cut tests passed in the primary task. Scene integration now adds `scene_checkpoint_validate(const void *, uint32_t, void *)`, matching the two-slot callback ABI, before the mutating restore. It validates the complete existing RFDS envelope, identity, admissions, map packing/projections, noise chain, const RGCH materials and prepared terrain face mappings; its visitor verifies overlay prerequisites and render subdivision capacity. The map reader is shared with restore. The subdivision function shares its edge predicate and sequential polygon insertion between sorted publication and read-only all-vertex count mode. This adds no cloned owner, second atlas or file buffer. Terrain prepare scratch/peak remain the documented exception to opaque-owner bytewise purity.

PC opt-in `RF_DEV_GEOMOD_CHECKPOINT_VALIDATE_AUDIT=1` checks twice and prints `GEOMOD_CHECKPOINT_VALIDATE_AUDIT PASS <status> <bytes>` only when return statuses and SHA256 fingerprints match. Fingerprints include input, all128 admission records and RNG, active noise/maps/bindings, atlas pixels, active draw mesh, overlay indices/bounds/pointers, live terrain mesh/tree and diagnostics; explicit fields avoid padding. Opaque core history is covered separately by core tests, and unused uninitialized overlay allocation is excluded. This source update has not yet been built or executed by this agent; primary owns build and malformed/replay validation. Two-slot mounting/selection and durable native flush are still pending.

The remaining sections preserve the preimplementation rationale and test obligations; statements there describing the core API as absent are historical, superseded by this update.

## Existing facilities and limits

There is no public terrain clone or history dry-run API. `rf_geomod_terrain_history_decode` at geomod.c1915 parses into bounded `terrain_history_copy`, validates cutters, temporarily exchanges history, calls `terrain_publish`, and restores history on failure. `terrain_publish`1683 prepares inactive mesh/position/face banks and a separate collision tree, then commits mesh, closes old tree, swaps tree/bank and publishes count. `rf_geomod_storage_pending/abort` already support an unpublished mesh. Terrain owner and collision preparation are opaque to scene code.

`terrain_history_roundtrip` in tests/geomod_interior_tests.c506 proves that opening a **second full owner** with the original geometry/filters/mapping and decoding reconstructs equivalent mesh/query state. It does not supply a clone API or prove a second owner fits the live64MiB scene. Current DEV owner uses4096 vertices,800 faces and1MiB owner budget (scene.c9153); opening another requires another owner/storage/two banks/tree allocation, separately charged. No artifact experiment was warranted: no public cheap clone exists to exercise, and rerunning the existing reopen test would not establish live memory safety.

## Smallest core extension: synchronous visitor, not a public transaction owner

Proposed header signatures:

```c
typedef int (*rf_geomod_history_check_fn)(
    const rf_geomod_terrain_view *candidate, void *context);
int rf_geomod_terrain_history_check(rf_geomod_terrain *terrain,
    const void *data, uint32_t bytes,
    rf_geomod_history_check_fn check, void *context);
```

The callback sees prepared mesh, matching collision faces and pending tree only for its duration. It must not retain pointers or call terrain edit/get APIs reentrantly. Returning RF_OK approves validation only; this function **always aborts**, even on success. Existing decode retains its ABI and is the only publication entry for imports. Published geometry/history/filter ownership is unchanged after check; scratch contents and peak-allocation diagnostics may change. Do not promise bytewise immutability of the opaque owner.

Inside geomod.c, factor current publish into private `terrain_prepare(t,count,pending)` / `terrain_commit(t,pending)` / `terrain_abort(t,pending)`, where a small stack descriptor holds pending mesh view, owned pending tree, target bank and count. Existing `terrain_publish` becomes the three-step wrapper. Factor current decode into a shared private import routine with an optional candidate visitor and a publish boolean; public decode passes publish=true/no visitor, check passes publish=false/visitor. Avoid duplicating the RGCH parser.

Sequence: validate envelope/count/mapping -> allocate existing rollback history (charge budget and peak) -> parse/validate cutters -> exchange candidate history -> prepare mesh/mapping/collision -> invoke visitor -> commit only for decode -> abort if check or any error -> restore history unless committed -> remove scratch charge/free. Keep collision-tree allocation inside the check; omitting it could accept a checkpoint whose real restore exceeds budget.

Rollback obligations:

- Any parse/prepare/visitor failure leaves published mesh generation, mesh pointer/content, tree pointer/content, bank, count and encoded cutter history unchanged.
- Abort destroys only the pending tree and clears storage editing, never the live tree. Always clear editing after a candidate prepare failure; maintain existing paths that already abort internally.
- Candidate history exchange is reversed after every check, including successful callback. Published source geometry/filters/generated filter/cavity/mapping remain caller configuration.
- The parser currently writes work planes before history exchange. These are scratch and recalculated by the next publish; tests must prove that next cut matches an uninterrupted control after successful check and rejected check.
- Maintain `base_bytes` scratch charging until pending tree is destroyed; include actual allocation in `peak_bytes` even when validation fails.
- Generation UINT32_MAX remains RF_RANGE (storage_begin cannot prepare another generation). No special bypass is needed for an unloadable checkpoint.

## Scene extraction: validate without writing atlas/admissions/bindings

Proposed private functions:

```c
static int scene_checkpoint_validate(const void *bytes, uint32_t length,
                                     void *scene_context);
static int scene_checkpoint_map_read(const unsigned char *record,
    scene_terrain_noise_map *local_map); /* one record, no owner writes */
static int scene_checkpoint_candidate_check(
    const rf_geomod_terrain_view *candidate, void *validation_context);
```

Use a small context holding scene pointer, const RFDS buffer/counts/offsets; do not allocate another buffer or1024-map owner. Extract identical parsing predicates from `scene_checkpoint_restore`9311 rather than changing them:

1. RFDS magic/version/exact size/reserved bytes; NUL-terminated level name and zero padding; all128 identity bytes; texture dimensions; history bounds; admission/map/core/face maxima and64-bit exact span total.
2. Admissions: all10 floats finite, positive scale, reserved bytes zero. Read into a local record or validate in place, never `s->terrain_history`/requested arrays.
3. Each88-byte map: decode one local map; finite/unit plane, ordered finite bounds, zero reserved material word,64x64 max dimensions/atlas placement, exact packed cursor order, strict dominant-axis tie rules, finite positive scale/finite offset and exact projection formulas, `rf_geomod_lightmap_size` agreement. Reuse that local map decoder later in restore to avoid divergent checks.
4. Noise sequence: expected start1 except the existing empty-cuts/maps lazy RNG0/1 rule. Require each base seed equal current chain. `rf_geomod_light_noise`88 consumes exactly one `rf_random_next` per texel; a pure loop can advance the local chain width*height times without RGB/atlas writes. Verify final RNG and atlas cursor/row. Keep the existing generation0 semantics for never-initialized empty terrain.
5. Require every encoded RGCH cutter face material0 and source-face sentinel. Do **not** call current `scene_checkpoint_materials` because it rewrites the input buffer. Split it into const check and later remap, or add an explicit validate-only helper sharing its parser. The dry-run core may keep generated material0: source material identity is already hashed; candidate geometry and collision filters do not depend on replacing material0 with runtime image slot.
6. Call `rf_geomod_terrain_history_check` on the unchanged RGCH subspan. Candidate visitor checks exact face count; every authored face's map index must65535; generated faces must material0 and a valid map index. Decode only the referenced map into a local record; compare plane with `scene_terrain_noise_plane` and every candidate vertex against map bounds with existing1e-4 tolerance. No need to retain all maps.
7. Validate render/binding capacity discussed below. Check light image array/non-null and count<256 before declaring the RFDS usable. No atlas reservation/registration, noise diagnostics, cache clearing, projection writes or random owner mutation is permitted in this callback.

After transport selects a candidate, restore once in a fresh scene using the existing real decode/rebuild path. Material remapping may then mutate that owned input buffer as it does now. Selection validation does not make fresh-scene restore infallible under later allocation failure; restore still aborts scene construction before frames on failure. Never retry another slot against the partially published same scene.

## Render capacity is part of semantic validity

`scene_checkpoint_restore`9377 calls `scene_terrain_bind`, which calls `scene_terrain_subdivide`8591. This can fail **after** RGCH publication because inserted seam points overflow a64-corner polygon or8192 draw vertices, or final counts disagree. Merely proving core face_count<=800 is insufficient.

Extract its edge-candidate predicate/minimum-fraction/minimum-index tie rule into a shared helper, then add a pure count-only pass. A single local polygon[64] carries the exact sequential insertions and UV interpolation; global inserted/written counts reproduce capacity checks. To avoid a24KiB temporary sorted[3][4096] allocation or mutating the live draw owner, the validation pass can scan all candidate vertices for each edge, using the same coordinate bounds/fraction/error/tie predicates. Production subdivision keeps the sorted acceleration. This is load-time work, with no new long-lived memory. Test equivalence is required before relying on the brute-force versus sorted enumeration: identical minimum/tie rules should make ordering irrelevant, but that is a proposed implementation property, not executed evidence.

`rf_geometry_collision_overlay_bind` (geometry.c968) additionally requires overlay.storage, tree.face_count==count<=index_capacity, non-null tree faces/source_indices for nonzero count, non-null nodes when node_count>0, and no alias of input IDs/tree source_indices with overlay.source_indices. Every tree source index must be<count and its resolved face ID non-sentinel; use the original source ID or scene terrain_fallback for generated faces and require fallback!=UINT32_MAX. Root node minimum/maximum must be finite and ordered. Check these against the pending tree without invoking the mutating overlay bind. Existing scene terrain_ids allocation must exist and not alias overlay storage. Render diagnostic arrays remain unchanged even when semantic validation succeeds.

## Existing tests versus new proof obligations

| Obligation | Existing evidence | Required addition |
| --- | --- | --- |
| RGCH parsing/cutter geometry | `terrain_history_roundtrip` rewrites total length at every truncation; NaN/collapsed face/duplicate face/source-ID rejection | Run same vectors through check; callback must never run on parser failure |
| Old mesh/history/tree on failure | Existing roundtrip snapshots generation/pointers/history; tight-budget and output-face-capacity rejection | Check success and callback-rejection also preserve mesh bytes/query results, old tree pointers and encoded history |
| Continued future cuts | Existing copy/control next-cut mesh and3-axis collision comparisons after failed decode | Add successful check then rejected check then next cut versus an uninterrupted control; current control itself decodes, so identify exact scope |
| Candidate equivalence | Existing second-owner decode matches source mesh/material/filter data | Visitor captures numeric summary/hash then compare to normal decode; verify candidate tree rays before abort |
| Fresh scene RFDS | `tools/dev_geomod_checkpoint_check.py`: empty/repeated/shallow/reset, empty next blasts, subsequent blast, identity/version/count/truncation rejects | Invoke pure validator twice on each file, compare scene admissions/maps/atlas/RNG/diagnostics and raw input bytes before/after |
| Map structural validity | Existing restore code checks normals/projection/RNG | Corrupt unused map normal, infinite projection, tiny-span overflow, packing/RNG, map indices, authored-face sentinel and late generated-face bounds; pure rejection before any writes |
| Render capacity | Existing live subdivision used by PC/Xbox checkpoint replay | Count-only/sorted agreement on retained checkpoints and deliberate64-corner/8192-overflow candidates |
| Slot fallback | New checkpoint_file_tests proves abstract caller semantic rejection fallback | Newer file with valid RFSG checksum but invalid late RFDS face mapping must fall back; subsequent store protects older valid file |

Malformed fixture generation is retained in `tools/generate_rfds_malformed.py`, execution in `tools/verify_rfds_malformed.py`. The failed-admission continuation reproducer is `tools/verify_geomod_failed_admission.py`.

No existing test proves purity of `scene_checkpoint_restore`; it intentionally mutates. No checkpoint transport test currently substitutes for these scene-specific checks. This extraction is bounded to existing RFDS/RGCH lifecycle and requires no clone owner, second atlas, generalized transaction API or full-game save framework.

## Executed validation audit

PC/NXDK builds pass. tools/verify_rfds_malformed.py --audit-validation completed23 cases in artifacts/geomod-rfds-validation-runs/20260916-091327-145070:21 malformed payloads reject, valid control round-trips exactly, and the previously identified zero-basis no-op remains an accepted unchanged investigation case. Every case records two same-status pure validation calls and matching defined published-state/input SHA256 fingerprints. Core history_check, checkpoint_file_storage, debris bounce and relaunch CTests pass. This does not prove full game-state saves or native HDD durability; both remain pending.
