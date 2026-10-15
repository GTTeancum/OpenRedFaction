#ifndef RF_SCENE_RETURN_LATE_PLAN_INCLUDED
#define RF_SCENE_RETURN_LATE_PLAN_INCLUDED
/* UNCALLED. Requires the staged owner authority, binding adapter and v2
 * crouched companion installed under their canonical include names. */
#include "scene_return_crouch_binding.inc"

/* Metadata is parent-derived, never an invented staged-return representation.
 * One metadata row per stage index. OWNER covers outgoing OR incoming return
 * owners, including retirement/death transitions. CROUCH requests an existing
 * shared crouch snapshot even when the row is not an incoming target.
 * TARGET describes incoming target dependencies only.
 * shared_extra_bytes is exact additional backing for that shared union row,
 * supplied from the parent's concrete owner/crouch layout (zero if inline).
 * Registration, health and full staged-return admission remain parent checks. */
enum { SCENE_RETURN_PLAN_OWNER=1,SCENE_RETURN_PLAN_TARGET=2,SCENE_RETURN_PLAN_CROUCH=4 };
typedef struct scene_return_plan_row {
    uint32_t stage_index,slot,uid,class_id,handle,flags,cache_bones,shared_extra_bytes;
} scene_return_plan_row;
/* Indices address metadata, not stage slots. UINT32_MAX means player target.
 * Duplicate identical pairs are one relation; different pairs remain distinct.
 * An outgoing owner alone creates no incoming target dependency. */
typedef struct scene_return_plan_relation {uint32_t owner,target;} scene_return_plan_relation;
enum { SCENE_RETURN_PLAN_LEGACY=0,SCENE_RETURN_PLAN_UNION=1 };
typedef struct scene_return_plan_layout {
    /* Zero initialization preserves legacy mode and grants no authority.
     * UNION requires actual driver composition before union_authority is set.
     * It ignores no caller charges: legacy shared sizes/extras must be zero. */
    uint32_t mode,union_authority;
    uint32_t shared_header_bytes,shared_row_bytes;
    uint32_t relation_header_bytes,relation_row_bytes,player_bytes;
    /* Parent promises its shared owner authority seals the complete ordinary
     * incoming target projection when OWNER and TARGET overlap without CROUCH.
     * Without that implementation, refuse rather than duplicate actor storage. */
    uint32_t owner_target_authority;
} scene_return_plan_layout;
/* Each capacity is the ACTUAL retained allocation length in uint32_t elements.
 * Arrays must be mutually disjoint and separate from all inputs and result.
 * Planner charges full capacities, not merely populated prefixes.
 * Written prefixes contain metadata indices, except relations contains input
 * relation indices. Unused tails are left unchanged. Zero capacity uses NULL. */
typedef struct scene_return_plan_lists {
    uint32_t *owners,*shared,*ordinary,*crouched,*owner_targets,*relations;
    /* owner_relations parallels owners: first input edge, or UINT32_MAX for
     * outgoing-only. Each owner still gets one parent-defined relation receipt.
     * crouched_shared parallels crouched ONLY in UNION mode and gives the row
     * index inside the shared owner authority (not a metadata/stage index). */
    uint32_t *owner_relations,*crouched_shared;
    uint32_t owners_capacity,shared_capacity,ordinary_capacity,crouched_capacity;
    uint32_t owner_targets_capacity,relations_capacity;
    uint32_t owner_relations_capacity,crouched_shared_capacity;
} scene_return_plan_lists;
typedef struct scene_return_late_plan {
    uint32_t mode,relation_receipts,union_bindings;
    uint32_t owners,shared,ordinary,crouched,owner_targets,relations,player;
    uint32_t cache_bones,shared_bytes,shared_deep_bytes,ordinary_bytes,crouched_bytes;
    uint32_t relation_bytes,player_bytes,planner_bytes,additional_bytes,total_bytes;
} scene_return_late_plan;

