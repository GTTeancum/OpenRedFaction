# Automatic extraction during private replay

The isolated extracted-replay test now uses `extract_replay_components` rather
than a hard-coded label deletion. The helper groups eligible faces, retains the
largest group by face count, moves its label to the end for traversal ordering,
classifies the remaining groups with the original-derived orientation rule,
then extracts accepted components. Stable original-face indices remap edge
support, face-plane IDs, filters and remaining labels through compaction.

The helper requires a disposable private replay owner, explicit current filters
and caller-owned bounded scratch. It allocates nothing. Failure can leave the
private owner partly changed; its caller must discard the entire transaction.
Detached mesh/body publication is not implemented. This helper remains under
RF_GEOMOD_TEST_CURRENT_SOLID until ownership and rollback integration exist;
it is not enabled in the normal Xbox/PC build.

The test now performs four cuts. Expected retained volumes after automatic
extraction are3600,3600,3568,1568. The second cut lies inside the already-removed
region and leaves exactly6 faces/24 corners. The third crosses the former
boundary. The fourth splits the remaining solid again and retains the component
with more faces, now on the opposite side, checking stable support remapping.
Every result is closed and stays outside previously removed regions.

A second run opens a fresh terrain, decodes cutter history saved after cut2,
appends cuts3/4, and applies the same explicit extraction policy during private
replay. Retained face and corner bytes match the uninterrupted run at all four
prefixes. This proves reconstruction from cutter history plus the same supplied
policy for this fixture. It does not persist the policy in the checkpoint or
restore dynamic detached bodies; live save/load integration remains open.

Validation: four related CTests pass; after guarding the test-only unused helper,
stock NXDK compilation succeeds. No new Xbox runtime behavior is claimed.
