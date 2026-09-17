# Solid contact impulse and settling

rf_physics_solid_contact reconstructs the ordinary nonrigid49d330 path after
contact admission and position advance. It consumes resolved surface elasticity,
friction, world point/normal and gravity. Position/tensors are preserved; the
solver publishes elasticity, linear velocity, momentum and angular velocity.
It also implements reset/skip gates and the stopped-body flag/motion reset.

The earlier helper contract secondary-re/weapons-nonrigid-response-contract.md
provides the ordered arithmetic and ownership map. This implementation verifies
actual49d330 independently with512 new inputs; it does not rely solely on those
notes. Original hash remains b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

Contact velocity uses angular momentum transformed by the column-oriented inverse
tensor, crossed with the world lever arm, then added to linear velocity. Normal
impulse uses the row-oriented tensor. Friction clamps linear and contact tangent
budgets in sequence; zero tangent lengths skip their respective friction branch.
Momentum receives the torque of normal plus contact-tangent impulses; angular
velocity is recomputed from momentum. Separating contacts preserve the body.

Elasticity decays after the effective contact coefficient is captured. Low
elasticity or both low linear/angular speeds enter the real417e00 stop mask and
clear velocity, momentum and angular velocity. A successful STOPPED result lets
the caller stop physics without deleting the piece or unrelated lifecycle state.

python tools/verify_physics_solid_contact.py --nxdk

The entire original49d330 executes with actual arithmetic and stop routines.
Only two material getter returns are supplied via explicit x87 fld/ret stubs.
512 cases vary mass, coefficients, axis/oblique normals, centered/off-axis contact,
tensors, motion and skip/reset gates. Original outputs match PC and compiled
NXDK bit for bit; untouched NXDK body bytes and result decisions also match.
Artifact: artifacts/physics-solid-contact.json. These are CPU probes, not XEMU
collision-query or gameplay evidence. Stock NXDK builds successfully.

physics_solid_propose CTest now also covers analytical centered bounce, current
pose/tensor preservation, stop flags, unsupported route rejection, nonfinite
normal and invalid effective mass, with atomic output preservation on errors.

Flags4000 (vehicle/rigid branch) and100 (random normal) return RF_NOT_FOUND without
mutation. They are not present in current extracted-piece birth flags8000003f.
No moving counterpart, query traversal, collision scheduler, partial angular
commit, scene registration or active-pose save is implemented by this helper.
Live fragments still remain at their birth poses. Next work is composition of
the verified arithmetic with scene collision and checkpoint ownership.

Final Release suite passes121/121 in artifacts/geomod-postedit-re/solid-contact-tests.log.
