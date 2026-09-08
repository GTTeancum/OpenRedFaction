#ifndef RF_MODEL_H
#define RF_MODEL_H
#include "rf/vpp.h"

/* Bounded names supplied by a model loader; length excludes the terminator.
 * Groups retain the original 0x51d5b0 search order, without original pointers
 * or object layout. Their semantic names await loader reconstruction. */
typedef struct rf_model_name { const char *data; size_t length; } rf_model_name;
typedef struct rf_model_name_group {
    const rf_model_name *names;
    uint32_t count;
} rf_model_name_group;
/* Default-locale ASCII case folding; output is untouched on failure.
 * RF_NOT_FOUND denotes absence, RF_FORMAT an embedded NUL or invalid name,
 * RF_RANGE invalid arguments or a combined index exceeding signed 32 bits. */
int rf_model_find_tag(const rf_model_name_group groups[3],
                      rf_model_name query, int32_t *index);
/* Raw BONE payload fields, before the original quaternion conversion.
 * Caller locates the section and owns output storage; no allocation is made.
 * Input/output storage must not overlap. Error returns leave output untouched. */
typedef struct rf_model_bone {
    char name[25];
    float rotation[4], position[3];
    int32_t parent;
} rf_model_bone;
int rf_model_decode_bones(const void *payload, size_t bytes,
                          rf_model_bone *bones, uint32_t capacity, uint32_t *count);
/* Reconstructs normalization (0x519720) and local transform (0x4fe900).
 * Output is nine rotation floats followed by three translation floats.
 * Zero-length/non-finite rotations are rejected; output is unchanged on error.
 * This is a local transform, before parent composition or animation. */
int rf_model_bone_transform(const float rotation[4], const float position[3], float transform[12]);
/* Reconstructed 0x51c620: row-vector local * parent, with implicit final column
 * (0,0,0,1). Aliased output is supported; errors leave it unchanged. */
int rf_model_compose_transform(const float local[12], const float parent[12], float result[12]);
/* Original 0x51cb50 orders byte indices by depth, stable within each depth.
 * Supports up to 256 bones; rejects cycles/invalid parents before output. */
int rf_model_bone_order(const rf_model_bone *bones, uint32_t count, uint8_t *order, uint32_t capacity);
#endif
