# Extracted-solid occupancy, mass and inertia

`tools/probe_geomod_solid_mass.py` executes original `4d1700` through return
against RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
It supplies polygon-linked solid records, not occupancy or hit results. Original
bounds rejection, segment/plane intersection, polygon containment, grid fill,
weighted center, inertia accumulation and inversion all execute. Only `4d1670`
(mesh recenter/rebuild mutation) is intercepted; the returned center is recorded.
This omission does not alter the grid/tensor calculations that follow, but this
probe does not validate recentered mesh publication. FPU control word is 0x27f.

## Recovered contract

- Spacing = max(bounds extent) / 4, half-spacing stored separately.
- X/Y rays begin at -1.5 spacing and advance by spacing while below max bounds.
  This is centered-solid sampling, not min-bound-origin sampling.
- Vertical segments run from -2 spacing to +2 spacing. Real face intersections
  quantize parameter into four cells, each with four subinterval bits.
- Negative-Z normals set entry bits in the low nibble; positive-Z normals set
  exit bits in the high nibble. Entries propagate forward until an exit bit.
- Low-nibble popcount / 4 is occupied fraction. Each cell contributes fraction
  times spacing cubed times density. Contributions and sums store float values.
- Weighted centers use each coarse cell center, not individual subinterval centers.
  The center is divided by accumulated mass when positive.
- Grid origin becomes (-1.5 spacing on each axis) minus weighted center.
  `4d1670` shifts vertices/bounds and rebuilds faces around that center.
- Tensor sums point-mass moments at grid-cell centers; no additional per-cell
  cube inertia term is present. `4fccf0` inverts it in place.
- If accumulated mass is nonpositive, return half the bounds volume times density.
  This fallback does not repair an empty grid or singular/zero inertia.

## Executed evidence

15 cases: cube, wide box, thin tall box, asymmetric trimmed box, and two separated
boxes, each at densities 1, 2.5 and 10. Assertions check known occupancy counts,
independent mass equations, expected weighted center, and tensor times inverse
against identity for nonsingular cases. Empty-grid cases assert zero tensors.
Generated output: artifacts/geomod-postedit-re/solid-mass.json.

The thin tall box has no occupied samples: its narrow X extent misses both
central rays after bounds padding. Mass falls back to half bounds volume and
inertia remains zero. Conversely, a narrow gap between two boxes falls between
rays and does not reduce sampled mass. These are properties of the original
coarse sampling, not exact solid-volume estimates.

## Integration remaining

The shared `rf_physics_solid_mass_prepare` now matches all 15 original executions
byte for byte for the 64 grid bytes, spacing/origin, center, mass and inverse
tensor. The existing `geomod_grid_physics` test also retains all 96 original
grid-to-sphere cases. No heap allocation is used. Invalid inputs or unsupported
grid indices preserve caller output. Caller supplies centered solid bounds and
closed outward polygon faces; mesh recentering is deliberately caller-owned.

Connect center shift, generated collision spheres, body registration and dynamic
piece rendering. Preserve a deliberate zero-inertia/no-sphere handling contract.
The existing no-geometric-solid physics fallback is not interchangeable with this
path. No Xbox runtime behavior or visuals changed in this shared-core step.

Stock-profile NXDK build succeeds (solid-mass-xbox.log); this is build evidence,
not native runtime or visual acceptance.
