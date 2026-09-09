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
typedef struct rf_physics_mass_tensor {
    float mass,tensor[9];
} rf_physics_mass_tensor;
/* 49ec90 existing-sphere accumulation before matrix inversion at 49edf0.
 * Starts from the supplied mass/tensor, including a negative initial mass;
 * caller selects the original mass-generation branch. Requires nonempty
 * spheres, finite inputs and nonnegative density/radii. No sphere self-inertia
 * term is added by the original. Errors leave output unchanged. */
int rf_physics_spheres_accumulate(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result);
/* Original 4fccf0: a zero determinant preserves the input matrix successfully.
 * Finite input required; unrepresentable intermediate/output values fail with
 * RF_RANGE, leaving output unchanged. Input and output may alias. */
int rf_physics_tensor_inverse(const float source[9],float result[9]);
/* 49cd30 world tensor update, using the original nine-float storage order.
 * Two matrix products with a float store between them. Inputs must be finite;
 * errors preserve output. Output may alias either input. */
int rf_physics_tensor_world(const float local[9],const float orientation[9],float result[9]);
/* Existing-sphere generated-mass branch including the inverse tensor step.
 * Does not copy spheres into a runtime body or initialize the other fields. */
int rf_physics_spheres_prepare(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result);
#endif
