#ifndef RF_SCENE_PURSUIT_RETURN_TRANSACTION_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_TRANSACTION_INCLUDED
#include "scene_pursuit_return_plan_binding.inc"
#include "scene_pursuit_return_player_projection.inc"
/* UNCALLED, UNCOMPILED. Outer owner authenticates all stage/world lifetimes,
 * holds the scene exclusive, and never calls this after early storage close.
 * Publication is already prepared/sealed before create. No gameplay admission,
 * resource rebase, native close, finish or retirement is performed here.
 * Allocator bridge must use the parent's real paired allocation API; charge
 * returns the COMPLETE reserved charge including headers/rounding, BEFORE
 * allocation. It is pure/deterministic; allocate has no scene side effects,
 * returns fresh suitably aligned storage, or NULL. No zero-overhead default.
 * Available base used calloc/free, not rf_alloc/rf_free: parent binds its actual
 * allocator. All driver storage stays private, stable and immutable except
 * through these functions. No record containing self pointers is copied.
 * existing_bytes includes all outside retained allocations/input metadata and
 * overhead. scratch_bytes is the parent's verified overlapping call-stack
 * allowance, including nested helpers; the lower bound below is ONLY known
 * planner scratch, not a claim of complete/compiled stack accounting. */
typedef struct scene_return_transaction_allocator {
    int (*charge)(void *,size_t,uint32_t *);
    void *(*allocate)(void *,size_t);
    void (*release)(void *,void *);
    void *context;
} scene_return_transaction_allocator;
enum { SCENE_RETURN_TX_FAILED=1,SCENE_RETURN_TX_PREPARED=2,
    SCENE_RETURN_TX_ARMED=3,SCENE_RETURN_TX_CAPTURED=4,SCENE_RETURN_TX_SEALED=5 };
typedef struct scene_return_transaction {
    const struct scene_return_transaction *self;
    scene_return_transaction_allocator allocator;
    uint32_t state,count,edges,stage_bytes,publication_bytes,gunner,turret;
    uint32_t budget,existing_bytes,scratch_bytes,first_charge,payload_charge,total_bytes;
    const scene_stream *stream;
    const scene_world_player_stage *player;
    const scene_npc_checkpoint_restore_stage *stage;
    const scene_npc_reload_publication_receipt *publication;
    scene_return_late_plan plan;
    scene_return_plan_lists lists;
    scene_return_plan_layout layout;
    scene_return_plan_row *metadata;
    /* Boundary phases use these sequentially; no scratch address escapes. */
    union {
        scene_return_plan_row row;
        scene_return_crouch_join entry;
        scene_pursuit_return_player_projection player;
    } scratch;
    scene_return_plan_relation *edge;
    uint32_t *targets,*deferred;
    scene_return_crouch_join *joins;
    void *payload;
    scene_return_owner_late *owner;
    scene_return_owner_row *owner_rows;
    scene_return_target_publication *ordinary;
    scene_return_target_publication_row *ordinary_rows;
    scene_return_crouch_binding *bindings;
    scene_pursuit_return_relation *relations;
    scene_pursuit_return_player_projection *player_projection;
} scene_return_transaction;
/* Offsets are plain values, never receipt copies. Padding is charged. */
typedef struct scene_return_transaction_offsets {
    uint32_t metadata,edge,lists[8],targets,deferred,joins;
    uint32_t owner,owner_rows,ordinary,ordinary_rows,bindings,relations,player,bytes;
} scene_return_transaction_offsets;
static int scene_return_transaction_reserve(uint64_t *at,size_t align,
    uint64_t bytes,uint32_t *offset)
{
    uint64_t n;
    if(!at||!offset||!align)return RF_RANGE;
    if(!bytes){*offset=0;return RF_OK;}
    n=*at+((align-*at%align)%align);
    if(n>UINT32_MAX||bytes>UINT32_MAX-n||n+bytes>SIZE_MAX)return RF_RANGE;
    *offset=(uint32_t)n;*at=n+bytes;return RF_OK;
}
static int scene_return_transaction_shape(uint32_t n,const scene_return_late_plan *p,
    scene_return_transaction_offsets *o)
{
    uint64_t at=p?0:sizeof(scene_return_transaction);int status;
    memset(o,0,sizeof(*o));
#define TX_SPACE(field,type,count) do { \
    status=scene_return_transaction_reserve(&at,_Alignof(type), \
        (uint64_t)(count)*sizeof(type),&o->field);if(status)return status; \
} while(0)
    if(!p){
        TX_SPACE(metadata,scene_return_plan_row,n);TX_SPACE(edge,scene_return_plan_relation,n);
        for(uint32_t i=0;i<8;i++){TX_SPACE(lists[i],uint32_t,n);}
        TX_SPACE(targets,uint32_t,n);TX_SPACE(deferred,uint32_t,n);
        TX_SPACE(joins,scene_return_crouch_join,n);
    }else{
        TX_SPACE(owner,scene_return_owner_late,p->shared?1:0);
        TX_SPACE(owner_rows,scene_return_owner_row,p->shared);
        TX_SPACE(ordinary,scene_return_target_publication,p->ordinary?1:0);
        TX_SPACE(ordinary_rows,scene_return_target_publication_row,p->ordinary);
        TX_SPACE(bindings,scene_return_crouch_binding,p->union_bindings);
        TX_SPACE(relations,scene_pursuit_return_relation,p->relation_receipts);
        TX_SPACE(player,scene_pursuit_return_player_projection,p->player);
    }
