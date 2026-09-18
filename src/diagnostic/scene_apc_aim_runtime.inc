#ifndef RF_SCENE_APC_AIM_RUNTIME_INC
#define RF_SCENE_APC_AIM_RUNTIME_INC
/* Seated relative eye state is separate from the saved on-foot look. */
static int scene_apc_aim_tick(scene_stream *s,uint32_t frame)
{
    scene_driller_runtime *r=s->driller_runtime;float camera[12];scene_vehicle_pose pose;const rf_entity_eye_limits *limits=&s->apc_aim_limits;
    const rf_entity_eye_limits gunner_limits={{-1.0471975512f,-3.1415926536f,0},{1.0471975512f,3.1415926536f,0}};int status;
    if((rf_scene_vehicle_enabled!=2 && rf_scene_vehicle_enabled!=3) || !r || !r->entry.session.active){s->apc_aim_active=0;return RF_OK;}
    if(!s->apc_aim_active)memset(&s->apc_aim,0,sizeof(s->apc_aim));
    status=scene_driller_entry_exit_pose(&r->entry,&pose,camera);if(status)return status;
    /* Jeep driver class locks relative eye movement. Gunner articulation is a
     * playable port policy until original turret controls are reconstructed:
     * +/-60degree elevation and one complete horizontal revolution. */
    if(rf_scene_vehicle_enabled==3 && scene_jeep_can_fire(&r->jeep_seat))limits=&gunner_limits;
    status=scene_vehicle_aim_update(&s->apc_aim,limits,player_input.look,1,
        frame?scene_step_seconds:0,camera,s->apc_aim_basis);if(status)return status;
    memcpy(s->apc_aim_eye,camera+9,12);s->apc_aim_active=1;return RF_OK;
}
static int scene_jeep_gun_tick(scene_stream *s)
{
    float primary[12];scene_driller_runtime *r=s->driller_runtime;
    if(!s->jeep_gun || !r)return RF_OK;
    if(s->apc_aim_active && scene_jeep_can_fire(&r->jeep_seat))
        return scene_jeep_gun_aim_pose(s->jeep_gun,r->physics.state.position,r->physics.state.orientation,
            s->apc_aim_basis+6,s->jeep_gun_pose,s->jeep_muzzle_pose);
    return scene_jeep_gun_attachment_pose(s->jeep_gun,r->physics.state.position,r->physics.state.orientation,
        s->jeep_gun_pose,s->jeep_muzzle_pose,primary);
}
static int scene_apc_aim_query(void *context,const float eye[3],const float end[3],float *fraction,uint32_t *matched)
{
    scene_apc_primary_query *q=context;rf_weapon_flight_contact contact={0};float delta[3];uint32_t i,liquid=0;int status;
    for(i=0;i<3;i++)delta[i]=end[i]-eye[i];
    status=scene_apc_primary_sweep_all(q,eye,delta,0,4,&contact,&liquid,matched);if(status)return status;
    *fraction=*matched?contact.hit.fraction:1;return RF_OK;
}
static int scene_apc_aim_direction(scene_stream *s,const float muzzle[3],float direction[3])
{
    scene_apc_primary_query query={s,s->driller_runtime->entry.host.handle,s->driller_runtime->entry.host.driver};
    return scene_vehicle_aim_ray(s->apc_aim_eye,s->apc_aim_basis,muzzle,1000,scene_apc_aim_query,&query,direction);
}
#endif
