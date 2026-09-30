#ifndef SCENE_BURNING_SAVE_FIXTURE_INC
#define SCENE_BURNING_SAVE_FIXTURE_INC
/* Opt-in process-local flame damage admission fixture. Stages armor and AI
 * only, then uses the ordinary damage/effect path to ignite the authored NPC. */
uint32_t rf_scene_burning_test_uid;
/* target UID, health, class health, burn slot, left, phase, source, frame */
uint32_t rf_scene_burning_live[8];
static int scene_burning_save_fixture(uint32_t frame)
{
    uint32_t i,bits;float seconds=(float)frame/60,amount=0;int status;
    campaign_npc_body *owner=NULL;combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    rf_damage_request request={1,campaign_player_object.handle,4,0,UINT32_MAX,0};
    if(!rf_scene_burning_test_uid||frame!=30||rf_scene_burning_restored[0])return RF_OK;
    for(i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==rf_scene_burning_test_uid){owner=campaign_npc_bodies+i;break;}
    if(!owner||!owner->registration.view||owner->damage.effects.health<=0)return RF_NOT_FOUND;
    status=campaign_ai_mode_apply(owner,1,feedback.now);if(status)return status;
    owner->damage.effects.armor=0;memcpy(&bits,&seconds,4);
    status=rf_scene_npc_damage(owner->registration.handle,&request,1,bits,&effects,&amount);
    if(!status)status=feedback.status;if(status)return status;
    return owner->damage.effects.burn?RF_OK:RF_FORMAT;
}
static void scene_burning_save_probe(uint32_t frame)
{
    uint32_t i,uid=rf_scene_burning_test_uid?rf_scene_burning_test_uid:rf_scene_burning_restored[1];
    if(!uid)return;
    for(i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){
        campaign_npc_body *owner=campaign_npc_bodies+i;uint32_t slot=owner->damage.effects.burn;
        rf_scene_burning_live[0]=uid;memcpy(rf_scene_burning_live+1,&owner->damage.effects.health,4);
        memcpy(rf_scene_burning_live+2,&owner->damage.effects.class_health,4);rf_scene_burning_live[3]=slot;
        rf_scene_burning_live[4]=slot&&slot<=SCENE_BURNING_CAPACITY?scene_burning_records[slot-1].left:0;
        rf_scene_burning_live[5]=slot&&slot<=SCENE_BURNING_CAPACITY?scene_burning_records[slot-1].phase:0;
        rf_scene_burning_live[6]=owner->damage.burn_source;rf_scene_burning_live[7]=frame;return;
    }
}
#endif
