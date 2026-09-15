# Original crater-center quantization (2026-09-15)

`artifacts/crater-shading-re/crater_position.py` executes unmodified4b5820/4b5900 and actual duplicate-gate decoding.28 encoding cases, five duplicate cases and two precision-control comparisons pass. JSON retains exact inputs/codes/decoded positions.

Encoding first tests each coordinate against inclusive solid bounds. If any coordinate is outside, all three uint16 codes become zero. Otherwise each code is the low16 bits of truncation of `(position-min)*(65536/(max-min))`, with the original x87 operation order. Exactly the upper bound therefore produces65536 and wraps to0. This is not clamp-to65535. Decoding is `min+(max-min)*code/65536`.

The initial28 cases and five gate cases explicitly use x87 control037f (64-bit significand). An independent rational arithmetic reference rounds the quotient/product to64 significant bits before truncation. Results are not precision-mode invariant: bounds[-1000,1000], input(-500,0,500) encodes(16384,32768,49152) at027f (53-bit), but(16383,32767,49151) at037f. Actual original startup/runtime FPU mode must determine which behavior is the appropriate target; this report does not claim037f matches every original live frame. The broader port commonly verifies027f behavior.

The complete encode/decode plus4671f7 duplicate branch uses original4b5900 without a world-position hook. In bounds[-1000,1000], old x=.015/new x=.205 is not rejected as duplicate, while old x=.03/new x=-.18 is rejected; raw-coordinate distance tests would give the opposite outcomes in these selected cases. Y/Z also undergo quantization. Thus a raw-float history is an approximation near the0.2 admission threshold.

Recommendation: retain bound-relative compressed history or a decoded equivalent computed with the explicitly chosen arithmetic environment; include upper-bound wrap, out-of-range zeroing and threshold cases. Do not silently replace this with round-to-nearest or65535 normalization while claiming exact stock behavior. Invalid/degenerate bounds were not tested and may remain rejected at the shared API boundary.

RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No shared source changes/builds/emulator runs.

## Shared53-bit codec

The shared rf_geomod_position_encode/decode use explicit double intermediate stores to retain53-bit arithmetic, supported by the separately executed CRT startup evidence. tools/verify_geomod_crater_position.py reruns the original codec in027f mode:28 original/shared encoding cases and decoded float comparisons pass. Five original decoding/duplicate-gate cases also execute, but live duplicate-history integration remains open. Upper-bound wrap, all-zero outside encoding, nonfinite/degenerate-bound rollback and the documented53-bit quarter-point case pass shared tests. Evidence uses crater-position-53bit.json; the earlier64-bit investigation remains a separate mode comparison. No visual change is claimed.
