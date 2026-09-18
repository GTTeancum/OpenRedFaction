#ifndef SCENE_DRILLER_PLAYER_ADAPTER_INC
#define SCENE_DRILLER_PLAYER_ADAPTER_INC
/* Live player publication for the DEV vehicle. Include after player/support
 * globals and scene_driller_entry_exit.inc. Parent owns scheduling: no ordinary
 * actor movement, stance/climb/swim, weapon fire or listener-pose update while
 * seated. Host rigid step runs first, then publish/camera use its accepted pose. */
static void scene_player_impact_relocated(void);
typedef struct scene_driller_player_adapter {
    rf_look_pose saved_look;uint32_t saved;
} scene_driller_player_adapter;
static int scene_driller_player_publish(scene_driller_player_adapter *adapter,
    scene_driller_entry_exit *vehicle,uint32_t changed)
{
    rf_physics_body_state state;rf_group_attached_pose group;scene_vehicle_pose pose;
    float camera[12];uint32_t active;int status;
    if(!adapter || !vehicle)return RF_RANGE;
    active=vehicle->session.active;
    if(!active && !changed)return RF_OK;
    pose=vehicle->player.pose;
    if(active){status=scene_driller_entry_exit_pose(vehicle,&pose,camera);if(status)return status;}
    if(!scene_vehicle_pose_valid(&pose))return RF_FORMAT;
    state=scene_actor_body.state;group=rf_scene_actor_pose;
    memcpy(state.position,pose.position,12);memcpy(state.next_position,pose.position,12);
    memcpy(state.orientation,pose.basis,36);memcpy(state.next_orientation,pose.basis,36);
    status=rf_physics_tensor_world(state.local_tensor,state.orientation,state.world_tensor);if(status)return status;
    memset(state.velocity,0,12);memset(state.vector_138,0,12);
    status=rf_physics_body_prepare_contact(&state);if(status)return status;
    status=rf_group_pose_set_position(&group,pose.position);if(status)return status;
    memset(group.velocity,0,12);memcpy(group.input_matrix,pose.basis,36);
    memcpy(group.output_matrix,pose.basis,36);memcpy(group.pending_matrix,pose.basis,36);
    memcpy(state.bounds.minimum,group.minimum,12);memcpy(state.bounds.maximum,group.maximum,12);
    if(active && !adapter->saved){adapter->saved_look=actor_look;adapter->saved=1;}
    if(active){memcpy(actor_look.body_orientation,pose.basis,36);memcpy(actor_look.eye_orientation,camera,36);}
    else if(adapter->saved){actor_look=adapter->saved_look;adapter->saved=0;}
    /* Reacquire floor on the first ordinary tick after exit, rather than carry
     * an old ladder, mover or preboarding support reference to this position. */
    if(!active){state.flags|=1u;rf_scene_actor_landing[1]=3;}
    scene_actor_body.state=state;rf_scene_actor_pose=group;
    memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));campaign_support_handle=0;
    memset(&campaign_piece_support,0,sizeof(campaign_piece_support));
    if(changed)scene_player_impact_relocated();
    return RF_OK;
}
/* Actual authored player eye offset passed to entry_exit_open: standing offset
 * is retained independent of camera bob/weapon view or a transient crouch. */
static const float *scene_driller_player_eye_offset(void)
{return rf_scene_actor_initial_eye_offsets;}
static int scene_driller_player_camera(scene_driller_entry_exit *vehicle,float position[3],float orientation[3][3])
{
    scene_vehicle_pose body;float camera[12];int status;
    if(!vehicle || !position || !orientation)return RF_RANGE;
    status=scene_driller_entry_exit_pose(vehicle,&body,camera);if(status)return status;
    memcpy(position,camera+9,12);memcpy(orientation,camera,36);return RF_OK;
}
#endif
