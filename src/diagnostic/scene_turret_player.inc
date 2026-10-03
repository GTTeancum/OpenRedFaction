#ifndef RF_SCENE_TURRET_PLAYER_INCLUDED
#define RF_SCENE_TURRET_PLAYER_INCLUDED
#include <float.h>
/* Mountable authored stationary turrets only ($Use turret ==4). Auto Turret
 * base/head have use-kind none and are never candidates. First-pass static
 * emplacement policy suppresses voluntary movement and redirects the camera
 * to the eye tag. Ordinary physical contacts/support remain active; neither
 * entry nor exit teleports the body or restores a stale preboarding pose.
 * This deliberately defers an exact seated-body animation/interface pose.
 * Include after turret scene shot backend and actor/player state. */
typedef struct scene_turret_player_state {
    uint32_t active,host,player,use_held,fire_held,last_frame,due,burst,release_mask;
    int32_t occupant,saved_link;
    float desired[3],anchor[3],eye_offset[3];
    rf_scene_input input;
} scene_turret_player_state;
static scene_turret_player_state scene_turret_player;
/* entries,exits,active,hostUID,aimticks,shots,blocked entry,errors,
 * playerhandle,hosthandle,lastframe,exitreason */
uint32_t rf_scene_turret_player[12];
/* Last sampled owner: active,UID,health,playerhealth,occupants,weaponowner,
 * playerlink,hostlink,body xyz,eye xyz,shots,frame,boarding body xyz.
 * Float fields are bitcasts. */
uint32_t rf_scene_turret_player_probe[19];
/* Existing ordinary player awareness service is defined later in scene.c. */
static int campaign_enemy_hear_shot(const float position[3],float radius,uint32_t frame,uint32_t *alerted);

static uint32_t scene_turret_player_active(void)
{return scene_turret_player.active;}
static uint32_t scene_turret_player_controls(uint32_t host)
{
    scene_turret_owner *o;
    if(!scene_turret_player.active||scene_turret_player.host!=host)return 0;
    o=scene_turret_lookup(host);
    return o&&o->view.weapon_owner==&campaign_player_view&&
        o->view.occupants==&scene_turret_player.occupant&&o->view.occupant_count==1&&
        scene_turret_player.occupant==(int32_t)scene_turret_player.player&&
        campaign_player_view.linked_handle==(int32_t)host&&
        scene_turret_player.player==campaign_player_object.handle&&
        rf_object_registry_lookup(&campaign_registry,scene_turret_player.player)==&campaign_player_object&&
        rf_entity_lookup(&campaign_entities,(int32_t)scene_turret_player.player)==&campaign_player_view;
}
/* UINT_MAX means AI. Shot collision still uses host; damage attribution uses
 * this player handle, and the player's own body/shield must be excluded. */
static uint32_t scene_turret_player_source(uint32_t host)
{return scene_turret_player_controls(host)?scene_turret_player.player:UINT32_MAX;}
/* Rendering/audio use the turret eye. NPC aim, pickups and the player's
 * shield still belong to the actual physical body at its current position. */
