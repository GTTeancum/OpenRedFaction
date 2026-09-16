# Generated crater mapping content lifecycle

## Result

No ordinary crater creation/draw/light-change route that replaces generated base noise with static illumination was established. A newly identified runtime updater does exist, but it consumes existing dirty flags rather than creating static invalidation. The original static-dirty marker is real and executable; its true/static argument is not used by the direct gameplay callers found in this bounded audit. This is evidence against adding an unconditional static bake after crater creation, not proof that every indirect/editor route is unreachable.

Original RF.exe SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Executed identity/content sequence

`tools/verify_geomod_mapping_lifecycle.py` writes `crater-mapping-lifecycle.json`. Five stages execute original instructions against one mapping, image, persistent RGB buffer and GPU buffer:

1. Whole `4f26a0`: dirty8 uploads persistent RGB, clears dirty.
2. Whole `4f9d30`: enabled clean mapping does no query or upload.
3. Whole `4f1f30`: intersecting supplied face marks that same mapping dirty1.
4. Whole `4f9d30`: global `5a3ed4=0` preserves pending dirty1.
5. Whole `4f9d30`: enabled, supplied dynamic query has zero sources; original `4f26a0` restores packed base RGB and clears dirty. GPU buffer had deliberately been overwritten with `ffff` before this stage.

Every stage preserves persistent RGB bytes `[32,64,95,80,40,56]`, image pointer and mapping identity. Only timer, bitmap lock/unlock, dynamic query returning zero and mapping-array lookup are supplied boundaries. This sequence does not execute positive dynamic lighting, whose addition/restoration behavior is covered by existing retained lightmap tools. Run: `python tools/verify_geomod_mapping_lifecycle.py`.

## New runtime draw caller and scope

Full executable-section direct-call scan finds `4f26a0` called at `4e50ac`, `4e529b`, `4e5c2e` and **`4f9d5d`**. The last is wrapper `4f9d30`, gated by byte `173c35c` and dword `5a3ed4`. It calls updater with final argument1 and accumulates timing; it does not assign static dirty bits. `4f9d00` enables the first gate and resets timing/counters.

`558ab6` and `558b9c` call the wrapper when mapping+8 is nonzero. They save mapping bounds+34/+40, transform them through `539a40`, update, restore bounds, then submit faces through `559870`. This is a backend-specific solid drawing route. Its presence does **not** establish that every ordinary room batch in `4f0c00` executes it, nor a requirement to transform static world crater coordinates. Generic dispatch `4f9d90` reaches `517180`; full backend-selection proof remains open. No implementation recommendation depends on treating that route as universal.

## Static invalidation: real, but explicitly requested

New executable probe `tools/verify_geomod_static_dirty.py` writes `crater-static-dirty.json`:

- Forty actual instruction intervals cover both face-list branches `4d8b8b..4d8bd2` and `4d8c62..4d8ca9`, dirty bytes0/1/2/3/8, intersection true/false, and static argument0/1. The intersection callback is supplied; the dirty tests/writes are original instructions.
- Dynamic argument0 skips an already bit1 dirty mapping; otherwise an intersecting face receives dirty1.
- Static argument1 skips an already bit2 dirty mapping; otherwise an intersecting face receives dirty3 at `4d8bc6` / `4d8c9d`.
- These are genuine mapping writes: enclosing `4d86d0` reads face signed mapping index+36 and retrieves mapping from solid array+c0. They are not light-object flags.
- Eight whole `4d9130` light-removal calls execute actual light-index resolution, reference decrement, list unlink, type clearing and invalidation dispatch. Invalidation helper `4d8660` and global refresh `4d9fc0` are supplied boundaries. If references remain positive, no invalidation happens. Otherwise dynamic byte+4d or explicit removal argument1 permits dispatch, forwarding the explicit argument.

Run: `python tools/verify_geomod_static_dirty.py`.

`4d8660` forwards its argument to `4d86d0` for the registered world solid and transformed additional solids. It requires lighting gate `879af8` and world pointer `c96880`.

A linear direct-call scan found18 calls to `4d8660` in `4d8dd3..4d967d`. Creation paths at `4d8dd3`, `4d8f6f`, `4d911c` pass their explicitly zeroed EAX; movement/radius/color/change paths push literal0. The exception is removal `4d916b`, which forwards its second argument. All eight direct callers of removal found in the executable pass0: `41e97d`, `41ebe8`, `425147`, `45f755`, `45fa13`, `4a6905`, `4bbe1b`, `4c8080`. At `425147` the zero is EBP, initialized by `424f5f xor ebp,ebp` and not reassigned before the call. This call-site audit is disassembly evidence, distinct from executed marker/removal fixtures. Indirect callers remain a limitation.

## Relation to static updater

Existing `4f26a0` gates permit static recalculation when dirty bits2/4 and inhibit byte+10 allow it. Existing bulk routine `4e5040` assigns dirty7 at `4e50a8` and later dirty8 at `4e5297`; no direct caller was found in this executable-section scan. Thus static generation capability alone is not evidence it normally runs after a generated crater's initial dirty8 upload.

## Recommendation and limits

Keep persistent generated base RGB and eligible dynamic addition/restoration. Do not add static replacement, exposure gain or blanket relight based on these findings. If scripted/static-light removal is later implemented, preserve the distinction between dynamic invalidation1 and explicitly requested static invalidation3. Parent independently verified a live121-map/7407-texel endpoint matching retained noise; that live result is parent evidence, not a stock-game capture executed here.

Next useful bounded task: prove runtime backend dispatch selection for `517180` and compare the complete base/lightmap combination in the selected original backend with the port, including texture upload channel/alpha interpretation. No stock visual parity or permanent-noise claim across all editor/save/indirect paths is made.
