#ifndef RF_SCENE_TACTICAL_SCHEDULER_INCLUDED
#define RF_SCENE_TACTICAL_SCHEDULER_INCLUDED
/* Exactly eight global bytes: only cursor is persistent. Initialize/reset the
 * guard to frame-1 on section construction and final successful restoration. */
/* Globals are declared with the receipt type. */
/* Call once before the existing ordinary combat traversal. Cheap receipt
 * maintenance is separate and never budgeted. A chosen actor may subsequently
 * fail range/LOS/weapon admission; no second expensive candidate is selected
 * that frame. Newly acquired targets may wait until the following frame. */
static int scene_tactical_schedule(uint32_t frame,int32_t now,uint32_t *selected)
{
    uint32_t slot,target;int status,expired;
    if(!selected||(campaign_npc_body_count&&!campaign_npc_bodies)||
       campaign_npc_body_count>RF_NPC_CHECKPOINT_MAX_COUNT||
       (campaign_npc_body_count?scene_tactical_cursor>=campaign_npc_body_count:scene_tactical_cursor!=0))return RF_FORMAT;
    *selected=UINT32_MAX;
    if(scene_tactical_service_frame==frame)return RF_OK;
    if(!campaign_npc_body_count){scene_tactical_service_frame=frame;return RF_OK;}
    slot=scene_tactical_cursor;
    for(uint32_t i=0;i<campaign_npc_body_count;i++,slot=(slot+1u)%campaign_npc_body_count){
        campaign_npc_body *o=campaign_npc_bodies+slot;
        if(!(o->tactical.flags&SCENE_TACTICAL_PRESENT)||o->tactical.phase!=SCENE_TACTICAL_STATIONARY||
           !scene_tactical_service_allowed(o))continue;
        status=rf_timer_expired(o->tactical.phase_due,now,&expired);if(status)return status;
        if(!expired)continue;
        target=scene_blind_target_handle(o);
        if(target!=o->tactical.target||!scene_blind_target_live(target))continue;
        status=scene_tactical_owner(slot);if(status==RF_NOT_FOUND)continue;if(status)return status;
        scene_tactical_cursor=(slot+1u)%campaign_npc_body_count;
        scene_tactical_service_frame=frame;*selected=slot;return RF_OK;
    }
    scene_tactical_service_frame=frame;return RF_OK;
}
/* Explicit first-playable backpressure after a selection miss/blocked route.
 * It uses private RNG and leaves independent primary recovery untouched. */
static int scene_tactical_retry(scene_tactical_receipt *r,int32_t now)
{
    scene_tactical_receipt next;int status=scene_tactical_receipt_admit(r);if(status)return status;
    if(!(r->flags&SCENE_TACTICAL_PRESENT)||r->phase!=SCENE_TACTICAL_STATIONARY)return RF_FORMAT;
    next=*r;status=rf_timer_set_random(&next.phase_due,now,200,500,&next.random);if(status)return status;
    *r=next;return RF_OK;
}
#endif
