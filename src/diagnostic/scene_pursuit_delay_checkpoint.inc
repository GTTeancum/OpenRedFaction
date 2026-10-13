#ifndef RF_SCENE_PURSUIT_DELAY_CHECKPOINT_INCLUDED
#define RF_SCENE_PURSUIT_DELAY_CHECKPOINT_INCLUDED
#include "scene_pursuit_delay_wire.inc"
static int scene_pursuit_delay_capture(uint32_t slot,int32_t now,rf_npc_checkpoint_record *row)
{
    const campaign_npc_body *o;uint32_t uid=0;int status;
    if(!row||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;row->pursuit_delay_profile=1;
    status=scene_pursuit_delay_admit(&o->pursuit_delay);if(status)return status;
    if(o->pursuit_delay.flags&SCENE_PURSUIT_DELAY_PRESENT){
        status=scene_tactical_source(slot);if(status)return status;
        if(row->retired||row->dead_pose||!row->blind_present||!o->blind.present||
           o->pursuit_delay.target!=o->blind.target||o->blind.target!=scene_blind_target_handle(o)||
           (o->pursuit_delay.flags>>SCENE_PURSUIT_DELAY_MODE_SHIFT)!=o->combat_scripted)return RF_FORMAT;
        uid=row->combat.active?row->combat.event:0u;
    }
    return scene_pursuit_delay_capture_wire(&o->pursuit_delay,uid,now,&row->pursuit_delay);
}
/* The already prepared Blind join resolves the persisted target UID to its
 * actual current generation. No raw saved runtime handle is trusted. */
static int scene_pursuit_delay_candidate(uint32_t slot,const rf_npc_checkpoint_record *row,
    uint32_t blind_target,int32_t now,scene_pursuit_delay_receipt *out)
{
    uint32_t target=0,mode=0;int status;
    if(!row||!out)return RF_RANGE;
    if(row->pursuit_delay.present){
        status=scene_tactical_source(slot);if(status)return status;
        if(row->retired||row->dead_pose||row->health<=0||!row->blind_present||!blind_target||
           !row->combat_alert||(row->combat.active!=0&&row->combat.active!=2&&row->combat.active!=4)||
           row->pursuit_delay.target_uid!=(row->combat.active?row->combat.event:0u))return RF_FORMAT;
        target=blind_target;mode=row->combat.active==2?1u:row->combat.active==4?2u:0u;
    }
    return scene_pursuit_delay_prepare_wire(&row->pursuit_delay,row->pursuit_delay_profile,target,mode,now,out);
}
#endif
