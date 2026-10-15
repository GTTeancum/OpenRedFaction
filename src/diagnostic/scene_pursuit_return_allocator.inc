#ifndef RF_SCENE_PURSUIT_RETURN_ALLOCATOR_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_ALLOCATOR_INCLUDED
#include "scene_pursuit_return_transaction.inc"
#include "rf/prepare_memory.h"
/* Uncalled adapter for the pinned stock32 NXDK PDCLib dlmalloc-2.8.6 build.
 * Paired calloc/free, without an invented rf_alloc API or allocator tuning.
 * Source: NXDK14d5ee97 malloc.c SHA2562671f19838010ef0d1bce228dd6157e1d287ae453f25077d20a8eb585c3fee7c;
 * winapi/sysinfo.c SHA256969b3ae253dbf1c3b6cf25759b18ca9d19e483817b7efeee9f4b2561bcebd004.
 * Audited defaults: 32-bit size/pointers, alignment8, no footers, page4096
 * and arena granularity65536. NXDK undefines WIN32; DEFAULT_GRANULARITY is
 * 64KiB (also confirmed in pinned malloc.obj), not GetSystemInfo's4096.
 * request2size, SYS_ALLOC_PADDING and mmap_alloc
 * require less than128 bytes beyond the requested block. Charge each block
 * as a fresh arena rounded conservatively to64KiB, even when heap storage is
 * reused. This covers chunk headers, minimum unsplit remainder, fenceposts,
 * segment storage and arena rounding. No reclaim credit is taken on failure.
 * Kernel bookkeeping and unrelated retained heap fragmentation remain in the
 * existing platform reserve, not a claim of exact process physical footprint.
 * Re-audit if toolchain/configuration changes or mallopt is introduced.
 * This does not establish the driver's separate overlapping scratch allowance.
 */
static int scene_pursuit_return_allocator_charge(void *context,size_t bytes,uint32_t *out)
{
    uint64_t charged;
    if(context||!out||!bytes||sizeof(void*)!=4||sizeof(size_t)!=4)return RF_RANGE;
    if(bytes>2u*1024u*1024u)return RF_RANGE;
    charged=((uint64_t)bytes+128u+65535u)&~UINT64_C(65535);
    if(charged>UINT32_MAX||charged>2u*1024u*1024u)return RF_RANGE;
    *out=(uint32_t)charged;return RF_OK;
}
static void *scene_pursuit_return_allocator_allocate(void *context,size_t bytes)
{
    uint32_t charged;
    if(scene_pursuit_return_allocator_charge(context,bytes,&charged))return NULL;
    return calloc(1,bytes);
}
static void scene_pursuit_return_allocator_release(void *context,void *p)
{
    (void)context;free(p);
}
/* No portable fallback: this charge contract is for the audited Xbox build. */
static inline int scene_pursuit_return_allocator_bind(scene_return_transaction_allocator *out)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    scene_return_transaction_allocator value={0};
    if(!out||sizeof(void*)!=4||sizeof(size_t)!=4)return RF_RANGE;
    value.charge=scene_pursuit_return_allocator_charge;
    value.allocate=scene_pursuit_return_allocator_allocate;
    value.release=scene_pursuit_return_allocator_release;
    *out=value;return RF_OK;
#else
    (void)out;return RF_NOT_FOUND;
#endif
}
/* Optional cumulative admission for the private combined-driver allocations.
 * UNCALLED, UNCOMPILED. Caller owns and pins this scope through final disposal.
 * Planning charge is pure: it neither admits nor reserves a request. Only an
 * actual allocate attempt admits one retained request before calloc. Failures
 * and frees never refund that cumulative charge. Admission is synchronous and
 * must obey rf_prepare_memory's exclusive accounting-only callback contract.
 * This bridge does not authenticate completeness of the outside heap ledger.
 * Snapshot outside cumulative heap BEFORE wrapper creation, with stack charged
 * separately exactly once. Do not feed a later mutated total back to planning.
 */
