#ifndef RF_SCENE_PURSUIT_WORK_SCHEDULER_INCLUDED
#define RF_SCENE_PURSUIT_WORK_SCHEDULER_INCLUDED
/* UNCALLED replacement proposal for the single tactical scheduling authority.
 * Probe is an internal, read-only, gameplay-callback-free owner/target/readiness
 * check, not an extension point for scripts or resource loading. It must not
 * recurse, change ownership or do graph/collision work. Caller owns the stable
 * actor-array lifetime for this scan and publishes the returned cursor/frame
 * BEFORE doing selected expensive work. No second selection after refusal.
 * Existing RFNC cursor is reused, frame guard stays transient. No N-row scratch.
 */
typedef struct scene_pursuit_work_selection {
    uint32_t cursor,service_frame,slot,kind;
} scene_pursuit_work_selection;
static int scene_pursuit_work_schedule(uint32_t count,uint32_t cursor,
    uint32_t service_frame,uint32_t frame,
    int (*probe)(void *,uint32_t,uint32_t *),void *context,
    scene_pursuit_work_selection *result)
{
    scene_pursuit_work_selection out={cursor,service_frame,UINT32_MAX,
        SCENE_PURSUIT_WORK_NONE};
    uint32_t slot;int status;
    if(!result||!probe)return RF_RANGE;
    if(count>RF_NPC_CHECKPOINT_MAX_COUNT||(count?cursor>=count:cursor!=0))
        return RF_FORMAT;
    if(service_frame==frame){*result=out;return RF_OK;}
    slot=cursor;
    for(uint32_t i=0;i<count;i++){
        uint32_t kind=SCENE_PURSUIT_WORK_NONE;
        status=probe(context,slot,&kind);if(status)return status;
        if(kind>SCENE_PURSUIT_WORK_RETURN_PRELUDE)return RF_FORMAT;
        if(kind!=SCENE_PURSUIT_WORK_NONE){
            out.slot=slot;out.kind=kind;out.cursor=slot+1u==count?0:slot+1u;
            break;
        }
        slot=slot+1u==count?0:slot+1u;
    }
    out.service_frame=frame;*result=out;return RF_OK;
}
/* Concrete internal adapter, still uncalled. Include after source/owner and
 * receipt readiness helpers. Ordinary tactical qualification retains its
 * existing cohort; the narrower no-Alarm source gate applies only to return.
 * This scan has no resource, geometry, gameplay or extension callbacks. */
static int scene_pursuit_work_probe(void *context,uint32_t slot,uint32_t *kind)
{
    const campaign_npc_body *o;uint32_t target,melee,ready;int status;
    if(!context||!kind||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    status=scene_pursuit_work_ready(&o->pursuit_return,&o->tactical,*(const int32_t*)context,&ready);
    if(status)return status;
    if(ready==SCENE_PURSUIT_WORK_NONE||!scene_tactical_service_allowed(o)){
        *kind=SCENE_PURSUIT_WORK_NONE;return RF_OK;
    }
    if(o->pursuit_return.phase){
        status=scene_pursuit_return_owner(slot,&target,&melee);
        if(status==RF_NOT_FOUND){*kind=SCENE_PURSUIT_WORK_NONE;return RF_OK;}
        if(status)return status;
        if(target!=o->pursuit_return.target||o->combat_scripted!=o->pursuit_return.combat_mode){
            *kind=SCENE_PURSUIT_WORK_NONE;return RF_OK;
        }
    }else{
        target=scene_blind_target_handle(o);
        if(target!=o->tactical.target||!scene_blind_target_live(target)){
            *kind=SCENE_PURSUIT_WORK_NONE;return RF_OK;
        }
        status=scene_tactical_owner(slot);
        if(status==RF_NOT_FOUND){*kind=SCENE_PURSUIT_WORK_NONE;return RF_OK;}
        if(status)return status;
    }
    *kind=ready;return RF_OK;
}
/* Sole future selection authority. Replace the old scheduler call; never
 * call both. Commit the shared cursor/frame before selected fallible work.
 * A later resource refusal cannot refund the slot or run another category.
 * Mismatched owners need separate cheap lifecycle cancellation, not writes
 * from this probe. Output and global bookkeeping survive scan errors intact. */
static int scene_pursuit_work_select(uint32_t frame,int32_t now,
    scene_pursuit_work_selection *selected)
{
    scene_pursuit_work_selection next;int status;
    if(!selected||(campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    status=scene_pursuit_execution_boundary();if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_pursuit_work_schedule(campaign_npc_body_count,scene_tactical_cursor,
        scene_tactical_service_frame,frame,scene_pursuit_work_probe,&now,&next);
    if(status)return status;
    scene_tactical_cursor=next.cursor;scene_tactical_service_frame=next.service_frame;
    *selected=next;return RF_OK;
}
#endif