#undef TX_SPACE
    o->bytes=(uint32_t)at;return RF_OK;
}
static void *scene_return_transaction_at(void *base,uint32_t offset,uint32_t present)
{return present?(unsigned char*)base+offset:NULL;}
/* Disposal never follows stage/world pointers or runs participant finalizers.
 * Before root allocation *out is NULL; after any later failure it owns
 * a poisoned transaction. Caller disposes either form exactly once. */
static void scene_return_transaction_dispose(scene_return_transaction **out)
{
    scene_return_transaction *r;scene_return_transaction_allocator a;
    if(!out||!*out)return;
    r=*out;*out=NULL;a=r->allocator;
    if(r->payload)a.release(a.context,r->payload);
    a.release(a.context,r);
}
/* Re-project metadata/joins from actual retained sources at EVERY boundary.
 * The scratch entry tuple is independently projected from the stage for binds;
 * joins[] always comes from the parent's publication bridge. */
static int scene_return_transaction_refresh(scene_return_transaction *r)
{
    uint32_t i,player=0,any_player=0;int status;
    status=scene_npc_reload_publication_lifetime(r->stage,r->publication);if(status)return status;
    status=scene_return_target_publication_layout(r->stage,r->publication,
        r->stage_bytes,r->publication_bytes);if(status)return status;
    if(r->stage->count!=r->count)return RF_FORMAT;
    for(i=0;i<r->count;i++){
        status=scene_pursuit_return_plan_row(r->stage,i,campaign_player_object.handle,0,
            &r->scratch.row,&player);if(status)return status;
        if(memcmp(&r->scratch.row,r->metadata+i,sizeof(r->scratch.row)))return RF_FORMAT;
        any_player|=player;
    }
    if(any_player!=r->plan.player)return RF_FORMAT;
    for(i=0;i<r->plan.shared;i++){
        status=scene_pursuit_return_plan_join(r->stage,r->publication,r->stage_bytes,
            r->publication_bytes,r->lists.shared[i],r->joins+i);if(status)return status;
    }
    return RF_OK;
}
static int scene_return_transaction_binding(scene_return_transaction *r,uint32_t i,
    uint32_t prepare,uint32_t captured)
{
    uint32_t row=r->lists.crouched[i],shared=r->lists.crouched_shared[i];
    const scene_npc_checkpoint_restore_entry *e;int status;
    if(row>=r->count||shared>=r->plan.shared||r->lists.shared[shared]!=row)return RF_FORMAT;
    e=r->stage->entries+row;memset(&r->scratch.entry,0,sizeof(r->scratch.entry));
    r->scratch.entry.profile=e->saved.pursuit_return_profile;
    r->scratch.entry.phase=e->saved.pursuit_return_phase;
    r->scratch.entry.deferred=e->pursuit_return_retire_pending;
    memcpy(r->scratch.entry.prepared,&e->pursuit_return_prepared,sizeof(r->scratch.entry.prepared));
    if(prepare){
        scene_return_crouch_binding_initialize(r->bindings+i);
        status=scene_return_crouch_bind_union(r->bindings+i,r->owner,shared,
            &r->scratch.entry,r->joins+shared);
    }else status=scene_return_crouch_binding_check(r->bindings+i,captured,
        &r->scratch.entry,r->joins+shared);
    return status;
}
/* Two owned allocations: root+retained planning storage, then participant
 * storage. Their complete charges overlap until disposal. Planner's embedded
 * plan/list bytes and eight full-capacity lists are counted exactly once by
 * subtracting only their KNOWN in-block sizes from its existing_bytes input.
 * No old snapshot reuse credit or hypothetical freed storage is subtracted. */
