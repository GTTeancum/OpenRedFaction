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
#endif
