# Translating mover contacts for active fragments

The scene now routes active fragment queries through the retained mover interval when an enabled mover translates without changing orientation. The timed fragment callback supplies the real remaining substep duration. Other frames retain the old query path. A subsequent bounded support-loss policy wakes settled fragments when translating or rotating support leaves and no current support remains (see GEOMOD-FRAGMENT-MOVER-SUPPORT-LOSS-20260918.md). Rotating movers continue using their prior committed-pose collision path.

## Geometry and ownership

The relative query aligns the mover origin using the reconstructed remaining-time calculation. A temporary fragment trajectory is expressed against the mover's final committed geometry by shifting only its starting position by the outstanding mover translation. Existing four-interval corner, reciprocal surface-vertex and edge queries then find the earliest contact. A mesh-derived swept bound rejects distant candidates. Contact points are shifted back to the actual contact-time world position; this mapping and the four-interval polyhedral composition are explicit port policies, not a claim of complete49bb70 reconstruction. The downstream original-derived nonrigid response remains unchanged.

A separate array of borrowed-pose copies marks translating movers disabled for the old static candidate passes, preventing a final-position hit from competing with the correct relative sweep. Indices, handles, metadata ownership and ordering remain the same; no global live mover pose is mutated. Corner/body wrapper variants accept an explicit mover collection, while all other clients retain the default collection. The extra pose array is allocated once per level and released on teardown, counted with registration storage. Together with the retained interval it costs340 bytes per mover on the current ABI; no per-query heap allocation is added.

The moving query resolves metadata only for its winning candidate and preserves outputs on miss/error. Invalid retained handle or committed-pose identity is rejected. Disabled movers remain excluded. A moving hit competes with the already selected world/static hit using a strict earlier-fraction rule.

## Checks

PC tests exercise a broad rising polygon against fragment corners, a narrow polygon contacting the fragment face interior, crossed rectangles requiring the edge route, partial remaining time after a crossing, a retreating surface, an earlier retained contact, disabled-mover rejection and atomic metadata/identity errors. All126 PC tests pass (artifacts/fragment-moving-ctest-final.log). The full source108 connected first/second destruction and saved/uninterrupted history passes (artifacts/fragment-moving-group108.log).

The optional native fragment-contact audit now includes16 additional words, headed [1,3,0,1]. Each of the three moving fixtures reports found1, fraction0.375, contact height0.75 and upward normal1 through the production moving-query function. The harness compares every word to PC and to those expected values. These are numerical moving-surface fixtures; the ordinary600-frame room replay remains a regression, not a visible moving-door/rubble demonstration.

## Still open

Broader settled-fragment support-loss coverage, moving-surface carry/crush response, rotating-surface swept geometry, coplanar cases and an authored visible mover/rubble scenario remain incomplete. Detecting an approaching surface does not by itself give a stationary fragment the mover's velocity; original49d330 ignores that recorded counterpart velocity. Do not mark moving-platform gameplay complete from these contact tests.

The optional inner-work profiling patch was rebased for the changed query routing and moving audit, with moving-query work attributed to its mover group; git apply --check passes. It remains disabled in the normal build.

## Native acceptance

artifacts/xemu/render-20260918-042756 passes81 checks over600 frames on stock64MiB, with3313 free pages (12.94MiB) and no plugged memory. All16 moving-audit words match PC and explicit expectations, alongside the prior96 contact/edge words. The5300-byte checkpoint remains byte exact to PC and the prior accepted state (SHAd27cfa99dae3f1595e695e85f32b777a8f8b856017c85b944cab34c241601aba). The framebuffer is byte-identical to the inspected render-20260918-020011 image; no duplicate screenshot is posted. The disc was restored and owned emulator closed. Mean active debris time was37.51ms in this regression, which is not a moving-surface performance claim or demonstrated optimization.

The initial042657 attempt stopped at compilation because NXDK treats ambiguous else indentation as an error. Explicit braces fixed that diagnostic; no emulator launched in that failed attempt. The successful run uses the rebuilt final source.

Wake-boundary update: original mover activation does not traverse the terrain-fragment list, and the inactive scheduler gate is verified. See GEOMOD-MOVER-WAKE-BOUNDARY-20260918.md before implementing support loss; do not substitute the projectile-list wake policy for fragments.
