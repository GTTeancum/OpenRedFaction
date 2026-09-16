# RFDS2 authored owner extension codec — 2026-09-16

Primary validation: authored_owner_extension CTest passes; NXDK builds the registered helper. Scene restore integration remains open. Files:

- `include/rf/authored_owner_extension.h`
- `src/core/authored_owner_extension.c`
- `tests/authored_owner_extension_tests.c`

The helper is registered in the PC and Xbox builds; existing scene save behavior is unchanged.

The pure encode/decode helper covers exactly128 bytes (the proposed RFDS2 owner extension at288..415). Words0..7 are LE UID, outward mode0, authored source count, neighbor count, publication revision1, collision revision1, material revision1 and publication serial. Bytes32..63/64..95/96..127 are publication/composed-collision/retained-material SHA256 digests. These are neither the source-domain digest nor arbitrary renderer buffers.

Both APIs require a caller-supplied `rf_authored_owner_expected`: UID/counts from freshly validated immutable loader ownership and three independently rebuilt candidate digests. Never construct expectations by copying the payload being validated. Decoder equality is an integrity/compatibility gate, not authentication.

Accepted source counts are4..32 and neighbor counts1..32, matching the bounded closed-solid authored profile; UID UINT32_MAX rejects. Only policy revision1 is accepted. Cuts are0..8, cannot exceed the serial, and serial UINT32_MAX rejects; no wrapping. Zero-cut serial3 is valid after reset, and a fresh empty owner has serial0. All errors preserve output; local128-byte staging makes encode atomic and local decoded value makes decode atomic. Inputs/output are documented disjoint. Exactly128 bytes is required even if an encoder's allocation has larger capacity: caller passes the selected extension span, not the remaining whole save buffer.

Prepared tests include a complete fixed128-byte LE vector, round-trip, every truncated length0..127 plus129-byte rejection, mutation of each96 digest bytes, all seven policy/identity header words, wrong caller identity, NULL pointers, serial overflow, cuts beyond serial/limit, reset serial and preserved sentinel output. All listed cases passed in the C test.

Parent-owned isolated build/run from repository root in an x86 MSVC command environment:

```bat
cl /nologo /std:c11 /W4 /MD /D_CRT_SECURE_NO_WARNINGS /O2 /Iinclude tests\authored_owner_extension_tests.c src\core\authored_owner_extension.c /Feartifacts\authored-post-live\authored-owner-extension-tests.exe
artifacts\authored-post-live\authored-owner-extension-tests.exe
```

Integration order remains caller-owned: validate outer RFCP/RFDS2/profile2 lengths and source identity; reconstruct RGCH/private publication/composed collision/lighting candidates; compute expected digests; decode extension against those values and cuts; validate saved body against the full pending room; publish only after every gate succeeds. Header parsing may need a separate bounded preliminary read of UID/count/serial to plan candidate storage, but must not call that preliminary read a successful semantic decode. This helper neither allocates candidate storage nor changes slot recovery/save publication behavior.
