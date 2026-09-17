# Generic solid translation before fragment motion integration

rf_physics_solid_propose reconstructs49f9c3..49fbe6 inside49f930 from the original
RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
This is the generic-body translation path, distinct from actor49f3c0 and its
flag4000 route. It does not reuse character contact response for chunks.

Body bit1 controls gravity. Body bit2 and object bit80000 together enable liquid
drag: speed times coefficient1 times2, applied against velocity. Acceleration is
evaluated at the initial velocity and again at the half-step velocity. Final
velocity receives the second acceleration. Next position uses updated velocity
minus half-dt-squared times that acceleration. Original float stores are retained.

Repeat-pass01000000 skips acceleration/velocity preparation but still uses the
retained acceleration scratch for predicted position. The original scratch is
at7c7048; the shared C API makes it caller-owned rather than global. Its lifecycle
must be retained across one body's contact retries, not silently zeroed.

python tools/verify_physics_solid_propose.py --nxdk

Executes the original instructions and their actual arithmetic callees without
patches.512 cases independently vary gravity and drag gates, liquid membership,
retry flag, zero/nonzero dt, mass, coefficients, force, velocity and retained
acceleration. PC outputs and compiled NXDK function outputs match original bits.
The NXDK probe also checks every untouched byte of the308-byte body state.
Artifact: artifacts/physics-solid-propose.json. This is CPU execution under
Unicorn, not a new XEMU gameplay run. Stock NXDK build also passes.

The physics_solid_propose CTest checks analytical free flight, retained retry
acceleration, invalid inputs and late-axis overflow with atomic preservation.
No allocations or changes to live scheduling are introduced.

Next: generic angular prediction and solid contact resolution, then wire body
steps into the scene together with explicit active-pose checkpoint ownership.
Current visible fragments remain stationary; this work alone does not establish
falling, collision response, liquid traversal or playable fragment physics.
