#ifndef RF_SCENE_RETURN_TARGET_PUBLICATION_INCLUDED
#define RF_SCENE_RETURN_TARGET_PUBLICATION_INCLUDED
#include "scene_return_target_candidate.inc"

/* UNCALLED. Include after scene_npc_reload_publication.inc and the three
 * target helpers. Parent supplies fresh, disjoint caller-owned allocations,
 * exact retained allocation extents, and one selected row per incoming target.
 * No selection, allocation, resource rebase, storage close or replay here.
 * Ordinary registered living targets only: no crouch or deferred retirement.
 *
 * The publication allocation and its rows/payload remain immutable throughout.
 * The parent's ONE transaction resource rebase may change clip reference
 * values only; this adapter never calls that operation, even with assign=0.
 * Parent must seal resource references globally before accepting final success.
 * Stable section ownership is required across each callback-free operation.
 * Caller must keep all receipt, stage and world allocations alive; matching
 * pointers/epochs cannot detect an allocator ABA after unauthorized release. */
enum {
    SCENE_RETURN_TARGET_EMPTY=0,
    SCENE_RETURN_TARGET_PREPARED=1,
    SCENE_RETURN_TARGET_CAPTURED=2,
    SCENE_RETURN_TARGET_SEALED=3,
    SCENE_RETURN_TARGET_FAILED=4
};
typedef struct scene_return_target_publication_row {
    uint32_t index;
    scene_return_target_candidate candidate;
    scene_return_target_snapshot snapshot;
    float normal_orientation[9];
    uint16_t pose_generations[50],cache_generations[50];
} scene_return_target_publication_row;
typedef struct scene_return_target_publication {
    uint32_t state,count,publication_bytes,stage_storage_bytes;
    const struct scene_return_target_publication *self;
    const scene_npc_checkpoint_restore_stage *stage;
    const scene_npc_reload_publication_receipt *publication;
    scene_return_target_publication_row *rows;
    /* One shared header copy, never one per target. Clip values live outside
     * this header, so the parent-only reference rebase does not invalidate it. */
    scene_npc_reload_publication_receipt header;
} scene_return_target_publication;

/* Exact compiler-dependent storage charge, with checked arithmetic. Parent
 * charges this in addition to existing stage/publication allocations. Does
 * not allocate or raise a cap; count cannot exceed the existing NPC cap.
 * Source-only accounting is discussed in the handoff, not asserted as sizeof. */
