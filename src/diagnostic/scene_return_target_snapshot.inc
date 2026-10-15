#ifndef RF_SCENE_RETURN_TARGET_SNAPSHOT_INCLUDED
#define RF_SCENE_RETURN_TARGET_SNAPSHOT_INCLUDED
#include "scene_return_target_assigned.inc"

/* UNCALLED. Own these records in the parent's already-budgeted late receipt.
 * Fixed capacities are existing restore limits, not a new memory allowance.
 * Headers are independent copies; their borrowed pointers remain identities.
 * Sphere/cache arrays are owned; optional pose arrays may reuse authenticated
 * immutable publication spans instead of duplicating their storage.
 * The embedded assignment has self-pointers: do not move/copy a prepared record.
 * No target selection, registry authority or staged return layout is invented. */
typedef struct scene_return_target_snapshot {
    scene_return_target_assignment assignment;
    campaign_npc_body before;
    rf_entity_pose pose_before;
    campaign_model_owner model_before;
    rf_entity_collision_cache cache_before;
    rf_physics_sphere spheres[8];
    float cache_matrices[50][12];
} scene_return_target_snapshot;
typedef struct scene_return_target_matrix_storage {
    float values[50][12];
} scene_return_target_matrix_storage;
typedef struct scene_return_target_override_storage {
    rf_model_bone_override values[50];
} scene_return_target_override_storage;
/* For each needed span choose exactly one: authenticated immutable publication
 * bytes OR independent owned storage. For an unneeded span supply neither.
 * Thus replaced pose matrices consume no extra owned buffer. Reused receipt
 * storage must outlive final seal; a later receipt rebase must not rewrite it. */
typedef struct scene_return_target_snapshot_storage {
    scene_return_target_bytes matrices, overrides;
    scene_return_target_matrix_storage *owned_matrices;
    scene_return_target_override_storage *owned_overrides;
} scene_return_target_snapshot_storage;

/* Check the entire destination, including unused fixed-capacity tails.
 * Zero-length source spans need no pointer. Arithmetic never wraps. */
static inline int scene_return_target_snapshot_disjoint(
    const void *destination,size_t destination_bytes,const void *source,size_t bytes)
{
    uintptr_t d=(uintptr_t)destination,s=(uintptr_t)source;
    if(!destination||destination_bytes>UINTPTR_MAX-d)return RF_RANGE;
    if(!bytes)return RF_OK;
    if(!source||bytes>UINTPTR_MAX-s)return RF_RANGE;
    return d<s+bytes&&s<d+destination_bytes?RF_FORMAT:RF_OK;
}

/* Call BEFORE assignment, AFTER parent lifetime/candidate authentication.
 * e, owner, pose, model and optional cache must be the authenticated same slot.
 * Parent also authenticates registry membership, source->target relation,
 * candidate immutability, complete allocation spans and pointer ownership.
 * This is incoming-target-only, living, standalone, no old/new crouch.
 * retirement_deferred is explicit parent input, not a guessed staged field.
 *
 * Preserve out AND optional owned buffers on every error: validate first.
 * Stable simulation/section ownership excludes mutation during the copy.
 * No callbacks, allocations, actor writes, or fallible calls after first write.
 * All caller storage (full record plus selected owned buffers) must fit
 * existing budgets. Reuse lowers additional storage, not existing ownership. */
