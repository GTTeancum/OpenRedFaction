# Restore lighting preparation

The private scene lighting transaction now separates clone from bake. A restore
caller can populate validated retained noise maps, atlas content and bindings
before any map allocation or random generation. Ordinary edits use the existing
prepare wrapper, which performs both steps in order.

An unbaked stage cannot draw or commit. A baked stage cannot bake twice.
Failures affect only disposable staging memory; live buffers and telemetry
remain untouched. This does not validate serialized maps: the future restore
decoder must enforce all seed-chain, map geometry, atlas packing and provenance
rules before passing populated state to this layer.

The installed-post unit test passes clone/discard isolation, rejection of
premature publication, existing late-failure rollback, and split preparation.
It also copies already validated retained state into a fresh private stage,
bakes, and compares the entire noise owner, atlas pixels and bindings exactly.
This is an in-memory staging test, not a saved-file round trip.

The ordinary840-frame PC reset/recut recording still completes and the combined
authored-control verifier passes. NXDK builds the modified implementation.
No new Xbox runtime result or authored live-save claim is made by these checks.
