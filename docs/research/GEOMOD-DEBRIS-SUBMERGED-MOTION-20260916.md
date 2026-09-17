# Submerged debris displacement

Original 48f932..48f98b checks the retained room's liquid byte for exactly 1,
then calls 4ce080 with the fragment's current position. That predicate compares
Y inclusively against the extended-precision sum of raw room depth and bottom.
For submerged fragments it selects float 0.2; otherwise it selects 1. Each
velocity component is multiplied by frame duration and stored as float, multiplied
by the selected scale and stored again, then added to position. Combining these
operations or dividing by 60 can change rounding.

`rf_geomod_debris_motion` reconstructs this proposed endpoint. It validates finite
inputs/results, supports start/output aliasing, allocates nothing and preserves
output on failure. It does not update velocity, gravity, spin, bounce, collision,
liquid-crossing effects or room ownership.

The live moving-chunk loop now uses this endpoint for collision and copies it
directly on a miss. It supplies the authored room marker and retained room's raw
depth/bottom. Settled chunks continue to skip movement. Existing gravity and
bounce rules remain separate; this is not full original debris simulation parity.

## Original instruction evidence

`python tools/probe_debris_motion.py` executes RF.exe 48f900 through 48f990,
including actual 4ce080 and vector helpers, and stops before the collision query.
No services are substituted. It checks the executable SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
It emits `artifacts/debris-motion/original.json` and the tracked fixture include.

The 72 cases cover flags 0/1/2/255, one float step below/on/above the raw water
surface, durations 0/1/60/0.25 and two mixed-sign velocity vectors.
`geomod_debris_motion` compares endpoint float words exactly, then checks aliasing
and failure atomicity. All cases pass. Other liquid-height magnitudes, general
collision ordering and the later wet contact/ripple route remain separate work.

The ctf06 two-blast PC replay exercises 2803 moving steps, including 585 submerged
steps, ending with 18 live chunks and 104 bounces. This differs from the old
full-speed motion (17 live and 105 bounces). Its five accepted wet birth positions
still match original instructions exactly. Changed trajectories are expected;
geometry publication and destruction history are independent of this correction.

## Native acceptance

The stock 64 MiB run `artifacts/xemu/render-20260916-235803` completed 550 frames
and passed all 61 PC/Xbox comparisons. Motion telemetry matches exactly:
`[2803,585,3,1,1215307649,0,0,0]`, including 585 submerged steps and the final
proposed-position hash. Debris state/render hash also matches PC.
The 3884-byte destruction/player checkpoint retains SHA256
`7e6900b99a39ce38dc0154b70e95b601b9bd8816fe837cf1198ccb4857d5fb94`, unchanged
from the preceding visibility run. 4159 physical pages remain free (16.246 MiB).
Disc restoration completed and the owned emulator exited.

The native endpoint was inspected: damaged post, scattered fragments, room,
weapon and HUD are present. This endpoint does not establish every animated
trajectory or audio correctness. The full PC build succeeded and all 115 CTests
pass; logs are under `artifacts/debris-motion/`.