typedef struct scene_return_allocator_memory {
    rf_prepare_memory memory;
} scene_return_allocator_memory;
static int scene_pursuit_return_accounted_charge(void *context,size_t bytes,uint32_t *out)
{
    const scene_return_allocator_memory *scope=context;
    if(!scope||!scope->memory.account)return RF_RANGE;
    return scene_pursuit_return_allocator_charge(NULL,bytes,out);
}
static void *scene_pursuit_return_accounted_allocate(void *context,size_t bytes)
{
    scene_return_allocator_memory *scope=context;uint32_t charged;
    if(scene_pursuit_return_accounted_charge(context,bytes,&charged))return NULL;
    if(rf_prepare_memory_admit(&scope->memory,NULL,bytes))return NULL;
    return calloc(1,bytes);
}
/* Scope is initialized by the caller before binding and must not be changed
 * or moved until every object retaining the allocator has been disposed. */
static int scene_pursuit_return_allocator_bind_accounted(
    scene_return_transaction_allocator *out,scene_return_allocator_memory *scope)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    scene_return_transaction_allocator value={0};
    if(!out||!scope||!scope->memory.account||sizeof(void*)!=4||sizeof(size_t)!=4)
        return RF_RANGE;
    value.charge=scene_pursuit_return_accounted_charge;
    value.allocate=scene_pursuit_return_accounted_allocate;
    value.release=scene_pursuit_return_allocator_release;
    value.context=scope;*out=value;return RF_OK;
#else
    (void)out;(void)scope;return RF_NOT_FOUND;
#endif
}
/* Uncalled, uncompiled caller-owned admission ledger; no allocation or free.
 * Initialize once before allocations, with an authenticated full stack reserve.
 * Never reset during the transaction, even with no live slots. Caller
 * authenticates actual storage extents and exclusive ownership of all fields.
 * Each slot represents ONE actual allocation request, never an aggregate.
 * Acquire before allocation. Each admitted attempt permanently spends its
 * fresh-arena charge, even if allocation fails or is abandoned. Release clears
 * only live accounting: after allocation failure, or after matching free.
 * Neither free nor failure refunds admission or assumes reuse/reclaimed pages.
 * Peak is the largest simultaneous live/reserved charge plus stack; it never
 * decreases and includes failed attempts. Tags are unique pinned owner
 * identities chosen before allocation, not unallocated payload addresses.
 * Caller supplies sufficient slots and exclusive access through final release.
 * Account for ledger/slot storage too: stack-backed storage is already in the
 * one-time stack reservation; heap-backed storage needs its own actual request.
 * Retained/transient totals describe currently live or reserved heap requests.
 * admitted_bytes is cumulative heap charge, separate from that live peak.
 * Admission compares cumulative charge plus the one-time full stack against
 * the same unchanged 2 MiB maximum, without credit for released slots.
 * This does not establish compiled stack fit or process physical footprint.
 */
