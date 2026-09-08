# Model batch data

`rf_model_file_batch` exposes file-relative ranges without allocating the LOD
blob. The original `0x569920` loader reserves 56 bytes per batch, aligns to
16 bytes and assigns eight region pointers in sequence: positions, normals,
UVs, indices, optional triangle planes, extra data, optional bone links and
optional auxiliary data. Each region is followed by 16-byte alignment.
The seven 16-bit lengths/counts and 32-bit format descriptor live in an
18-byte table after the blob and an intervening 32-bit field.

The port retains batch-table offset/count, flags and auxiliary count in each
LOD directory entry. On-demand access walks preceding descriptors to recover
the requested ranges, validates them against the attachment boundary and
publishes output only on success. Empty regions use offset zero. This adds
2,048 bytes to the fixed maximum-size model directory, with no geometry heap
allocation. Repeated batch queries are quadratic in batch count; a future
resident geometry load should traverse the directory once.

Evidence is the original executable's Ghidra export for `0x569920` (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`)
and independent traversal of every installed V3C file. The verifier
`tools/verify_model_batches.py` checks 95 models, 170 LODs and 599 batches,
including all eight offsets/sizes, counts and format bits. Probe checks reject
out-of-range batch indices, truncated descriptor ranges and truncated payload
ranges without changing output. All 599 descriptors have value 5344321.

PC build, four CTest checks and NXDK build pass. The existing numeric 64 MiB
XEMU scene/animation regression also passes at
`artifacts/xemu/20260908-192720-951644/report.json`, without a framebuffer capture;
it does not exercise batch decoding or model drawing. This establishes region
boundaries, not decoded vertex/index semantics or rendered model fidelity.
Next recover the descriptor mapping in `0x569d20`, validate numeric data and
bone links, then connect geometry to owned materials and the renderer.