static uint32_t scene_turret_player_body_eye(float out[3])
{
    if(!out||!scene_turret_player_controls(scene_turret_player.host))return 0;
    for(uint32_t k=0;k<3;k++)out[k]=scene_actor_body.state.position[k]+scene_turret_player.eye_offset[k];
    return 1;
}
static void scene_turret_player_filter(rf_scene_input *input)
{
    scene_turret_player.input=*input;
    if(scene_turret_player.active)memset(input,0,sizeof(*input));
    else {
        /* Ordinary weapon/vehicle latches saw zero while mounted. Require a
         * release before handing an already held mount control back to them. */
        if(!input->use)scene_turret_player.release_mask&=~1u;
        if(!input->fire)scene_turret_player.release_mask&=~2u;
        if(!input->alt_fire)scene_turret_player.release_mask&=~4u;
        if(scene_turret_player.release_mask&1u)input->use=0;
        if(scene_turret_player.release_mask&2u)input->fire=0;
        if(scene_turret_player.release_mask&4u)input->alt_fire=0;
    }
}
static int scene_turret_player_camera(float position[3],float orientation[3][3])
{
    scene_turret_owner *o=scene_turret_lookup(scene_turret_player.host);uint32_t affiliation,alive;int status;
    if(!scene_turret_player_controls(scene_turret_player.host)||!o)return RF_NOT_FOUND;
    status=scene_turret_target(o->registration.handle,position,&affiliation,&alive);if(status)return status;
    if(!alive)return RF_NOT_FOUND;
    memcpy(orientation,o->basis,36);return RF_OK;
}
static int scene_turret_player_exit(uint32_t reason)
{
    scene_turret_player_state *p=&scene_turret_player;scene_turret_owner *o;
    if(!p->active)return RF_OK;o=scene_turret_lookup(p->host);
    if(o){
        uint32_t owned=o->view.weapon_owner==&campaign_player_view;
        if(owned)o->view.weapon_owner=NULL;
        if(o->view.occupants==&p->occupant){o->view.occupants=NULL;o->view.occupant_count=0;}
        if(owned){
            if(scene_turret_combat_ready)scene_turret_combat_release(o,scene_turret_combat_states+(o-scene_turrets));
            combat_sound("Turret Detach",o->position);
        }
    }
    if(campaign_player_object.handle==p->player&&campaign_player_view.linked_handle==(int32_t)p->host)
        campaign_player_view.linked_handle=p->saved_link;
    /* No clearance relocation is necessary: control never moved or resized
     * this body, and ordinary collision continues while mounted. Do not copy
     * a saved look/body over a newer teleport, generation or controller. */
    p->release_mask|=(p->input.use?1u:0u)|(p->input.fire?2u:0u)|(p->input.alt_fire?4u:0u);
    p->active=0;p->occupant=-1;p->due=p->burst=0;
    ++rf_scene_turret_player[1];rf_scene_turret_player[2]=0;rf_scene_turret_player[11]=reason;
    return RF_OK;
}
/* Exit before owner teardown; reset once on fresh setup so teardown retains
 * the final telemetry for the bounded native harness. */
