#ifndef SCENE_PLAYER_SUPPORT_SAVE_FIXTURE_INCLUDED
#define SCENE_PLAYER_SUPPORT_SAVE_FIXTURE_INCLUDED
/* Read-only observations for the isolated L20S2 Fighter4717 support fixture.
 * Include after scene_world_load.inc; call after the late frame-zero load,
 * before autosave. This never seeds support or changes gameplay state.
 * Rows: exact post-load0, post-physics1/2/5/18/38, source post-physics148.
 * A150-frame presentation has its last physics tick at148, not149. */
uint32_t rf_scene_player_support_save_fixture[7][32];
/* runs,frame,checks,pass mask,first failed case+1,status,RFEN bytes/version,
 * support UID,roundtrip exact,bad-output NULL mask,RFVA bytes,host admission
 * flags,closed environment/passive stages,reserved,then16 raw statuses. */
uint32_t rf_scene_player_support_save_audit[32];
static int scene_player_support_save_audit_check(uint32_t index,int status,int passed)
{
    uint32_t *a=rf_scene_player_support_save_audit;
    ++a[2];a[16+index]=(uint32_t)status;
    if(passed)a[3]|=1u<<index;
    else if(!a[4]){a[4]=index+1;a[5]=(uint32_t)status;}
    return passed;
}
static void scene_player_support_save_audit(uint32_t frame)
{
    unsigned char identity[32]={0},data[512],passive_data[96];
    uint32_t *a=rf_scene_player_support_save_audit,uid=0,bytes=0,passive_bytes=0,bits,i;
    float bad_velocity[3];int status;scene_world_environment_stage *environment=NULL;
    scene_passive_vehicle_checkpoint_stage *passive=NULL,copy={0};scene_passive_vehicle_saved saved={0};
    if(frame!=148||rf_scene_passive_roof_fixture[9]!=8||a[0])return;
    ++a[0];a[1]=frame;
    status=scene_checkpoint_player_rotating_point(campaign_support_handle,
        scene_actor_body.state.position,campaign_support_velocity,&uid);
    a[8]=uid;scene_player_support_save_audit_check(0,status,status==RF_OK&&uid==4717);
    status=scene_checkpoint_player_rotating_point(campaign_support_handle^0x10000u,
        scene_actor_body.state.position,campaign_support_velocity,&uid);
    scene_player_support_save_audit_check(1,status,status==RF_NOT_FOUND&&!uid);
    memcpy(bad_velocity,campaign_support_velocity,12);bits=0x7fc00000u;memcpy(bad_velocity,&bits,4);
    status=scene_checkpoint_player_rotating_point(campaign_support_handle,
        scene_actor_body.state.position,bad_velocity,&uid);
    scene_player_support_save_audit_check(2,status,status==RF_FORMAT&&!uid);
    bad_velocity[0]=10001.f;
    status=scene_checkpoint_player_rotating_point(campaign_support_handle,
        scene_actor_body.state.position,bad_velocity,&uid);
    scene_player_support_save_audit_check(3,status,status==RF_FORMAT&&!uid);
    status=scene_checkpoint_player_support_uid(scene_actor_collision_owner,&uid);
    scene_player_support_save_audit_check(4,status,status==RF_NOT_FOUND&&!uid);
    memcpy(identity,"player-support-save-audit",25);
    status=scene_world_environment_encode(identity,campaign_blackout_now,data,sizeof(data),&bytes);
    a[6]=bytes;a[7]=!status&&bytes>=8?scene_history_word(data+4):0;
    if(!scene_player_support_save_audit_check(5,status,status==RF_OK&&bytes>=244&&bytes<=sizeof(data)&&a[7]==7))goto done;
    status=scene_world_environment_prepare(identity,campaign_blackout_now,data,bytes,2u*1024u*1024u,&environment);
    a[9]=!status&&environment&&environment->support_point_present==1&&environment->support_uid==4717&&
        !memcmp(environment->support_point_velocity,campaign_support_velocity,12);
    scene_player_support_save_audit_check(6,status,status==RF_OK&&a[9]);
    scene_world_environment_close(&environment);a[13]=environment==NULL;
    /* Corrupt only local bytes and refresh the RFEN checksum, so rejection
     * proves semantic validation rather than envelope checksum detection. */
    for(i=0;i<3;i++){
        uint32_t offset=i==2?76:bytes-12,original=scene_history_word(data+offset);
        if(i==0)scene_history_put(data+offset,0x7fc00000u);
        else if(i==1)scene_history_put_float(data+offset,10001.f);
        else scene_history_put(data+offset,0);
        scene_history_put(data+12,scene_history_hash(data,bytes));
        status=scene_world_environment_prepare(identity,campaign_blackout_now,data,bytes,2u*1024u*1024u,&environment);
        if(!environment)a[10]|=1u<<i;
        scene_player_support_save_audit_check(7+i,status,status==RF_FORMAT&&!environment);
        scene_world_environment_close(&environment);
        scene_history_put(data+offset,original);scene_history_put(data+12,scene_history_hash(data,bytes));
    }
    /* Capture and decode into scratch storage, then test stack copies of the
     * real RFVA2 saved row. Never assign a stage or mutate its live owner. */
    status=scene_passive_vehicle_checkpoint_capture(passive_data,sizeof(passive_data),0,&passive_bytes);
    if(!status)status=scene_passive_vehicle_checkpoint_prepare(passive_data,passive_bytes,4096,&passive);
    a[11]=passive_bytes;
    if(!scene_player_support_save_audit_check(10,status,status==RF_OK&&passive&&passive->count==1&&passive->version==2))goto done;
    copy=*passive;saved=passive->rows[0];copy.rows=&saved;
    a[12]=!!saved.loaded|(!!saved.has_damage<<1)|(!!saved.attached<<2)|
        ((saved.health>0&&!saved.destroyed)<<3)|(!(saved.object_flags&(2u|0x4000u))<<4);
    status=scene_world_player_point_host_valid(&copy,0,4717,campaign_support_velocity);
    scene_player_support_save_audit_check(11,status,status==RF_OK&&a[12]==31);
    saved.uid^=1u;status=scene_world_player_point_host_valid(&copy,0,4717,campaign_support_velocity);
    scene_player_support_save_audit_check(12,status,status==RF_FORMAT);saved=passive->rows[0];
    saved.attached=0;status=scene_world_player_point_host_valid(&copy,0,4717,campaign_support_velocity);
    scene_player_support_save_audit_check(13,status,status==RF_FORMAT);saved=passive->rows[0];
    saved.object_flags|=0x4000u;status=scene_world_player_point_host_valid(&copy,0,4717,campaign_support_velocity);
    scene_player_support_save_audit_check(14,status,status==RF_FORMAT);saved=passive->rows[0];
    saved.health=0;status=scene_world_player_point_host_valid(&copy,0,4717,campaign_support_velocity);
    scene_player_support_save_audit_check(15,status,status==RF_FORMAT);
done:
    scene_world_environment_close(&environment);scene_passive_vehicle_checkpoint_close(&passive);
    a[14]=passive==NULL;
}
static void scene_player_support_save_fixture_tick(uint32_t frame)
{
    static const uint32_t frames[7]={0,1,2,5,18,38,148};
    const scene_passive_vehicle *owner;uint32_t i,*row;
    if(!frame){memset(rf_scene_player_support_save_fixture,0,sizeof(rf_scene_player_support_save_fixture));
        memset(rf_scene_player_support_save_audit,0,sizeof(rf_scene_player_support_save_audit));}
    if(!campaign_spawn||strcmp(campaign_current_level,"L20S2.rfl")||
       campaign_passive_vehicle_count!=1||campaign_passive_vehicles[0].uid!=4717||
       (rf_scene_passive_roof_fixture[9]!=8&&!rf_scene_world_checkpoint_state[8]))return;
    for(i=0;i<7&&frames[i]!=frame;i++){ /* Search the bounded observation slots. */ }
    if(i==7)return;
    owner=campaign_passive_vehicles;row=rf_scene_player_support_save_fixture[i];
    row[0]=1;row[1]=frame;row[2]=campaign_player_object.handle;row[3]=campaign_support_handle;
    row[4]=rf_scene_actor_landing[1];row[5]=scene_actor_body.state.flags;
    memcpy(row+6,scene_actor_body.state.position,12);memcpy(row+9,scene_actor_body.state.velocity,12);
    memcpy(row+12,campaign_support_velocity,12);row[15]=owner->handle;row[16]=owner->uid;
    memcpy(row+17,owner->pose.position,12);memcpy(row+20,owner->pose.input_matrix,36);
    memcpy(row+29,&owner->damage.state.effects.health,4);row[30]=owner->damage.object_flags;
    row[31]=rf_scene_player_jump[1];
    scene_player_support_save_audit(frame);
}
#endif
