# Crater backend routing and opaque lightmap combination

## Finding

Original renderer ID102 (`0x66`) selects the discovered `558960` draw route; initialization request106 (`0x6a`) selects102 as its default. The solid lightmapped mode uses unattenuated base texture RGB followed by supported two-times lightmap modulation. Alpha blending and testing are disabled for this mode. No hidden lightmap-alpha darkening, extra color multiplier or unconditional static relight was demonstrated. No shared-source correction is recommended from this bounded result.

Applies to original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Executed evidence

Run `python tools/verify_geomod_backend_stages.py`. Results: `artifacts/crater-shading-re/crater-backend-stages.json`.

- Six whole `517180` calls with backend0/101/102/103/104/106. Only102 dispatches to `558960`, with all four caller arguments unchanged. The backend callee is a supplied recording boundary.
- Six original initialization intervals `50c25f..50c282`. The code writes102, then replaces it with the caller-requested ID unless that request is106. This is the selector interval, not execution of window/device creation.
- Four whole `54f160` calls for texture sources4/5, capability byte `1cfcc1d`0/1, and the ordinary solid fields color1/alpha0/blend0/depth4/fog0. A supplied COM device records actual original texture-stage and render-state writes. Original packed mode decoding/cache behavior executes; batch is empty so GPU submission is excluded.
- Eight whole `55cfa0` calls with native format5 and supplied lock/release verify packed channel interpretation and source alpha independently of modulation: bits10..14 red,5..9 green,0..4 blue,15 alpha. The CPU result expands five-bit colors by8. Red `fc00`, green `83e0`, blue `801f` each return the expected248 in their own channel and alpha255. `0000` and `8000` differ only in alpha. This is CPU sampler evidence, **not** proof of actual device texture creation/upload or normalized GPU sampling.

## Texture combination actually recorded

Mode `00400024` (texture4,color1,alpha0,blend0,depth4,fog0):

- Stage0 COLOROP SELECTARG1(2), COLORARG1 TEXTURE(2); stage0 selects base texture rather than multiplying vertex diffuse into RGB.
- Stage1 COLOROP MODULATE(4) without capability; capability1 upgrades to MODULATE2X(5). Arguments are TEXTURE(2) and CURRENT(1).
- Explicit texture5 uses MODULATE2X irrespective of this capability byte in the executed mode setup.
- Stage0 alpha selects texture. For upgraded/source5 stage1 alpha selects CURRENT; source4 fallback disables stage1 alpha. In all four tested solid modes, ALPHABLENDENABLE27=0 and ALPHATESTENABLE15=0. Thus neither lightmap's alpha nor texture alpha supplies an extra darkening factor in these opaque modes.
- Both textures use linear min/mag filtering. Base UV wraps, lightmap UV clamps; stage coordinate indices0/1 respectively.

Names/values are the same D3D8 definitions already established in `docs/RENDER_STATE.md`; this task adds full-function executed records for the actual opaque solid fields rather than a new API assumption.

## Backend and room-route boundaries

Static disassembly `50c55d..50c57a` routes selected102 into `545960` renderer initialization. `50e2e0` bitmap lock dispatch likewise only accepts102 and calls `55ce00`; unlock `50e310` calls `55cf60` only for102. These are inspected instruction paths, not full initialization/lock executions in this task.

The earlier `558960` route checks dirty mappings and invokes wrapper `4f9d30`; it is now tied to selected102, but this does not mean all room geometry takes that function. Cached room preparation `4f0c00` itself checks selected102 and capability `1cfcc1c` to choose one two-texture batch versus two passes, and does not call `517180` directly. Its base/lightmap selection was executed separately in the retained16-case draw-state report. The current task proves renderer selector behavior, not a full live room-to-device call chain.

Existing packing/upload, brightening-query, image-sampling and dynamic-rectangle oracles remain the stronger evidence for their individual operations; no extra RGB brightness conversion is inferred here. CPU channel*8 sampling must not replace normalized hardware5-bit sampling.

## Remaining uncertainty and next useful task

No real original-device texture allocation/upload or original-game frame was observed. A remaining bounded original-code task is tracing format5 from native image creation through D3D texture allocation/lock to establish the actual device format and any conversion, then connecting cached room-batch submission to `54f160`. Another worker is independently auditing live material/UV binding, which is separate from this backend result.