static inline int scene_return_target_snapshot_prepare(
    const scene_npc_checkpoint_restore_entry *e,
    const campaign_npc_body *owner,const rf_entity_pose *pose,
    const campaign_model_owner *model,const rf_entity_collision_cache *cache,
    const float *normal_orientation,uint32_t retirement_deferred,
    const scene_return_target_snapshot_storage *storage,
    scene_return_target_snapshot *out)
{
    size_t sphere_bytes,matrix_bytes,override_bytes,cache_bytes;
    const void *matrix_copy,*override_copy;
    uint32_t bones,cache_bones;
    int status;
    if(!e||!owner||!pose||!model||!storage||!out||retirement_deferred>1)return RF_RANGE;
#define TARGET_SNAPSHOT_DISJOINT(p,n) do { \
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),(p),(n)); \
    if(status)return status; \
    if(storage->owned_matrices){ \
        status=scene_return_target_snapshot_disjoint(storage->owned_matrices, \
            sizeof(*storage->owned_matrices),(p),(n));if(status)return status; \
    } \
    if(storage->owned_overrides){ \
        status=scene_return_target_snapshot_disjoint(storage->owned_overrides, \
            sizeof(*storage->owned_overrides),(p),(n));if(status)return status; \
    } \
} while(0)
    TARGET_SNAPSHOT_DISJOINT(storage,sizeof(*storage));
    if(storage->owned_matrices){
        status=scene_return_target_snapshot_disjoint(out,sizeof(*out),
            storage->owned_matrices,sizeof(*storage->owned_matrices));if(status)return status;
    }
    if(storage->owned_overrides){
        status=scene_return_target_snapshot_disjoint(out,sizeof(*out),
            storage->owned_overrides,sizeof(*storage->owned_overrides));if(status)return status;
    }
    if(storage->owned_matrices&&storage->owned_overrides){
        status=scene_return_target_snapshot_disjoint(storage->owned_matrices,
            sizeof(*storage->owned_matrices),storage->owned_overrides,
            sizeof(*storage->owned_overrides));if(status)return status;
    }
    TARGET_SNAPSHOT_DISJOINT(e,sizeof(*e));
    TARGET_SNAPSHOT_DISJOINT(owner,sizeof(*owner));
    TARGET_SNAPSHOT_DISJOINT(pose,sizeof(*pose));
    TARGET_SNAPSHOT_DISJOINT(model,sizeof(*model));
    if(cache)TARGET_SNAPSHOT_DISJOINT(cache,sizeof(*cache));
    if(e->saved.retired>1||e->saved.animation_present>1||
       e->saved.airborne.present>1||e->movement_resolved>1||
       e->crouch_shape_present>1||e->crouch_retire_pending>1)return RF_FORMAT;
    if(retirement_deferred||e->saved.retired||e->saved.dead_pose||
       e->crouch_retire_pending||e->crouch_shape_present||e->saved.crouch.flags||
       owner->crouch.flags||(owner->view.flags_810&0x400u))return RF_NOT_FOUND;
    if(e->registered!=1||!e->handle||e->handle==UINT32_MAX||
       owner->registration.handle!=e->handle||owner->registration.object_kind||
       owner->registration.view!=&owner->view||(uint32_t)owner->view.handle!=e->handle||
       owner->view.type||owner->damage.effects.handle!=e->handle||
       !owner->persistence_registered||owner->view.linked_handle!=-1||
       owner->view.occupant_count||owner->attachment_75c!=-1||
       (owner->object_flags&8u)||(owner->view.flags_810&1u)||
       !isfinite(owner->damage.effects.health)||owner->damage.effects.health<=0||
       !isfinite(e->saved.health)||e->saved.health<=0||
       !isfinite(e->body.mass)||e->body.mass<=0)return RF_FORMAT;
    if(owner->body.spheres.count>8||
       !!owner->body.spheres.count!=!!owner->body.spheres.items)return RF_FORMAT;
    sphere_bytes=(size_t)owner->body.spheres.count*sizeof(*owner->body.spheres.items);
    if(owner->body.spheres.allocated_bytes!=sizeof(owner->body.spheres)+sphere_bytes||
       owner->body.allocated_bytes!=sizeof(owner->body)+sphere_bytes)return RF_FORMAT;
    bones=pose->bone_count;cache_bones=cache?cache->bone_count:0;
    if(bones>50||cache_bones>50||e->bone_count>50||
       pose->playback.completion.active.count>16||
       e->playback.completion.active.count>16||model->owned)return RF_FORMAT;
    if(pose->skeleton==UINT32_MAX){
        if(bones||pose->matrices||pose->generations||pose->overrides||
           pose->playback.completion.active.count||e->bone_count||
           e->playback.completion.active.count)return RF_FORMAT;
    }else if(!bones||!pose->matrices||!pose->generations||!pose->overrides||
             !model->registration.loaded||model->pose!=pose||
             model->registration.active!=&pose->playback.completion.active||
             owner->selection.mapping.skeleton!=pose->skeleton||
             e->selection.mapping.skeleton!=pose->skeleton||
             owner->selection.mapping.class_index!=e->saved.class_id||
             e->selection.mapping.class_index!=e->saved.class_id)return RF_FORMAT;
    if(e->bone_count&&(e->bone_count!=bones||!e->matrices||!e->generations||
       e->original_matrices!=pose->matrices||e->original_generations!=pose->generations||
       e->matrices==pose->matrices||e->generations==pose->generations))return RF_FORMAT;
    if(cache&&cache_bones&&(!cache->matrices||!cache->generations||
       cache_bones!=bones||cache->skeleton!=pose->skeleton))return RF_FORMAT;
    if(e->movement_resolved&&!normal_orientation)return RF_RANGE;

    /* Even replaced matrices and unsnapshotted stamps must not be clobbered
     * by the output. Parent has already authenticated their readable spans. */
    TARGET_SNAPSHOT_DISJOINT(owner->body.spheres.items,sphere_bytes);
    TARGET_SNAPSHOT_DISJOINT(pose->matrices,(size_t)bones*48);
    TARGET_SNAPSHOT_DISJOINT(pose->generations,(size_t)bones*2);
    TARGET_SNAPSHOT_DISJOINT(pose->overrides,
        pose->overrides?(size_t)bones*sizeof(*pose->overrides):0);
    if(e->bone_count){
        TARGET_SNAPSHOT_DISJOINT(e->matrices,(size_t)e->bone_count*48);
        TARGET_SNAPSHOT_DISJOINT(e->generations,(size_t)e->bone_count*2);
    }
    if(cache){
        TARGET_SNAPSHOT_DISJOINT(cache->matrices,(size_t)cache_bones*48);
        TARGET_SNAPSHOT_DISJOINT(cache->generations,(size_t)cache_bones*2);
    }
    if(normal_orientation)TARGET_SNAPSHOT_DISJOINT(normal_orientation,9*sizeof(float));
    matrix_bytes=e->bone_count?0:(size_t)bones*48;
    override_bytes=pose->overrides?(size_t)bones*sizeof(*pose->overrides):0;
    cache_bytes=cache&&cache->matrices?(size_t)cache_bones*48:0;
    matrix_copy=NULL;override_copy=NULL;
    if(matrix_bytes){
        if(storage->matrices.data||storage->matrices.bytes){
            if(storage->owned_matrices||!storage->matrices.data||
               storage->matrices.bytes!=matrix_bytes)return RF_FORMAT;
            TARGET_SNAPSHOT_DISJOINT(storage->matrices.data,matrix_bytes);
            status=scene_return_target_snapshot_disjoint(storage->matrices.data,
                matrix_bytes,pose->matrices,matrix_bytes);if(status)return status;
            if(memcmp(storage->matrices.data,pose->matrices,matrix_bytes))return RF_FORMAT;
            matrix_copy=storage->matrices.data;
        }else{
            if(!storage->owned_matrices)return RF_RANGE;
            matrix_copy=storage->owned_matrices->values;
        }
    }else if(storage->matrices.data||storage->matrices.bytes||storage->owned_matrices)
        return RF_RANGE;
    if(override_bytes){
        if(storage->overrides.data||storage->overrides.bytes){
            if(storage->owned_overrides||!storage->overrides.data||
               storage->overrides.bytes!=override_bytes)return RF_FORMAT;
            TARGET_SNAPSHOT_DISJOINT(storage->overrides.data,override_bytes);
            status=scene_return_target_snapshot_disjoint(storage->overrides.data,
                override_bytes,pose->overrides,override_bytes);if(status)return status;
            if(memcmp(storage->overrides.data,pose->overrides,override_bytes))return RF_FORMAT;
            override_copy=storage->overrides.data;
        }else{
            if(!storage->owned_overrides)return RF_RANGE;
            override_copy=storage->owned_overrides->values;
        }
    }else if(storage->overrides.data||storage->overrides.bytes||storage->owned_overrides)
        return RF_RANGE;