static void scene_turret_player_reset(void)
{
    (void)scene_turret_player_exit(3);memset(&scene_turret_player,0,sizeof(scene_turret_player));
    scene_turret_player.host=scene_turret_player.player=UINT32_MAX;
    scene_turret_player.occupant=-1;scene_turret_player.last_frame=UINT32_MAX;
    memset(rf_scene_turret_player,0,sizeof(rf_scene_turret_player));
    memset(rf_scene_turret_player_probe,0,sizeof(rf_scene_turret_player_probe));
}
static int scene_turret_player_select(scene_stream *stream,uint32_t *handle)
{
    float best=FLT_MAX;uint32_t selected=UINT32_MAX;int status;
    if(!stream||!handle)return RF_RANGE;
    for(uint32_t i=0;i<scene_turret_authored_count;i++) {
        scene_turret_owner *o=scene_turrets+i;const rf_entity_class_physics *c=&campaign_seeds.classes[o->class_index].physics;
        float point[3],delta[3],distance=0,dot=0;uint32_t affiliation,alive,blocked;
        if(scene_turret_lookup(o->registration.handle)!=o||
           c->use_kind!=4||!isfinite(c->use_radius)||c->use_radius<=0||o->dead||
           !scene_turret_primary_for(o->view.weapons[0])||
           o->view.weapon_owner||o->view.occupant_count||o->view.linked_handle!=-1||
           scene_turret_operator_find(o->registration.handle)||scene_turret_generated_find(o->registration.handle))continue;
        status=scene_turret_target(o->registration.handle,point,&affiliation,&alive);if(status)return status;
        if(!alive)continue;
        for(uint32_t k=0;k<3;k++) {
            float body_delta=o->position[k]-scene_actor_body.state.position[k];
            delta[k]=point[k]-campaign_listener_position[k];dot+=delta[k]*actor_look.eye_orientation[6+k];
            distance+=body_delta*body_delta;
        }
        if(distance>c->use_radius*c->use_radius||distance>=best||dot<=0)continue;
        status=combat_shot_obstructed(stream,campaign_listener_position,delta,1,&blocked);if(status)return status;
        if(blocked){++rf_scene_turret_player[6];continue;}
        selected=o->registration.handle;best=distance;
    }
    *handle=selected;return RF_OK;
}
static int scene_turret_player_tick(scene_stream *stream,uint32_t frame)
{
    scene_turret_player_state *p=&scene_turret_player;scene_turret_owner *o=NULL;
    scene_turret_combat_state *aim;uint32_t edge,alive,handle,reachable,affiliation;int status;
    float eye[3],matrix[9],direction[3],point[3],pose[12],ray[3];
    if(p->last_frame==frame)return RF_OK;p->last_frame=frame;
    edge=p->input.use&&!p->use_held;p->use_held=!!p->input.use;
    alive=scene_turret_scene_player_live();
    if(p->active){
        o=scene_turret_lookup(p->host);
        uint32_t owns=scene_turret_player_controls(p->host),out_of_range=0;
        if(o&&owns&&alive){
            float distance=0,range=campaign_seeds.classes[o->class_index].physics.use_radius;
            for(uint32_t k=0;k<3;k++){
                float delta=scene_actor_body.state.position[k]-o->position[k];distance+=delta*delta;
            }
            out_of_range=!isfinite(distance)||!isfinite(range)||range<=0||distance>(range+.05f)*(range+.05f);
        }
        if(!o||!owns||out_of_range||o->dead||o->damage.effects.health<=0||(o->view.flags_7c&(2u|0x4000u))||!alive||edge||
           campaign_cutscene_runtime.active||campaign_endgame.phase||campaign_defuse.active){
            status=scene_turret_player_exit(!alive?2:!owns?4:!o||o->dead?1:out_of_range?5:0);if(status)goto fail;o=NULL;
        }
    }else if(frame&&edge&&alive&&scene_turret_combat_ready&&!scene_driller_active(stream)&&
        !campaign_cutscene_runtime.active&&!campaign_endgame.phase&&!campaign_defuse.active&&
        campaign_player_view.linked_handle==-1) {
        status=scene_turret_player_select(stream,&handle);if(status)goto fail;
        if(handle!=UINT32_MAX){
            o=scene_turret_lookup(handle);if(!o){status=RF_NOT_FOUND;goto fail;}
            p->active=1;p->host=handle;p->player=campaign_player_object.handle;
            p->saved_link=campaign_player_view.linked_handle;
            memcpy(p->anchor,scene_actor_body.state.position,12);p->occupant=(int32_t)p->player;
            for(uint32_t k=0;k<3;k++)p->eye_offset[k]=campaign_listener_position[k]-p->anchor[k];
            o->view.occupants=&p->occupant;o->view.occupant_count=1;o->view.weapon_owner=&campaign_player_view;
            campaign_player_view.linked_handle=(int32_t)handle;
            aim=scene_turret_combat_states+(o-scene_turrets);scene_turret_combat_release(o,aim);
            memcpy(p->desired,aim->angles,12);p->due=p->burst=p->fire_held=0;
            /* Cancel queued handheld rounds, retaining its cooldown and the
             * independently owned reload state. */
            combat_trigger.remaining=combat_trigger.delay=0;combat_trigger.held=!!p->input.fire;
            ++rf_scene_turret_player[0];rf_scene_turret_player[2]=1;rf_scene_turret_player[3]=o->uid;
            rf_scene_turret_player[8]=p->player;rf_scene_turret_player[9]=handle;
            memset(scene_actor_body.state.velocity,0,12);combat_sound("Turret Attach",o->position);
            /* Filtering ran before entry this frame. Consume this Use/fire/
             * movement now too, before later ordinary player dispatch. */
            memset(&player_input,0,sizeof(player_input));
        }
    }
    if(p->active) {
        const rf_entity_seed_class *cls=campaign_seeds.classes+o->class_index;
        const rf_weapon_primary_definition *primary=scene_turret_primary_for(o->view.weapons[0]);
        if(!primary){status=RF_NOT_FOUND;goto fail;}
        aim=scene_turret_combat_states+(o-scene_turrets);
        for(uint32_t k=0;k<2;k++) {
            if(!isfinite(p->input.look[k])||fabsf(p->input.look[k])>1){status=RF_FORMAT;goto fail;}
            p->desired[k]+=p->input.look[k]*cls->rotation.maximum_velocity/60.f;
            if(k==1&&cls->eye_limits.maximum[k]-cls->eye_limits.minimum[k]>=6.2831854820251465f)
                p->desired[k]=scene_turret_angle_error(p->desired[k],0);
            p->desired[k]=fmaxf(cls->eye_limits.minimum[k],fminf(cls->eye_limits.maximum[k],p->desired[k]));
        }
        status=rf_look_orientation(p->desired,matrix);if(status)goto fail;
        scene_turret_world_vector(aim->mount,matrix+6,direction);
        status=scene_turret_target(p->host,eye,&affiliation,&alive);if(status)goto fail;
        for(uint32_t k=0;k<3;k++)point[k]=eye[k]+direction[k]*100;
        status=scene_turret_combat_turn(o,aim,eye,point,&reachable);if(status)goto fail;
        ++rf_scene_turret_player[4];
        if(p->input.fire&&(!primary->semi_automatic||!p->fire_held)&&frame>=p->due) {
            scene_turret_scene_context context={stream,frame,{0}};uint32_t remaining=p->burst,due;
            status=scene_turret_target(p->host,eye,&affiliation,&alive);if(status)goto fail;
            status=scene_turret_model_tag(o->live_model,"muzzle_1",o->position,o->basis,pose);if(status)goto fail;
            memcpy(context.player_eye,eye,12);
            for(uint32_t k=0;k<3;k++)ray[k]=o->basis[6+k]*primary->ai_attack_range;
            status=campaign_enemy_cadence(primary,frame,&remaining,&due);if(status)goto fail;
            status=scene_turret_scene_shot(&context,p->host,UINT32_MAX,o->view.weapons[0],eye,pose+9,ray,primary);if(status==RF_NOT_FOUND)goto mounted_fire_done;if(status)goto fail;
            p->burst=remaining;p->due=due;++rf_scene_turret_player[5];
            {scene_player_shot_hearing_policy hearing;uint32_t alerted;
             int32_t weapon=rf_weapon_name_find(&campaign_weapon_reset.names,o->view.weapons[0]==scene_turret_heap_id?"HEAP":"Vauss");
             if(weapon<0){status=RF_NOT_FOUND;goto fail;}
             status=scene_player_shot_hearing(campaign_weapon_reset.definitions[weapon].flags_264,
                o->view.weapons[0],-1,0,1,&hearing);if(status)goto fail;
             if(hearing.emit){
                 status=campaign_enemy_hear_shot(eye,hearing.radius,frame,&alerted);if(status)goto fail;
                 if(alerted&&campaign_player_form.active){campaign_player_form.compromised=1;rf_scene_player_form[2]=1;}
             }}
        }
mounted_fire_done:
        p->fire_held=!!p->input.fire;
    }
    rf_scene_turret_player[10]=frame;
    {uint32_t *q=rf_scene_turret_player_probe;float camera[3],basis[3][3];
     o=scene_turret_lookup(p->host);q[0]=p->active;q[1]=o?o->uid:UINT32_MAX;
     q[2]=q[4]=q[5]=0;q[7]=UINT32_MAX;
     if(o){memcpy(q+2,&o->damage.effects.health,4);q[4]=o->view.occupant_count;q[5]=o->view.weapon_owner!=NULL;q[7]=(uint32_t)o->view.linked_handle;}
     memcpy(q+3,&campaign_player_damage.state.effects.health,4);q[6]=(uint32_t)campaign_player_view.linked_handle;
     memcpy(q+8,scene_actor_body.state.position,12);
     if(p->active&&!scene_turret_player_camera(camera,basis))memcpy(q+11,camera,12);
     q[14]=rf_scene_turret_player[5];q[15]=frame;memcpy(q+16,p->anchor,12);}
    return RF_OK;
fail:++rf_scene_turret_player[7];return status;
}
#endif
