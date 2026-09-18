#ifndef RF_SCENE_JEEP_SEATS_INC
#define RF_SCENE_JEEP_SEATS_INC
/* Include after entry and player publication adapters. DEV single-player role
 * switch only: same registered host/session/occupant, no fabricated NPC driver.
 * Host.driver remains the possession owner token even in gunner role; parent
 * MUST gate steering with can_steer and gunfire with can_fire. Parked policy:
 * speed <=.1m/s, angular speed<=.1rad/s. World/mover sphere-center chords follow
 * the existing rigid rotation admission policy; not swept curved rotation.
 * Seated transfer uses actual head-sphere world/mover clearance; standing
 * capsule destination support remains the exit adapter responsibility.
 */
typedef struct scene_jeep_seat_state {uint32_t role,held;} scene_jeep_seat_state;
static uint32_t scene_jeep_can_steer(const scene_jeep_seat_state *s){return s&&s->role==0;}
static uint32_t scene_jeep_can_fire(const scene_jeep_seat_state *s){return s&&s->role==1;}
static int scene_jeep_seat_index(const scene_driller_resources *resource,uint32_t role,int32_t *index)
{
    uint32_t i;const char *name=role?"interface_2":"interface_1";
    if(!resource||!index||role>1)return RF_RANGE;
    for(i=0;i<resource->tags.count;i++)if(!strcmp(resource->tags.items[i].name,name)){*index=(int32_t)i;return RF_OK;}
    return RF_NOT_FOUND;
}
static int scene_jeep_seats_open(scene_jeep_seat_state *state,scene_driller_resources *resource)
{
    int32_t driver,gunner;int status;
    if(!state||!resource)return RF_RANGE;
    status=scene_jeep_seat_index(resource,0,&driver);if(status)return status;
    status=scene_jeep_seat_index(resource,1,&gunner);if(status)return status;
    if(driver==gunner)return RF_FORMAT;
    state->role=state->held=0;resource->seat=driver;return RF_OK;
}
static int scene_jeep_seats_tick(scene_jeep_seat_state *state,scene_driller_resources *resource,
    scene_driller_entry_exit *entry,scene_driller_player_adapter *adapter,uint32_t held,uint32_t *changed)
{
    scene_vehicle_pose target,old_pose,from;
    scene_driller_player_adapter old_adapter;float tag[12],offset[3],tensor[3][3],speed2=0,turn2=0;
    uint32_t i,j,edge,found;int32_t old_seat,next_seat;int status;
    if(!state||!resource||!entry||!adapter||!changed||held>1||state->role>1)return RF_RANGE;
    *changed=0;edge=held&&!state->held;state->held=held;
    if(!edge||!entry->session.active||!entry->host.alive||campaign_player_damage.state.effects.health<=0)return RF_OK;
    if(entry->resource!=resource||entry->player_body!=&scene_actor_body||entry->player_view!=&campaign_player_view||
       entry->host.handle!=entry->registration.handle||entry->host.driver!=entry->player.handle||
       entry->session.driver!=entry->player.handle||entry->session.host!=entry->host.handle||
       entry->player.host!=entry->host.handle||entry->occupant!=(int32_t)entry->player.handle||
       entry->player_view->linked_handle!=(int32_t)entry->host.handle||
       rf_entity_lookup(entry->entities,(int32_t)entry->host.handle)!=&entry->view||
       rf_entity_lookup(entry->entities,(int32_t)entry->player.handle)!=entry->player_view)return RF_NOT_FOUND;
    status=scene_jeep_seat_index(resource,state->role,&old_seat);if(status)return status;
    if(resource->seat!=old_seat)return RF_FORMAT;
    status=scene_jeep_seat_index(resource,1-state->role,&next_seat);if(status)return status;
    status=rf_physics_tensor_world(entry->physics->state.inverse_inertia,entry->physics->state.orientation,(float*)tensor);if(status)return status;
    for(i=0;i<3;i++){float angular=0,v=entry->physics->state.velocity[i];
        if(!isfinite(v))return RF_FORMAT;
        speed2+=v*v;
        for(j=0;j<3;j++)angular+=tensor[i][j]*entry->physics->state.momentum[j];
        if(!isfinite(angular))return RF_FORMAT;
        turn2+=angular*angular;
    }
    if(speed2>.01f||turn2>.01f)return RF_OK;
    status=rf_static_model_tag_place(&resource->tags,next_seat,entry->physics->state.orientation,entry->physics->state.position,tag);if(status)return status;
    scene_driller_seat_basis(entry,next_seat,tag,target.basis);memcpy(target.position,tag+9,12);
    scene_driller_point((const float[3]){0,0,0},target.basis,entry->eye_offset,offset);
    for(i=0;i<3;i++)target.position[i]-=offset[i];
    memcpy(from.position,scene_actor_body.state.position,12);memcpy(from.basis,scene_actor_body.state.orientation,36);
    status=scene_vehicle_seated_path(entry,&from,&target,&found);if(status)return status;
    if(!found)return RF_OK;
    old_pose=entry->player.pose;old_adapter=*adapter;resource->seat=next_seat;
    status=scene_driller_player_publish(adapter,entry,1);
    if(status){resource->seat=old_seat;entry->player.pose=old_pose;*adapter=old_adapter;return status;}
    state->role=1-state->role;*changed=1;return RF_OK;
}
#endif
