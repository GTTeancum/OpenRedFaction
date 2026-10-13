#ifndef RF_SCENE_BLIND_PURSUIT_CHECKPOINT_INCLUDED
#define RF_SCENE_BLIND_PURSUIT_CHECKPOINT_INCLUDED
/* Pure capture/candidate joins. No ray, age maintenance, target setter or live
 * receipt mutation is permitted before the composed storage-close boundary. */
static int scene_blind_live_receipt(uint32_t slot,uint32_t frame,uint32_t initializing,uint32_t *present,uint32_t *saved_age)
{
    const campaign_npc_body *o=campaign_npc_bodies+slot;float seconds;uint32_t target,age;int status;
    *present=*saved_age=0;
    if(!o->blind.present&&(o->blind.target||o->blind.last_seen))return RF_FORMAT;
    if(o->blind.present>1)return RF_FORMAT;
    target=scene_blind_target_handle(o);
    status=scene_blind_owner(slot,o->view.weapons[0],!!o->blind.present,&seconds);
    if(status!=RF_OK&&status!=RF_NOT_FOUND)return status;
    if(status==RF_NOT_FOUND||!target||target==o->registration.handle||!scene_blind_target_live(target))
        return o->blind.present?RF_FORMAT:RF_OK;
    if(!o->blind.present)return initializing?RF_OK:RF_FORMAT;
    if(o->blind.target!=target)return RF_FORMAT;
    age=frame-o->blind.last_seen;
    *present=1;*saved_age=age>scene_blind_age_limit(seconds)?scene_blind_age_limit(seconds):age;
    return RF_OK;
}
static int scene_blind_capture(uint32_t slot,uint32_t frame,uint32_t initializing,rf_npc_checkpoint_record *row)
{
    int status;row->blind_profile=1;
    status=scene_blind_live_receipt(slot,frame,initializing,&row->blind_present,&row->blind_age);if(status)return status;
    if(row->blind_present&&(row->retired||row->dead_pose||!row->combat_alert||
       (row->combat.active!=0&&row->combat.active!=2&&row->combat.active!=4)))return RF_FORMAT;
    return RF_OK;
}
static int scene_blind_live_present(uint32_t frame,uint32_t *present)
{
    uint32_t i,receipt,age;int status;
    if(!present||(campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;*present=0;
    for(i=0;i<campaign_npc_body_count;i++){
        status=scene_blind_live_receipt(i,frame,0,&receipt,&age);if(status)return status;*present|=receipt;
    }
    return RF_OK;
}
/* An absent legacy row has no elapsed receipt. It receives fresh grace only
 * when this source/target join succeeds. New-profile absence instead refuses
 * the same join, so a malformed RFNC37 row cannot replenish unseen time. */
static int scene_blind_row_target(uint32_t slot,const rf_npc_checkpoint_record *row,
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *target,float *seconds)
{
    uint32_t i,j,uid;int status;
    if(!row||!target||!seconds||(count&&!rows)||slot>=campaign_npc_body_count||
       row->uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
       row->class_id!=campaign_seeds.items[slot].class_index)return RF_FORMAT;
    *target=0;
    if(row->retired||row->dead_pose||row->health<=0||!row->combat_alert||
       (row->combat.active!=0&&row->combat.active!=2&&row->combat.active!=4))return RF_NOT_FOUND;
    status=scene_blind_source(slot,row->primary,!!row->blind_present,seconds);if(status)return status;
    if(campaign_npc_bodies[slot].view.flags_810&1u)return RF_FORMAT;
    uid=row->combat.active?row->combat.event:0;
    if(!uid){
        if(row->combat.active==4)return RF_FORMAT;
        if(!campaign_player_object.handle||campaign_player_object.view!=&campaign_player_view||
           rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)!=&campaign_player_object||
           rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view)return RF_FORMAT;
        *target=campaign_player_object.handle;return RF_OK;
    }
    for(i=0;i<count;i++)if(rows[i].uid==uid)break;
    if(i==count)return RF_NOT_FOUND; /* Vehicle/generated target belongs elsewhere. */
    if(rows[i].retired||rows[i].dead_pose||rows[i].health<=0||uid==row->uid)return RF_FORMAT;
    for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==uid)break;
    if(j==campaign_npc_body_count||scene_npc_reload_identity(j,campaign_npc_bodies+j)||
       (campaign_npc_bodies[j].view.flags_810&1u))return RF_FORMAT;
    *target=campaign_npc_bodies[j].registration.handle;return RF_OK;
}
static int scene_blind_candidate(uint32_t slot,const rf_npc_checkpoint_record *row,
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t frame,uint32_t *target,uint32_t *last_seen)
{
    float seconds=0;uint32_t resolved=0;int status;
    if(!target||!last_seen||row->blind_profile>1||row->blind_present>1||
       (!row->blind_present&&row->blind_age)||(!row->blind_profile&&row->blind_present))return RF_FORMAT;
    *target=*last_seen=0;status=scene_blind_row_target(slot,row,rows,count,&resolved,&seconds);
    if(status==RF_NOT_FOUND)return row->blind_present?RF_FORMAT:RF_OK;
    if(status)return status;
    /* Absence is enforced only after full composed RFNS/player qualification. */
    if(row->blind_present&&row->blind_age>scene_blind_age_limit(seconds))return RF_FORMAT;
    *target=resolved;*last_seen=frame-(row->blind_present?row->blind_age:0);return RF_OK;
}
static int scene_blind_incoming_present(const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *present)
{
    uint32_t i,j,target,last_seen;int status;
    if(!present||(count&&!rows))return RF_RANGE;*present=0;
    for(i=0;i<count;i++){
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count)return RF_FORMAT;
        status=scene_blind_candidate(j,rows+i,rows,count,0,&target,&last_seen);if(status)return status;
        *present|=!!target;
    }
    return RF_OK;
}
#endif