/* Validate output allocation ranges before writing any prefix. */
static inline int scene_return_plan_output(const scene_return_plan_lists *lists,
    const scene_return_plan_row *rows,uint32_t row_count,
    const scene_return_plan_relation *relations,uint32_t relation_count,
    const scene_return_plan_layout *layout,scene_return_late_plan *out)
{
    const void *dest[9]={out,lists->owners,lists->shared,lists->ordinary,
        lists->crouched,lists->owner_targets,lists->relations,
        lists->owner_relations,lists->crouched_shared};
    uint64_t bytes[9]={sizeof(*out),(uint64_t)lists->owners_capacity*4,
        (uint64_t)lists->shared_capacity*4,(uint64_t)lists->ordinary_capacity*4,
        (uint64_t)lists->crouched_capacity*4,(uint64_t)lists->owner_targets_capacity*4,
        (uint64_t)lists->relations_capacity*4,(uint64_t)lists->owner_relations_capacity*4,
        (uint64_t)lists->crouched_shared_capacity*4};
    int status;
    for(uint32_t i=0;i<9;i++){
        if(!!dest[i]!=!!bytes[i]||bytes[i]>SIZE_MAX)return RF_RANGE;
        if(!bytes[i])continue;
#define RETURN_PLAN_DISJOINT(p,n) do { \
    if((uint64_t)(n)>SIZE_MAX)return RF_RANGE; \
    status=scene_return_target_snapshot_disjoint(dest[i],(size_t)bytes[i],(p),(size_t)(n)); \
    if(status)return status; \
} while(0)
        RETURN_PLAN_DISJOINT(rows,(uint64_t)row_count*sizeof(*rows));
        RETURN_PLAN_DISJOINT(relations,(uint64_t)relation_count*sizeof(*relations));
        RETURN_PLAN_DISJOINT(layout,sizeof(*layout));
        RETURN_PLAN_DISJOINT(lists,sizeof(*lists));
        for(uint32_t j=0;j<i;j++)RETURN_PLAN_DISJOINT(dest[j],bytes[j]);
#undef RETURN_PLAN_DISJOINT
    }
    return RF_OK;
}
static inline int scene_return_plan_first_relation(
    const scene_return_plan_relation *relations,uint32_t at)
{
    for(uint32_t j=0;j<at;j++)
        if(relations[j].owner==relations[at].owner&&relations[j].target==relations[at].target)
            return 0;
    return 1;
}

/* Two passes: validate/size locally, then fill lists and publish result.
 * No output changes on error. Caller retains immutable inputs through both
 * passes, authenticates every supplied allocation, and excludes concurrency.
 * existing_bytes includes already-reserved stage/publication/crouch-independent
 * storage, input metadata and allocation overhead. Do not count the new shared
 * union, adapters, relations or output lists there a second time.
 * budget is the complete unchanged world-load allowance, at most 2 MiB.
 * LEGACY uses supplied shared row/header/extras plus old v2 companions/cache.
 * UNION uses sizeof the concrete owner root/rows, and union-only binding costs.
 * All OWNER or CROUCH rows appear once in shared; that list is owner-row order.
 * All shared rows also marked TARGET must be prepared with owner target=1.
 * Only targets absent from shared get the ordinary target adapter.
 * In UNION, crouched_bytes contains bindings only; cache_bones is diagnostic
 * metadata, not another charge (cache storage is inline in the owner rows).
 * Every OWNER has a relation receipt, including owners with no incoming edge.
 * relation_row/header and player_bytes remain actual parent-supplied sizeofs.
 * Existing shared allocations must be charged here, not also in existing_bytes.
 * No subtraction credit is granted for unproven reuse.
 * Zero rows/targets and player-only projections are supported. */
