# Restored destruction rendering follows committed terrain

## Cause

actor_follow_view selected the intact static world and skipped generated terrain
when rf_scene_geomod[1] was zero. That array is diagnostic telemetry. Authored
checkpoint publication restored the terrain, collision, lighting and detached
registry without updating it. A later bind refreshed the counter, making final
state comparisons pass even though gameplay had rendered the intact post.

Temporary per-piece traces confirmed identical local vertices and birth origin
(-5.01583624,1.69920778,2.5) in control and continuation. The discrepancy was not
fragment reconstruction. The trace code has been removed.

## Change and regression

The view callback reads cut count from the committed rf_geomod_terrain owner,
then consistently uses it to select the static-world exclusion view and append
generated terrain. Authored checkpoint commit also refreshes diagnostic presence,
cut count, generation and memory only after successful publication.

check_authored_restart.py --case middle-shot now compares all5320 pixels of the
fixed-camera post rectangle (300,180)-(338,320). It includes the entire projected
post and surrounding opening, excluding independently restarted weapon/HUD
animation. The old middle-continuation captures fail; middle-render-fixed passes
with SHA256 bf79640281aafa6ad5847e9652346a4cd9f182b08efd490c7f2e6651eb156bb0.
The corrected PC continuation frame was inspected and restores the broken-post
appearance. Checkpoint bytes and detached ownership still match.

This is fixed-camera rendering coverage, not whole-frame equivalence, motion,
audio, arbitrary camera coverage or complete save-state acceptance. Detached
body motion/contact response and active-pose saves remain open.

## Native and final validation

Stock64MiB XEMU continuation artifacts/xemu/render-20260917-053940 passes69
checks. Its native frame was inspected and all5320 post-region pixels match the
uninterrupted native render-20260917-052820 capture exactly. Its exported2416-byte
checkpoint is also identical. Endpoint available memory is4055 pages; disc
staging was restored and the harness-owned emulator exited.

The PC middle-render-verified report includes the image regression. The complete
Release suite passes120/120 (artifacts/geomod-postedit-re/reload-render-tests.log).
The stock NXDK build explicitly rebuilt scene.obj before this native run.
This closes the specific stale-counter rendering discrepancy documented in
GEOMOD-REAL-ROCKET-DETACHMENT-20260917.md, not the remaining physics/save scope.
