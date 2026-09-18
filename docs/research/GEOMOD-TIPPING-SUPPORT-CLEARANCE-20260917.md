# Tipping fragment overlap correction

The stronger rotation probe exposed a gameplay defect, not the anticipated
support-loss case. Lifting the actual radius0.530725 post fragment and releasing
it with an X-axis angular impulse12 leaves the player standing after settling,
but the old support publication embedded a player sphere by0.067091244 units.
At player(-4.65693378,0.273248255,2.63444066), the unchanged save validator rejected
the pose with `PLAYER_CHECKPOINT_REJECT rubble -3`, source sphere0/target sphere6.

The existing downward clamp sweeps the full player body from its previous pose.
That cannot recover an overlap already caused by the fragment's new orientation.
The port now computes an upward separating height from all current player and
selected-support spheres. The calculation applies only to an admitted
sphere-mode fragment (radius>.5 and<=1), only when an actual overlap exceeds
the existing placement tolerance, and only for actor spheres above the target
sphere. It preserves unoverlapped positions and does not lift an actor from
underneath a fragment. A normal world/mover body sweep must admit the upward
correction; a blocked correction is not forced through ceilings.

This is explicitly a port overlap-recovery adapter, not a claimed original
decompilation. Original support math, shape admission, fragment mass/inertia,
damage and save-fit tolerances are unchanged. Polygon-mode large pieces keep
their existing path. Other-fragment obstruction and confined crushing behavior
remain broader coverage; this change does not claim to solve all depenetration.

The reproduced PC case now settles at playerY0.355281323 and exports a valid
checkpoint. FragmentY remains-0.815638602. It remains supported throughout this
trajectory, so this is not counted as support-loss acceptance. The fixture is
mode4/`--tip` in `check_rotating_rubble_support.py`, with one artificial impulse
and ordinary physics after release. Source tests independently exercise rotated
sphere offsets, analytical separating heights, nonoverlap, underside and both
admission boundaries. All123 CTests pass after a full PC build.

The existing moving-support runner still passes lift/stop/retire and gravity
release. Large-fragment standing, exact standing/walk-away continuation and both
missing-support/floor controls also pass. The correction is not a replacement
for those established polygon-contact and save-validation paths.

Stock64MiB run `artifacts/xemu/render-20260917-220626` passes77 checks for the
tipping sequence. All280 sampled body/player/orientation/angular words match PC,
and its2654-byte save equals the independent corrected PC fixture byte-for-byte.
Endpoint3974 free pages is15.523MiB. Native final image inspected: settled camera,
room/post, launcher and HUD are intact; the support is underfoot, so intermediate
shape motion is established by the sampled state rather than this endpoint.
Disc restored, owned emulator exited, audio disabled, no GitHub screenshot.
Logs: `tipping-support-{all-build,tests,native}.log` and
`tipping-support-{moving,large}-regression.log` under artifacts.
