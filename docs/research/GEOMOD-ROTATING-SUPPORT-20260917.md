# Rotating extracted support

The moving-support fixture now has mode3: lift a real saved fragment by0.25,
then seed angular momentum once at frame90 and let ordinary fragment physics
handle gravity, angular integration, world contact and settling. The stimulus
uses the body's existing world inverse tensor inverted to obtain momentum for
an initial world-Y angular velocity3. No geometry, mass or inertia is replaced.
This is a process-local diagnostic impulse, not a reconstructed weapon impulse.

`tools/check_rotating_rubble_support.py` compares this with the same lift/drop
without the impulse. The actual radius0.530725 post fragment rotates visibly in
its recorded basis: basis component0 changes from-0.9977722 to-0.7894081 by
frame102 and-0.7578674 at rest. The test checks finite orthonormal bases, live
support throughout this particular trajectory, unchanged playerX/Z, and zero
angular velocity after settling. PlayerY ends at0.4447868 versus0.5121653 in the
no-spin control; contact therefore follows the changed sphere arrangement instead
of retaining the old support height. The body itself ends atY-0.7448609 in both.

Mode3 records120 additional words (basis plus angular velocity at ten samples),
alongside the160 body/player words. The native harness reads them directly from
guest memory, retaining them before later checkpoint assertions. The opt-in
disc flag is restored with the rest of the staged disc. Normal sessions have
the fixture disabled; this adds480 bytes of diagnostic static storage, no heap.

The `--large` control uses the real radius1.213922 beam. Both its saved inverse
tensors are all zero, as permitted by the recovered empty/singular mass path.
It remains unrotated, and player/body endpoint matches its no-spin control. The
test explicitly checks those tensors rather than weakening the spin assertion
for any body that fails to rotate. Do not substitute invented inertia to force
this geometry to spin. General large-fragment angular coverage remains open.

PC reports: `artifacts/rotating-rubble-support/report.json` and
`artifacts/rotating-large-rubble-support/report.json`. The PC final frame was
inspected: room, post, weapon and HUD remain rendered at the supported height.
The fragment is underfoot; sampled orientation/contact state establishes the
motion, not that endpoint image. No retail screenshots, host input or GitHub
image upload. Angular attachment/carry parity is not claimed: recovered41e370
copies linear support velocity; this verifies contact with changing geometry.

Full PC build and all123 CTests pass. Stock64MiB native run
`artifacts/xemu/render-20260917-215828` passes77 checks: all280 sampled words
match PC exactly, as does the2654-byte checkpoint against both the harness PC
reference and independent spin fixture.3974 free pages is15.523MiB endpoint
headroom, not a peak guarantee. Native frame inspected: room, post, launcher and
HUD remain intact at the settled support height; no intermediate native rotation
frames were captured. Native zero-inertia control and support-loss trajectories
remain separate work. Disc restored and the owned emulator exited; audio disabled.
