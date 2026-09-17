# Precommit resource check for template cuts

`rf_geomod_terrain_cut_template_checked` now exposes the already prepared
mesh/collision candidate before core publication. The caller can stage lighting
and draw resources and reject the cut before any live mesh, collision tree,
generation or successful-cutter history changes. Callback errors propagate
unchanged. Ordinary template/star APIs retain their previous behavior through
the same implementation with a null callback.

This introduces no terrain clone or extra core allocation. Candidate resident
accounting includes the existing and pending collision trees; external staging
must still be counted by the scene against its destruction budget. Borrowed
candidate pointers are callback-only, immutable and non-reentrant. The callback
must not publish external resources. After successful return, the caller must
commit its staged resources without further fallible operations.

New `geomod_checked_cut` test compares the candidate against an independently
committed control with mixed box/template history. Injected RANGE, IO and FORMAT
rejections preserve live pointers, vertex/face bytes, generation, history bytes
and collision-query results. Invalid scale skips the callback. Accepted retry
and a subsequent box cut match uninterrupted geometry, collision and encoded
history. All112 default CTests pass after the full PC build.
The expanded NXDK build also succeeds with the existing .edata merge warning;
this turn did not launch XEMU or validate the new callback on native hardware.

**The legacy DEV lighting exhaustion bug is not yet fixed end-to-end.** The
scene still calls the ordinary template API. Next integration must use its
existing private lighting/draw stage in the callback, validate collision-overlay
binding before publication, charge concurrent staging memory, and commit the
overlay/stage only after core success. Preserve admission/RNG semantics for a
rejected blast. Prove this with a deliberately exhausted map budget and a later
successful edit, then run stock64MiB Xbox comparisons. No claim of runtime
rollback or new visual behavior follows from the core primitive alone.
