# Staged component geometry extraction

The shared `rf_geomod_component_extract` adapter packs retained terrain and
one selected component into separate views backed by caller-owned arrays.
Each view uses local corner indices. World-space corner bytes, UVs, material
IDs, authored source-face IDs and generation are preserved. A parallel index
array maps every output face to its prior face index, allowing the caller to
transfer filters and lightmap ownership without confusing generated faces
whose source-face IDs are all UINT32_MAX.

The operation allocates nothing and does not mutate input terrain. It checks
all face ranges, finite positions/UVs and combined capacities before writing
any output. Missing labels, malformed data and capacity failures preserve the
staging arrays and descriptors. Excluded labels UINT32_MAX remain in terrain.
Callers must provide disjoint inputs and outputs and correctly sized labels.

`geomod_disconnected` now applies extraction to real zero/one/two-cut meshes
on all three principal axes, including their reload/continuation counterparts.
It verifies a closed opposite-edge pairing in both nonempty output meshes,
exact corner/material/source-ID preservation, stable input face ordering and
collision rebinding with identical planes and positions. Tests also cover
missing selection, insufficient face/corner buffers and a late nonfinite UV,
checking that every staging output remains unchanged on failure.

This is an adapter for the reconstructed face-corner representation, not a
claim that original4d0590's room ownership and lightmap transfer are reproduced.
It does not perform orientation acceptance, choose the retained component,
recenter geometry, transfer atlas ownership, publish bodies or alter history.
The live transaction still needs to integrate those operations and ensure
subsequent chronological rebuilds do not resurrect extracted pieces.

Validation: Release geomod_disconnected passes. Stock-profile NXDK compile
produces default.xbe and the diagnostic ISO. No native runtime/visual acceptance
is claimed for this change, since the scene does not call the adapter yet.
