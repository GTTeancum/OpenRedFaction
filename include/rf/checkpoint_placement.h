#ifndef RF_CHECKPOINT_PLACEMENT_H
#define RF_CHECKPOINT_PLACEMENT_H
#include "rf/geomod.h"
#include "rf/geometry.h"
#include "rf/physics.h"
typedef struct rf_checkpoint_placement {
    const rf_geometry_collision_world *world;
    uint32_t replaced_room,query_flags;
    const rf_physics_sphere *spheres;uint32_t count;
    float position[3],basis[9];
} rf_checkpoint_placement;
enum rf_checkpoint_placement_reason {
    RF_CHECKPOINT_PLACEMENT_FITS=0,RF_CHECKPOINT_PLACEMENT_SURFACE=1,
    RF_CHECKPOINT_PLACEMENT_SOLID=2,RF_CHECKPOINT_PLACEMENT_AMBIGUOUS=3,RF_CHECKPOINT_PLACEMENT_UNSUPPORTED=4
};
typedef struct rf_checkpoint_placement_result {uint32_t sphere,reason,retries;} rf_checkpoint_placement_result;
/* Pure, bounded standing-body fit: candidate REPLACES THE ENTIRE replaced_room.
 * Not valid for future subset/brush replacement without a different adapter.
 * Caller supplies already-restored static-world identity and actual body spheres;
 * rejects unsupported moving/crouched/attached modes, actors and movers itself.
 * Ordered primary/child eligibility follows static body queries; liquid/alpha
 * modes1180 are unsupported. No texture lookup, allocation or tree scratch writes.
 * Basis must be finite, right-handed orthonormal;1..8 finite spheres with radius
 * >.002. Existing DEV .002 clearance tolerance accepts exact surface tangency.
 * Uses original-derived4e3800 nearest-face classification including physical
 * details, not a zero-motion sweep or a recovered original save ABI.
 * RF_OK=fit; RF_NOT_FOUND=surface/solid/ambiguous (16 retries). Optional result
 * is published for either outcome; all other failures preserve result.
 * Geometry views must stay valid and unchanged throughout; no output aliasing. */
int rf_checkpoint_placement_check(const rf_geomod_terrain_view *,const rf_checkpoint_placement *,rf_checkpoint_placement_result *);
/* Adds original-derived grounded probe: lowest-center sphere, static support0,
 * grounded depth from caller dt/class_speed, nearest hit fraction<1 and normalY>=.5.
 * Full fit runs first. Same pure rollback/output rules; requires standing yaw-only
 * basis (upright Y). This is static support eligibility, not moving-platform state. */
int rf_checkpoint_standing_check(const rf_geomod_terrain_view *,const rf_checkpoint_placement *,float dt,float class_speed,rf_checkpoint_placement_result *);
typedef struct rf_checkpoint_support_hit {float fraction,normal[3];uint32_t stable;} rf_checkpoint_support_hit;
/* Optional read-only support provider, queried with the static-world nearest
 * limit. A nearer unstable surface rejects; static geometry wins exact ties.
 * Provider returns RF_OK and matched0/1, or an error without publishing state. */
typedef int (*rf_checkpoint_support_query)(void *,const rf_physics_ground_probe *,float limit,rf_checkpoint_support_hit *,uint32_t *matched);
int rf_checkpoint_standing_check_with_support(const rf_geomod_terrain_view *,const rf_checkpoint_placement *,
    float dt,float class_speed,rf_checkpoint_support_query,void *,rf_checkpoint_placement_result *);
/* Closed outward solid, local-space sphere. Shared .002 clearance tolerance;
 * rejects surface overlap, interior centers and unresolved edge ambiguity.
 * Uses nearest polygon/edge distance plus original-derived face classification,
 * not a zero-motion sweep. Faces must describe one closed oriented component. */
int rf_checkpoint_solid_sphere_check(const rf_collision_face *,uint32_t count,
    uint32_t flags,const float center[3],float radius,rf_checkpoint_placement_result *);
#endif
