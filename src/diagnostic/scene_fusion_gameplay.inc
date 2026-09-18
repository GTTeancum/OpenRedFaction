#include "scene_fusion_input.inc"
#include "scene_fusion_projectile.inc"
static scene_fusion_input_state scene_fusion_input;
typedef struct scene_fusion_launch_context {const float *position,*forward;} scene_fusion_launch_context;
static int scene_fusion_input_launch(void *context,uint32_t *spawned)
{
    scene_fusion_launch_context *c=context;
    return scene_fusion_launch(c->position,c->forward,campaign_player_object.handle,
        campaign_primary[12].damage,&campaign_fusion,spawned);
}
static int scene_fusion_gameplay_tick(scene_stream *s,uint32_t frame,const float position[3],const float forward[3])
{
    scene_fusion_input_event event;scene_fusion_launch_context context={position,forward};int status;
    uint32_t selected=campaign_equipped_slot==12 && !campaign_explicit_unarmed;
    if(!frame){scene_fusion_input_reset(&scene_fusion_input);scene_fusion_projectile_reset();}
    status=scene_fusion_projectile_tick(s,frame,scene_fusion_impact_start);if(status)return status;
    status=scene_fusion_input_tick(&scene_fusion_input,&campaign_player_inventory,
        campaign_weapon_supply.definitions+campaign_fusion_id,campaign_fusion_id,&campaign_primary[12],frame,
        selected,campaign_player_damage.state.effects.health<=0,!!player_input.fire,!!player_input.alt_fire,
        !!player_input.reload,scene_fusion_input_launch,&context,&event);if(status)return status;
    if(event.shot){++rf_scene_combat[0];campaign_last_alt=0;combat_sound("Shoulder Launch",position);}
    if(event.reload_started)combat_sound("Shoulder Reload",position);
    if(selected){rf_scene_combat[6]=scene_fusion_input.reloading?scene_fusion_input.reload_due-frame:0;campaign_ammo_publish();}
    return RF_OK;
}
