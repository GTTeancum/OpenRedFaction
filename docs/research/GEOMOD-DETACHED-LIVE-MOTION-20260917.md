# Live extracted fragment motion

The scene now schedules retained detached bodies before ordinary debris and
rocket updates. Births from the current rocket edit start motion on the next
frame. The shared bounded solid step supplies prediction, contact response and
settling; the scene body query uses each body's sampled sphere set (60 spheres
for the middle-post fragment), the committed composed collision world, current
mover geometry and resolved surface elasticity/friction. It adds no allocation
per frame and retains the existing2MiB detached-owner budget.

The scene uses its body basis directly for fragment drawing, including at a
partial-only substep cutoff. This is the port's current presentation policy.
Flags0x460 reuse the existing shared scene body-sweep filtering; this integration
is validated for the dry enemy-free post, not every original generic-solid
filter branch. Empty sphere sets remain empty; no oversized-sphere fallback.

PC installed middle-shot evidence:
- Frame275: center(-5.01583624,1.65020788,2.5), vertical velocity-0.980000079.
- Frame290: center(-5.01583624,1.0989579,2.5), vertical velocity-3.43000054.
- Frame350: center(-4.96960354,-0.558699608,2.53502345), zero velocity and sleeping.
- Restored frame350 plus200 frames and uninterrupted550 frames produce the
  same body hash2190671051 and2752-byte checkpoint SHA256
  2c092811f0729ba29bc8190f8a0d46a7927a6962c9e2f0d38b909b753ca8af76.
- All5320 fixed-camera post-region pixels agree. Inspected captures show the
  fragment resting tilted on the remaining stub; intermediate captures contain
  the explosion/smoke, which obscure some of the actual falling body.

The updated restart harness requires the real60-sphere body, settled below
its birth height, matching motion state and pose after reload, no step-limit
hit and no error. All121 PC tests pass, and NXDK builds.

XEMU continuation evidence: artifacts/xemu/render-20260917-063219, stock64MiB,
4051 available pages at completion. Native body hash, position/velocity bits
and ownership match PC; native framebuffer inspected and shows the same tilted
chunk. The full fresh rocket sequence is validated separately from this
already-settled reload run. Fresh550-frame XEMU sequence
artifacts/xemu/render-20260917-063427 passes71 checks with the same complete
body hash and exact pose bits as PC. Both native checkpoints equal their PC
counterparts; uninterrupted/reloaded native post-region pixels are identical.
The fresh native framebuffer was also inspected. Both harness runs restored
the staged disc and closed only their own emulator instance.

Remaining limits: detached bodies are not yet registered as obstacles for the
player/weapons or other detached bodies; moving-surface velocity response,
liquid classification and later blast wake remain open. An airborne full-player
save at frame270 was correctly rejected by the existing weapon-cooldown gate;
that restriction was preserved. Moving-state serialization has scene/core
fixture coverage, but this real replay's save occurs after settling. No audio
quality claim, general campaign acceptance or visual parity claim is made.