static inline int scene_return_target_publication_bytes(
    uint32_t count,uint32_t budget,uint32_t *bytes)
{
    uint64_t n;
    if(!bytes||!count||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    n=(uint64_t)sizeof(scene_return_target_publication)+
        (uint64_t)count*sizeof(scene_return_target_publication_row);
    if(n>budget||n>UINT32_MAX||n>SIZE_MAX)return RF_RANGE;
    *bytes=(uint32_t)n;return RF_OK;
}
/* Only fresh private storage, before exposure to any transaction. Never use
 * initialization to retry a failed or consumed transaction. Records contain
 * self-pointers and must remain at their original addresses. */
static inline void scene_return_target_publication_initialize(
    scene_return_target_publication *r)
{
    if(r){memset(r,0,sizeof(*r));r->self=r;}
}

/* Authenticate allocation layout before following row or payload pointers.
 * Main stage extent excludes separately owned medic-child allocations.
 * Exact extents are supplied from the parent's allocation bookkeeping. */
static inline int scene_return_target_publication_layout(
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *p,
    uint32_t stage_bytes,uint32_t publication_bytes)
{
    uint64_t fixed,at,n;uint32_t i;
    if(!stage||!p||publication_bytes<sizeof(*p)||stage_bytes<sizeof(*stage)||
       !scene_npc_reload_publication_span(p,publication_bytes,p,publication_bytes)||
       !scene_npc_reload_publication_span(stage,stage_bytes,stage,stage_bytes))
        return RF_RANGE;
    if(p->bytes!=publication_bytes||p->count>RF_NPC_CHECKPOINT_MAX_COUNT||
       p->count!=stage->count||p->stage!=stage||stage_bytes>stage->bytes||
       p->child_count>2u*p->count||p->link_count>RF_OBJECT_CAPACITY||
       p->medic_count>p->count||
       (uint64_t)p->resource_count>(uint64_t)p->count*SCENE_RELOAD_SEAL_REQUESTS)
        return RF_FORMAT;
    fixed=sizeof(*stage)+(uint64_t)p->count*sizeof(*stage->entries);
    if(fixed>stage_bytes)return RF_FORMAT;
    at=sizeof(*p);
#define RETURN_LAYOUT_ARRAY(member,count) do { \
    n=(uint64_t)(count)*sizeof(*p->member); \
    if(at>publication_bytes||n>publication_bytes-at|| \
       (uintptr_t)p->member!=(uintptr_t)p+at)return RF_FORMAT; \
    at+=n; \
} while(0)
    RETURN_LAYOUT_ARRAY(rows,p->count);
    RETURN_LAYOUT_ARRAY(clips,p->resource_count);
    RETURN_LAYOUT_ARRAY(children,p->child_count);
    RETURN_LAYOUT_ARRAY(links,p->link_count);
#undef RETURN_LAYOUT_ARRAY
    for(i=0;i<p->count;i++){
        const scene_npc_reload_publication_row *v=p->rows+i;
        uint32_t bones=v->skeleton.count;
        if(v->registered>1||v->bone_count>50)return RF_FORMAT;
        if(v->registered&&v->skeleton_id!=UINT32_MAX){
            if(!bones||bones>50||(v->bone_count&&v->bone_count!=bones))
                return RF_FORMAT;
            n=(uint64_t)bones*(50+sizeof(rf_model_bone_override))+
                (uint64_t)v->bone_count*50;
            if(at>publication_bytes||n>publication_bytes-at||
               v->pose_bytes!=n||(uintptr_t)v->pose_copy!=(uintptr_t)p+at)
                return RF_FORMAT;
            at+=n;
            n=p->collisions&&v->slot<p->model_count?
                (uint64_t)v->collision.bone_count*2:0;
            if(v->collision.bone_count>50||v->collision_bytes!=n||
               at>publication_bytes||n>publication_bytes-at)return RF_FORMAT;
            if(p->collisions&&v->slot<p->model_count){
                if((uintptr_t)v->collision_copy!=(uintptr_t)p+at)return RF_FORMAT;
            }else if(v->collision_copy)return RF_FORMAT;
            at+=n;
        }else if(v->pose_copy||v->pose_bytes||v->collision_copy||v->collision_bytes)
            return RF_FORMAT;
        if(v->bone_count){
            if(!scene_npc_reload_publication_span((const unsigned char*)stage+fixed,
                   stage_bytes-fixed,v->matrices,(uint64_t)v->bone_count*48)||
               !scene_npc_reload_publication_span((const unsigned char*)stage+fixed,
                   stage_bytes-fixed,v->generations,(uint64_t)v->bone_count*2)||
               (uintptr_t)v->generations!=(uintptr_t)v->matrices+v->bone_count*48u)
                return RF_FORMAT;
        }
    }
    return at==publication_bytes?RF_OK:RF_FORMAT;
}
/* This shared check precedes all retained row/candidate/world dereferences.
 * The root r and publication pointers themselves remain parent-owned. */
