#ifndef RF_SCENE_FLAME_INPUT_INC
#define RF_SCENE_FLAME_INPUT_INC
/* Include after scene_flame_gameplay.inc. Selected-slot caller must return
 * before generic trigger/reload/hitscan code: scene_flame_tick alone drains gas.
 * Sound requests use existing combat_sound; continuous voice ownership remains
 * presentation work. Alternate canister launch is not implemented here. */
static struct {uint32_t ignition,delay,reload_left,reload_elapsed,drained;} scene_flame_input;
/* Checkpoint does not serialize ignition/reload or fractional gas drain.
 * Completed reload bookkeeping is historical, so it must not block saving. */
static uint32_t scene_flame_input_pending(void)
{
    return scene_flame_input.ignition!=0 || scene_flame_input.delay!=0 ||
        scene_flame_input.reload_left!=0 || scene_flame_gas_remainder!=0;
}
static void scene_flame_input_reset(void)
{memset(&scene_flame_input,0,sizeof(scene_flame_input));scene_flame_reset();}
static void scene_flame_input_stop(const float position[3])
{
    if(scene_flame_input.ignition)combat_sound("Flame Thrower Ignite 3",position);
    scene_flame_input.ignition=scene_flame_input.delay=0;
}
static int scene_flame_input_tick(scene_stream *stream,uint32_t frame,const float position[3],
    const float forward[3],int32_t weapon,const rf_weapon_primary_definition *definition,
    float start_delay_seconds,uint32_t selected,uint32_t inhibited,uint32_t primary,uint32_t reload,uint32_t *active)
{
    const rf_weapon_acquire_definition *supply;uint32_t pulse=0,admitted=0,start_ticks,reload_ticks,drain_tick;int status;
    if(!stream || !position || !forward || !definition || !active || weapon<0 || weapon>=64 ||
       selected>1 || inhibited>1 || primary>1 || reload>1 || !isfinite(start_delay_seconds) ||
       start_delay_seconds<0 || start_delay_seconds>60 || !isfinite(definition->reload_seconds) ||
       definition->reload_seconds<=0 || definition->reload_seconds>60 || !isfinite(definition->reload_drain_seconds) ||
       definition->reload_drain_seconds<0 || definition->reload_drain_seconds>definition->reload_seconds)return RF_RANGE;
    supply=campaign_weapon_supply.definitions+weapon;
    if(supply->ammo_type<0 || supply->ammo_type>=32)return RF_RANGE;
    start_ticks=(uint32_t)ceilf(start_delay_seconds*60);
    reload_ticks=(uint32_t)ceilf(definition->reload_seconds*60);
    drain_tick=(uint32_t)ceilf(definition->reload_drain_seconds*60);
    *active=0;
    if(!selected || inhibited || !campaign_player_inventory.owned[weapon]) {
        scene_flame_input_stop(position);scene_flame_input.reload_left=scene_flame_input.reload_elapsed=scene_flame_input.drained=0;
        if(selected)rf_scene_combat[6]=0;
        return scene_flame_tick(stream,frame,position,forward,weapon,definition,0,&pulse);
    }
    if(scene_flame_input.reload_left) {
        scene_flame_input_stop(position);++scene_flame_input.reload_elapsed;
        if(!scene_flame_input.drained && scene_flame_input.reload_elapsed>=drain_tick) {
            campaign_player_inventory.loaded[weapon]=0;scene_flame_gas_remainder=0;scene_flame_input.drained=1;campaign_ammo_publish();
        }
        if(!--scene_flame_input.reload_left) {
            uint32_t moved;status=rf_weapon_reload_transfer(&campaign_player_inventory,supply,weapon,&moved);
            if(status)return status;scene_flame_gas_remainder=0;
            rf_scene_player_ammo[3]+=moved;++rf_scene_player_ammo[4];campaign_ammo_publish();
        }
        rf_scene_combat[6]=scene_flame_input.reload_left;
    } else if((reload || (primary && !campaign_player_inventory.loaded[weapon])) &&
        campaign_player_inventory.loaded[weapon]<supply->magazine && campaign_player_inventory.reserve[supply->ammo_type]>0) {
        scene_flame_input_stop(position);scene_flame_input.reload_left=reload_ticks;
        scene_flame_input.reload_elapsed=scene_flame_input.drained=0;
        rf_scene_combat[6]=reload_ticks;combat_sound("Flame Reload",position);
    } else if(!primary || !campaign_player_inventory.loaded[weapon])scene_flame_input_stop(position);
    else {
        if(!scene_flame_input.ignition) {
            scene_flame_input.ignition=1;scene_flame_input.delay=start_ticks;
            combat_sound("Flame Thrower Ignite 1",position);
        } else if(scene_flame_input.delay)--scene_flame_input.delay;
        if(!scene_flame_input.delay) {
            if(scene_flame_input.ignition==1)combat_sound("Flame Thrower Ignite 2",position);
            scene_flame_input.ignition=2;admitted=1;
        }
    }
    status=scene_flame_tick(stream,frame,position,forward,weapon,definition,admitted,&pulse);if(status)return status;
    *active=admitted;
    if(pulse){uint32_t alerted;++rf_scene_combat[0];campaign_last_alt=0;
        status=campaign_enemy_hear_shot(position,16.0f,frame,&alerted);if(status)return status;}
    return RF_OK;
}
#endif
