# Call-local motion track reuse

Source-written post16 candidate against frozen a653795. The frozen input is
untouched. No helper build, test, emulator, archive compression or large-file
hashing was performed. Actual benefit has not been measured.

## Redundant work removed

The preceding sampler change decoded a bone track once per sample instead of
repeating the metadata read for each key. Skeletal model evaluation still
looked up the same track twice: first to obtain its blend-weight envelope, then
again in rf_motion_file_sample for each positive-weight contribution.

model_sample_playback now retains those already decoded descriptors on its
stack until the current bone's weights and samples are finished. The new
rf_motion_file_sample_track entry point samples the same immutable file using
that descriptor, without another metadata read. The original indexed sampler
continues to perform its own normal track lookup. Both paths share the exact
same key search, decoding, interpolation, weight sampling and output commit.

## Ownership, bounds and compatibility

- Descriptors belong to this one bone evaluation and never escape it.
- The caller must use the successful lookup's descriptor with that exact file;
  its header and immutable payload must remain unchanged through sampling.
- The descriptor's spans/counts and each actual key read remain bounded.
- No resources are retained, no heap allocation is added and no eviction or
  model/level lifecycle changes are needed.
- At most 16 descriptors are present. A static assertion caps this call-local
  array at 640 bytes. It does not scale with actor count or retained clips.
- Weight/envelope checks, blending order, animation cadence, pose publication,
  collision, controller events and per-actor playback remain unchanged.

This patch does not cache bone order or introduce a cross-frame motion cache.
It is pending the parent's chosen validation window after the current 16:00
run. Existing pose-sharing/model/motion sampler probes and same-scene timing
are the relevant bounded checks; no result is claimed yet.
