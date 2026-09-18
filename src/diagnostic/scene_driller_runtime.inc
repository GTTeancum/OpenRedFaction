#ifndef SCENE_DRILLER_RUNTIME_INC
#define SCENE_DRILLER_RUNTIME_INC
#include "scene_driller_physics.inc"
#include "scene_ground_vehicle_physics.inc"
#include "scene_vehicle_possession.inc"
#include "scene_vehicle_collision.inc"
#include "scene_submarine_motion.inc"
#include "scene_fighter_motion.inc"
#include "scene_driller_support.inc"
#include "scene_driller_entry_exit.inc"
#include "scene_driller_player_adapter.inc"
#include "scene_jeep_seats.inc"
#include "scene_driller_audio.inc"
#include "scene_driller_actor_collision.inc"
#include "scene_driller_damage_runtime.inc"
#include "scene_driller_projectile_contact.inc"
struct scene_driller_runtime {
    scene_driller_physics physics;scene_vehicle_collision collision;scene_jeep_seat_state jeep_seat;
    scene_submarine_motion submarine;scene_fighter_motion fighter;rf_liquid_rooms submarine_liquids;
    rf_vehicle_submersible_parameters submarine_parameters;
    scene_driller_support_context support;scene_driller_entry_exit entry;
    scene_driller_damage_runtime damage;scene_driller_audio audio;rf_geomod_template shapes[2];scene_driller_live_contact contact;
    scene_driller_player_adapter player;uint32_t last_frame;float driver_seconds;
};
#include "scene_driller_blast.inc"
uint32_t rf_scene_vehicle_state[16];
uint32_t rf_scene_jeep_seats[8]; /* ticks,driver frames,gunner frames,switches,role,seat,steer,fire */
uint32_t rf_scene_vehicle_damage[8]; /* health bits,amount bits,calls,destroyed,pending,exits,source,alive */
uint32_t rf_scene_drill_state[8]; /* ticks,contacts,damage,cuts,requests,rejections,postwork,status */
static int scene_driller_runtime_cut(void *context,const float *center,const float *basis,
    const char *name,float radius,uint32_t flags,uint32_t source,uint32_t *accepted)
{
    scene_driller_live_contact *c=context;scene_driller_runtime *r=c->stream->driller_runtime;
    uint32_t index=!strcmp(name,"bit_driller_double.v3d");int status,post=0;
    *accepted=0;if(!c->has_world || source!=c->source)return RF_OK;
    ++rf_scene_drill_state[4];
    status=scene_driller_excavate(c->stream,c->frame,r->shapes+index,&c->world_contact,
        center,basis,radius,flags,accepted,&post);
    rf_scene_drill_state[6]=(uint32_t)post;rf_scene_drill_state[7]=(uint32_t)status;
    printf("DRILL_CUT %u %u %d %u %.9g %.9g %.9g\n",c->frame,index,status,*accepted,center[0],center[1],center[2]);
    if(status==RF_NOT_FOUND){++rf_scene_drill_state[5];return RF_OK;}
    return status;
}
static uint32_t scene_driller_active(const scene_stream *s)
{return s && s->driller_runtime && s->driller_runtime->entry.session.active;}
/* Player movement is queried against the committed host pose, not the whole
 * previous frame displacement on every solver substep. Roof riding and host
 * pushing a stationary actor require a separate pair scheduler. */