static inline int scene_return_late_plan_prepare(
    const scene_return_plan_row *rows,uint32_t row_count,uint32_t stage_count,
    const scene_return_plan_relation *relations,uint32_t relation_count,
    uint32_t player_projection,const scene_return_plan_layout *layout,
    uint32_t existing_bytes,uint32_t budget,const scene_return_plan_lists *lists,
    scene_return_late_plan *out)
{
    scene_return_late_plan plan={0};
    uint64_t shared_bytes,shared_deep=0,relation_bytes,planner_bytes,total;
    uint32_t i,j,flags,concrete_shared=0;int status;
    if(!layout||!lists||!out||(row_count&&!rows)||(relation_count&&!relations)||
       row_count>RF_NPC_CHECKPOINT_MAX_COUNT||stage_count>RF_NPC_CHECKPOINT_MAX_COUNT||
       row_count>stage_count||relation_count>RF_NPC_CHECKPOINT_MAX_COUNT||
       player_projection>1||layout->owner_target_authority>1||
       layout->mode>SCENE_RETURN_PLAN_UNION||layout->union_authority>1||
       budget>2u*1024u*1024u||existing_bytes>budget)return RF_RANGE;
    status=scene_return_plan_output(lists,rows,row_count,relations,relation_count,layout,out);
    if(status)return status;
    plan.mode=layout->mode;
    if(plan.mode==SCENE_RETURN_PLAN_UNION&&
       (layout->shared_header_bytes||layout->shared_row_bytes))return RF_RANGE;
    for(i=0;i<row_count;i++){
        const scene_return_plan_row *r=rows+i;
        flags=r->flags;
        if(flags&~7u||r->stage_index>=stage_count||r->cache_bones>50||
           (plan.mode==SCENE_RETURN_PLAN_UNION&&r->shared_extra_bytes))return RF_FORMAT;
        for(j=0;j<i;j++){
            if(rows[j].stage_index==r->stage_index||rows[j].slot==r->slot||
               rows[j].uid==r->uid)return RF_FORMAT;
            if(r->handle&&r->handle!=UINT32_MAX&&rows[j].handle==r->handle)return RF_FORMAT;
        }
        if(flags&SCENE_RETURN_PLAN_OWNER)++plan.owners;
        if(flags&(SCENE_RETURN_PLAN_OWNER|SCENE_RETURN_PLAN_CROUCH)){
            ++plan.shared;shared_deep+=r->shared_extra_bytes;
        }else if(r->shared_extra_bytes)return RF_FORMAT;
        if(!(flags&SCENE_RETURN_PLAN_TARGET))continue;
        if(!r->handle||r->handle==UINT32_MAX)return RF_FORMAT;
        /* Every selected target must have an actual incoming relation. */
        for(j=0;j<relation_count;j++)if(relations[j].target==i)break;
        if(j==relation_count)return RF_FORMAT;
        if(plan.mode==SCENE_RETURN_PLAN_UNION&&
           (flags&(SCENE_RETURN_PLAN_OWNER|SCENE_RETURN_PLAN_CROUCH))&&
           !layout->owner_target_authority)return RF_NOT_FOUND;
        if(flags&SCENE_RETURN_PLAN_CROUCH){
            ++plan.crouched;plan.cache_bones+=r->cache_bones;
        }else if(flags&SCENE_RETURN_PLAN_OWNER){
            if(!layout->owner_target_authority)return RF_NOT_FOUND;
            ++plan.owner_targets;
        }else ++plan.ordinary;
    }
    plan.player=player_projection;
    for(i=0;i<relation_count;i++){
        const scene_return_plan_relation *r=relations+i;
        if(r->owner>=row_count||!(rows[r->owner].flags&SCENE_RETURN_PLAN_OWNER))
            return RF_FORMAT;
        if(r->target==UINT32_MAX)plan.player=1;
        else if(r->target>=row_count||r->target==r->owner||
                !(rows[r->target].flags&SCENE_RETURN_PLAN_TARGET))return RF_FORMAT;
        /* One prepared return receipt has exactly one incoming target.
         * Repeated identical edges may share storage; contradictory edges
         * must not manufacture extra target authority from planner metadata. */
        for(j=0;j<i;j++)
            if(relations[j].owner==r->owner&&relations[j].target!=r->target)
                return RF_FORMAT;
        if(scene_return_plan_first_relation(relations,i))++plan.relations;
    }
    plan.relation_receipts=plan.owners;
    plan.union_bindings=plan.mode==SCENE_RETURN_PLAN_UNION?plan.crouched:0;
    if(plan.mode==SCENE_RETURN_PLAN_UNION&&plan.shared&&!layout->union_authority)
        return RF_NOT_FOUND;
    if(plan.owners>lists->owners_capacity||plan.shared>lists->shared_capacity||
       plan.ordinary>lists->ordinary_capacity||plan.crouched>lists->crouched_capacity||
       plan.owner_targets>lists->owner_targets_capacity||
       plan.relations>lists->relations_capacity||
       plan.relation_receipts>lists->owner_relations_capacity||
       plan.union_bindings>lists->crouched_shared_capacity)return RF_RANGE;
    if((plan.mode==SCENE_RETURN_PLAN_LEGACY&&plan.shared&&
        (!layout->shared_header_bytes||!layout->shared_row_bytes))||
       (plan.relation_receipts&&!layout->relation_row_bytes)||
       (plan.player&&!layout->player_bytes))return RF_RANGE;
    if(plan.mode==SCENE_RETURN_PLAN_UNION){
        if(plan.shared){
            status=scene_return_owner_bytes(plan.shared,budget,&concrete_shared);
            if(status)return status;
        }
        shared_bytes=concrete_shared;
    }else shared_bytes=plan.shared?(uint64_t)layout->shared_header_bytes+
        (uint64_t)plan.shared*layout->shared_row_bytes+shared_deep:0;
    relation_bytes=plan.relation_receipts?(uint64_t)layout->relation_header_bytes+
        (uint64_t)plan.relation_receipts*layout->relation_row_bytes:0;
    if(plan.ordinary){
        status=scene_return_target_publication_bytes(plan.ordinary,budget,&plan.ordinary_bytes);
        if(status)return status;
    }
    if(plan.crouched){
        if(plan.mode==SCENE_RETURN_PLAN_UNION)
            status=scene_return_crouch_binding_bytes(plan.crouched,0,0,budget,&plan.crouched_bytes);
        else status=scene_return_crouch_bytes(plan.crouched,plan.cache_bones,budget,&plan.crouched_bytes);
        if(status)return status;
    }
    /* List descriptor is retained too; caller need not retain input metadata
     * after construction if parent relation records contain their own identity. */
    planner_bytes=sizeof(plan)+sizeof(*lists)+4*((uint64_t)lists->owners_capacity+
        lists->shared_capacity+lists->ordinary_capacity+lists->crouched_capacity+
        lists->owner_targets_capacity+lists->relations_capacity+
        lists->owner_relations_capacity+lists->crouched_shared_capacity);
    if(shared_bytes>UINT32_MAX||relation_bytes>UINT32_MAX||planner_bytes>UINT32_MAX)
        return RF_RANGE;
    plan.shared_bytes=(uint32_t)shared_bytes;plan.shared_deep_bytes=(uint32_t)shared_deep;
    plan.relation_bytes=(uint32_t)relation_bytes;
    plan.planner_bytes=(uint32_t)planner_bytes;plan.player_bytes=plan.player?layout->player_bytes:0;
    total=(uint64_t)existing_bytes+shared_bytes+relation_bytes+planner_bytes+
        plan.ordinary_bytes+plan.crouched_bytes+plan.player_bytes;
    if(total>budget||total>UINT32_MAX||total>SIZE_MAX)return RF_RANGE;
    plan.total_bytes=(uint32_t)total;plan.additional_bytes=(uint32_t)(total-existing_bytes);

    /* Commit. No callback, allocation, validation or failure beyond here. */
    {
        uint32_t owners=0,shared=0,ordinary=0,crouched=0,owner_targets=0,relation=0;
        for(i=0;i<row_count;i++){
            flags=rows[i].flags;
            if(flags&SCENE_RETURN_PLAN_OWNER){
                lists->owners[owners]=i;
                for(j=0;j<relation_count;j++)if(relations[j].owner==i)break;
                lists->owner_relations[owners++]=j==relation_count?UINT32_MAX:j;
            }
            if(flags&(SCENE_RETURN_PLAN_OWNER|SCENE_RETURN_PLAN_CROUCH))lists->shared[shared++]=i;
            if(!(flags&SCENE_RETURN_PLAN_TARGET))continue;
            if(flags&SCENE_RETURN_PLAN_CROUCH){
                if(plan.mode==SCENE_RETURN_PLAN_UNION)lists->crouched_shared[crouched]=shared-1u;
                lists->crouched[crouched++]=i;
            }
            else if(flags&SCENE_RETURN_PLAN_OWNER)lists->owner_targets[owner_targets++]=i;
            else lists->ordinary[ordinary++]=i;
        }
        for(i=0;i<relation_count;i++)if(scene_return_plan_first_relation(relations,i))
            lists->relations[relation++]=i;
    }
    memcpy(out,&plan,sizeof(plan));return RF_OK;
}
#endif
