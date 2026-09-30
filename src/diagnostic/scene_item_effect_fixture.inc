/* Opt-in native fixture: only alters this game's armor state. World pickup,
 * cover, retirement, sound and scripted grant still use ordinary gameplay. */
uint32_t rf_scene_item_effect_mode,rf_scene_item_effect_test[18];
uint32_t rf_scene_vital_effect_test[24];
static int scene_vital_effect_stage(uint32_t frame)
{
    rf_damage_effect_state *v=&campaign_player_damage.state.effects;uint32_t *row;int status;
    if(frame==1){
        memset(rf_scene_vital_effect_test,0,sizeof(rf_scene_vital_effect_test));
        v->health=v->class_health=80;v->armor=v->class_armor=0;
        return campaign_set_invulnerable(NULL,campaign_player_object.handle,1);
    }
    if(frame==30)v->health=60;
    if(frame==50 || frame==60 || frame==70 || frame==75 || frame==80){
        const char *name=frame==50?"Medical Kit":frame==60?"First Aid Kit":"Suit Repair";
        if(frame==50){v->health=10;rf_scene_campaign_countdown.difficulty=0;}
        if(frame==60)rf_scene_campaign_countdown.difficulty=2;
        if(frame==75){v->armor=140;v->class_armor=150;rf_scene_campaign_countdown.difficulty=3;}
        status=campaign_give_item(campaign_triggers.give_item_context,name);if(status)return status;
    }
    if(frame!=25 && frame!=45 && frame!=65 && frame!=85)return RF_OK;
    row=rf_scene_vital_effect_test+(frame==25?0:frame==45?6:frame==65?12:18);
    memcpy(row,&v->health,4);memcpy(row+1,&v->armor,4);
    memcpy(row+2,&v->class_health,4);memcpy(row+3,&v->class_armor,4);
    row[4]=rf_scene_nonweapon_items[2];row[5]=rf_scene_pickups[3];return RF_OK;
}
static int scene_item_effect_stage(scene_stream *stream,uint32_t frame)
{
    rf_damage_effect_state *v=&campaign_player_damage.state.effects;
    uint32_t *row;int status;
    if(!rf_scene_item_effect_mode)return RF_OK;
    if(rf_scene_item_effect_mode==2)return scene_vital_effect_stage(frame);
    if(frame==1){
        memset(rf_scene_item_effect_test,0,sizeof(rf_scene_item_effect_test));
        if(v->class_armor<=0)return RF_FORMAT;
        v->armor=v->class_armor;
        return campaign_set_invulnerable(NULL,campaign_player_object.handle,1);
    }
    if(frame==30)v->armor=v->class_armor*.25f;
    if(frame==60 || frame==70){
        campaign_item_grant request={0};
        request.weapon=CAMPAIGN_ITEM_MINER_SUIT;request.quantity=1;
        memcpy(request.pickup_messages,stream->pickup_resources[SCENE_PICKUP_MINER_SUIT-1].definition.pickup_messages,sizeof(request.pickup_messages));
        if(frame==60)v->armor=v->class_armor*.5f;
        status=campaign_apply_item_grant(&request);if(status)return status;
    }
    if(frame!=25 && frame!=50 && frame!=75)return RF_OK;
    row=rf_scene_item_effect_test+(frame==25?0:frame==50?6:12);
    memcpy(row,&v->armor,4);memcpy(row+1,&v->class_armor,4);
    row[2]=rf_scene_pickups[3];row[3]=rf_scene_pickup_audio[1];
    row[4]=rf_scene_nonweapon_items[2];row[5]=rf_scene_pickups[1];
    return RF_OK;
}
