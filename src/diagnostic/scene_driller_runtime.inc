#ifndef SCENE_DRILLER_RUNTIME_INC
#define SCENE_DRILLER_RUNTIME_INC
#include "scene_driller_physics.inc"
#include "scene_vehicle_possession.inc"
#include "scene_vehicle_collision.inc"
#include "scene_driller_support.inc"
#include "scene_driller_entry_exit.inc"
#include "scene_driller_player_adapter.inc"
#include "scene_driller_audio.inc"
struct scene_driller_runtime {
    scene_driller_physics physics;scene_vehicle_collision collision;
    scene_driller_support_context support;scene_driller_entry_exit entry;
    scene_driller_audio audio;
    scene_driller_player_adapter player;uint32_t last_frame;
};
uint32_t rf_scene_vehicle_state[16];
static uint32_t scene_driller_active(const scene_stream *s)
{return s && s->driller_runtime && s->driller_runtime->entry.session.active;}
static int scene_driller_runtime_support(void *context,const rf_vehicle_rigid_state *state,rf_vehicle_rigid_support *out)
{return scene_driller_support_sample(&((scene_driller_runtime*)context)->support,state,out);}
static int scene_driller_runtime_resolve(void *context,const rf_vehicle_rigid_state *state,const rf_vehicle_rigid_proposal *proposal,rf_vehicle_rigid_proposal *out)
{return scene_vehicle_collision_resolve(&((scene_driller_runtime*)context)->collision,state,proposal,out);}
static int scene_driller_runtime_open(scene_stream *s)
{
    scene_driller_runtime *r;int status;
    if(!s->driller)return RF_OK;
    if(s->driller_runtime)return RF_OK;
    if(!rf_scene_dev_room_enabled || rf_scene_dev_npc_enabled)return RF_RANGE;
    r=calloc(1,sizeof(*r));if(!r)return RF_IO;r->last_frame=UINT32_MAX;
    status=scene_driller_physics_initialize(s->driller,s->driller_position,s->driller_basis,scene_gravity.acceleration,&r->physics);
    if(status)goto fail;
    r->collision.world=s->collision;r->collision.movers=&campaign_movers;
    r->collision.spheres=r->physics.collision;r->collision.count=r->physics.sphere_count;
    r->collision.radius=r->physics.radius;r->collision.flags=4;
    status=scene_driller_support_initialize(&r->support,s,&r->collision,&r->physics);if(status)goto fail;
    status=scene_driller_entry_exit_open(&r->entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,s->driller,&r->physics,&r->collision,scene_driller_player_eye_offset());if(status)goto fail;
    /* Audio unavailable/full slots must not prevent driving. */
    (void)scene_driller_audio_open(&r->audio,r->physics.state.position);
    s->driller_runtime=r;memset(rf_scene_vehicle_state,0,sizeof(rf_scene_vehicle_state));return RF_OK;
fail:free(r);return status;
}
static int scene_driller_runtime_tick(scene_stream *s,uint32_t frame)
{
    scene_driller_runtime *r;rf_vehicle_rigid_command command={0};rf_vehicle_rigid_backend backend;
    uint32_t changed=0,was,stage=0;int status;
    if(!s->driller)return RF_OK;
    status=scene_driller_runtime_open(s);if(status)return status;r=s->driller_runtime;
    if(r->last_frame==frame)return RF_OK;
    was=r->entry.session.active;
    stage=1;status=scene_driller_entry_exit_use(&r->entry,player_input.use,campaign_player_damage.state.effects.health>0,&changed);if(status)goto fail;
    if(changed){if(r->entry.session.active)++rf_scene_vehicle_state[1];else ++rf_scene_vehicle_state[2];}
    command.controlled=r->entry.session.active;command.throttle=command.controlled?player_input.move[2]:0;
    command.turn=command.controlled?player_input.move[0]:0;
    backend.context=r;backend.support=scene_driller_runtime_support;backend.resolve=scene_driller_runtime_resolve;
    r->physics.parameters.gravity=scene_gravity.acceleration;
    stage=2;if(frame){status=rf_vehicle_rigid_step(&r->physics.state,&r->physics.parameters,&command,scene_step_seconds,&backend);if(status)goto fail;}
    stage=3;status=scene_driller_player_publish(&r->player,&r->entry,changed);if(status)goto fail;
    stage=4;if(r->audio.active){status=scene_driller_audio_tick(&r->audio,r->physics.state.position,r->physics.state.velocity,r->physics.parameters.maximum_speed,!r->entry.host.alive);if(status)goto fail;}
    memcpy(s->driller_position,r->physics.state.position,12);memcpy(s->driller_basis,r->physics.state.orientation,36);
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
    {int closed=scene_driller_entry_exit_close(&s->driller_runtime->entry,1);if(!status)status=closed;}
    free(s->driller_runtime);s->driller_runtime=NULL;return status;
}
#endif
