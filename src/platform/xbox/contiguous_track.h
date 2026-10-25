#ifndef RF_XBOX_CONTIGUOUS_TRACK_H
#define RF_XBOX_CONTIGUOUS_TRACK_H
/* Diagnostic accounting of game-owned contiguous (GPU-visible) memory:
 * [0] live blocks, [1] live bytes (page-rounded by the kernel), [2] blocks
 * ever allocated, [3] blocks ever freed. Include after xboxkrnl.h; the macros
 * route every game allocation and free through the counters. */
#include <stdint.h>
#include <xboxkrnl/xboxkrnl.h>
#include "rf/heap_audit.h"
extern uint32_t rf_xbox_contiguous[4];
static inline PVOID rf_contiguous_allocate(SIZE_T bytes,ULONG_PTR low,ULONG_PTR high,ULONG_PTR align,ULONG protect,
    const char *file,int line)
{
    PVOID p=(MmAllocateContiguousMemoryEx)(bytes,low,high,align,protect);
    if(p){uint32_t size=(uint32_t)MmQueryAllocationSize(p);++rf_xbox_contiguous[0];rf_xbox_contiguous[1]+=size;++rf_xbox_contiguous[2];
        rf_audit_contiguous_add(p,size,file,line);}
    return p;
}
static inline void rf_contiguous_free(PVOID p)
{
    if(p){--rf_xbox_contiguous[0];rf_xbox_contiguous[1]-=(uint32_t)MmQueryAllocationSize(p);++rf_xbox_contiguous[3];rf_audit_contiguous_remove(p);}
    (MmFreeContiguousMemory)(p);
}
#define MmAllocateContiguousMemoryEx(bytes,low,high,align,protect) rf_contiguous_allocate((bytes),(low),(high),(align),(protect),__FILE__,__LINE__)
#define MmFreeContiguousMemory rf_contiguous_free
#endif
