# Original component selection before changed-box publication

`python tools/probe_geomod_components.py` executes complete4d0990 with real
array accessors40a480/40a490, real eligibility4ce480, graph traversal, component
labeling, largest-component choice and final label compaction. Supplied services
are linked iterators, face count, allocation/free, region exclusion (false), and
4e1180's downstream component-classification result. These supplied boundaries
must not be mistaken for execution of full CSG, extraction or classification.

Twelve scenarios pass: single face, vertex-only connection, unequal disconnected
components, equal-size tie, largest component encountered last, and a flagged
bridge face; each with downstream classification accepted or rejected.
Independent Python graph traversal checks partition/retained choice/count.
Output: artifacts/geomod-postedit-re/components.json. RF.exe SHA-256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

Observed contracts:

- Eligibility4ce480 rejects face property low bits0x0c or positive signed
  short at face+34. A filtered face cannot bridge eligible faces.
- Connectivity walks each face's corner vertices, then each vertex's incident
  face array. Sharing one actual vertex identity connects eligible faces; a
  shared full edge is not required. Coincident positions with distinct vertex
  identities were not tested and must not automatically be welded by epsilon.
- Components are measured by eligible face count. First encountered wins ties.
- With multiple components, the largest is relabeled to the final index and
  excluded from extraction; eligible remaining components pass4e1180. Rejected
  components receive label-1; later labels are compacted. If there is only one
  component, the function returns0 before that relabel/filter pass, so its
  temporary label can remain0 despite there being nothing to extract.
- The worker then calls4d0590(world,0) repeatedly for the returned count and
  obtains placement from4d1330 before publishing padded bounds. Raw4d0590
  decompilation shows face cloning/removal and mapping transfer, but that owner
  mutation has not been executed by this probe.

Next: establish4e1180's geometric acceptance and4d1330's recentering, then map
original shared-vertex ownership to the reconstructed terrain's face/corner
representation. Do not use triangle count, volume, a whole-room box or broad
coordinate welding as a substitute for the verified selection. No gameplay,
build, emulator or visual acceptance is claimed by this read-only execution.
