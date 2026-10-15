#ifndef RF_SCENE_WORLD_STAGE_ARENA_INCLUDED
#define RF_SCENE_WORLD_STAGE_ARENA_INCLUDED
#include "rf/prepare_storage.h"
#include <string.h>
/* Only temporary prepare-stage storage. No global scope, pointer interception,
 * slice reuse or admission refunds. Every backing request is charged once by
 * the unchanged native ledger before calloc. Dispose after every borrower.
 */
typedef struct scene_world_stage_chunk {
    struct scene_world_stage_chunk *next;
    size_t raw_bytes,used;
} scene_world_stage_chunk;
typedef struct scene_world_stage_arena {
    scene_world_stage_chunk *chunks;
    const rf_prepare_memory *memory;
} scene_world_stage_arena;
static int scene_world_stage_arena_allocate(void *context,size_t bytes,void **out)
{
    scene_world_stage_arena *arena=context;scene_world_stage_chunk *chunk;
    size_t aligned,header,required,raw;uint32_t charge;int status;
    if(!out||*out)return RF_RANGE;
    if(!arena||!arena->memory||!arena->memory->account||!bytes||bytes>SIZE_MAX-7u)
        return RF_RANGE;
    aligned=(bytes+7u)&~(size_t)7u;
    header=(sizeof(scene_world_stage_chunk)+7u)&~(size_t)7u;
    if(aligned>SIZE_MAX-header)return RF_RANGE;
    required=header+aligned;
    for(chunk=arena->chunks;chunk;chunk=chunk->next){
        if(chunk->used<header||chunk->used>chunk->raw_bytes||(chunk->used&7u))return RF_FORMAT;
        if(aligned<=chunk->raw_bytes-chunk->used){
            *out=(unsigned char*)chunk+chunk->used;memset(*out,0,bytes);chunk->used+=aligned;
            return RF_OK;
        }
    }
    status=scene_pursuit_return_allocator_charge(NULL,required,&charge);if(status)return status;
    if(charge<128u)return RF_FORMAT;
    raw=(size_t)charge-128u;
    if(required>raw)return RF_FORMAT;
    status=rf_prepare_memory_admit(arena->memory,NULL,raw);if(status)return status;
    chunk=calloc(1,raw);if(!chunk)return RF_IO;
    chunk->raw_bytes=raw;chunk->used=required;
    chunk->next=arena->chunks;arena->chunks=chunk;
    *out=(unsigned char*)chunk+header;return RF_OK;
}
static void scene_world_stage_arena_release(void *context,void *pointer)
{
    /* Caller supplies only slices obtained from this pinned arena. Retaining
     * the entire backing through terminal disposal prevents stale borrowers. */
    (void)context;(void)pointer;
}
static void scene_world_stage_arena_dispose(scene_world_stage_arena *arena)
{
    scene_world_stage_chunk *chunk,*next;
    if(!arena)return;
    for(chunk=arena->chunks;chunk;chunk=next){next=chunk->next;free(chunk);}
    arena->chunks=NULL;
}
#endif
