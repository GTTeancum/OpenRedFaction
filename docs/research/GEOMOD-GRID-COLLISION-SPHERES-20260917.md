# Solid grid collision-sphere reconstruction

`tools/probe_geomod_grid_spheres.py` executes49ee3d..49efa4 with supplied
post-mass grid bytes, spacing and origin. Vector construction/copy and bound
arithmetic execute unchanged. Only417f30 list append is intercepted to collect
the six-word sphere record. Its undefined last word is omitted from fixtures;
the port initializes that word to0.

`rf_physics_grid_spheres` matches all96 fixtures: all16 occupancy nibbles,
high-nibble noise at fractional spacing/origin, and a separate occupied cell
at every one of the64 grid positions. Exact comparisons cover sphere order,
center coordinates, radius, parameter10 and final body-radius bits.

Recovered rule:

- Scan X outermost, then Y, then Z: cell index16X+4Y+Z.
- Occupancy is popcount(cell&15)/4. Only occupancy>0.25 creates a sphere.
  Earlier notes saying all nonzero cells produce spheres were incorrect.
- Center component is float(index*spacing+origin).
- Sphere radius is float(occupancy*float(spacing*0.5)); parameter10=-1.
- Body bound is sqrt of the stored-float maximum of center-length-squared plus
  sphere-radius-squared. This is not the more conservative distance+radius.

The shared helper allocates nothing, uses a64-record temporary array, checks
caller capacity and publishes output only after all finite/bounds checks pass.
Short capacity and invalid input preserve sphere output/count/radius. It does
not generate occupancy, compute mass/inertia, shift the mesh center, register
an object or move a piece. Input grid geometry must already reflect4d1700.

Validation: geomod_grid_physics passes all96 exact original comparisons and
failure controls. Stock NXDK compile succeeds. This is shared setup code;
no new runtime motion or visual acceptance is claimed.