static inline int scene_return_target_publication_lifetime(
    const scene_return_target_publication *r)
{
    int status;
    if(!r||r->self!=r||!r->stage||!r->publication||!r->rows||
       !r->count||r->count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_FORMAT;
    if(memcmp(r->publication,&r->header,sizeof(r->header)))return RF_FORMAT;
    status=scene_npc_reload_publication_lifetime(r->stage,r->publication);
    if(status)return status;
    return scene_return_target_publication_layout(r->stage,r->publication,
        r->stage_storage_bytes,r->publication_bytes);
}

/* Caller has passed shared lifetime. Authenticate row IDs and fixed scene
 * identities before target helpers follow any retained pose/cache pointer. */
static inline int scene_return_target_publication_identity(
    const scene_return_target_publication *r,uint32_t index)
{
    const scene_npc_checkpoint_restore_entry *e;
    const scene_npc_reload_publication_row *v;
    const campaign_npc_body *o;const rf_entity_pose *p;uint32_t s;
    if(index>=r->header.count)return RF_FORMAT;
    e=r->stage->entries+index;v=r->publication->rows+index;s=v->slot;
    if(e->slot!=s||e->handle!=v->handle||e->saved.uid!=v->uid||
       e->saved.class_id!=v->class_id||e->registered!=1||v->registered!=1||
       e->bone_count!=v->bone_count||e->saved.retired||v->retired||
       e->pursuit_return_retire_pending||v->pursuit_return_retire_pending||
       e->saved.pursuit_return_profile!=v->pursuit_return_profile||
       e->saved.pursuit_return_phase!=v->pursuit_return_phase||
       memcmp(&e->pursuit_return_prepared,&v->pursuit_return_prepared,
           sizeof(e->pursuit_return_prepared))||
       e->saved.dead_pose||v->dead_pose||e->crouch_shape_present||
       e->crouch_retire_pending||e->saved.crouch.flags||
       v->crouch_shape_present||v->crouch_retire_pending||v->crouch_saved.flags||
       e->matrices!=v->matrices||e->generations!=v->generations||
       e->original_matrices!=v->original_matrices||
       e->original_generations!=v->original_generations||
       s>=campaign_npc_body_count||s>=campaign_poses.count||
       s>=campaign_model_owner_count||s>=campaign_seeds.records.count||
       !campaign_npc_bodies||!campaign_poses.items||!campaign_model_owners||
       !campaign_seeds.records.items||!campaign_seeds.items)return RF_FORMAT;
    o=campaign_npc_bodies+s;p=campaign_poses.items+s;
    if((uint32_t)campaign_seeds.records.items[s].record.uid!=v->uid||
       campaign_seeds.items[s].class_index!=v->class_id||
       v->class_id>=campaign_seeds.class_count||!v->handle||v->handle==UINT32_MAX||
       o->registration.handle!=v->handle||o->registration.object_kind||
       o->registration.view!=&o->view||(uint32_t)o->view.handle!=v->handle||
       rf_object_registry_lookup(&campaign_registry,v->handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)v->handle)!=&o->view||
       !o->persistence_registered||o->persistence_slot>=rf_scene_defeated_actors.count||
       rf_scene_defeated_actors.items[o->persistence_slot].uid!=v->uid||
       rf_scene_defeated_actors.items[o->persistence_slot].retired||
       p->skeleton!=v->skeleton_id)return RF_FORMAT;
    if(v->skeleton_id!=UINT32_MAX){
        s=v->skeleton_id;
        if(s>=campaign_playback_resources.model_count||
           s>=campaign_motion_catalog.model_count||s>=campaign_skeletons.count||
           !campaign_playback_resources.models||!campaign_motion_catalog.models||
           !campaign_skeletons.items||
           memcmp(campaign_playback_resources.models+s,&v->resources,sizeof(v->resources))||
           memcmp(campaign_motion_catalog.models+s,&v->motions,sizeof(v->motions))||
           memcmp(campaign_skeletons.items+s,&v->skeleton,sizeof(v->skeleton)))
            return RF_FORMAT;
    }
    return RF_OK;
}

/* Prepare AFTER the parent's successful ordinary pre-assignment publication
 * seal; it must already have filled exact old owner/pose snapshots. Indices
 * are parent-selected unique incoming targets; deferred flags must all be 0.
 * budget covers this adapter only, inside the unchanged total world budget.
 * inputs/outputs must be independently allocated and disjoint from world
 * owners and all their deep buffers. No caller storage may move afterwards.
 * Failure leaves state FAILED even if some private rows were prepared. */
static inline int scene_return_target_publication_prepare(
    scene_return_target_publication *r,
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *publication,
    uint32_t stage_storage_bytes,uint32_t publication_bytes,
    scene_return_target_publication_row *rows,uint32_t count,
    const uint32_t *indices,const uint32_t *deferred,
    const float *normal_orientation,uint32_t budget)
{
    uint32_t bytes,i,j;int status;
    scene_return_target_candidate_copies copies;
    scene_return_target_snapshot_storage storage;
    if(!r)return RF_RANGE;
    if(r->self!=r||r->state!=SCENE_RETURN_TARGET_EMPTY){
        r->state=SCENE_RETURN_TARGET_FAILED;return RF_FORMAT;
    }
    r->state=SCENE_RETURN_TARGET_FAILED;
    if(!rows||!indices||!deferred||!stage||!publication)return RF_RANGE;
    status=scene_return_target_publication_bytes(count,budget,&bytes);if(status)return status;
    if(stage_storage_bytes<sizeof(*stage)||publication_bytes<sizeof(*publication))
        return RF_RANGE;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    status=scene_return_target_publication_layout(stage,publication,
        stage_storage_bytes,publication_bytes);if(status)return status;
    /* Protect root records and all supplied source arrays from private writes. */
#define RETURN_PRIVATE_SOURCE(p,n) do { \
    status=scene_return_target_snapshot_disjoint(r,sizeof(*r),(p),(n)); \
    if(status)return status; \
    status=scene_return_target_snapshot_disjoint(rows,(size_t)count*sizeof(*rows),(p),(n)); \
    if(status)return status; \
} while(0)
    status=scene_return_target_snapshot_disjoint(r,sizeof(*r),rows,
        (size_t)count*sizeof(*rows));if(status)return status;
    RETURN_PRIVATE_SOURCE(stage,stage_storage_bytes);
    RETURN_PRIVATE_SOURCE(publication,publication_bytes);
    RETURN_PRIVATE_SOURCE(indices,(size_t)count*sizeof(*indices));
    RETURN_PRIVATE_SOURCE(deferred,(size_t)count*sizeof(*deferred));
    if(normal_orientation)RETURN_PRIVATE_SOURCE(normal_orientation,9*sizeof(float));
