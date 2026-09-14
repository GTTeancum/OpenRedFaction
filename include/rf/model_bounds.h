#ifndef RF_MODEL_BOUNDS_H
#define RF_MODEL_BOUNDS_H
#include "rf/model_file.h"

/* Port-owned conservative bounds for retained GPU batches. Immutable source
 * positions are bounded separately for each influencing bone. A posed vertex
 * is in their convex hull when nonnegative weights sum to at most 256/256;
 * include the origin for sums below one. Larger sums disable this optimization.
 * Rigid batches use slot zero and ignore authored skin weights. No allocation. */
typedef struct rf_model_bounds {
    float minimum[50][3],maximum[50][3];
    uint32_t used[2],bones,include_origin;
} rf_model_bounds;
/* Output unchanged on error/unsupported input (RF_NOT_FOUND). */
int rf_model_bounds_build(const rf_model_geometry *geometry,uint32_t batch,
    uint32_t bones,rf_model_bounds *out);
/* Posed model-space boxes may be shared when prepared matrix bytes match;
 * placement/camera projection remains per draw. Bounds/source must be immutable. */
typedef struct rf_model_box {float center[3],extent[3];uint32_t populated;} rf_model_box;
int rf_model_bounds_pose(const rf_model_bounds *bounds,const float (*matrices)[12],rf_model_box *out);
int rf_model_box_visible(const rf_model_box *box,const rf_model_projection *view,
    uint32_t width,uint32_t height,uint32_t *visible);
/* Only rejects a complete bound beyond one camera plane. Crossing/uncertain
 * bounds remain visible; per-triangle GPU clipping and facing still apply.
 * No near/far policy change: test the four screen sides and behind the camera.
 * Matrices are prepared model-space skinning transforms; rigid uses NULL.
 * Rounding margins account for the GPU's float operations. Output preserved
 * on malformed input. Unsupported projection returns visible. */
int rf_model_bounds_visible(const rf_model_bounds *bounds,const float (*matrices)[12],
    const rf_model_projection *view,uint32_t width,uint32_t height,uint32_t *visible);
#endif
