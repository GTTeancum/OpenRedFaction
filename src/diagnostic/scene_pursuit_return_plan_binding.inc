#ifndef RF_SCENE_PURSUIT_RETURN_PLAN_BINDING_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_PLAN_BINDING_INCLUDED
#include "scene_pursuit_return_late_selection.inc"
#include "scene_pursuit_return_relation.inc"
#include "scene_return_late_plan.inc"
/* Uncalled typed bridge from the current prepared stage into the explicit
 * planner metadata. Parent first authenticates the complete stage, current
 * scene/model/cache containers and their lifetimes; retained arrays remain
 * immutable through planning and preparation. Outputs must be disjoint from
 * the stage and every retained input. One metadata row per stage row,
 * in the SAME order, makes relation indices unambiguous. No allocation,
 * callback, gameplay write, selection override or storage-close authority. */
static int scene_pursuit_return_plan_row(
    const scene_npc_checkpoint_restore_stage *stage,uint32_t row,
    uint32_t player_handle,uint32_t shared_extra_bytes,
    scene_return_plan_row *out,uint32_t *needs_player)
{
    scene_return_plan_row result={0};
    const scene_npc_checkpoint_restore_entry *e;
    uint32_t flags,player;int status;
    if(!out||!needs_player||player_handle!=campaign_player_object.handle)return RF_RANGE;
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),
        needs_player,sizeof(*needs_player));if(status)return status;
    status=scene_pursuit_return_late_selection(stage,row,player_handle,&flags,&player);
    if(status)return status;
    e=stage->entries+row;
    result.stage_index=row;result.slot=e->slot;result.uid=e->saved.uid;
    result.class_id=e->saved.class_id;result.handle=e->handle;
    if(flags&SCENE_RETURN_LATE_OWNER)result.flags|=SCENE_RETURN_PLAN_OWNER;
    if(flags&SCENE_RETURN_LATE_TARGET)result.flags|=SCENE_RETURN_PLAN_TARGET;
    if(e->crouch_shape_present>1)return RF_FORMAT;
    if(e->crouch_shape_present)result.flags|=SCENE_RETURN_PLAN_CROUCH;
    if(result.flags&(SCENE_RETURN_PLAN_OWNER|SCENE_RETURN_PLAN_CROUCH))
        result.shared_extra_bytes=shared_extra_bytes;
    else if(shared_extra_bytes)return RF_RANGE;
    if(result.flags&SCENE_RETURN_PLAN_TARGET){
        if(!e->registered||e->saved.retired||e->saved.dead_pose||
           e->crouch_retire_pending||e->pursuit_return_retire_pending||
           !isfinite(e->saved.health)||e->saved.health<=0)return RF_FORMAT;
        if(campaign_collision_caches){
            if(e->slot>=campaign_model_owner_count)return RF_FORMAT;
            result.cache_bones=campaign_collision_caches[e->slot].bone_count;
            if(result.cache_bones>50)return RF_FORMAT;
        }
    }
    *out=result;*needs_player=player;return RF_OK;
}
/* Incoming dependency pairs only. The owner relation receipt itself also
 * covers outgoing-only owners and must be separately charged for those rows.
 * RF_NOT_FOUND means no incoming edge, not permission to omit owner retention.
 * Parent provides disjoint output and keeps metadata in stage-row order. */
static int scene_pursuit_return_plan_edge(
    const scene_npc_checkpoint_restore_stage *stage,uint32_t row,
    scene_return_plan_relation *out)
{
    scene_pursuit_return_relation relation;scene_return_plan_relation edge;int status;
    if(!out)return RF_RANGE;
    status=scene_pursuit_return_relation_prepare(stage,row,&relation);if(status)return status;
    if(!relation.incoming.phase)return RF_NOT_FOUND;
    edge.owner=row;edge.target=relation.player?UINT32_MAX:relation.target_row;
    *out=edge;return RF_OK;
}
/* Build adapter joins from the actual authenticated publication row, never
 * from a second copy of candidate values. The caller repeats this binding at
 * each boundary and keeps the resulting private tuple arrays disjoint/stable.
 * This is a tuple projection only; complete transaction seals remain required. */
static int scene_pursuit_return_plan_join(
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_reload_publication_receipt *publication,
    uint32_t stage_bytes,uint32_t publication_bytes,uint32_t row,
    scene_return_crouch_join *out)
{
    const scene_npc_checkpoint_restore_entry *e;
    const scene_npc_reload_publication_row *p;
    scene_return_crouch_join result={0};int status;
    if(!stage||!publication||!out)return RF_RANGE;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    status=scene_return_target_publication_layout(stage,publication,stage_bytes,publication_bytes);
    if(status)return status;
    if(row>=stage->count||row>=publication->count)return RF_RANGE;
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),stage,stage_bytes);
    if(status)return status;
    status=scene_return_target_snapshot_disjoint(out,sizeof(*out),publication,publication_bytes);
    if(status)return status;
    e=stage->entries+row;p=publication->rows+row;
    if(e->slot!=p->slot||e->handle!=p->handle||e->registered!=p->registered||
       e->saved.uid!=p->uid||e->saved.class_id!=p->class_id||
       p->pursuit_return_retire_pending>1||
       e->saved.pursuit_return_profile!=p->pursuit_return_profile||
       e->saved.pursuit_return_phase!=p->pursuit_return_phase||
       e->pursuit_return_retire_pending!=p->pursuit_return_retire_pending||
       memcmp(&e->pursuit_return_prepared,&p->pursuit_return_prepared,
           sizeof(p->pursuit_return_prepared)))return RF_FORMAT;
    result.profile=p->pursuit_return_profile;result.phase=p->pursuit_return_phase;
    result.deferred=p->pursuit_return_retire_pending;
    memcpy(result.prepared,&p->pursuit_return_prepared,sizeof(result.prepared));
    *out=result;return RF_OK;
}
#endif