#undef TARGET_SNAPSHOT_DISJOINT

    /* Commit: no further rejection, fallible operation or callback. */
    memset(out,0,sizeof(*out));
    memcpy(&out->before,owner,sizeof(*owner));
    memcpy(&out->pose_before,pose,sizeof(*pose));
    memcpy(&out->model_before,model,sizeof(*model));
    if(cache)memcpy(&out->cache_before,cache,sizeof(*cache));
    if(sphere_bytes)memcpy(out->spheres,owner->body.spheres.items,sphere_bytes);
    if(storage->owned_matrices){
        memset(storage->owned_matrices,0,sizeof(*storage->owned_matrices));
        memcpy(storage->owned_matrices->values,pose->matrices,matrix_bytes);
    }
    if(storage->owned_overrides){
        memset(storage->owned_overrides,0,sizeof(*storage->owned_overrides));
        memcpy(storage->owned_overrides->values,pose->overrides,override_bytes);
    }
    if(cache_bytes)memcpy(out->cache_matrices,cache->matrices,cache_bytes);
    out->assignment.candidate=e;
    out->assignment.before=&out->before;out->assignment.live=owner;
    out->assignment.pose_before=&out->pose_before;out->assignment.pose=pose;
    out->assignment.model_before=&out->model_before;out->assignment.model=model;
    out->assignment.cache_before=cache?&out->cache_before:NULL;
    out->assignment.cache=cache;out->assignment.normal_orientation=normal_orientation;
    out->assignment.spheres.data=sphere_bytes?out->spheres:NULL;
    out->assignment.spheres.bytes=sphere_bytes;
    out->assignment.matrices.data=matrix_copy;
    out->assignment.matrices.bytes=matrix_bytes;
    out->assignment.overrides.data=override_copy;
    out->assignment.overrides.bytes=override_bytes;
    out->assignment.cache_matrices.data=cache_bytes?out->cache_matrices:NULL;
    out->assignment.cache_matrices.bytes=cache_bytes;
    /* retirement_deferred remains zero: unsupported inputs returned above. */
    return RF_OK;
}
#endif
