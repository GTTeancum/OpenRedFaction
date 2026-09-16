# Generated lightmap native upload and cached-room submission

## Result

No discrepancy with a packed1555 lightmap atlas was established. Actual original lock/unlock wrappers and the complete RGB updater write the expected1555 bytes directly into a supplied native-format25 texture buffer, preserving pitch padding and original base RGB. Cached room submission selects the group's packed mode and binds base at stage0/lightmap at stage1. There is no extra conversion or gain hidden in these executed intervals.

Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Native allocation and upload probe

Run `python tools/verify_geomod_device_upload.py`; result `crater-device-upload.json` in that directory.

Four original format-selection intervals `545e20..545e54` exercise support sets{25,26,29},{26,29},{29},{} with a supplied device-support query `5463a0`. Internal image format5 uses `1cfc5e4`, which selects the first supported native format from25,26,29, retaining-1 if none. This is capability fallback, not a per-crater brightness choice. Normal supported25 is A1R5G5B5; alternate format semantics/conversion are not claimed equivalent by this task.

Four complete allocation/upload lifecycles use1x1,2x1,3x2,8x4 and varied pitch padding:

1. Whole `55b970` receives internal format5, dimensions, levels1, output pointer. Real selection chooses native25 and calls the supplied device CreateTexture method at vtable+50. Captured arguments are device,width,height,levels1,usage0,format25,pool1,output. The supplied method returns a texture object.
2. Whole `4f26a0` executes dirty8 upload, including actual `50e2e0 -> 55ce00`. `55ce00` resolves the retained bitmap record and calls supplied texture LockRect at vtable+40, then GetLevelDesc at+38. Original descriptor decoding identifies native25 as internal5 and passes the returned pixel pointer/pitch through.
3. Original RGB packing writes the native buffer. Every byte including row padding matches independently constructed `0x8000 | max(R>>3,4)<<10 | max(G>>3,4)<<5 | max(B>>3,4)`.
4. Actual `50e310 -> 55cf60` invokes supplied texture UnlockRect at+44; the retained lock count returns tozero. Mapping dirty byte clears. Persistent RGB remains byte-identical.

Only device/texture COM operations and bitmap-handle lookup `50f440` are supplied. The fixture provides an already resident bitmap record after allocation; full bitmap residency/allocation bookkeeping `55cc00` is not executed. This is original CPU evidence through device API boundaries, not a GPU or emulator test.

Static inspection adds the residency chain: `55cc00` calls `55c8a0` for single-level images; `55c9a9` passes levels1 into `55b970`. Bitmap mip count>1 takes a separate `55b700` path. Generated lightmap allocation therefore does not acquire additional mip levels through the single-level route. This does not establish the base material's mip pyramid.

## Cached room route

Run `python tools/verify_geomod_cached_submission.py`; result `crater-cached-submission.json`.

The original cached room caller contains `4f0c00` preparation at `55f755` and retry at `55f768`; it consumes room+4 cache. This is distinct from uncached/transformed `517180 -> 558960` previously discussed.

Four executed intervals `55ffe0..560059` cover matching/different cached mode and matching/different previously bound base texture:

- Real `52fcd0` compares group+20 packed mode against `1e64da0`.
- Changed mode invokes the **complete original `54f160`** at `560001`; unchanged mode skips it.
- `560025` binds group+40 to stage0 unless it equals the previously bound base handle.
- `560049` binds group+44 to stage1 every iteration.
- Supplied `55cad0` boundary records [stage,handle,tile] and returns unit texture scaling. Fixture handles47(base)/17(lightmap) are distinct sentinels, not original allocation observations.

All four cases preserve the expected ordering. Full54f160 calls use the existing fake COM state recorder, and the existing report covers their opaque solid MODULATE2X semantics. This new probe proves that mode/texture slots produced by cached room preparation feed those stages; it does not execute full room batching, culling, final vertex writes or GPU draws.

## Practical implications

Current atlas packing/channel interpretation remains supported. No unconditional bake, gain or alpha correction should be added from these findings. Generated mapping mip count is1 on the inspected allocation route, so missing generated-map mip levels are not an explanation for the dark interior. Base texture mip/filter behavior is being investigated independently.

Remaining gaps include real device capabilities/native-format fallback handling, actual GPU normalized sampling, full residency upload for base assets, atlas border behavior and stock-game visual comparison. The probes do not claim that all those paths match.
