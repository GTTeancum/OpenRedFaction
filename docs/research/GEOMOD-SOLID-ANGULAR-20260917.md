# Generic solid angular prediction

rf_physics_solid_angular_propose reconstructs49fbe6..49fd84 inside49f930 from the
same hash-locked original RF.exe used by the translation probe. It updates
angular momentum, angular velocity and predicted orientation only. Current
orientation and world inverse inertia are not committed or recomputed here.

On the first pass, body bit2 enables angular drag proportional to angular speed
and coefficient1, regardless of liquid membership. Subtract it from torque,
integrate momentum, then transform momentum by the world inverse inertia tensor.
Repeat01000000 skips preparation but still recomputes angular velocity. If its
magnitude exceeds15, scale both angular velocity and momentum with the stored
float ratio. Float rounding can leave magnitude slightly over15; a subsequent
repeat can therefore rescale again. Tests must not assume an exact15 result.

Scale angular velocity by dt, transform it by the current orientation, normalize
the resulting axis using the stored float angle, and build the incremental
rotation. Reuse the binary-verified GeoMod debris quaternion/matrix routine.
Then reproduce4fc960's valid-basis path: normalize forward and up, rebuild right
as up cross forward and rebuild up as forward cross right. Degenerate/parallel
input directions reject rather than inventing a fallback pose; active bodies
are required to carry a finite nondegenerate basis.

python tools/verify_physics_solid_angular.py --nxdk

512 cases execute original49fbe6..49fd84 and actual arithmetic callees with no
hooks. Flags, dt, coefficient, torque, momentum, incoming angular velocity,
identity/non-diagonal tensors and rotated bases vary. PC and compiled NXDK
momentum, angular velocity and predicted orientation match original bits in all
cases. Untouched NXDK body bytes are also checked. This is Unicorn CPU execution,
not an XEMU gameplay run. Artifact: artifacts/physics-solid-angular.json.

The CTest physics_solid_propose also checks angular cap behavior, unchanged
current pose/tensor, repeat torque bypass, zero dt and atomic rejection of NaN,
nonfinite tensor, degenerate basis, parallel basis and late-component overflow.
Both512-case translation suites were rerun and still pass. Stock NXDK builds.

The earlier helper grenade prediction audit identified these call boundaries
and cap; this implementation independently verifies the instructions and native
compiled function. No agent was resumed. See secondary-re/weapons-grenade-first-prediction.md.

Not yet connected to live fragment scheduling. Solid contact response, partial
rotation commit, scene collision registration and moving-body checkpoints remain
open. Existing real rocket fragments still render at their birth positions.