#undef RETURN_PRIVATE_SOURCE
    r->stage=stage;r->publication=publication;r->rows=rows;r->count=count;
    r->stage_storage_bytes=stage_storage_bytes;r->publication_bytes=publication_bytes;
    memcpy(&r->header,publication,sizeof(r->header));
    for(i=0;i<count;i++){
        const scene_npc_checkpoint_restore_entry *e;
        const scene_npc_reload_publication_row *v;
        const rf_entity_pose *p;const campaign_npc_body *o;
        const campaign_model_owner *m;const rf_entity_collision_cache *c;
        const unsigned char *base;uint32_t bones;
        scene_return_target_publication_row *a=rows+i;
        if(deferred[i])return RF_NOT_FOUND;
        status=scene_return_target_publication_identity(r,indices[i]);if(status)return status;
        for(j=0;j<i;j++)if(indices[j]==indices[i]||
            stage->entries[indices[j]].slot==stage->entries[indices[i]].slot)
                return RF_FORMAT;
        e=stage->entries+indices[i];v=publication->rows+indices[i];
        o=campaign_npc_bodies+e->slot;p=campaign_poses.items+e->slot;
        m=campaign_model_owners+e->slot;
        c=campaign_collision_caches?campaign_collision_caches+e->slot:NULL;
        /* Existing owner check proves deep destinations and registry identity
         * against the pre-assignment receipt, never called after assignment. */
        status=scene_npc_reload_publication_owner(e);if(status)return status;
        if(memcmp(m,&v->model,sizeof(*m))||
           (c&&memcmp(c,&v->collision,sizeof(*c))))return RF_FORMAT;
        bones=p->bone_count;base=v->pose_copy;
        memset(a,0,sizeof(*a));a->index=indices[i];
        memset(&copies,0,sizeof(copies));memset(&storage,0,sizeof(storage));
        copies.selection.data=&v->selection;copies.selection.bytes=sizeof(v->selection);
        copies.playback.data=&v->playback;copies.playback.bytes=sizeof(v->playback);
        if(e->bone_count){
            copies.matrices.data=base+(size_t)bones*(50+sizeof(*p->overrides));
            copies.matrices.bytes=(size_t)e->bone_count*48;
            copies.generations.data=(const unsigned char*)copies.matrices.data+copies.matrices.bytes;
            copies.generations.bytes=(size_t)e->bone_count*2;
        }else if(bones){
            storage.matrices.data=base;storage.matrices.bytes=(size_t)bones*48;
        }
        if(bones&&p->overrides){
            storage.overrides.data=base+(size_t)bones*50;
            storage.overrides.bytes=(size_t)bones*sizeof(*p->overrides);
        }
        status=scene_return_target_candidate_prepare(e,&copies,&a->candidate);
        if(status)return status;
        status=scene_return_target_snapshot_prepare(e,o,p,m,c,normal_orientation,0,
            &storage,&a->snapshot);if(status)return status;
        if(normal_orientation)memcpy(a->normal_orientation,normal_orientation,
            sizeof(a->normal_orientation));
    }
    r->state=SCENE_RETURN_TARGET_PREPARED;return RF_OK;
}

/* Validate every row first, then capture all rows without a fallible operation.
 * Call immediately after NPC assignment, before any unrelated publication.
 * Old snapshot headers are deliberately repurposed as assigned headers only
 * after target_assigned succeeds; do not call that projection after capture. */
