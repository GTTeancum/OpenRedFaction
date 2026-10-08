# Single-contribution pose copies

Source-written 2026-10-08 for the parent 19:00 batch; no helper builds/tests.

rf_model_blend_pose's count==1 path does no actual blending. Previously it
copied the sole rotation and position to temporary arrays, called the
attachment transform into another temporary, then copied that matrix to the
output. The existing Xbox object emits three ordinary libc memcpy calls.

After the same pointer/count/weight/rotation/position guards, the single-pose
path now calls the same rf_model_attachment_transform directly with those
input arrays and the requested output. That helper stages all 12 output
components until validation succeeds, so overlap with input storage and
output-on-failure behavior remain safe. The count>2-only matrix[0] adjustment
never applied to this branch. Two-or-more-contribution blending is unchanged.

No arithmetic, finite guard, clip selection, timestep, actor count, generation,
cache key or persistent storage changes. The separate attachment-normalization
candidate is not required for this copy-only change. Parent target-code/parity
and neutral L1S1 FPS validation remain pending; no gain is quantified.
