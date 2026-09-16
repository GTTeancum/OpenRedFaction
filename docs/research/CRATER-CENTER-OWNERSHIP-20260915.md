# Requested versus adjusted crater center ownership — 2026-09-15

`tools/verify_geomod_crater_center_ownership.py` executes original467275..467375 with four deliberately different requested/adjusted centers. It captures the437230 queue boundary and then separately executes complete436fc0, proving an exact100-byte descriptor copy. Position encoding4b5820, orientation encoding, direction packing and auxiliary vector copies execute original bytes. FPU precision is53-bit. Same original RF.exe SHA-256 as the prior-cut report. `crater-center-ownership.json` retains results.

**Two distinct centers are necessary.**

| Consumer/storage | Center used | Evidence |
|---|---|---|
| Queued descriptor+8 | Adjusted center produced by45cff0 |467275..46727a passes local descriptor; actual436fc0 copies it unchanged|
| Packed history648608 +32*i | Original requested center |4672aa pushes EBP (original param4) into real4b5820|
| Auxiliary history646a28 +36*i | Adjusted center |467315..46732e copies local descriptor center|
| Duplicate admission | Decoded old requested center versus new requested center |4671f7..46726a, prior actual-decoding oracle in crater_position.py|
| Shallow prior-cut alignment | Old auxiliary adjusted center versus new requested center |Complete45cff0 probe in shallow_prior_cuts.py|

The auxiliary record also copies the first and second signed limit vectors at467333..46736a. The32-byte record stores the adjusted descriptor's scale at4672ea/4672fb, but its center remains the requested position. Count increments only after these writes at46736f.

For requested(0,0,0), adjusted(0,1,0), bounds[-32768,32768], the real packed record stores(32768,32768,32768); queue and auxiliary history retain(0,1,0). The remaining fixtures use distinct values on all axes to avoid accidental aliasing.

Recommendation: preserve requested position for quantized admission/duplicate history and adjusted position for geometry queue and shallow alignment history. Never substitute the final cut center for both. Queue allocation, asynchronous CSG processing and save/replay reconstruction were not executed here. The supplied437230 boundary is explicit; exact descriptor copying is proved separately by its real436fc0 helper.

Primary review: retained and reran this executable probe successfully. The live port stores requested and adjusted centers separately and deliberately retains initial codec bounds; this evidence does not claim identical original mutable-bound behavior.
