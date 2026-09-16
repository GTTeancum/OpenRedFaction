# Authored source identity helper — 2026-09-16

The helper is registered in PC and NXDK builds. The primary task ran `geomod_authored_identity` successfully, including the independent canonical digest and malformed-input cases, and built the Xbox XBE. Installed-asset capture and live save integration remain unverified; no emulator run was performed for this helper.

## API and exact scope

`rf_geomod_authored_identity(input, digest32)` computes the **source** domain for the proposed authored checkpoint profile. It uses actual `rf_geomod_authored_post_view` source/window/neighbor data, ordered neighbor solids, original filters, source planes and replacement ownership. It is not the three eventual publication/composed-collision/material-tail digests and does not replace template, generated-substrate or admission-region domains. It does not implement RFDSv2.

No reusable core SHA helper existed. The private SHA256 implementation already in `scene.c` was copied into this isolated source with private names; existing code remains unchanged. The canonical stream begins bytes `RFAS`, LE u32 version1. Integers and finite binary32 words are explicitly little-endian; strings are length-prefixed. Padding, pointers, buffer capacities, resident/peak counters, mesh generations and renderer slots are not hashed. Signed zero retains its actual binary32 identity. Invalid input leaves the 32-byte output unchanged; there are no allocations or mutation callbacks.

Caller-supplied immutable spans are the original compiled geometry section and original editor section0x2000000. These bind original transform/operation/ownership bytes without a second editor parser. Selected source UID/room, brush/source counts, settings texture/hardness and all four loader/publication/collision/material policy revisions are also hashed. Operation must be2 (solid; original44d870/44d830 flags contract), source mode0 (outward). There is no speculative air/general CSG profile.

## Capture contract before source resources close

1. Use `asset_view` from the loader, **before renderer remapping**. `scene_terrain_authored_assets` keeps loader view separately from its remapped face storage; pass the original view. Set material_domain=`RF_GEOMOD_IDENTITY_COMPILED_MATERIALS`. The API cannot infer the history of an integer: falsely declaring renderer slots to be compiled IDs is a caller bug, not detectable magic.
2. Build material rows for every distinct compiled material in all three meshes, including hidden source/neighbor faces. Each row maps its compiled key to canonical lowercase member name (forward slash), logical dimensions, stable format, bytes-per-pixel2/4 and tightly packed row-major decoded pixels. Never pass GPU tiling/pitch/padding or allocator bytes. Capture from immutable source images, not a dynamically relit atlas.
3. Build unique reference rows for each non-sentinel source/window/neighbor origin. Each row carries that exact owner/authored token, compiled material, `rf_geometry_initial_collision_filter` result, and immutable source projection/chart content. Binding helper rows are **not directly cast-compatible**: their material field is already a renderer slot and they lack content/filter identity. Unlit sources explicitly set unlit1 with zero chart/projection fields. Lit rows use stable source chart names and logical original chart pixels; never use current atlas/image handles in a name.
4. Duplicate numeric lookup keys reject. A real reference absent from the table rejects. Owner/token/material mismatch rejects. Hidden source and neighbor faces whose loader origin explicitly has `UINT32_MAX` remain valid and hash absence; a visible window may never lack a reference. Replaced IDs must be unique, belong to selected owner, cover every visible window, and resolve to actual source tokens/materials. Source authored tokens and neighbor owner IDs must be unique in their respective ownership lists.
5. Numeric material/reference keys resolve rows but are excluded from the canonical stream. Replacement order is hashed through stable owner/token/chart values and window multiplicity. Caller table order does not matter. Returned digests remain equal when pointers, numeric lookup keys and mesh generations relocate consistently. Raw compiled/editor bytes deliberately remain strict: a changed compiler window split is not a migration path.

The function borrows all resources only for its call. Its bounded local SHA state/work is a few hundred bytes; it does not add image copies itself. Caller image repacking/section loading must be separately charged to the stock64MiB scene budget. Current source rehashes image content per referenced face rather than retaining a larger digest cache; correctness is prioritized for this one-time identity path. Limits are768 faces/4096 corners per mesh,32 neighbor solids,128 material rows,768 reference rows and64MiB per content span, **not a claim that simultaneous maximum inputs fit Xbox**.

## Prepared validation

The independent Python hashlib serialization of the minimal706-byte RFAS v1 fixture yields:

`c709500ea908c491b7df1e6ee27681aa70b96418865fab5e02824d8d66d8474f`

The C test has this exact fixed expected vector, plus pointer/material/reference relocation, table permutation, pixel/editor/policy/chart changes, duplicate/missing keys, wrong owner, renderer-domain rejection, invalid image size, NaN geometry, wrong suppression ID, missing visible window and explicit hidden-source absence. The minimal triangle fixture exercises canonicalization, **not closed-solid topology**; production callers still use the validated authored loader. No installed-content digest has been measured yet.

Parent-owned isolated compile/run, from repository root in an x86 MSVC cmd environment:

```bat
cl /nologo /std:c11 /W4 /MD /D_CRT_SECURE_NO_WARNINGS /O2 /Iinclude tests\geomod_authored_identity_tests.c src\core\geomod_authored_identity.c /Feartifacts\authored-post-live\authored-identity-tests.exe
artifacts\authored-post-live\authored-identity-tests.exe
```

Next production integration requires capturing the real material/chart rows and raw section spans while the level owner is alive, comparing PC/Xbox source digests under changed renderer allocation order, then calling the existing remaining identity domains. This helper should be registered only after its focused test and installed-content adapter are reviewed. A hash match is a compatibility/corruption check, not authentication or a substitute for candidate history/placement validation.
