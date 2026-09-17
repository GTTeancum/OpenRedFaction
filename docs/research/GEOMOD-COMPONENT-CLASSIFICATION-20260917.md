# Component orientation classification

Added rf_geomod_component_classify, reconstructing4e1180 through the existing
original-derived null-reference collision-room face query. It traverses faces in
order and filters their component labels (negative selector means all faces).
For each candidate it computes a sequential float-sum corner mean, offsets that
sample along the face normal by the sum of the component bounding-box extents,
and casts back along the negated normal with length extents_sum+1. An ambiguous
edge encounter retries from the next face sample. The first nonambiguous query
returns whether its selected surface is front-facing; exhausted candidates
return false. This is not a general watertightness or manifold validator.

`python tools/probe_geomod_component_classify.py` executes complete original
4e1180 and real centroid, vector, query setup, face intersection, edge ambiguity
and final classification helpers. Component bounds and linked-list iteration
are supplied; all geometric classification arithmetic executes original bytes.
Twenty-four box-shell cases vary center, unequal extents, inward/outward normals,
and selected/all/missing component. Outward shells return1, inward shells0,
missing labels0. Generated raw-word geometry fixtures exercise the actual C
implementation through geomod_notify_tests; all cases and malformed-bounds
output preservation pass.

RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Evidence: artifacts/geomod-postedit-re/component-classification.json and
component-build.log. This does not prove arbitrary concave shapes or all retry
paths. Broader non-box/retry fixtures remain useful before live extraction.

The classifier is not connected to terrain mutation yet. Remaining integration:
retain shared-vertex component identity, reconstruct piece extraction/recentering,
publish affected bounds atomically, then notify registered physics owners.
Do not remove every smaller component by face count alone: the original excludes
the largest and separately classifies other components before extraction.
No new live scene, emulator or visual acceptance is claimed here.

Stock-profile NXDK compilation/link also succeeds (component-xbox-build.log); this is build acceptance, not native execution of the new classifier.