static inline int scene_return_target_publication_capture(
    scene_return_target_publication *r)
{
    uint32_t i;int status;
    if(!r)return RF_RANGE;
    if(r->state!=SCENE_RETURN_TARGET_PREPARED){
        r->state=SCENE_RETURN_TARGET_FAILED;return RF_FORMAT;
    }
    r->state=SCENE_RETURN_TARGET_FAILED;
    status=scene_return_target_publication_lifetime(r);if(status)return status;
    for(i=0;i<r->count;i++){
        scene_return_target_publication_row *v=r->rows+i;
        const scene_return_target_assignment *a=&v->snapshot.assignment;
        status=scene_return_target_publication_identity(r,v->index);if(status)return status;
        status=scene_return_target_candidate_check(r->stage->entries+v->index,&v->candidate);
        if(status)return status;
        if(a->normal_orientation&&memcmp(a->normal_orientation,v->normal_orientation,
            sizeof(v->normal_orientation)))return RF_FORMAT;
        status=scene_return_target_assigned(a);if(status)return status;
    }
    for(i=0;i<r->count;i++){
        scene_return_target_publication_row *v=r->rows+i;
        scene_return_target_snapshot *s=&v->snapshot;
        scene_return_target_assignment *a=&s->assignment;
        memcpy(&s->before,a->live,sizeof(s->before));
        memcpy(&s->pose_before,a->pose,sizeof(s->pose_before));
        memcpy(&s->model_before,a->model,sizeof(s->model_before));
        if(a->cache)memcpy(&s->cache_before,a->cache,sizeof(s->cache_before));
        if(a->pose->bone_count)memcpy(v->pose_generations,a->pose->generations,
            (size_t)a->pose->bone_count*2);
        if(a->cache&&a->cache->bone_count)memcpy(v->cache_generations,a->cache->generations,
            (size_t)a->cache->bone_count*2);
        if(v->candidate.fields.bone_count)a->matrices=v->candidate.copies.matrices;
        /* Spheres, overrides and cache matrices already equal their immutable
         * snapshots; retaining them avoids another copy or a new allocation. */
    }
    r->state=SCENE_RETURN_TARGET_CAPTURED;return RF_OK;
}

/* After all later fallible publication, before parent resource/player seals
 * and the exactly-once common close. Any failure poisons this adapter.
 * Every header compares BEFORE any deep live pointer is followed. Whole
 * owner/model/pose/cache bytes seal fields outside the assignment projection.
 * Embedded foreign pointers are identities, not recursively traversed graphs;
 * parent seals staged return relationships and other subsystem ownership. */
static inline int scene_return_target_publication_seal(
    scene_return_target_publication *r)
{
    uint32_t i;int status;
    if(!r)return RF_RANGE;
    if(r->state!=SCENE_RETURN_TARGET_CAPTURED){
        r->state=SCENE_RETURN_TARGET_FAILED;return RF_FORMAT;
    }
    r->state=SCENE_RETURN_TARGET_FAILED;
    status=scene_return_target_publication_lifetime(r);if(status)return status;
    for(i=0;i<r->count;i++){
        const scene_return_target_publication_row *v=r->rows+i;
        const scene_return_target_snapshot *s=&v->snapshot;
        const scene_return_target_assignment *a=&s->assignment;
        status=scene_return_target_publication_identity(r,v->index);if(status)return status;
        status=scene_return_target_candidate_check(r->stage->entries+v->index,&v->candidate);
        if(status)return status;
        if(memcmp(a->live,&s->before,sizeof(s->before))||
           memcmp(a->pose,&s->pose_before,sizeof(s->pose_before))||
           memcmp(a->model,&s->model_before,sizeof(s->model_before))||
           (a->cache&&memcmp(a->cache,&s->cache_before,sizeof(s->cache_before)))||
           (a->normal_orientation&&memcmp(a->normal_orientation,v->normal_orientation,
               sizeof(v->normal_orientation))))return RF_FORMAT;
        status=scene_return_target_bytes_equal(a->live->body.spheres.items,&a->spheres,
            (size_t)a->live->body.spheres.count*sizeof(rf_physics_sphere));if(status)return status;
        status=scene_return_target_bytes_equal(a->pose->matrices,&a->matrices,
            (size_t)a->pose->bone_count*48);if(status)return status;
        status=scene_return_target_bytes_equal(a->pose->overrides,&a->overrides,
            a->pose->overrides?(size_t)a->pose->bone_count*sizeof(rf_model_bone_override):0);
        if(status)return status;
        if(a->pose->bone_count&&memcmp(a->pose->generations,v->pose_generations,
            (size_t)a->pose->bone_count*2))return RF_FORMAT;
        if(a->cache){
            status=scene_return_target_bytes_equal(a->cache->matrices,&a->cache_matrices,
                (size_t)a->cache->bone_count*48);if(status)return status;
            if(a->cache->bone_count&&memcmp(a->cache->generations,v->cache_generations,
                (size_t)a->cache->bone_count*2))return RF_FORMAT;
        }
    }
    r->state=SCENE_RETURN_TARGET_SEALED;return RF_OK;
}
#endif
