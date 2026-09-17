# Component grouping on reconstructed terrain

Added rf_geomod_mesh_components with explicit caller-owned scratch sized by
rf_geomod_component_work_size. The port stores separate face corners; its
existing seed adjacency already defines shared geometry by exact numeric
position equality. This adapter uses that same rule, including equal signed
zeros, with a hash table and disjoint-set grouping. It neither epsilon-welds
nearby points nor merges per-face UV. Original pointer identity is not recovered
from positions; a caller with distinct coincident original vertices must retain
that topology separately instead of treating this adapter as proof of equivalence.

Optional face filters apply the original low0x0c flag and positive signed-short
eligibility exclusions. Raw groups use first-face encounter order. Largest group
uses eligible face count, with first group winning ties. There is no removal or
orientation classification in this routine. Region eligibility is the caller's
responsibility. Inputs and public outputs stay unchanged on validation/capacity
failure; scratch may change. No allocation is performed inside the helper.

Six supplied-graph configurations previously verified against complete original
4d0990 now pass the C adapter: single, vertex-only contact, disconnected, tie,
largest-last, and excluded bridge. Tests assign distinct corner UV, alternate
signed-zero coordinates, cover all-filtered output, and check insufficient
scratch/nonfinite input preservation. geomod_notify CTest passes; stock-profile
NXDK build succeeds. Native execution of this adapter is not yet claimed.

The new read-only rf_geomod_components_probe reads RGM1 snapshots. Three actual
captured port meshes all have one component:

| Snapshot | Faces | Corners | Scratch bytes |
|---|---:|---:|---:|
| crater-effect-decay/900/physical.mesh |155|798|18244|
| authored-post-live/prefix-02.mesh |157|800|18268|
| authored-post-live/prefix-15.mesh |1563|7929|149828|

These snapshots lack eligibility flags, so the tool deliberately treats all faces
as eligible and does not claim original component parity. Artifact paths are
under artifacts/; exact file hashes and outputs are retained in
artifacts/geomod-postedit-re/captured-components.json. This is geometry analysis,
not a new rendered or emulated run.

Next integration needs an actual disconnected cut fixture, face/lightmap relocation,
component ownership across chronological rebuild/save, and peak-budget reservation
for scratch. Do not allocate this scratch unaccounted alongside transaction clones.
The single-component captures supply no justification for deleting current faces.
