#ifndef RF_PHYSICS_H
#define RF_PHYSICS_H
#include "rf/vpp.h"
typedef struct rf_physics_fallback {
    float mass;
    float center[3],radius,parameter_10;
} rf_physics_fallback;
/* Recovered fields for 49ec90's no-model, empty-sphere-list path. Caller has
 * selected sphere mode and resolved material density. No allocation, inertia
 * inversion or runtime registration; the original undefined sixth sphere word
 * is deliberately not represented. Nonfinite inputs, negative density/radius
 * and overflowing output mass return RF_RANGE without modifying output. */
int rf_physics_fallback_prepare(float density,float radius,float mass,rf_physics_fallback *result);
typedef struct rf_physics_sphere {
    float center[3],radius,parameter_10;uint32_t opaque_14;
} rf_physics_sphere;
typedef struct rf_physics_spheres {
    rf_physics_sphere *items;uint32_t count,allocated_bytes;
} rf_physics_spheres;
/* Copies ordered records into one exact-sized allocation. Budget includes the
 * owner and records, excluding allocator overhead. Empty destination required;
 * source may be released after success. Rejects nonfinite centers/radii and
 * negative radii; other field bits are preserved. Errors preserve output. */
int rf_physics_spheres_open(const rf_physics_sphere *source,uint32_t count,uint32_t budget,rf_physics_spheres *result);
void rf_physics_spheres_close(rf_physics_spheres *spheres);
#endif
