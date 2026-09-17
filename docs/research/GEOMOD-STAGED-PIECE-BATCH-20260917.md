# Staged detached-piece geometry and bodies

The scene currently commits rocket terrain edits before spawning ordinary
fragments. Detached solids need geometry and physics bodies prepared before
terrain publication so allocation failure cannot leave removed terrain without
its required pieces. This change supplies that private owner; it does not yet
change scene publication.

`rf_geomod_piece_batch_open` stages subdivision using a local RNG, owns the
resulting geometry bank, allocates body records together, and prepares every
body's sphere storage. Only complete success publishes the owner and RNG.
Failure releases initialized bodies and geometry. Close is idempotent. Geometry
views and mutable body pointers remain valid until close.

The budget includes subdivision peak and subsequent simultaneous geometry,
owner/body records and sphere storage. Worker scratch is gone before bodies
are allocated, so those phases are not incorrectly added together. Allocator
metadata is excluded. No renderer or scene resources are included yet.

## Verification

Release rf_geomod_disconnected_tests passes. Three seeds produce:

| Seed | Pieces | Resident PC bytes | Peak PC bytes |
| --- | ---: | ---: | ---: |
| 0 | 11 | 140960 | 933044 |
| 12345 | 10 | 140372 | 933044 |
| 24690 | 11 | 143024 | 933044 |

Each body's state and sphere array matches separately prepared reference
bodies; geometry and RNG match direct subdivision. Closing twice is safe.
Peak-minus-one budget rejection preserves the output and RNG; this exercises
worker rejection, not an injected late sphere-allocation failure.

Stock-profile NXDK build passes, recorded in
artifacts/geomod-postedit-re/staged-piece-batch-xbox.log. Build success does not
establish native runtime or visual acceptance.

## Remaining integration

Stage these owners with terrain edits and transfer ownership only on successful
publication. Resolve source T-junctions, renderer material/lightmap lifetime,
body scheduling and response, rollback under late allocation failure, actor
notifications, save policy, and stock64MiB runtime verification. No new native
visuals were produced by this change.
