#ifndef RF_SCENE_NPC_TELEPORT_INCLUDED
#define RF_SCENE_NPC_TELEPORT_INCLUDED
/* Immediate type4 entity slice. Include after scene_npc_seat_bind.inc.
 * Original4b9650 detaches, publishes destination, then selects Waiting2 before
 * event propagation. No deferred reset may erase a subsequently issued Goto.
 * Unknown attachment families and dead/corpse owners are outside this slice. */
uint32_t rf_scene_npc_teleport[8]; /* calls,applied,detached,rejected,eventUID,actorUID,handle,status */
/* Immutable first success: valid,eventUID,actorUID,handle,nowMs,oldParent,
 * newParent,AI,scriptActive,health,position[3],basis[9],velocity[3],angular[3],
 * oldHostDriver,newHostDriver,oldHostHealth,newHostHealth,oldHostPosition[3],
 * newHostPosition[3],seatActive,room. Floats are bitcasts. */
uint32_t rf_scene_npc_teleport_apply[40];
/* Live first-success actor: valid,handle,parent,AI,scriptActive,seatActive,
 * health,position[3],basis[9],velocity[3],hostDriver,hostHealth,
 * hostPosition[3],nowMs,support,retainedRouteCount,combatScripted,status. */
uint32_t rf_scene_npc_teleport_live[32];
static uint32_t scene_npc_teleport_actor=UINT32_MAX,scene_npc_teleport_host=UINT32_MAX;
static void scene_npc_teleport_reset(void)
{
    memset(rf_scene_npc_teleport,0,sizeof(rf_scene_npc_teleport));
    memset(rf_scene_npc_teleport_apply,0,sizeof(rf_scene_npc_teleport_apply));
    memset(rf_scene_npc_teleport_live,0,sizeof(rf_scene_npc_teleport_live));
    scene_npc_teleport_actor=scene_npc_teleport_host=UINT32_MAX;
}
static void scene_npc_teleport_host_probe(uint32_t handle,uint32_t *driver,uint32_t *health,float position[3])
{
    scene_stream *s=scene_npc_seat_stream;scene_driller_runtime *r=s?s->driller_runtime:NULL;
    *driver=UINT32_MAX;*health=0;memset(position,0,12);
    if(r&&r->entry.registration.handle==handle&&
       rf_object_registry_lookup(&campaign_registry,handle)==&r->entry.registration){
        *driver=r->entry.host.driver;memcpy(health,&r->damage.damage.state.effects.health,4);
        memcpy(position,r->physics.state.position,12);
    }
}
static int campaign_teleport_npc(void *context,uint32_t handle,const rf_level_event *event)
{
    campaign_npc_body *o;rf_entity_pose *pose;rf_physics_body_state body;
    rf_spawn_look_angles angles;rf_entity_ai_transition_state ai;
    float basis[9],published[3],old_host_position[3],host_position[3];
    uint32_t slot,cls,i,j,flags,old_parent,old_driver,old_health,was_seated=0;
    int status;int32_t now=campaign_blackout_now;(void)context;
    ++rf_scene_npc_teleport[0];rf_scene_npc_teleport[4]=event?event->uid:0;rf_scene_npc_teleport[6]=handle;
    if(!event||!event->has_orientation||now<0||now>RF_TIMER_PERIOD){status=RF_RANGE;goto fail;}
    status=campaign_npc_motion_owner(handle,&o,&cls,&pose);if(status)goto fail;
    slot=(uint32_t)(o-campaign_npc_bodies);
    if(slot>=campaign_model_owner_count||o->movement_slot>=16||!o->body.allocated_bytes||!campaign_npc_eyes||
       rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view||
       o->damage.effects.health<=0||(o->view.flags_810&1u)){status=RF_NOT_FOUND;goto fail;}
    rf_scene_npc_teleport[5]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    for(i=0;i<3;i++)if(!isfinite(event->position[i])){status=RF_FORMAT;goto fail;}
    for(i=0;i<9;i++)if(!isfinite(event->orientation_disk[i])){status=RF_FORMAT;goto fail;}
    memcpy(basis,event->orientation_disk+3,24);memcpy(basis+6,event->orientation_disk,12);
    /* Reject degenerate/sheared event frames before detachment. Installed
     * authored single-precision bases use ordinary rounding tolerance. */
    for(i=0;i<3;i++)for(j=i;j<3;j++){
        float dot=0;for(uint32_t k=0;k<3;k++)dot+=basis[i*3+k]*basis[j*3+k];
        if(fabsf(dot-(i==j?1.f:0.f))>.002f){status=RF_FORMAT;goto fail;}
    }
    old_parent=(uint32_t)o->view.linked_handle;
    if(o->view.linked_handle!=-1){
        if(!scene_npc_seat_attached(handle)){status=RF_NOT_FOUND;goto fail;}was_seated=1;
    }else if(o->ai_mode.action_280==13){status=RF_NOT_FOUND;goto fail;}
    body=o->body.state;flags=o->object_flags;ai=o->ai_mode;
    memcpy(body.position,event->position,12);memcpy(body.next_position,event->position,12);
    memcpy(body.orientation,basis,36);memcpy(body.next_orientation,basis,36);
    status=rf_physics_tensor_world(body.local_tensor,basis,body.world_tensor);if(status)goto fail;
    status=rf_physics_publish_position(&body,published,&flags);if(status)goto fail;
    /* The event installs a full body frame, unlike the constructor's
     * yaw-only starting body. Keep its facing in body_angles and zero local
     * eye angles: ordinary commit sums both and would otherwise double yaw.
     * Invert this engine's rf_look_orientation forward exactly (also used by
     * turret aiming), rather than feed standard Euler pitch to that mapping.
     * The event matrix including roll is exact at publication. Ordinary NPC
     * motion has no roll representation and subsequently orthogonalizes it. */
    memset(&angles,0,sizeof(angles));
    {float horizontal=sqrtf(basis[6]*basis[6]+basis[8]*basis[8]);
     float span=horizontal+fabsf(basis[7]);
     if(!isfinite(span)||span<=0){status=RF_FORMAT;goto fail;}
     angles.body[0]=asinf(fmaxf(-1,fminf(1,basis[7]/span)));
     angles.body[1]=atan2f(basis[6],basis[8]);}
    status=rf_entity_ai_set_action(&ai,2,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)goto fail;
    scene_npc_teleport_host_probe(old_parent,&old_driver,&old_health,old_host_position);
    if(was_seated){status=scene_npc_seat_unbind(handle,now);if(status)goto fail;}
    status=campaign_ai_mode_apply(o,2,now);if(status)goto fail;
    scene_npc_loop_audio_remove(handle);
    memset(&o->script_move,0,sizeof(o->script_move));o->script_move.stop=1;
    memset(&o->navigation,0,sizeof(o->navigation));
    memcpy(o->navigation.start.position,event->position,12);
    memcpy(o->navigation.goal.position,event->position,12);
    o->script_look.active=o->script_shoot.active=0;
    while(o->script_once.count)campaign_script_single_pop(o,0);
    memset(o->command_714,0,12);memset(o->look.command_708,0,12);
    memset(o->look.angles.vector_870,0,12);memset(o->look.angles.delta_888,0,12);
    memset(o->look.angles.offset_894,0,12);
    memcpy(o->look.body_angles,angles.body,12);memcpy(o->look.angles.angles_87c,angles.eye,12);
    memcpy(o->look.orientation,basis,36);memcpy(campaign_model_owners[slot].basis,basis,36);
    o->body.state=body;o->object_flags=flags;
    o->support.handle=0;o->support.material=-1;o->body.state.flags&=~0x400000u;
    memset(o->support_velocity,0,12);memset(&o->piece_support,0,sizeof(o->piece_support));o->piece_reacquire=0;
    memset(&o->collision_contact,0,sizeof(o->collision_contact));o->collision_contact.material=UINT32_MAX;
    o->body.state.reference_15c=-1;o->body.state.scalar_144=1;
    o->previous_region_13ec=NULL;memcpy(o->previous,event->position,12);
    status=rf_scene_npc_publish_position(handle);if(status)goto fail;
    status=campaign_npc_eye_update(slot);if(status)goto fail;
    status=rf_scene_npc_refresh_room(handle);if(status)goto fail;
    ++rf_scene_npc_teleport[1];rf_scene_npc_teleport[2]+=was_seated;rf_scene_npc_teleport[7]=0;
    if(!rf_scene_npc_teleport_apply[0]){
        uint32_t *p=rf_scene_npc_teleport_apply;
        p[0]=1;p[1]=event->uid;p[2]=rf_scene_npc_teleport[5];p[3]=handle;p[4]=(uint32_t)now;p[5]=old_parent;
        p[6]=(uint32_t)o->view.linked_handle;p[7]=(uint32_t)o->ai_mode.action_280;p[8]=o->script_move.active;
        memcpy(p+9,&o->damage.effects.health,4);memcpy(p+10,o->published,12);memcpy(p+13,o->body.state.orientation,36);
        memcpy(p+22,o->body.state.velocity,12);memcpy(p+25,o->body.state.vector_c8,12);
        p[28]=old_driver;p[30]=old_health;memcpy(p+32,old_host_position,12);
        scene_npc_teleport_host_probe(old_parent,p+29,p+31,host_position);memcpy(p+35,host_position,12);
        p[38]=scene_npc_seat_attached(handle);p[39]=o->room.room;
        scene_npc_teleport_actor=handle;scene_npc_teleport_host=old_parent;
    }
    return RF_OK;
fail:++rf_scene_npc_teleport[3];rf_scene_npc_teleport[7]=(uint32_t)status;return status;
}
/* Parent calls after campaign_script_step; read-only, no input or fixture. */
static void scene_npc_teleport_sample(void)
{
    uint32_t *p=rf_scene_npc_teleport_live,cls;campaign_npc_body *o;rf_entity_pose *pose;float position[3];int status;
    if(scene_npc_teleport_actor==UINT32_MAX)return;
    status=campaign_npc_motion_owner(scene_npc_teleport_actor,&o,&cls,&pose);p[31]=(uint32_t)status;
    if(status){p[0]=0;return;}
    p[0]=1;p[1]=scene_npc_teleport_actor;p[2]=(uint32_t)o->view.linked_handle;
    p[3]=(uint32_t)o->ai_mode.action_280;p[4]=o->script_move.active;p[5]=scene_npc_seat_attached(p[1]);
    memcpy(p+6,&o->damage.effects.health,4);memcpy(p+7,o->published,12);memcpy(p+10,o->body.state.orientation,36);
    memcpy(p+19,o->body.state.velocity,12);
    scene_npc_teleport_host_probe(scene_npc_teleport_host,p+22,p+23,position);memcpy(p+24,position,12);
    p[27]=(uint32_t)campaign_blackout_now;p[28]=o->support.handle;p[29]=o->navigation.retained.count;p[30]=o->combat_scripted;
}
#endif
