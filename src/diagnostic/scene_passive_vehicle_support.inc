#ifndef SCENE_PASSIVE_VEHICLE_SUPPORT_INC
#define SCENE_PASSIVE_VEHICLE_SUPPORT_INC
/* Exact point carry for registered passive chassis. Reuse the existing mover
 * interval math, replacing center carry rather than pretranslating an actor.
 * A contact is already in the end frame; a bound rider starts in the old one. */
int rf_scene_npc_support_interval_target(const scene_mover_interval *,const float *,float,float *,float *);
int rf_scene_npc_support_interval_velocity(const scene_mover_interval *,const float *,float,float *);
uint32_t rf_scene_passive_support_motion[2][48],rf_scene_passive_support_live[2][16];
uint32_t rf_scene_passive_support_release[8];
static void scene_passive_vehicle_support_reset(void)
{
    memset(rf_scene_passive_support_motion,0,sizeof(rf_scene_passive_support_motion));
    memset(rf_scene_passive_support_live,0,sizeof(rf_scene_passive_support_live));
    memset(rf_scene_passive_support_release,0,sizeof(rf_scene_passive_support_release));
    campaign_passive_vehicle_motion_active=0;campaign_passive_vehicle_motion_frame=0;
    campaign_player_support_epoch=0;
}
static const scene_passive_vehicle *scene_passive_vehicle_support_owner(uint32_t handle)
{
    uint32_t i;void *object;
    if(!handle||!(object=rf_object_registry_lookup(&campaign_registry,handle)))return NULL;
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle&&
        object==campaign_passive_vehicles+i)
        return campaign_passive_vehicles+i;
    return NULL;
}
static int scene_passive_vehicle_support_point(uint32_t handle,uint32_t actor,const float position[3],
    uint32_t from_end,float target[3],float velocity[3],uint32_t *matched)
{
    const scene_passive_vehicle *owner;scene_mover_interval interval={0};
    float point[3],speed[3]={0};uint32_t i;int status;
    if(!position||!target||!velocity||!matched||from_end>1)return RF_RANGE;
    *matched=0;owner=scene_passive_vehicle_support_owner(handle);
    if(!owner||(owner->damage.object_flags&0x4000u))return RF_OK;
    for(i=0;i<3;i++)if(!isfinite(position[i]))return RF_FORMAT;
    memcpy(point,position,12);
    /* Stopped/detached or restore-invalidated hosts have no current interval.
     * Do not replay their last rotation or extrapolate a stale saved velocity. */
    if(owner->attached&&owner->motion_valid&&campaign_passive_vehicle_motion_active){
        memcpy(interval.start,owner->previous_position,12);memcpy(interval.end,owner->pose.position,12);
        memcpy(interval.matrix,owner->previous_basis,36);memcpy(interval.end_matrix,owner->pose.input_matrix,36);
        interval.handle=handle;interval.changed=(memcmp(interval.start,interval.end,12)!=0)|
            ((memcmp(interval.matrix,interval.end_matrix,36)!=0)<<1);
        if(!(interval.changed&2u)){
            /* Preserve the established translating-host arithmetic and the
             * existing player-save exact center-velocity admission. */
            memcpy(speed,owner->velocity,12);
            if(!from_end)for(i=0;i<3;i++)point[i]=position[i]+speed[i]*scene_step_seconds;
            status=RF_OK;
        }else if(from_end)status=rf_scene_npc_support_interval_velocity(&interval,position,scene_step_seconds,speed);
        else status=rf_scene_npc_support_interval_target(&interval,position,scene_step_seconds,point,speed);
        if(status){rf_scene_passive_support_motion[from_end][7]=(uint32_t)status;return status;}
        if(interval.changed&2u){
            uint32_t *row=rf_scene_passive_support_motion[from_end];
            ++row[0];row[1]=actor;row[2]=handle;row[3]=owner->uid;row[4]=interval.changed;row[5]=1;
            row[6]=campaign_passive_vehicle_motion_frame;row[7]=0;
            memcpy(row+8,position,12);memcpy(row+11,point,12);memcpy(row+14,speed,12);
            memcpy(row+17,interval.start,12);memcpy(row+20,interval.end,12);
            memcpy(row+23,interval.matrix,36);memcpy(row+32,interval.end_matrix,36);
            memcpy(row+41,&owner->damage.state.effects.health,4);row[42]=owner->damage.object_flags;
        }
    }
    memcpy(target,point,12);memcpy(velocity,speed,12);*matched=1;return RF_OK;
}
static void campaign_player_passive_support_release(rf_physics_body_state *state,uint32_t frame)
{
    const scene_passive_vehicle *owner=scene_passive_vehicle_support_owner(campaign_support_handle);
    if(!owner||!state)return;
    ++rf_scene_passive_support_release[0];rf_scene_passive_support_release[1]=frame;
    rf_scene_passive_support_release[2]=campaign_support_handle;
    rf_scene_passive_support_release[3]=campaign_player_object.handle;
    memcpy(rf_scene_passive_support_release+4,campaign_support_velocity,12);
    campaign_support_handle=0;campaign_player_support_epoch=0;state->flags&=~0x400000u; /* Preserve the last additive point velocity. */
}
static void scene_passive_vehicle_support_live_probe(uint32_t frame,uint32_t player_mode)
{
    uint32_t *row=rf_scene_passive_support_live[0],i;
    row[0]=1;row[1]=frame;row[2]=campaign_player_object.handle;row[3]=campaign_support_handle;row[4]=player_mode;
    memcpy(row+5,&campaign_player_damage.state.effects.health,4);memcpy(row+6,scene_actor_body.state.position,12);
    memcpy(row+9,campaign_support_velocity,12);row[12]=campaign_player_view.flags_7c;
    row[13]=(uint32_t)campaign_player_view.linked_handle;row[14]=scene_actor_body.state.flags;
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *npc=campaign_npc_bodies+i;
        if(!npc->registration.view||npc->registration.handle!=rf_scene_passive_npc_fixture[0])continue;
        row=rf_scene_passive_support_live[1];row[0]=1;row[1]=frame;row[2]=npc->registration.handle;
        row[3]=npc->support.handle;row[4]=npc->movement_slot<16?(uint32_t)campaign_modes[npc->movement_slot].index:UINT32_MAX;
        memcpy(row+5,&npc->damage.effects.health,4);memcpy(row+6,npc->body.state.position,12);
        memcpy(row+9,npc->support_velocity,12);row[12]=npc->object_flags;
        row[13]=(uint32_t)npc->view.linked_handle;row[14]=npc->body.state.flags;break;
    }
}
#endif
