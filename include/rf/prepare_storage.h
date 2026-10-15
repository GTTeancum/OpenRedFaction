#ifndef RF_PREPARE_STORAGE_H
#define RF_PREPARE_STORAGE_H
#include "rf/prepare_memory.h"
#include <stdlib.h>
/* Separate storage ownership, not an extension of the pure memory-admission
 * callback. allocate publishes one zero-filled, >=8-byte-aligned disjoint
 * stable slice; on failure *out is NULL. The caller pins descriptor/context
 * through all matching disposals. Slices cannot outlive the enclosing owner.
 * release may defer physical reclamation until enclosing arena disposal.
 * Never use this interface for storage transferred into persistent runtime.
 */
typedef struct rf_prepare_storage {
    int (*allocate)(void *context,size_t bytes,void **out);
    void (*release)(void *context,void *pointer);
    void *context;
} rf_prepare_storage;
static inline int rf_prepare_storage_allocate(const rf_prepare_storage *storage,
    const rf_prepare_memory *memory,const void *owner,size_t bytes,void **out)
{
    void *p;int status;
    if(!out||*out||!bytes)return RF_RANGE;
    if(storage){
        if(!storage->allocate||!storage->release)return RF_RANGE;
        return storage->allocate(storage->context,bytes,out);
    }
    status=rf_prepare_memory_admit(memory,owner,bytes);if(status)return status;
    p=calloc(1,bytes);
    if(!p)return owner?rf_prepare_memory_finish(memory,owner,RF_IO):RF_IO;
    *out=p;return RF_OK;
}
static inline int rf_prepare_storage_release(const rf_prepare_storage *storage,
    const rf_prepare_memory *memory,const void *owner,void *p,int status)
{
    if(!p)return status;
    if(storage){storage->release(storage->context,p);return status;}
    free(p);
    return owner?rf_prepare_memory_finish(memory,owner,status):status;
}
#endif
