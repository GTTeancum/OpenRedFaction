#ifndef RF_SCENE_TACTICAL_CHECKPOINT_INCLUDED
#define RF_SCENE_TACTICAL_CHECKPOINT_INCLUDED
#include "scene_tactical_receipt_checkpoint.inc"
#include "scene_tactical_route_checkpoint.inc"
/* Scene capture is read-only. Qualification failure cannot silently drop an
 * existing recovery or route. New-profile absence remains genuine absence. */
static int scene_tactical_capture(uint32_t slot,int32_t now,rf_npc_checkpoint_record *row)
{
    const campaign_npc_body *o;uint32_t target,mode;int status;
    if(!row||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;row->tactical_profile=1;
    status=scene_tactical_receipt_capture(&o->tactical,now,&row->tactical);if(status)return status;
    if(!row->tactical.present)return RF_OK;
    status=scene_tactical_owner(slot);if(status)return status;
    if(o->tactical.flags&SCENE_TACTICAL_PRESENT){
        status=scene_patrol_resources(slot,&o->selection,0);if(status)return status;
        target=scene_blind_target_handle(o);mode=o->tactical.flags>>SCENE_TACTICAL_MODE_SHIFT;
        if(!target||target!=o->tactical.target||!scene_blind_target_live(target)||
           mode!=o->combat_scripted||!row->combat_alert||
           row->combat.active!=(mode==1?2u:mode==2?4u:0u))return RF_FORMAT;
    }
    return scene_tactical_saved_route(row,campaign_navigation.references,campaign_navigation.count,
        o->navigation_radius,o->death_item_extent_7c4);
}
static int scene_tactical_live_present(uint32_t *present)
{
    uint32_t value=0;int status;
    if(!present||(campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        status=scene_tactical_receipt_admit(&campaign_npc_bodies[i].tactical);if(status)return status;
        if(campaign_npc_bodies[i].tactical.flags){
            status=scene_tactical_owner(i);if(status)return status;value=1;
        }
    }
    *present=value;return RF_OK;
}
/* Prepared generation-aware target only. Candidate player/seat/attachment
 * ownership must still join the complete RFNS/player transaction before any
 * irreversible close. Do not mark the stage joined merely from this result. */
static int scene_tactical_candidate_target(const rf_npc_checkpoint_record *row,
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *target,uint32_t *mode)
{
    uint32_t uid,i,j;
    if(!row||!target||!mode||(count&&!rows)||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items))||
       campaign_npc_body_count>campaign_seeds.records.count)return RF_RANGE;
    *target=*mode=0;
    if(!row->tactical.present||!row->tactical.phase)return RF_OK;
    if(!row->combat_alert||(row->combat.active!=0&&row->combat.active!=2&&row->combat.active!=4))return RF_FORMAT;
    uid=row->combat.active?row->combat.event:0;
    *mode=row->combat.active==2?1u:row->combat.active==4?2u:0u;
    if(!uid){
        if(row->combat.active==4||!campaign_player_object.handle||campaign_player_object.handle==UINT32_MAX||
           campaign_player_object.view!=&campaign_player_view||
           rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)!=&campaign_player_object||
           rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view)return RF_FORMAT;
        *target=campaign_player_object.handle;return RF_OK;
    }
    if(uid==row->uid)return RF_FORMAT;
    for(i=0;i<count;i++)if(rows[i].uid==uid)break;
    if(i==count||rows[i].retired||rows[i].dead_pose||rows[i].health<=0)return RF_FORMAT;
    for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==uid)break;
    if(j==campaign_npc_body_count||scene_npc_reload_identity(j,campaign_npc_bodies+j))return RF_FORMAT;
    *target=campaign_npc_bodies[j].registration.handle;return RF_OK;
}
static int scene_tactical_candidate(uint32_t slot,const rf_npc_checkpoint_record *row,
    const rf_npc_checkpoint_record *rows,uint32_t count,int32_t now,scene_tactical_receipt *prepared)
{
    uint32_t target,mode;int status;
    if(!row||!prepared||!campaign_npc_bodies||slot>=campaign_npc_body_count||!campaign_seeds.items||!campaign_seeds.records.items||
       slot>=campaign_seeds.records.count||row->uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
       row->class_id!=campaign_seeds.items[slot].class_index)return RF_FORMAT;
    if(row->tactical.present){
        status=scene_tactical_source(slot);if(status)return status;
        if(row->retired||row->dead_pose||row->health<=0)return RF_FORMAT;
        status=scene_tactical_saved_route(row,campaign_navigation.references,campaign_navigation.count,
            campaign_npc_bodies[slot].navigation_radius,campaign_npc_bodies[slot].death_item_extent_7c4);if(status)return status;
    }
    status=scene_tactical_candidate_target(row,rows,count,&target,&mode);if(status)return status;
    return scene_tactical_receipt_prepare(&row->tactical,row->tactical_profile,target,mode,now,prepared);
}
#endif
