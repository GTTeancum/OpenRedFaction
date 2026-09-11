#include "rf/player.h"

void rf_player_detach_sp(rf_player_entity_link *link,uint8_t *activity_fb0)
{link->entity_handle=-1;*activity_fb0=0;}
#include "rf/collision.h"
#include <string.h>
#include <math.h>
uint32_t rf_player_is_dead(const rf_entity_registry *registry,const rf_player_entity_link *link)
{return !link || !rf_entity_lookup(registry,link->entity_handle);}
uint32_t rf_player_is_dying(const rf_entity_registry *registry,const rf_player_entity_link *link)
{
    const rf_entity_view *entity=link?rf_entity_lookup(registry,link->entity_handle):NULL;
    return entity?(entity->flags_810&1u):0;
}
int rf_player_force_replace(rf_player_force_state *state,const rf_player_force_input *input,
    uint32_t *selected_descriptor,rf_player_force_sound sound,void *context)
{
    float velocity[3],cap;uint32_t i,flags,selected;int status;
    if(!state || !input || !selected_descriptor || !input->descriptors || !input->identity ||
        (!(state->physics_flags&0x200000) && !sound) || !isfinite(input->influence.strength))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(input->influence.direction[i]) || !isfinite(input->position[i]))return RF_RANGE;
        velocity[i]=(float)((double)input->influence.direction[i]*input->influence.strength);
    }
    flags=state->physics_flags|1;
    status=rf_physics_force_air_cap(velocity,input->class_speed,&cap,&flags);if(status)return status;
    memcpy(state->velocity,velocity,sizeof(velocity));
    selected=rf_movement_fall(input->descriptors,input->class_flags,&state->physics_flags);
    state->movement=input->descriptors+selected;state->orientation=input->identity;*selected_descriptor=selected;
    if(!(state->physics_flags&0x200000))sound(context,state,input->position,0x53);
    state->alternate_cap=cap;state->physics_flags=flags|0x80000000u;return RF_OK;
}
uint32_t rf_player_support_route(const rf_player_support_input *input)
{
    if(!input)return RF_PLAYER_SUPPORT_NONE;
    if(input->actor_flags&2)return RF_PLAYER_SUPPORT_FALL;
    if(input->movement_mode==3 || input->movement_mode==8 ||
       (input->kind_one && input->attachment==-1))return RF_PLAYER_SUPPORT_QUERY;
    if(input->movement_mode==1 && input->parent==-1 &&
       (input->moved || (input->physics_flags&0x400000) || (input->object_flags&8)))
        return RF_PLAYER_SUPPORT_QUERY;
    return RF_PLAYER_SUPPORT_NONE;
}
uint32_t rf_player_jump_enabled(const rf_player_jump_gate *gate)
{
    return gate && gate->entity_present &&
        !(gate->override_enabled && gate->game_state==34) &&
        gate->control_kind!=5 && gate->parent_kind!=4 &&
        !(gate->actor_flags&1) && !gate->key_29_held;
}
int rf_player_jump(rf_player_jump_state *state,const rf_player_jump_input *input,
    uint32_t *selected_descriptor,rf_player_jump_sound sound,void *context)
{
    int32_t mode;uint32_t selected;long double impulse;
    if(!state)return RF_OK;
    if(!input || !state->movement)return RF_RANGE;
    mode=state->movement->index;
    if((mode!=1 && (mode!=4 || (state->actor_flags&0x2000))) ||
       (state->actor_flags&0x400) || input->parent_blocked==1)return RF_OK;
    if(input->parent_blocked>1 || input->alternate_fall>1 || !input->descriptors ||
       !input->identity || !selected_descriptor || !sound)return RF_RANGE;
    if(!isfinite(input->strength) || !isfinite(input->frame_dt) ||
       !isfinite(state->vertical_velocity))return RF_FORMAT;
    impulse=input->strength;
    /* 428935..428977: no binary32 spill before adding downward velocity. */
    if(mode==4)impulse=((long double)1.25f-
        ((long double)0.1f-input->frame_dt)*(long double)-4.200000286102295f)*impulse;
    if(state->vertical_velocity<0)impulse+=state->vertical_velocity;
    state->vertical_velocity=(float)impulse;state->actor_flags|=2;
    selected=rf_movement_fall(input->descriptors,input->alternate_fall?0x400u:0u,&state->physics_flags);
    state->movement=input->descriptors+selected;*selected_descriptor=selected;
    state->orientation=input->identity;
    sound(context,state,input->class_sound);state->jump_time=input->now;return RF_OK;
}
int rf_player_climb_exit(rf_player_climb_state *state,const rf_player_climb_exit_input *input,
    uint32_t *selected_descriptor,rf_player_try_stand stand,void *context)
{
    rf_movement_settings speed;uint32_t stood,selected=0,walk;int status;
    if(!state || !input || !input->config)return RF_RANGE;
    walk=(input->config->flags&1)!=0;
    if(walk && (!input->descriptors || !input->identity || !selected_descriptor ||
        input->default_index < -1 || input->default_index>=16 || input->crouched>1 ||
        (input->crouched && !stand)))return RF_RANGE;
    if(walk && input->crouched) {
        stood=0;status=stand(context,&stood);if(status)return status;
        if(stood>1)return RF_FORMAT;if(!stood)return RF_OK;
    }
    speed=state->speed;
    status=rf_movement_set_mode(&speed,input->config,1,input->forced_action,input->entity_scale,input->override_enabled);
    if(status)return status;
    if(!walk){state->speed=speed;return RF_OK;}
    state->previous_region=NULL;state->speed=speed;
    if(input->default_index>=0) {
        selected=(input->descriptors[input->default_index].enabled&255)?(uint32_t)input->default_index:0;
        *selected_descriptor=selected;state->movement=input->descriptors+selected;
    } else state->movement=NULL;
    state->orientation=input->identity;state->vertical_velocity=0;return RF_OK;
}
int rf_player_climb_enter(rf_player_climb_state *state,const rf_player_climb_input *input,
    uint32_t *selected_descriptor,rf_player_climb_sound sound,void *context)
{
    rf_movement_settings speed;rf_player_sound_request request;uint32_t selected,emit;int status;
    if(!state || !input || !input->config)return RF_RANGE;
    if(!(input->config->flags&4))return RF_OK;
    if(!input->region || !input->descriptors || !selected_descriptor || input->free_motion>1)return RF_RANGE;
    speed=state->speed;
    status=rf_movement_set_mode(&speed,input->config,1,input->forced_action,input->entity_scale,input->override_enabled);
    if(status)return status;
    emit=input->free_motion && input->region->kind==2;
    if(emit) {
        rf_player_sound_input routing=input->sound;
        if(!sound)return RF_RANGE;
        routing.sound_id=18;routing.volume=1;routing.pan=0;
        status=rf_player_sound_route(&routing,&request);if(status)return status;
    }
    selected=(input->descriptors[2].enabled&255)?2:0;
    state->previous_region=NULL;state->region=input->region;
    if(emit)sound(context,state,&request);
    state->speed=speed;*selected_descriptor=selected;
    state->movement=input->descriptors+selected;state->orientation=input->region->matrix;
    state->contact_handle=-1;return RF_OK;
}
int rf_player_sound_route(const rf_player_sound_input *input,rf_player_sound_request *request)
{
    rf_player_sound_request value={0};uint32_t i;
    if(!input || !request || input->owner_present>1)return RF_RANGE;
    if(!isfinite(input->volume))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(input->position[i]))return RF_FORMAT;
    value.sound_id=input->sound_id;
    value.spatial=!(input->entity_type==0 && input->owner_present && input->camera_mode==0);
    if(value.spatial){memcpy(value.position,input->position,12);value.volume=1;}
    else {value.volume=input->volume;value.pan=input->pan;}
    *request=value;return RF_OK;
}
int rf_player_movement_region_find(const rf_player_movement_region *regions,
    uint32_t count,const float point[3],uint32_t *index)
{
    uint32_t i,inside;int status;
    if((count && !regions) || !point || !index)return RF_RANGE;
    for(i=0;i<count;++i) {
        status=rf_collision_point_oriented_box(point,regions[i].center,
            regions[i].matrix,regions[i].size,&inside);if(status)return status;
        if(inside){*index=i;return RF_OK;}
    }
    *index=UINT32_MAX;return RF_OK;
}
uint32_t rf_player_stance_enabled(const rf_player_stance_gate *gate)
{
    if(!gate || !gate->owns_entity || gate->environment_present ||
       gate->movement_mode==2 || gate->blocked_f38 || gate->global_blocked)return 0;
    return (gate->movement_mode==1 && (gate->speed_mode==0 || gate->speed_mode==1)) ||
        gate->movement_mode==3 || gate->movement_mode==8 ||
        (gate->entity_kind==1 && gate->attachment_1380==-1);
}
int rf_player_motion_choose(const rf_player_motion_input *input,int32_t *state)
{
    int32_t selected;float a,b,c,swap;long double partial,magnitude;uint32_t moving;
    if(!input || !state)return RF_RANGE;
    if(input->entity_present>1 || input->first_seat>1 || input->second_seat>1 ||
       input->crouched>1 || input->free_motion>1 || input->swim_motion>1 || input->weapon_hidden>1)return RF_RANGE;
    if(!isfinite(input->direction[0]) || !isfinite(input->direction[1]) || !isfinite(input->direction[2]))return RF_RANGE;
    if(!input->entity_present)selected=-1;
    else if(input->parent_kind==4)selected=15;
    else if(input->first_seat)selected=20;
    else if(input->second_seat)selected=21;
    else if(input->crouched)selected=(input->direction[0]>.1f || input->direction[0]<-.1f ||
        input->direction[2]>.1f || input->direction[2]<-.1f)?10:9;
    else if(input->free_motion)selected=14;
    else if(input->swim_motion)selected=(input->direction[0]!=0 || input->direction[1]!=0 || input->direction[2]!=0)?19:18;
    else {
        a=fabsf(input->direction[0]);b=fabsf(input->direction[1]);c=fabsf(input->direction[2]);
        if(a<b){swap=a;a=b;b=swap;}if(b<c){swap=b;b=c;c=swap;}if(a<b){swap=a;a=b;b=swap;}
        /* 4fa7a0 returns an extended approximate magnitude, without a float
         * store before the .25 comparison. a >= b >= c. */
        partial=(long double)c*.125f+(long double)b*.25f;
        magnitude=(partial*.5f+partial)+a;moving=magnitude>=.25f;
        if(input->attachment_75c!=-1)selected=moving?17:16;
        else if(!input->weapon_hidden && input->primary_weapon!=-1)selected=moving?5:1;
        else selected=moving?4:0;
    }
    *state=selected;return RF_OK;
}
uint32_t rf_player_can_crouch(const rf_player_crouch_input *input)
{
    return input && input->entity_present && input->control_kind!=5 &&
        input->parent_kind!=1 && input->parent_kind!=4 && input->attachment_75c==-1 &&
        (input->movement_mode==1 || input->movement_mode==3);
}

