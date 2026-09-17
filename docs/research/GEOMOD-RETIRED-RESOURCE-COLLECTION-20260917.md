# Retired rubble resource collection

`rf_geomod_piece_registry_collect_retired` releases collision-sphere allocations
for retired chunks and shared geometry once every chunk in a batch is retired.
It allocates nothing and rejects calls during an edit transaction. Scene collection
runs between uses of borrowed piece views at the start of detached-body ticking.
Healthy pieces are not expired or collected.

Batch/ordinal/piece slots, body state, life state and immutable birth health remain
in the small owner allocation. RFPB encoding is unchanged, including retired
records. Decoding a matching retired snapshot still works after collection.
Revival into a collected owner rejects before any state mutation; loading older
live saves uses the normal history-rebuilt owner path. Get returns RF_NOT_FOUND
for collected slots. Zero-sphere live bodies remain valid and are distinguished
from collected bodies through ownership state, not the sphere pointer alone.

Tests cover healthy retention, released-byte accounting, idempotent collection,
unchanged encoded snapshot bytes, collected-state decoding, atomic rejection of
legacy revival into a collected owner, and transaction exclusion. All121 tests
pass, including the real authored extraction/digest test with zero-sphere bodies.

`tools/check_detached_rocket.py` exercises actual rocket extraction and destruction.
`tools/check_retired_resources.py` compares healthy retention with retirement and
reload, then compares the latter against uninterrupted two-rocket continuation.
The measured registry residency is133780bytes healthy and1852bytes retired;
131928bytes released, with byte-identical final RFCP. Evidence lives in
`artifacts/geomod-postedit-re/retired-resources/` and `detached-rocket/`.

The stock-profile NXDK build passes. Native collection/continued destruction is
not yet validated. Shared geometry in partly live batches remains allocated;
per-piece geometry compaction and the historical sixteen-batch limit remain open.
This collection changes storage lifetime only, not damage, healthy expiry, physics
shape, or save identity semantics.
