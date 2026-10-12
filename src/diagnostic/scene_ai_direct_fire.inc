#ifndef RF_SCENE_AI_DIRECT_FIRE_INCLUDED
#define RF_SCENE_AI_DIRECT_FIRE_INCLUDED
/* Laser and Sonar accept an immediate prepaid flight. Keep the inventory,
 * cadence and once-request endpoint complete before action/audio callbacks.
 * This is the existing launch consumer, not a replay or a new firing mode. */
static uint32_t scene_ai_direct_checkpoint_busy(void)
{
    return scene_ai_direct_service_depth||scene_ai_laser_updating||scene_ai_sonar_updating;
}
static int scene_ai_direct_fire_owned(uint32_t slot,uint32_t frame,const float target[3],
    uint32_t once,uint32_t mode)
{
    campaign_enemy_weapon_selection selected={0};campaign_npc_body *owner;
    uint32_t burst,remaining,due,sonar;int status;
    if(!target||slot>=campaign_npc_body_count||once>1||mode>2)return RF_RANGE;
    owner=campaign_npc_bodies+slot;sonar=campaign_enemy_sonar_weapon(owner->view.weapons[0]);
    status=sonar?campaign_enemy_sonar_select(owner,&campaign_weapon_supply,&selected):
        campaign_enemy_laser_select(owner,&campaign_weapon_supply,&selected);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    burst=remaining=owner->combat_burst_remaining;due=owner->combat_due;
    if(!once){status=campaign_enemy_cadence(selected.primary,frame,&remaining,&due);if(status)return status;}
    /* Both launches are callback-free through debit and publication. A full
     * pool preserves ammo, spread RNG, the once FIFO and accepted cadence. */
    status=sonar?scene_ai_sonar_launch(owner,target):scene_ai_laser_launch(owner,target);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(!once){owner->combat_burst_remaining=remaining;owner->combat_due=due;}
    if(owner->combat_scripted==3&&!once)++rf_scene_script_shoot_at[3];
    if(once)campaign_script_single_pop(owner,1);
    ++rf_scene_enemy_combat[2];
    if(rf_scene_combat_trace)printf("ENEMY_SHOT_TRACE %u %u\n",frame,campaign_seeds.records.items[slot].record.uid);
    /* Never dereference an outgoing source or FIFO after a callback replaces
     * the world. The accepted flight now owns the eventual hit separately. */
    status=campaign_enemy_fire_presentation(slot,!(once&&mode==2),selected.primary,burst,once);
    rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND){++rf_scene_enemy_fire[2];return RF_OK;}
    return status;
}
static int scene_ai_direct_fire(uint32_t slot,uint32_t frame,const float target[3],uint32_t once,uint32_t mode)
{
    int status;if(scene_ai_direct_service_depth)return RF_NOT_FOUND;
    ++scene_ai_direct_service_depth;status=scene_ai_direct_fire_owned(slot,frame,target,once,mode);
    --scene_ai_direct_service_depth;return status;
}
#endif