static int scene_return_transaction_create(
    const scene_stream *stream,const scene_world_player_stage *player,
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *publication,
    uint32_t stage_bytes,uint32_t publication_bytes,uint32_t gunner,uint32_t turret,
    uint32_t existing_bytes,uint32_t scratch_bytes,uint32_t budget,
    const scene_return_transaction_allocator *allocator,scene_return_transaction **out)
{
    scene_return_transaction_offsets first,payload;
    scene_return_transaction *r;uint32_t charge,n,i,needs_player=0,any_player=0;
    uint64_t known_planner,base,total,minimum_scratch;int status;
    if(!out||*out||!allocator||!allocator->charge||!allocator->allocate||!allocator->release||
       !stream||!stage||!publication||gunner>1||turret>1||
       budget>2u*1024u*1024u||existing_bytes>budget)return RF_RANGE;
    minimum_scratch=sizeof(scene_return_late_plan)+9*(sizeof(void*)+sizeof(uint64_t))+
        2*sizeof(scene_return_transaction_offsets);
    if(scratch_bytes<minimum_scratch||(uint64_t)existing_bytes+scratch_bytes>budget)return RF_RANGE;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    status=scene_return_target_publication_layout(stage,publication,stage_bytes,publication_bytes);
    if(status)return status;
    if((uint64_t)stage->bytes+publication_bytes>existing_bytes)return RF_RANGE;
    n=stage->count;if(n>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    status=scene_return_transaction_shape(n,NULL,&first);if(status)return status;
    status=allocator->charge(allocator->context,first.bytes,&charge);if(status)return status;
    if(charge<first.bytes||(uint64_t)existing_bytes+scratch_bytes+charge>budget)return RF_RANGE;
    r=allocator->allocate(allocator->context,first.bytes);if(!r)return RF_IO;
    memset(r,0,first.bytes);r->self=r;r->allocator=*allocator;r->state=SCENE_RETURN_TX_FAILED;
    *out=r;r->count=n;r->first_charge=charge;r->budget=budget;
    r->existing_bytes=existing_bytes;r->scratch_bytes=scratch_bytes;
    r->stream=stream;r->player=player;r->stage=stage;r->publication=publication;
    r->stage_bytes=stage_bytes;r->publication_bytes=publication_bytes;r->gunner=gunner;r->turret=turret;
    r->metadata=scene_return_transaction_at(r,first.metadata,n);
    r->edge=scene_return_transaction_at(r,first.edge,n);
    r->targets=scene_return_transaction_at(r,first.targets,n);
    r->deferred=scene_return_transaction_at(r,first.deferred,n);
    r->joins=scene_return_transaction_at(r,first.joins,n);
#define TX_LIST(name,index) do { \
    r->lists.name=scene_return_transaction_at(r,first.lists[index],n); \
    r->lists.name##_capacity=n; \
} while(0)
    TX_LIST(owners,0);TX_LIST(shared,1);TX_LIST(ordinary,2);TX_LIST(crouched,3);
    TX_LIST(owner_targets,4);TX_LIST(relations,5);TX_LIST(owner_relations,6);TX_LIST(crouched_shared,7);
#undef TX_LIST
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    if(stage->count!=n)return RF_FORMAT;
    for(i=0;i<n;i++){
        status=scene_pursuit_return_plan_row(stage,i,campaign_player_object.handle,0,
            r->metadata+i,&needs_player);if(status)return status;
        any_player|=needs_player;
        if(!(r->metadata[i].flags&SCENE_RETURN_PLAN_OWNER))continue;
        status=scene_pursuit_return_plan_edge(stage,i,r->edge+r->edges);
        if(status==RF_NOT_FOUND)continue;
        if(status)return status;++r->edges;
    }
    r->layout.mode=SCENE_RETURN_PLAN_UNION;
    /* These promises are private to this concrete composition: all shared
     * targets go through owner prepare/capture/seal, including crouched ones. */
    r->layout.union_authority=1;r->layout.owner_target_authority=1;
    r->layout.relation_row_bytes=sizeof(scene_pursuit_return_relation);
    r->layout.player_bytes=sizeof(scene_pursuit_return_player_projection);
    known_planner=sizeof(r->plan)+sizeof(r->lists)+(uint64_t)8*n*sizeof(uint32_t);
    if(known_planner>first.bytes)return RF_FORMAT;
    base=(uint64_t)existing_bytes+scratch_bytes+charge-known_planner;
    if(base>budget)return RF_RANGE;
    status=scene_return_late_plan_prepare(r->metadata,n,n,r->edge,r->edges,any_player,
        &r->layout,(uint32_t)base,budget,&r->lists,&r->plan);if(status)return status;
    status=scene_return_transaction_shape(n,&r->plan,&payload);if(status)return status;
    if(payload.bytes){
        status=allocator->charge(allocator->context,payload.bytes,&charge);if(status)return status;
        if(charge<payload.bytes)return RF_RANGE;
    }else charge=0;
    total=(uint64_t)existing_bytes+scratch_bytes+r->first_charge+charge;
    if(total>budget||total<r->plan.total_bytes)return RF_RANGE;
    r->payload_charge=charge;r->total_bytes=(uint32_t)total;
    if(payload.bytes){
        r->payload=allocator->allocate(allocator->context,payload.bytes);
        if(!r->payload)return RF_IO;memset(r->payload,0,payload.bytes);
    }
#define TX_PTR(field,count) r->field=scene_return_transaction_at(r->payload,payload.field,count)
    TX_PTR(owner,r->plan.shared);TX_PTR(owner_rows,r->plan.shared);
    TX_PTR(ordinary,r->plan.ordinary);TX_PTR(ordinary_rows,r->plan.ordinary);
    TX_PTR(bindings,r->plan.union_bindings);TX_PTR(relations,r->plan.relation_receipts);
#undef TX_PTR
    r->player_projection=scene_return_transaction_at(r->payload,payload.player,r->plan.player);
    status=scene_return_transaction_refresh(r);if(status)return status;
    for(i=0;i<r->plan.shared;i++)
        r->targets[i]=!!(r->metadata[r->lists.shared[i]].flags&SCENE_RETURN_PLAN_TARGET);
    if(r->owner){
        scene_return_owner_initialize(r->owner);
        status=scene_return_owner_prepare(r->owner,stage,publication,stage_bytes,publication_bytes,
            r->owner_rows,r->plan.shared,r->lists.shared,r->targets,r->joins,budget);
        if(status)return status;
    }
    if(r->ordinary){
        scene_return_target_publication_initialize(r->ordinary);
        status=scene_return_target_publication_prepare(r->ordinary,stage,publication,
            stage_bytes,publication_bytes,r->ordinary_rows,r->plan.ordinary,
            r->lists.ordinary,r->deferred,campaign_identity[0],budget);if(status)return status;
    }
    for(i=0;i<r->plan.union_bindings;i++){
        status=scene_return_transaction_binding(r,i,1,0);if(status)return status;
    }
    for(i=0;i<r->plan.relation_receipts;i++){
        uint32_t row=r->lists.owners[i],edge=r->lists.owner_relations[i];
        status=scene_pursuit_return_relation_prepare(stage,row,r->relations+i);if(status)return status;
        if(edge==UINT32_MAX){if(r->relations[i].incoming.phase)return RF_FORMAT;}
        else if(edge>=r->edges||r->edge[edge].owner!=row||!r->relations[i].incoming.phase||
            r->edge[edge].target!=(r->relations[i].player?UINT32_MAX:r->relations[i].target_row))
            return RF_FORMAT;
    }
    if(r->player_projection){
        if(!player||player->npcs!=stage)return RF_FORMAT;
        status=scene_pursuit_return_player_projection_prepare(stream,player,gunner,turret,
            r->player_projection);if(status)return status;
    }
    r->state=SCENE_RETURN_TX_PREPARED;return RF_OK;
}
/* Boundary helpers poison first: no capture or resource-work retries after
 * partial effects. Caller authenticates r and outer storage before invoking.
 * arm is immediately before NPC assignment; capture immediately after that
 * callback-free assignment, before unrelated player/vehicle publication. */
