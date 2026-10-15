#ifndef RF_PREPARE_MEMORY_H
#define RF_PREPARE_MEMORY_H
#include "rf/vpp.h"
#include <stddef.h>
/* Optional synchronous allocation admission, not an allocator replacement.
 * NULL owner means one retained request: its reservation lasts until terminal
 * transaction cleanup. A non-NULL owner is a pinned transient identity, released
 * after free or failed malloc. Release never refunds cumulative admission.
 * Callbacks may change only exclusive caller-owned accounting. They must not
 * allocate, mutate scene owners, or call preparation recursively. Retained
 * failures abort the enclosing transaction; do not continue/reset its ledger.
 * No aggregate stage-byte total is an allocation request. NULL memory preserves
 * legacy callers. Caller authenticates scope lifetime and exact storage capacity.
 */
typedef int (*rf_prepare_memory_callback)(void *context,const void *owner,size_t bytes,uint32_t acquire);
typedef struct rf_prepare_memory {rf_prepare_memory_callback account;void *context;} rf_prepare_memory;
static inline int rf_prepare_memory_admit(const rf_prepare_memory *memory,const void *owner,size_t bytes)
{return !memory?RF_OK:!memory->account?RF_RANGE:memory->account(memory->context,owner,bytes,1);}
static inline int rf_prepare_memory_finish(const rf_prepare_memory *memory,const void *owner,int status)
{
    int released=!memory?RF_OK:!memory->account?RF_RANGE:memory->account(memory->context,owner,0,0);
    return status?status:released;
}
#endif
