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
#endif