static int scene_return_transaction_enter(scene_return_transaction *r,uint32_t expected)
{
    if(!r)return RF_RANGE;
    if(r->self!=r){r->state=SCENE_RETURN_TX_FAILED;return RF_FORMAT;}
    if(r->state!=expected){r->state=SCENE_RETURN_TX_FAILED;return RF_FORMAT;}
    r->state=SCENE_RETURN_TX_FAILED;return scene_return_transaction_refresh(r);
}
static int scene_return_transaction_arm(scene_return_transaction *r)
{
    int status=scene_return_transaction_enter(r,SCENE_RETURN_TX_PREPARED);if(status)return status;
    for(uint32_t i=0;i<r->plan.relation_receipts;i++){
        status=scene_pursuit_return_relation_check(r->stage,r->relations+i,0);if(status)return status;
    }
    for(uint32_t i=0;i<r->plan.union_bindings;i++){
        status=scene_return_transaction_binding(r,i,0,0);if(status)return status;
    }
    if(r->player_projection){
        if(r->player_projection->stage!=r->player)return RF_RANGE;
        status=scene_pursuit_return_player_projection_prepare(r->stream,r->player,
            r->gunner,r->turret,&r->scratch.player);if(status)return status;
        if(memcmp(&r->scratch.player,r->player_projection,sizeof(r->scratch.player)))
            return RF_FORMAT;
    }
    r->state=SCENE_RETURN_TX_ARMED;return RF_OK;
}
static int scene_return_transaction_capture(scene_return_transaction *r)
{
    int status=scene_return_transaction_enter(r,SCENE_RETURN_TX_ARMED);if(status)return status;
    if(r->owner){status=scene_return_owner_capture(r->owner,r->joins);if(status)return status;}
    if(r->ordinary){status=scene_return_target_publication_capture(r->ordinary);if(status)return status;}
    for(uint32_t i=0;i<r->plan.union_bindings;i++){
        status=scene_return_transaction_binding(r,i,0,1);if(status)return status;
    }
    for(uint32_t i=0;i<r->plan.relation_receipts;i++){
        status=scene_pursuit_return_relation_check(r->stage,r->relations+i,1);if(status)return status;
    }
    r->state=SCENE_RETURN_TX_CAPTURED;return RF_OK;
}
/* After all remaining publication: targets/player first, relation checks last.
 * Success authorizes only this participant's seal. Parent still owns its one
 * resource barrier, all other world seals, common close, finish and retirement.
 * Do not invoke any driver function except dispose after successful close. */
static int scene_return_transaction_seal(scene_return_transaction *r)
{
    int status=scene_return_transaction_enter(r,SCENE_RETURN_TX_CAPTURED);if(status)return status;
    if(r->owner){status=scene_return_owner_seal(r->owner,r->joins);if(status)return status;}
    if(r->ordinary){status=scene_return_target_publication_seal(r->ordinary);if(status)return status;}
    for(uint32_t i=0;i<r->plan.union_bindings;i++){
        status=scene_return_transaction_binding(r,i,0,1);if(status)return status;
    }
    if(r->player_projection){
        status=scene_pursuit_return_player_projection_seal(r->stream,r->player,r->player_projection);
        if(status)return status;
    }
    for(uint32_t i=0;i<r->plan.relation_receipts;i++){
        status=scene_pursuit_return_relation_check(r->stage,r->relations+i,1);if(status)return status;
    }
    r->state=SCENE_RETURN_TX_SEALED;return RF_OK;
}
#endif
