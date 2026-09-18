#ifndef RF_SCENE_EVENT_GAMEPLAY_INC
#define RF_SCENE_EVENT_GAMEPLAY_INC
/* First-pass Teleport_Player63. Pose semantics: original4b9820/48a230;
 * disk conversion:462150 ->52cac0. See FUTURE-CAMPAIGN-TELEPORT-PLAYER.
 * Include after actor_room_locate and before scene_frame. No allocations.
 * Vehicle-host teleport is deliberately not guessed here. */
static uint32_t campaign_teleport_ready,campaign_teleport_pending;
static rf_level_event campaign_teleport_destination;

static int campaign_teleport_prepare(const rf_level_event *event,
    rf_physics_body_state *body,rf_group_attached_pose *pose,rf_look_pose *look)
{
    const float identity[9]={1,0,0,0,1,0,0,0,1};
    const uint32_t rotation[3]={1,1,1};
    rf_physics_body_state next;rf_group_attached_pose publication;rf_look_pose facing;
    rf_spawn_look_angles angles;float matrix[9];uint32_t i;int status;
    if(!event || !body || !pose || !look)return RF_RANGE;
    if(!event->has_orientation)return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(event->position[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(event->orientation_disk[i]))return RF_FORMAT;
    memcpy(matrix,event->orientation_disk+3,24);memcpy(matrix+6,event->orientation_disk,12);
    next=*body;publication=*pose;facing=*look;
    memcpy(next.position,event->position,12);memcpy(next.next_position,event->position,12);
    memcpy(next.orientation,matrix,36);memcpy(next.next_orientation,matrix,36);
    status=rf_physics_tensor_world(next.local_tensor,matrix,next.world_tensor);if(status)return status;
    publication.radius=next.bounds.radius;
    status=rf_group_pose_set_position(&publication,event->position);if(status)return status;
    memcpy(next.bounds.minimum,publication.minimum,12);memcpy(next.bounds.maximum,publication.maximum,12);
    memcpy(publication.input_matrix,matrix,36);memcpy(publication.output_matrix,matrix,36);
    memcpy(publication.pending_matrix,matrix,36);
    status=rf_look_spawn_angles(matrix,identity,rotation,&angles);if(status)return status;
    /* The look owner stores eye angles relative to its yaw-only body. */
    memcpy(facing.state.body_angles,angles.body,12);memcpy(facing.state.eye_angles,angles.eye,12);
    facing.state.eye_angles[1]-=angles.body[1];
    memcpy(facing.body_orientation,matrix,36);memcpy(facing.eye_orientation,matrix,36);
    /* Preserve linear/angular velocity, commands, health and inventory. */
    *body=next;*pose=publication;*look=facing;return RF_OK;
}
static int campaign_teleport_apply(const rf_level_event *event)
{
    rf_physics_body_state body=scene_actor_body.state;
    rf_group_attached_pose pose=rf_scene_actor_pose;rf_look_pose look=actor_look;
    rf_entity_room_state room={0};actor_room_context location={campaign_alpha_world,0,UINT32_MAX,0};
    int status;
    if(!scene_actor_body.allocated_bytes)return RF_NOT_FOUND;
    if(campaign_player_view.linked_handle!=-1)return RF_NOT_FOUND;
    status=campaign_teleport_prepare(event,&body,&pose,&look);if(status)return status;
    room.flags=pose.flags;
    if(location.world) {
        status=rf_entity_room_refresh(&room,body.position,0,actor_room_locate,NULL,&location);if(status)return status;
        pose.flags=room.flags;
    }
    scene_actor_body.state=body;rf_scene_actor_pose=pose;actor_look=look;rf_scene_actor_room_state=room;
    campaign_player_view.flags_7c|=0x04000000u;
    campaign_support_handle=0;memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));
    memset(&campaign_piece_support,0,sizeof(campaign_piece_support));
    /* Reacquire ground/liquid/ladder at the destination on the next normal tick. */
    if(rf_scene_actor_landing[1]==1)rf_scene_actor_landing[1]=3;
    campaign_climb.previous_region=0;
    memset(&campaign_player_contact,0,sizeof(campaign_player_contact));
    return RF_OK;
}
static int campaign_teleport_player(void *context,const rf_level_event *event)
{
    (void)context;
    if(!event)return RF_RANGE;
    {
        /* Commit at the frame boundary, after controller/physics local proposals.
         * Last destination wins if a chain requests several teleports this frame.
         * Linked callbacks see the previous pose until this boundary (port policy). */
        uint32_t i;if(!event->has_orientation)return RF_FORMAT;
        if(campaign_teleport_ready && campaign_player_view.linked_handle!=-1)return RF_NOT_FOUND;
        for(i=0;i<3;i++)if(!isfinite(event->position[i]))return RF_FORMAT;
        for(i=0;i<9;i++)if(!isfinite(event->orientation_disk[i]))return RF_FORMAT;
        campaign_teleport_destination=*event;campaign_teleport_pending=1;return RF_OK;
    }
}

static int campaign_teleport_begin(void)
{
    int status=RF_OK;campaign_teleport_ready=1;
    if(campaign_teleport_pending) {
        status=campaign_teleport_apply(&campaign_teleport_destination);
        if(!status)campaign_teleport_pending=0;
    }
    return status;
}

#endif
