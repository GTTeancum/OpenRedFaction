# Destruction capacity integration

The configured committed-cutter limit remains eight. This is distinct from the
original128 admission journal. No longer-session gameplay is claimed yet.

Added include/rf/geomod_limits.h as the shared cut-count and maximum RGCH size
definition. History maximum is28+1544*cut_limit; the1544 wire maximum is24
metadata bytes,60 vertices of20 bytes and20 faces of16 bytes. A compile-time
guard prevents expanding beyond the32-bit star mask representation.

Replaced independent eight-cut guards in retained-material digest, authored
owner extension, restored publication and journal import. Replaced12380-byte
limits/buffers in scene validation, edit transaction, core layout and affected
probe/test scratch. Scene transport maximum now aliases RF_CHECKPOINT_FILE_MAX.
The outer110524-byte cap intentionally remains independent: increasing cutter
capacity must still account for admission, chart, face and player payloads.

Seven rebuilt focused tests pass: geometry/interior, repeated cuts, history
validation, material digest, authored layout, owner extension and publication
candidate. Owner-extension boundary test now accepts the configured maximum
and rejects maximum+1 on both encode and decode, preserving outputs. NXDK
build passes; this mechanical refactor has no new native runtime acceptance.
Logs: artifacts/authored-post-live/capacity-{build,xbox-build}.log and
capacity-boundary-build.log.

Next: deliberately exercise16 cutters with retained old holes and save/reload,
measure chronological replay cost and transactional peaks, then decide the
geometry/tree/atlas budgets from those results. Existing eight-cut stress peaks
near1MiB, so a larger cut array cannot be assumed to fit the same core budget.
Do not evict earlier cuts or silently reset destruction to make room.
