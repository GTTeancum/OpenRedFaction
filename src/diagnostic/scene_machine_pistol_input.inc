#ifndef RF_SCENE_MACHINE_PISTOL_INPUT_INCLUDED
#define RF_SCENE_MACHINE_PISTOL_INPUT_INCLUDED

/* Shared live presentation boundary for manual and accepted dry-fire requests.
 * Include after scene_weapon_empty_selection.inc. The candidate mode is only
 * published after its fallible custom-action operations succeed. The existing
 * owner retains all pending/due/completed mode and checkpoint responsibility. */
static int scene_machine_mode_apply(scene_stream *stream,uint32_t frame,const float position[3],
    const scene_machine_pistol_mode_state *next,const scene_machine_pistol_mode_event *event,
    uint32_t automatic)
{
    uint32_t was_pending=campaign_machine_mode.pending,i;int status;
    if(!stream || !position || !next || !event || automatic>1)return RF_RANGE;
    if(event->started){
        uint32_t source=campaign_machine_mode.special;
        status=scene_weapon_custom_actions_start(stream->machine_custom[source],
            stream->player_weapon[source?17:13],event->action==11);
        if(status){
            /* start may have transferred its resident payload before a pose
             * failure. Best-effort detach retains exact payload ownership;
             * propagate the original failure and never publish target mode.
             * If detach also fails, custom-active still vetoes checkpointing. */
            (void)scene_weapon_custom_actions_detach(stream->machine_custom[source],
                stream->player_weapon[source?17:13]);
            return status;
        }
        scene_machine_pistol_audio_close();
    }
    if(event->changed || (was_pending && !next->pending)){
        for(i=0;i<2;i++){
            status=scene_weapon_custom_actions_detach(stream->machine_custom[i],stream->player_weapon[i?17:13]);
            if(status)return status;
        }
        stream->player_pose_frame=0;stream->player_slot=UINT32_MAX;
    }
    campaign_machine_mode=*next;
    if(event->started){
        combat_sound(campaign_machine_mode.special?"Machine Pistol Switch":"Machine Pistol Switch Back",position);
        ++rf_scene_machine_mode[5];
        if(automatic)scene_player_auto_select_inputs(); /* Keeps real cooldown. */
        else {memset(&combat_trigger,0,sizeof(combat_trigger));combat_trigger.held=!!player_input.fire;}
    }
    if(event->changed){campaign_select_primary(13);campaign_ammo_publish();++rf_scene_machine_mode[6];}
    rf_scene_machine_mode[0]=campaign_machine_mode.special;rf_scene_machine_mode[1]=campaign_machine_mode.pending;
    rf_scene_machine_mode[2]=(uint32_t)event->weapon;
    rf_scene_machine_mode[3]=(uint32_t)campaign_player_inventory.loaded[event->weapon];
    {int32_t ammo=campaign_weapon_supply.definitions[event->weapon].ammo_type;
     rf_scene_machine_mode[4]=ammo>=0 && ammo<32?(uint32_t)campaign_player_inventory.reserve[ammo]:0;}
    if((event->started || event->changed) && rf_scene_combat_trace)printf("MACHINE_SWITCH %u %u %u %u %u %u\n",frame,
        event->started,event->changed,event->special,rf_scene_machine_mode[3],rf_scene_machine_mode[4]);
    return RF_OK;
}

static int scene_machine_mode_input(scene_stream *stream,uint32_t frame,const float position[3])
{
    scene_machine_pistol_mode_state next=campaign_machine_mode;
    scene_machine_pistol_mode_event event;
    uint32_t selected=!scene_driller_active(stream) && !scene_turret_player_active() && campaign_equipped_slot==13 && !campaign_explicit_unarmed;
    int status;
    if(!stream->machine_custom[0] || !stream->machine_custom[1])return RF_OK;
    status=scene_machine_pistol_mode_tick(&next,&campaign_player_inventory,
        campaign_extra_ids[0],campaign_machine_special_id,stream->machine_transition_ticks,frame,
        selected,campaign_player_damage.state.effects.health<=0 || rf_scene_combat[6] || player_input.reload,
        !!player_input.alt_fire,&event);
    if(!status)status=scene_machine_mode_apply(stream,frame,position,&next,&event,0);
    rf_scene_machine_mode[7]=(uint32_t)status;return status;
}
#endif