int rf_player_bind_local(rf_player_local_binding *local,rf_player_entity_binding *entity,
    rf_player_select_weapon select_weapon,void *context)
{
    if(!local || !entity || !select_weapon)return RF_RANGE;
    local->entity=entity;
    local->inventory=&entity->inventory;
    entity->mode_1f8=2;
    if(entity->type_24==0)entity->state_560=-1;
    local->entity_handle=entity->handle_2c;
    select_weapon(context,local,local->inventory->primary_weapon);
    return RF_OK;
}

int rf_player_spawn_prepare(rf_player_spawn_state *player,int local_player,
    int32_t skin_count,rf_player_position_override *override,
    rf_player_spawn_request *request)
{
    if(!player || !override || !request || override->pending>255)return RF_RANGE;
    if(request->skin_index<0 || request->skin_index>=skin_count) {
        if(local_player)player->skin_f5c=0;
        request->skin_index=0;
    }
    if(local_player)player->flags_10&=~8u;
    if(override->pending==1) {
        memcpy(request->position,override->position,sizeof(request->position));
        override->pending=0;
    }
    return RF_OK;
}

int rf_player_slow_enter(rf_player_climb_state *state,uint32_t *actor_flags,
    const rf_player_slow_input *input,uint32_t *selected_descriptor,rf_player_try_stand stand,void *context)
{
    uint32_t walk,stood=0,selected;int status;
    if(!state || !input || !input->config)return RF_RANGE;
    walk=input->config->flags&1u;
    if(walk) {
        if(!actor_flags || !input->descriptors || !input->identity || !selected_descriptor ||
           (!(input->forced_crouch&255u) && (*actor_flags&0x400u) && !stand))return RF_RANGE;
        if(input->forced_crouch&255u)*actor_flags|=0x400u;
        else if(*actor_flags&0x400u) {
            status=stand(context,&stood);if(status)return status;
            if(stood>1)return RF_FORMAT;
        }
    }
    status=rf_movement_set_mode(&state->speed,input->config,0,input->forced_action,input->entity_scale,input->override_enabled);
    if(status || !walk)return status;
    selected=(input->descriptors[1].enabled&255u)?1u:0u;
    state->movement=input->descriptors+selected;*selected_descriptor=selected;
    state->orientation=input->identity;state->vertical_velocity=0;return RF_OK;
}
