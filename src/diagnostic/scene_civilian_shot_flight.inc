#ifndef RF_SCENE_CIVILIAN_SHOT_FLIGHT_INCLUDED
#define RF_SCENE_CIVILIAN_SHOT_FLIGHT_INCLUDED
/* Original loud-shot426afe..426b0c ->408280, unarmed40852e..408550.
 * One accepted loud on-foot player shot requests one10..20 second episode.
 * This is a bounded consumer of the existing civilian flight owner, not a
 * synthetic damage notification or a new perception/target/persistence bank.
 * Include after scene_medic_flight.inc. Armed hearing and its alert count are
 * independent. Radius remains the existing16-unit port hearing policy. */
static uint32_t scene_civilian_noise_owner(const campaign_npc_body *o)
{
    uint32_t slot,cls,flags;
    if(!scene_civilian_flight_owner(o)||!scene_medic_flight_live(o)||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||
       (o->object_flags&(2u|0x4000u))||(o->view.flags_810&1u)||
       o->registration.handle==campaign_player_object.handle||
       o->ai_mode.action_280!=2||o->view.action_520!=2||o->flight.entered||
       o->damage.effects.affiliation==2||
       ((o->view.flags_814|o->damage.effects.flags_814)&0x10u)||
       o->script_animation.active||o->script_animation.freeze||
       o->scripted_physics||o->scripted_physics_seen||o->attachment_75c!=-1||
       scene_npc_seat_attached(o->registration.handle)||scene_medic_flight_overlap(o)||
       o->movement_slot>=16||(campaign_modes[o->movement_slot].index!=1&&
       campaign_modes[o->movement_slot].index!=3))return 0;
    /*408346/389/418: suppressed Goto, noise exclusion800 and800000.
     * Waiting-only is the current authored family; active flight does not
     * restart on every automatic round (original peers admit only2/4). */
    flags=o->ai_mode.flags_530|o->view.flags_7d0|o->firing.flags_7d0;
    if(flags&(SCENE_NPC_GOTO_AI_SUPPRESSION|0x800u|0x800000u))return 0;
    slot=(uint32_t)(o-campaign_npc_bodies);
    if(!campaign_seeds.items||!campaign_seeds.classes||slot>=campaign_seeds.records.count)return 0;
    cls=campaign_seeds.items[slot].class_index;if(cls>=campaign_seeds.class_count)return 0;
    /*408367/378 reject turret classes and zero class mass. Existing flight
     * qualification already proves grounded/sentient/humanoid/no Alarm. */
    return !(campaign_seeds.classes[cls].physics.flags&0x2000u)&&
        isfinite(campaign_seeds.classes[cls].physics.mass)&&campaign_seeds.classes[cls].physics.mass>0;
}
static int scene_civilian_hear_player_shot(scene_stream *stream,const float position[3],
    float radius,uint32_t frame,uint32_t source,uint64_t epoch)
{
    scene_medic_flight_threat_view threat={0};scene_npc_reload_player_receipt player;
    int32_t now;float radius_squared;int status;
    if(!stream||!position||!isfinite(radius)||radius<=0||radius>100)return RF_RANGE;
    for(uint32_t j=0;j<3;j++)if(!isfinite(position[j]))return RF_RANGE;
    if(!frame||scene_medic_flight_restoring||campaign_cutscene_runtime.active||
       campaign_endgame.phase||epoch!=scene_continuous_charge_runtime.epoch||
       source!=campaign_player_object.handle||!isfinite(campaign_player_damage.state.effects.health)||
       campaign_player_damage.state.effects.health<=0)return RF_OK;
    /* Same actual on-foot/registry/body/possession admission used by civilian
     * damage flight; never reinterpret mounted or replaced-player shots. */
    status=scene_npc_reload_player_admit(stream,&player);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(source!=player.player||stream!=scene_actor_collision_owner)return RF_OK;
    threat.handle=source;threat.view=&campaign_player_view;
    memcpy(threat.position,rf_scene_actor_pose.public_position,12);
    status=scene_medic_player_eye(frame,threat.eye);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    threat.model_radius=campaign_player_geometry.model_radius;
    radius_squared=radius*radius;now=scene_medic_now(frame);
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;uint32_t supported,handle;
        uint64_t lifetime;float distance=0,seconds;
        if(!scene_civilian_noise_owner(o))continue;
        /*4083fc compares the actor's published origin with the actual noise
         * origin. No sight cone/clear-ray requirement is added to hearing. */
        for(uint32_t j=0;j<3;j++){
            float delta=o->published[j]-position[j];distance+=delta*delta;
        }
        if(!isfinite(distance)||distance>radius_squared)continue;
        /* Capability admission, not a visible-only gate. The existing flight
         * visibility/navigation owns later obstructed-threat behavior. */
        supported=0;status=scene_medic_flight_visibility_admit(stream,o,&threat,&supported);
        if(status)return status;if(!supported)continue;
        handle=o->registration.handle;lifetime=scene_medic_flight_lifetime;
        status=scene_civilian_flight_resources(i);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(epoch!=scene_continuous_charge_runtime.epoch||source!=campaign_player_object.handle||
           scene_npc_reload_player_seal(stream,&player))return RF_OK;
        if(lifetime!=scene_medic_flight_lifetime||o!=campaign_npc_bodies+i||
           o->registration.handle!=handle||!scene_civilian_noise_owner(o))continue;
        /*40853f stores504e40's result as binary32 before4085f0 truncates
         * seconds*1000. Exactly one duration draw/request, unlike damage's
         * distinct two60..120-second requests. Preflight draws nothing. */
        seconds=(float)(10.0+(double)scene_medic_flight_random()*(10.0/32768.0));
        rf_scene_medic_flight[10]=source;
        status=scene_medic_flight_request(stream,o,&threat,now,seconds);if(status)return status;
        /* A cancelled/replaced world cannot lend the next civilian an old
         * noise event. Accepted effects remain owned, with no replay. */
        if(epoch!=scene_continuous_charge_runtime.epoch||source!=campaign_player_object.handle||
           scene_npc_reload_player_seal(stream,&player))return RF_OK;
    }
    return RF_OK;
}
#endif
