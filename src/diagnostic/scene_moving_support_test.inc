/* Explicit test motion on an actually extracted saved support. This is a
 * kinematic stimulus, not a claim that ordinary debris rises under gravity. */
uint32_t rf_scene_moving_support_enabled,rf_scene_moving_support_test[10][16];
static scene_piece_support moving_support_target;
static float moving_support_origin[3];
static uint32_t moving_support_ready;
static int scene_moving_support_stimulus(uint32_t frame)
{
    rf_physics_body *body;uint32_t steps,k;
    if(!rf_scene_moving_support_enabled)return RF_OK;
    if(!rf_scene_dev_room_enabled || !rf_scene_player_checkpoint_state[1])return RF_RANGE;
    if(!frame) {
        moving_support_ready=0;memset(&moving_support_target,0,sizeof(moving_support_target));
        memset(rf_scene_moving_support_test,0,sizeof(rf_scene_moving_support_test));
    }
    if(frame<30 || frame>120)return RF_OK;
    if(frame==30) {
        moving_support_target=campaign_piece_support;
        body=campaign_piece_support_body(&moving_support_target);
        if(!body || rf_scene_actor_landing[1]!=1)return RF_NOT_FOUND;
        memcpy(moving_support_origin,body->state.position,12);moving_support_ready=1;
    }
    if(!moving_support_ready)return RF_FORMAT;
    if(frame==120)return scene_detached_sources_damage(scene_actor_collision_owner,
        moving_support_target.tag-1,moving_support_target.piece,400);
    body=campaign_piece_support_body(&moving_support_target);if(!body)return RF_NOT_FOUND;
    steps=frame<90?frame-29:60;
    for(k=0;k<3;k++)body->state.position[k]=body->state.next_position[k]=moving_support_origin[k];
    body->state.position[1]=body->state.next_position[1]=(float)((double)moving_support_origin[1]+steps/240.0);
    memset(body->state.velocity,0,12);body->state.velocity[1]=frame<90?.25f:0;
    body->state.flags&=~0x80000000u; /* Kinematic fixture owns pose for this interval. */
    return rf_physics_body_prepare_sweep(&body->state);
}
static void scene_moving_support_record(uint32_t frame)
{
    static const uint32_t frames[10]={29,30,59,89,90,119,120,150,180,240};
    uint32_t i;rf_physics_body *body;
    if(!rf_scene_moving_support_enabled)return;
    for(i=0;i<10;i++)if(frames[i]==frame) {
        uint32_t *row=rf_scene_moving_support_test[i];
        row[0]=frame;row[1]=frame<30?0:frame<90?1:frame<120?2:3;
        row[2]=rf_scene_actor_landing[1];row[3]=campaign_piece_support.tag;
        memcpy(row+4,scene_actor_body.state.position,12);
        body=campaign_piece_support_body(moving_support_ready?&moving_support_target:&campaign_piece_support);
        if(body)memcpy(row+7,body->state.position,12);
        memcpy(row+10,campaign_support_velocity,12);row[13]=body!=NULL;row[14]=moving_support_ready;
        printf("MOVING_SUPPORT");for(uint32_t k=0;k<16;k++)printf(" %u",row[k]);printf("\n");
        return;
    }
}
