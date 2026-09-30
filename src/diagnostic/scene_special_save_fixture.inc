/* Optional Xbox-only save fixture. Grants use ordinary item services; shield
 * hits enter after contact admission, so this does not test mesh interception. */
uint32_t rf_scene_special_save_mode,rf_scene_special_save_probe[25];
static int scene_special_save_stage(uint32_t frame)
{
    uint32_t phase,*row,accepted,broken;int status;
    if(!rf_scene_special_save_mode)return RF_OK;
    if(frame==60 && rf_scene_special_save_mode==2){
        campaign_select_primary(12);campaign_ammo_publish();player_input.fire=1;return RF_OK;
    }
    if(frame!=30 && !(frame==40 && rf_scene_special_save_mode==2))return RF_OK;
    if(rf_scene_special_save_mode==1){
        campaign_item_grant grant={0};grant.weapon=campaign_shield_id;grant.quantity=1;grant.gives_weapon=1;
        status=campaign_apply_item_grant(&grant);if(status)return status;
        grant.weapon=campaign_fusion_id;status=campaign_apply_item_grant(&grant);if(status)return status;
        campaign_select_primary(11);campaign_ammo_publish();
    }
    phase=rf_scene_special_save_probe[0];if(phase>=3)return RF_RANGE;
    row=rf_scene_special_save_probe+1+phase*8;
    memcpy(row,&scene_player_shield_damage_state.damage.health,4);
    row[2]=campaign_player_inventory.owned[campaign_shield_id];row[4]=campaign_equipped_slot;
    row[6]=(uint32_t)campaign_player_inventory.loaded[campaign_fusion_id];row[7]=frame;
    {scene_player_shield_candidate contact={0};rf_damage_request request={0};
     contact.player=campaign_player_object.handle;contact.forward[2]=1;contact.travel[2]=-1;
     request.amount=frame==40?scene_npc_shields.definition.life*2:scene_player_shield_damage_state.damage.health*.5f;
     request.kind=-1;request.source=UINT32_MAX;
     status=scene_player_shield_commit(&contact,&request,&accepted,&broken);if(status)return status;
     if(!accepted || broken!=(frame==40))return RF_FORMAT;}
    memcpy(row+1,&scene_player_shield_damage_state.damage.health,4);
    row[3]=campaign_player_inventory.owned[campaign_shield_id];row[5]=campaign_equipped_slot;
    ++rf_scene_special_save_probe[0];return RF_OK;
}
