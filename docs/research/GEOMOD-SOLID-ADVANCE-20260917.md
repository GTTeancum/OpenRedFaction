# Solid prediction acceptance

Shared `rf_physics_solid_advance` composes the accepted-body pose needed between
collision queries and contact response. This is not yet a live scene scheduler.

Full fraction1 follows original49d280: accept predicted position and basis,
publish basis, refresh world inverse inertia from local inertia, set contact
fraction1 and rebuild radius bounds. The caller's remaining time becomes zero.
Partial fraction follows the already recovered ordinary4a01b0 translation
adapter, retains swept bounds and the last published basis, and accepts the
full incoming-step predicted rotation before refreshing inertia. The caller
must supply the unchanged prediction for that same dt; do not recompute angular
prediction with fraction-scaled dt or rerun torque integration.

Full acceptance was checked by executing original49d280 and the compiled NXDK
helper in Unicorn across128 positions, radii and orientations, using anisotropic
inertia. All pose, tensor, bounds, contact-fraction and published-basis bytes
match, with no unexpected body writes. Run:

```
python tools/verify_physics_solid_advance.py
```

The shared physics_solid_propose test also checks partial spatial setback,
remaining time, retained bounds/public basis, anisotropic tensor rotation,
full acceptance and atomic rejection of nonfinite/overflow/unsupported inputs.
Partial acceptance is composition coverage, not a new original-binary parity
grid. Existing original partial/full evidence is recorded in
secondary-re/weapons-grenade-first-prediction.md.

Limitations: nonrigid bodies only; nonnegative finite radius on full acceptance;
partial translation currently requires nonzero predicted displacement through
the existing adapter. No collision admission, material lookup, response, position
publication, active-body checkpoint persistence or live rendering is added here.
Those remain necessary before enabling detached-fragment simulation.
