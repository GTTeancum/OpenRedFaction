#ifndef RF_SCENE_JEEP_GUNNER_DRIVER_TRANSFER_INCLUDED
#define RF_SCENE_JEEP_GUNNER_DRIVER_TRANSFER_INCLUDED
/* Include after scene_jeep_seats.inc. An authored driver's completed detach
 * can leave the player alone at interface_2. Permit the ordinary parked seat
 * switch into that genuinely empty driver seat without dismount/reboarding.
 * This does not remove an NPC, restart its route, or invent a new owner bank. */
uint32_t rf_scene_jeep_driver_transfer[9]; /* requests,moving,blocked,transfers,host,player,host XYZ bits */
static int scene_jeep_gunner_driver_transfer(scene_jeep_seat_state *state,
    scene_driller_resources *resource,scene_driller_entry_exit *entry,
    scene_driller_player_adapter *adapter,uint32_t held,uint32_t *changed,uint32_t *handled)
{
    scene_vehicle_pose target,from,old_pose;scene_driller_player_adapter old_adapter;
    float tag[12],offset[3],tensor[9],speed2=0,turn2=0;
    uint32_t i,j,edge,clear;int32_t old_seat,next_seat;int status;
    if(!state||!resource||!entry||!adapter||!changed||!handled||held>1)return RF_RANGE;
    *changed=*handled=0;
    if(!scene_jeep_npc_gunner_active(entry)||entry->host.driver!=UINT32_MAX||entry->occupant!=-1)return RF_OK;
    *handled=1;edge=held&&!state->held;state->held=held;
    if(!edge||!entry->host.alive||campaign_player_damage.state.effects.health<=0)return RF_OK;
    ++rf_scene_jeep_driver_transfer[0];
    status=scene_jeep_npc_gunner_valid(entry);if(status)return status;
    /* An existing gunner must already own its preboarding look snapshot.
     * Otherwise publish() would replace it with the current mounted look. */
    if(!adapter->saved||state->role!=1||entry->resource!=resource||entry->player_body!=&scene_actor_body||
       entry->player_view!=&campaign_player_view)return RF_NOT_FOUND;
    status=scene_jeep_seat_index(resource,1,&old_seat);if(status)return status;
    if(resource->seat!=old_seat)return RF_FORMAT;
    status=scene_jeep_seat_index(resource,0,&next_seat);if(status)return status;
    status=rf_physics_tensor_world(entry->physics->state.inverse_inertia,
        entry->physics->state.orientation,tensor);if(status)return status;
    for(i=0;i<3;i++){
        float angular=0,v=entry->physics->state.velocity[i];
        if(!isfinite(v))return RF_FORMAT;speed2+=v*v;
        for(j=0;j<3;j++)angular+=tensor[i*3+j]*entry->physics->state.momentum[j];
        if(!isfinite(angular))return RF_FORMAT;turn2+=angular*angular;
    }
    if(speed2>.01f||turn2>.01f){++rf_scene_jeep_driver_transfer[1];return RF_OK;}
    status=rf_static_model_tag_place(&resource->tags,next_seat,
        entry->physics->state.orientation,entry->physics->state.position,tag);if(status)return status;
    scene_driller_seat_basis(entry,next_seat,tag,target.basis);memcpy(target.position,tag+9,12);
    scene_driller_point((const float[3]){0,0,0},target.basis,entry->eye_offset,offset);
    for(i=0;i<3;i++)target.position[i]-=offset[i];
    memcpy(from.position,scene_actor_body.state.position,12);memcpy(from.basis,scene_actor_body.state.orientation,36);
    status=scene_vehicle_seated_path(entry,&from,&target,&clear);if(status)return status;
    if(!clear){++rf_scene_jeep_driver_transfer[2];return RF_OK;}
    /* Player publication stages every fallible body/tensor/group operation
     * before assignment. Roll back its early pose write on failure, as the
     * existing single-player seat switch does. Keep original session.saved. */
    old_pose=entry->player.pose;old_adapter=*adapter;resource->seat=next_seat;
    status=scene_driller_player_publish(adapter,entry,1);
    if(status){resource->seat=old_seat;entry->player.pose=old_pose;*adapter=old_adapter;return status;}
    entry->host.driver=entry->player.handle;entry->occupant=(int32_t)entry->player.handle;
    entry->gunner_occupant=-1;state->role=0;*changed=1;
    ++rf_scene_jeep_driver_transfer[3];rf_scene_jeep_driver_transfer[4]=entry->host.handle;
    rf_scene_jeep_driver_transfer[5]=entry->player.handle;
    memcpy(rf_scene_jeep_driver_transfer+6,entry->physics->state.position,12);return RF_OK;
}
#endif
