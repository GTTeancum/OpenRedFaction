#ifndef RF_GLARE_H
#define RF_GLARE_H
#include "rf/object_registry.h"
typedef struct rf_glare_class {
    float radius_minimum,radius_maximum;const void *definition;
} rf_glare_class;
typedef struct rf_glare_state {
    uint32_t parent;int32_t tag;uint8_t active,reserved[3];
    int32_t timer;float samples[6];const void *definition;int32_t class_index;
    uint32_t flags;rf_object_link link;float last_position[3];uint32_t word_2cc;
    uint8_t byte_2d0,padding[3];float vectors[2][3];
} rf_glare_state;
typedef struct rf_glare_create_descriptor {
    uint32_t parent;float radius,position[3],matrix[9];
} rf_glare_create_descriptor;
typedef struct rf_glare_create_backend {
    int (*radius)(void *,float minimum,float maximum,float *result);
    int (*tag_pose)(void *,uint32_t parent,int32_t tag,float result[12]);
    int (*allocate)(void *,const rf_glare_create_descriptor *,rf_glare_state **);
    void *context;
} rf_glare_create_backend;
/*413d20 parented glare. Radius sampling precedes parent/tag lookup and type10
 * allocation (identifier-1,parent,flags30000,final0; descriptor flags0/scale1).
 * Backend supplies generic object ownership with unlinked glare state. NULL
 * allocation succeeds without insertion. Class out of range returns NULL
 * without callbacks. Success appends in factory order; class storage/list and
 * allocated state must outlive their borrowers. No rendering/deletion here.
 * Callback failures stop with output preserved; allocation errors must clean
 * untransferred ownership. Successful nonnull allocation is published before
 * initialization. Reserved bytes retain generic allocator values. No heap
 * allocation in this function. Finite inputs and disjoint outputs required. */
int rf_glare_create(const rf_glare_class *classes,uint32_t count,int32_t index,
    uint32_t parent,int32_t tag,uint32_t flag,rf_object_list *list,
    const rf_glare_create_backend *backend,rf_glare_state **out);
#endif