static int scene_driller_player_collision(scene_stream *s,const rf_collision_body_query *query,
    rf_geometry_body_hit *hit,uint32_t *matched)
{
    scene_driller_actor_collision host={0};scene_driller_runtime *r;int status;
    if(!s || !(r=s->driller_runtime))return RF_OK;
    host.spheres=r->physics.collision;host.count=r->physics.sphere_count;
    host.handle=r->entry.host.handle;host.driver=r->entry.host.driver;host.material=s->driller->physics.material.index;
    memcpy(host.position,r->physics.state.position,12);memcpy(host.next_position,host.position,12);
    memcpy(host.orientation,r->physics.state.orientation,36);memcpy(host.next_orientation,host.orientation,36);
    memcpy(host.velocity,r->physics.state.velocity,12);
    ++rf_scene_vehicle_state[14];
    status=scene_driller_actor_compose(&host,campaign_player_object.handle,(uint32_t)campaign_player_view.linked_handle,query,hit,matched);
    if(!status && *matched && hit->contact.object_id==host.handle)++rf_scene_vehicle_state[15];
    return status;
}
static int scene_driller_runtime_support(void *context,const rf_vehicle_rigid_state *state,rf_vehicle_rigid_support *out)
{return scene_driller_support_sample(&((scene_driller_runtime*)context)->support,state,out);}
static int scene_driller_runtime_resolve(void *context,const rf_vehicle_rigid_state *state,const rf_vehicle_rigid_proposal *proposal,rf_vehicle_rigid_proposal *out)
{return scene_vehicle_collision_resolve(&((scene_driller_runtime*)context)->collision,state,proposal,out);}
static int scene_submarine_player_water(void *context,const rf_collision_body_sphere *spheres,uint32_t count,
    const scene_vehicle_pose *from,const scene_vehicle_pose *to,uint32_t *allowed)
{
    return scene_submarine_water_body_path(context,spheres,count,
        from->position,from->basis,to->position,to->basis,allowed);
}
static int scene_driller_runtime_open(scene_stream *s)
{
    scene_driller_runtime *r;int status;uint32_t stage=0;
    if(!s->driller)return RF_OK;
    if(s->driller_runtime)return RF_OK;
    if(!rf_scene_dev_room_enabled || rf_scene_dev_npc_enabled)return RF_RANGE;
    r=calloc(1,sizeof(*r));if(!r)return RF_IO;r->last_frame=UINT32_MAX;
    if(rf_scene_vehicle_enabled==1){
#ifdef RF_IMAGE_XBOX_NATIVE
    status=rf_geomod_template_load("D:\\driller-single.bin",r->shapes);
    if(!status)status=rf_geomod_template_load("D:\\driller-double.bin",r->shapes+1);
#else
    status=rf_geomod_template_load("build/data/driller-single.bin",r->shapes);
    if(!status)status=rf_geomod_template_load("build/data/driller-double.bin",r->shapes+1);
#endif
    if(status)goto fail;
    }
    stage=1;if(rf_scene_vehicle_enabled==4 || rf_scene_vehicle_enabled==5)status=scene_vehicle_body_physics_initialize(rf_scene_vehicle_enabled==5?"Fighter01":"sub",&s->driller->physics,&s->driller->movement,&s->driller->rotation,&s->driller->render,s->driller_position,s->driller_basis,0,&r->physics);
    else if(rf_scene_vehicle_enabled>=2)status=scene_ground_vehicle_physics_initialize(rf_scene_vehicle_enabled==3?"Jeep01":"APC",&s->driller->physics,&s->driller->movement,&s->driller->rotation,&s->driller->render,s->driller_position,s->driller_basis,scene_gravity.acceleration,&r->physics);
    else status=scene_driller_physics_initialize(s->driller,s->driller_position,s->driller_basis,scene_gravity.acceleration,&r->physics);
    if(status)goto fail;
    r->collision.world=s->collision;r->collision.movers=&campaign_movers;
    r->collision.spheres=r->physics.collision;r->collision.count=r->physics.sphere_count;
    r->collision.radius=r->physics.radius;r->collision.flags=4;
    stage=2;
    if(rf_scene_vehicle_enabled==4 || rf_scene_vehicle_enabled==5){
        r->submarine_liquids.items=s->liquid_rooms;r->submarine_liquids.count=s->swim_room_count;r->submarine_liquids.bytes=s->swim_bytes;
        r->submarine.collision=r->collision;r->submarine.liquids=&r->submarine_liquids;
        r->fighter.collision=r->collision;r->fighter.liquids=&r->submarine_liquids;
        r->submarine_parameters=(rf_vehicle_submersible_parameters){s->driller->physics.authored.mass,
            s->driller->movement.speed,s->driller->movement.acceleration,s->driller->rotation.maximum_velocity,
            s->driller->rotation.acceleration,2};
    }else{status=scene_driller_support_initialize(&r->support,s,&r->collision,&r->physics);if(status)goto fail;}
    stage=3;status=scene_driller_entry_exit_open(&r->entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,s->driller,&r->physics,&r->collision,scene_driller_player_eye_offset());if(status)goto fail;
    if(rf_scene_vehicle_enabled==4){r->entry.water_context=&r->submarine;r->entry.water_path=scene_submarine_player_water;}
    if(rf_scene_vehicle_enabled==3){status=scene_jeep_seats_open(&r->jeep_seat,s->driller);if(status)goto fail;}
    stage=4;status=scene_driller_damage_runtime_open(&r->damage,&r->entry,&r->player,&s->driller_damage_prototype);if(status)goto fail;
    /* Audio unavailable/full slots must not prevent driving. */
    if(rf_scene_vehicle_enabled==2)(void)scene_vehicle_audio_open(&r->audio,r->physics.state.position,"APC_idle.wav",8,.5f,.4f);
    else if(rf_scene_vehicle_enabled==3)(void)scene_vehicle_audio_open(&r->audio,r->physics.state.position,"Jeep_idle.wav",6,.8f,.6f);
    else if(rf_scene_vehicle_enabled==1)(void)scene_driller_audio_open(&r->audio,r->physics.state.position);
    s->driller_runtime=r;memset(rf_scene_jeep_seats,0,sizeof(rf_scene_jeep_seats));memset(rf_scene_drill_state,0,sizeof(rf_scene_drill_state));memset(rf_scene_vehicle_state,0,sizeof(rf_scene_vehicle_state));return RF_OK;
fail:fprintf(stderr,"VEHICLE_OPEN_ERROR stage%u status%d\n",stage,status);scene_driller_damage_runtime_close(&r->damage);(void)scene_driller_entry_exit_close(&r->entry,1);free(r);return status;
}
static int scene_driller_runtime_tick(scene_stream *s,uint32_t frame)
{
    scene_driller_runtime *r;rf_vehicle_rigid_command command={0};rf_vehicle_rigid_backend backend;
    uint32_t changed=0,was,stage=0;int status;
    if(!s->driller)return RF_OK;
    status=scene_driller_runtime_open(s);if(status)return status;r=s->driller_runtime;
    if(r->last_frame==frame)return RF_OK;
    was=r->entry.session.active;
    stage=1;status=scene_driller_damage_runtime_tick(&r->damage,&changed);if(status)goto fail;
    if(r->entry.host.alive){status=scene_driller_entry_exit_use(&r->entry,player_input.use,campaign_player_damage.state.effects.health>0,&changed);if(status)goto fail;}
    if(changed){if(r->entry.session.active)++rf_scene_vehicle_state[1];else ++rf_scene_vehicle_state[2];}
    if(rf_scene_vehicle_enabled==3){uint32_t seat_changed=0;
        status=scene_jeep_seats_tick(&r->jeep_seat,s->driller,&r->entry,&r->player,player_input.cycle_weapon,&seat_changed);if(status)goto fail;
        if(seat_changed){s->apc_aim_active=0;++rf_scene_jeep_seats[3];}
    }
    if(frame && r->entry.session.active)r->driver_seconds+=scene_step_seconds;
    command.controlled=r->entry.session.active && r->entry.host.alive && (rf_scene_vehicle_enabled!=3 || scene_jeep_can_steer(&r->jeep_seat));command.throttle=command.controlled?player_input.move[2]:0;
    command.turn=command.controlled?player_input.move[0]:0;
    backend.context=r;backend.support=scene_driller_runtime_support;backend.resolve=scene_driller_runtime_resolve;
    r->physics.parameters.gravity=scene_gravity.acceleration;
    stage=2;if(frame){
        if(rf_scene_vehicle_enabled==4 || rf_scene_vehicle_enabled==5){
            rf_vehicle_submersible_command sub={0};rf_vehicle_submersible_result result;
            rf_vehicle_submersible_backend wet_backend={&r->submarine,scene_submarine_motion_water_path,scene_submarine_motion_resolve};
            if(rf_scene_vehicle_enabled==5){wet_backend.context=&r->fighter;wet_backend.water_path=scene_fighter_motion_air_path;wet_backend.resolve=scene_fighter_motion_resolve;}
            sub.controlled=command.controlled;
            if(sub.controlled){sub.throttle=player_input.move[2];sub.strafe=player_input.move[0];
                sub.rise=(float)player_input.jump-(float)player_input.crouch;
                sub.yaw=player_input.look[1];sub.pitch=player_input.look[0];}
            status=rf_vehicle_submersible_step(&r->physics.state,&r->submarine_parameters,&sub,scene_step_seconds,&wet_backend,&result);
        }else status=rf_vehicle_rigid_step(&r->physics.state,&r->physics.parameters,&command,scene_step_seconds,&backend);
        if(status)goto fail;
    }
    stage=3;status=scene_driller_player_publish(&r->player,&r->entry,changed);if(status)goto fail;
    if(rf_scene_vehicle_enabled==3 && was && !r->entry.session.active){status=scene_jeep_seats_open(&r->jeep_seat,s->driller);if(status)goto fail;}
    stage=4;if(r->audio.active){status=scene_driller_audio_tick(&r->audio,r->physics.state.position,r->physics.state.velocity,r->physics.parameters.maximum_speed,!r->entry.host.alive);if(status)goto fail;}
    stage=5;
    r->contact.stream=s;r->contact.frame=frame;r->contact.source=r->entry.host.handle;r->contact.has_world=0;
    if(rf_scene_vehicle_enabled==1){scene_driller_weapon_backend weapon_backend={&r->contact,scene_driller_live_trace,scene_driller_live_damage,scene_driller_runtime_cut};
     status=scene_driller_weapon_tick(&s->driller_weapon,&s->driller->tags,r->physics.state.position,
        r->physics.state.orientation,r->entry.host.handle,r->entry.session.active && r->entry.host.alive && player_input.fire,
        frame?scene_step_seconds:0,&weapon_backend);if(status)goto fail;}
    ++rf_scene_drill_state[0];rf_scene_drill_state[1]=s->driller_weapon.contacts;
    rf_scene_drill_state[2]=s->driller_weapon.damage_calls;rf_scene_drill_state[3]=s->driller_weapon.accepted_cuts;
    memcpy(s->driller_position,r->physics.state.position,12);memcpy(s->driller_basis,r->physics.state.orientation,36);
    memcpy(rf_scene_vehicle_damage,&r->damage.damage.state.effects.health,4);
    memcpy(rf_scene_vehicle_damage+1,&r->damage.last_amount,4);rf_scene_vehicle_damage[2]=r->damage.damage_calls;
    rf_scene_vehicle_damage[3]=r->damage.damage.destroyed;rf_scene_vehicle_damage[4]=r->damage.eject_pending;
    rf_scene_vehicle_damage[5]=r->damage.exit_count;rf_scene_vehicle_damage[6]=r->damage.damage.state.responsible_handle;
    rf_scene_vehicle_damage[7]=r->entry.host.alive;
    if(rf_scene_vehicle_enabled==3){
        ++rf_scene_jeep_seats[0];
        if(r->entry.session.active)++rf_scene_jeep_seats[scene_jeep_can_fire(&r->jeep_seat)?2:1];
        rf_scene_jeep_seats[4]=r->jeep_seat.role;rf_scene_jeep_seats[5]=(uint32_t)s->driller->seat;
        rf_scene_jeep_seats[6]=command.controlled;
        rf_scene_jeep_seats[7]=r->entry.session.active&&r->entry.host.alive&&scene_jeep_can_fire(&r->jeep_seat);
    }
    r->last_frame=frame;++rf_scene_vehicle_state[0];rf_scene_vehicle_state[3]=r->entry.session.active;
    rf_scene_vehicle_state[4]=r->support.hits;rf_scene_vehicle_state[5]=0;
    memcpy(rf_scene_vehicle_state+6,r->physics.state.position,12);memcpy(rf_scene_vehicle_state+9,r->physics.state.velocity,12);
    rf_scene_vehicle_state[12]=r->entry.registration.handle;rf_scene_vehicle_state[13]=was;
    return RF_OK;
fail:rf_scene_vehicle_state[5]=(uint32_t)status;fprintf(stderr,"DRILLER_RUNTIME_ERROR frame%u stage%u status%d\n",frame,stage,status);return status;
}
static int scene_driller_runtime_close(scene_stream *s)
{
    int status=RF_OK;if(!s->driller_runtime)return RF_OK;
    status=scene_driller_audio_close(&s->driller_runtime->audio);
    scene_driller_damage_runtime_close(&s->driller_runtime->damage);
    {int closed=scene_driller_entry_exit_close(&s->driller_runtime->entry,1);if(!status)status=closed;}
    free(s->driller_runtime);s->driller_runtime=NULL;return status;
}
#endif
