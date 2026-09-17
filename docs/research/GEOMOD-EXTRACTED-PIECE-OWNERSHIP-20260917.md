# Bounded extracted geometry ownership

`rf_geomod_piece_bank` provides a fixed-allocation staging owner for extracted
meshes. Each entry contains recentered mesh corners, faces, placement, copied
eligibility filters, the previous face-index mapping and a caller-provided
unique identity. Entries borrow no replay geometry after append. Capacity is
explicit in vertices, faces and pieces, with a checked byte budget including
the owner/descriptors. Append allocates nothing. Existing entries and counts
survive failure; spare staging bytes may change.

The private extraction helper can now emit a candidate piece before compacting
retained terrain. The callback copies it into this bank. Callback rejection
returns to the private transaction without deleting that component. A later
failure after earlier accepted components still requires discarding the whole
private terrain and staged bank; this is not a complete live rollback contract.

The extracted-replay test owns both pieces from the four-cut sequence, then
compares them after saved-history continuation. Local geometry, UVs, placement,
face mapping and identities match after replay storage is reused. Their signed
volumes are3600 and1200. An undersized bank rejects each actual extraction while
the active candidate mesh bytes/counts remain unchanged. A sufficient bank then
accepts the retry. Exact-byte-budget admission and duplicate-ID rejection are
also checked. The test bank (128 corners,32 faces,4 entries) reserves4304 bytes
on the PC build; this is not an Xbox runtime memory measurement.

The bank is shared code and compiles with NXDK. Its entries are geometry owners,
not active physics objects: collision-tree ownership, atlas resources, dynamic
state, rendering, notifications, checkpoint state and scene commit remain open.
Production replay is still not switched to automatic extraction. Relevant
chronological/replay tests pass and the stock Xbox diagnostic build succeeds;
there is no new native runtime or visual acceptance.
