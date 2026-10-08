# Coarse mover bounds before shared CPU projection

The15:00 L1S1 profile attributes2.875ms to world geometry preparation after
retained static-world admission. The shared projector still visits every
moving-solid mesh and transforms its vertices even when completely outside
the current camera. This patch is source-written; root owns16:00 validation.

`rf_geometry_collision_movers_open` computes `bounds.origin_radius` from all
local geometry vertices and stores it in each `rf_group_attached_pose.radius`.
The accepted pose preserves that radius while publishing current position and
output matrix. `world_mesh` now uses the enclosing local cube[-radius,radius]
transformed into camera coordinates for a conservative six-plane rejection.
These are exactly the projector's near0.1, far1000, horizontal±z and
vertical±0.75z planes. Axis intervals deliberately enlarge the bound; no
orthonormal or unit-scale assumption is required. A relative rounding guard
keeps borderline cases on the original projector. Invalid/zero bounds fall
back to the old path. No room visibility inference or new geometry owner is
introduced, and generated GeoMod meshes keep their separate existing path.

The original per-object render dispatcher still runs and publishes its exact
successful outer marker. Rejected objects produce zero triangles, as their
full triangle clipping would have. Everything that may intersect the frustum
continues through the original transformation, lighting, clipping, material
remapping and draw order. No vertex detail, actor simulation or draw distance
changes. No new allocation or cached state. Existing call-level structural
and material admission checks remain.

`rf_preview_mover_bounds[4]` counts calls, wholly rejected bounds, uncertain
fallbacks and retained bounds. These counters and the existing world phase
are evidence for the next comparison; no speed improvement is claimed yet.

## Post16 tight local bounds

The16:00 run recorded1,200 broad tests with zero rejects, so the original
origin-centered radius was not effective in L1S1. `rf_geometry_movers_open`
now retains the exact local minimum/maximum over all immutable vertices once
at load. The existing per-mover record budget accounts for28 added bytes; no
separate allocation or per-frame vertex walk is added. Current pose matrices
transform the local box center/extents, retaining the same conservative plane
intervals and rounding guard. Hand-built records without bounds keep the old
radius fallback. These are render metadata only; collision bounds, geometry,
poses, clipping and save wires are unchanged. Original loaded mover vertices
are immutable; GeoMod-generated geometry retains its separate draw path.
Source-written after16:00, awaiting17:00 measurement.