enum { SCENE_RETURN_LEDGER_RETAINED=1,SCENE_RETURN_LEDGER_TRANSIENT=2 };
typedef struct scene_return_memory_slot {
    const void *owner;size_t request_bytes;uint32_t charged_bytes,kind;
} scene_return_memory_slot;
typedef struct scene_return_memory_ledger {
    scene_return_memory_slot *slots;
    uint32_t slot_count,budget,stack_bytes,retained_bytes,transient_bytes,peak_bytes;
    uint32_t admitted_bytes; /* Monotonic heap charge; excludes stack. */
} scene_return_memory_ledger;
/* Check representable, disjoint ranges; actual capacity is caller-owned. */
static int scene_return_memory_storage(const scene_return_memory_ledger *ledger,
    const scene_return_memory_slot *slots,uint32_t count)
{
    uintptr_t a=(uintptr_t)ledger,b=(uintptr_t)slots;
    uint64_t bytes=(uint64_t)count*sizeof(*slots);
    if(!ledger||!slots||!count||sizeof(*ledger)>UINTPTR_MAX-a||
       bytes>SIZE_MAX||bytes>UINTPTR_MAX-b)return RF_RANGE;
    if(a<b+bytes&&b<a+sizeof(*ledger))return RF_RANGE;
    return RF_OK;
}
static int scene_return_memory_initialize(scene_return_memory_ledger *ledger,
    scene_return_memory_slot *slots,uint32_t count,uint32_t budget,uint32_t stack_bytes)
{
    scene_return_memory_ledger value={0};int status;
    if(sizeof(void*)!=4||sizeof(size_t)!=4||!stack_bytes||
       budget>2u*1024u*1024u||stack_bytes>budget)return RF_RANGE;
    status=scene_return_memory_storage(ledger,slots,count);if(status)return status;
    value.slots=slots;value.slot_count=count;value.budget=budget;
    value.stack_bytes=stack_bytes;value.peak_bytes=stack_bytes;
    memset(slots,0,(size_t)count*sizeof(*slots));*ledger=value;return RF_OK;
}
/* Recompute bounded live totals; invalid state never earns release credit. */
static int scene_return_memory_validate(const scene_return_memory_ledger *ledger)
{
    uint64_t retained=0,transient=0,total;uint32_t i,j,charge;int status;
    if(!ledger)return RF_RANGE;
    status=scene_return_memory_storage(ledger,ledger->slots,ledger->slot_count);
    if(status)return status;
    if(!ledger->stack_bytes||ledger->budget>2u*1024u*1024u||
       ledger->stack_bytes>ledger->budget)return RF_FORMAT;
    for(i=0;i<ledger->slot_count;i++){
        const scene_return_memory_slot *slot=ledger->slots+i;
        if(!slot->owner){
            if(slot->request_bytes||slot->charged_bytes||slot->kind)return RF_FORMAT;
            continue;
        }
        status=scene_pursuit_return_allocator_charge(NULL,slot->request_bytes,&charge);
        if(status)return status;
        if(charge!=slot->charged_bytes)return RF_FORMAT;
        for(j=0;j<i;j++)if(ledger->slots[j].owner==slot->owner)return RF_FORMAT;
        if(slot->kind==SCENE_RETURN_LEDGER_RETAINED)retained+=charge;
        else if(slot->kind==SCENE_RETURN_LEDGER_TRANSIENT)transient+=charge;
        else return RF_FORMAT;
    }
    total=retained+transient+ledger->stack_bytes;
    if(retained!=ledger->retained_bytes||transient!=ledger->transient_bytes||
       total>ledger->peak_bytes||
       ledger->peak_bytes>(uint64_t)ledger->stack_bytes+ledger->admitted_bytes||
       (uint64_t)ledger->stack_bytes+ledger->admitted_bytes>ledger->budget)return RF_FORMAT;
    return RF_OK;
}
/* On every rejection, ledger and all caller slots remain byte-for-byte intact. */
static int scene_return_memory_acquire(scene_return_memory_ledger *ledger,
    uint32_t index,const void *owner,size_t request_bytes,uint32_t kind)
{
    scene_return_memory_slot value={0};uint64_t total,admitted;uint32_t i,charge;int status;
    status=scene_return_memory_validate(ledger);if(status)return status;
    if(!owner||index>=ledger->slot_count||
       (kind!=SCENE_RETURN_LEDGER_RETAINED&&kind!=SCENE_RETURN_LEDGER_TRANSIENT))return RF_RANGE;
    if(ledger->slots[index].owner)return RF_FORMAT;
    for(i=0;i<ledger->slot_count;i++)if(ledger->slots[i].owner==owner)return RF_FORMAT;
    status=scene_pursuit_return_allocator_charge(NULL,request_bytes,&charge);if(status)return status;
    total=(uint64_t)ledger->stack_bytes+ledger->retained_bytes+ledger->transient_bytes+charge;
    admitted=(uint64_t)ledger->admitted_bytes+charge;
    if(admitted+ledger->stack_bytes>ledger->budget)return RF_RANGE;
    value.owner=owner;value.request_bytes=request_bytes;value.charged_bytes=charge;value.kind=kind;
    ledger->slots[index]=value;ledger->admitted_bytes=(uint32_t)admitted;
    if(kind==SCENE_RETURN_LEDGER_RETAINED)ledger->retained_bytes+=charge;
    else ledger->transient_bytes+=charge;
    if(total>ledger->peak_bytes)ledger->peak_bytes=(uint32_t)total;
    return RF_OK;
}
static int scene_return_memory_release(scene_return_memory_ledger *ledger,
    uint32_t index,const void *owner)
{
    scene_return_memory_slot *slot;int status;
    status=scene_return_memory_validate(ledger);if(status)return status;
    if(!owner||index>=ledger->slot_count)return RF_RANGE;
    slot=ledger->slots+index;if(slot->owner!=owner)return RF_FORMAT;
    if(slot->kind==SCENE_RETURN_LEDGER_RETAINED)ledger->retained_bytes-=slot->charged_bytes;
    else ledger->transient_bytes-=slot->charged_bytes;
    memset(slot,0,sizeof(*slot));return RF_OK;
}
#endif
