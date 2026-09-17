# Sixteen-cut rendered depth attribution

The current fixed16 crater's visible dark patch is recessed in the inspected PC
view, despite its rock-like appearance. No reversal of generated depth or normals
is justified by these measurements. Appearance remains unaccepted.

`tools/dev_crater_depth_check.py` now accepts a build directory and expected cut
count while preserving its existing strict thresholds. Run against the expanded
PC build and `artifacts/geomod-9k-live/input.bin`, it executes the full2500-frame
cut replay and a paired intact control with only fire buttons cleared. Camera
words match exactly, debris has expired and both players remain alive.

Results in `artifacts/geomod-16-depth/report.json`:

-4056 substantially farther pixels, rectangle[281,195,357,270].
-7 nearer pixels; largest raw nearer difference159 encoded depth units.
-0 newly uncovered pixels.
-The legacy check **fails**: it requires more than10000 farther pixels and zero
 nearer pixels. Neither tolerance nor minimum count was changed to make it pass.

New `tools/audit_crater_depth_owners.py` independently projects pixel centers
into every eligible recorded world triangle and selects the closest depth. It
attributes every flagged pixel, retaining triangle/material/lightmap IDs and
source hashes. Every reconstructed depth in this fixture matches exactly:

-All4056 farther pixels select material49, the independently verified rock02
 substrate.
-All7 nearer pixels select material0/lightmap0 in both cut and intact renders,
 outside the crater rectangle. These are the authored floor. Cut triangles
195/196/216 replace intact triangles2/3; no flagged nearer pixel selects crater
 material. Earlier side-view research demonstrated floor projection rounding,
 but this new audit establishes ownership rather than repeating that analytical
 rounding bound for this camera.

Both endpoint images were inspected sequentially: intact wall versus the dark
faceted opening, with matching room/camera presentation. Weapon ammunition differs
as expected because the intact control does not fire. No original-game image
was consulted; no Xbox run or audio validation belongs to this depth audit.

Scope remains one stationary viewpoint and the upper350 framebuffer rows. This
is stronger current16-cut evidence than the older two-cut side views, but not a
global depth/nonintersection proof or visual-fidelity acceptance. Geometry,
texture identity and recovered idle lighting now all argue against an arbitrary
normal flip or brightness gain as a fix for the ambiguous silhouette.
