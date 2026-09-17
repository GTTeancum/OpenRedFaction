# Restore support from settled detached geometry

Authored checkpoint staging now combines the static-world ground probe with a
read-only query of the privately restored piece registry. The closest actual
polygon contact wins; static geometry wins exact ties. A nearer unstable piece
rejects instead of falling through to a farther floor. A walkable normal still
requires Y>=0.5, and the independent full-body/world and piece-clearance gates
remain in place. No player relocation, support attachment or dynamic carry is
introduced.

The optional checkpoint support provider uses the same prepared lowest-sphere
probe as static standing validation and transforms world-space queries through
the saved chunk poses. Only inactive bodies with exactly zero linear and angular
velocity qualify as stable save support. This is an explicit bounded save policy,
not a claim of recovered original moving-platform persistence. Retired pieces
are already excluded by the shared registry sweep. Null/empty registries preserve
static-only behavior. Query errors leave output untouched and checkpoint failure
discards the private candidate before live publication.

Tests cover static ties, nearer unstable support, nonwalkable normals, malformed
provider outputs and provider failure. Real extracted-piece provider tests cover
active/inactive bodies, residual linear/angular velocity and empty registries.
The installed ctf06 scene checkpoint test positions an actual extracted post
piece in open room space and saves its body. It uses a small synthetic sphere
placed from the actual upward polygon centroid and normal. Restore preparation
accepts stable support, rejects the identical active body, and rejects surface
penetration. Every rejected candidate leaves the existing scene/serial/pending
ownership intact. This exercises real extraction, serialization, private body
restoration and scene staging, but is not an ordinary full-player jump/stand
replay or a native runtime acceptance test.

Remaining acceptance: ordinary player movement onto a chunk, saving there,
reloading and continuing movement on PC and stock64MiB XEMU. Moving/rotating
supports remain deliberately excluded until attachment state is implemented.

Validation: all121 CTest cases pass and NXDK produces the stock-profile XBE/ISO.
No new XEMU runtime or visual acceptance is claimed for this support change.
