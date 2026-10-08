#ifndef SCENE_ACTIVE_VEHICLE_VISIBILITY_AUDIT_INCLUDED
#define SCENE_ACTIVE_VEHICLE_VISIBILITY_AUDIT_INCLUDED
/* Native semantic codec audit, not a legacy-save gameplay restore. Include
 * after the opted-in visibility fixture and RFVC/RFPV implementations. Only
 * local wire/typed candidates are changed; no checkpoint stage is assigned.
 *
 * Words: runs,frame,UID,handle,live flags before/after,RFVC bytes,RFPV bytes,
 * wire version/object bits,capture status,v2 prepare/admit,exact v2 row,
 * v1 prepare/admit,v1-hidden reject,wrong-UID reject,hidden-v2 prepare/admit,
 * occupied RFVC validation,hidden-occupied reject,pass mask,first error.
 * Pass bits0..13: fixture state,capture,v2 prepare,v2 admit,v1 prepare,
 * v1 admit,v1-hidden reject,UID reject,hidden-v2 prepare,hidden-v2 admit,
 * occupied RFVC valid,hidden-occupied reject,live identity/flags intact,
 * stages closed. Complete success is0x3fff with first error0. */
uint32_t rf_scene_active_vehicle_visibility_audit[24];
static void scene_active_vehicle_visibility_audit_tick(scene_stream *s,uint32_t frame)
{
    unsigned char captured[256],scratch[512];const unsigned char *inner=NULL;
    scene_vehicle_checkpoint_record vehicle={0},occupied;
    scene_vehicle_physics_checkpoint_stage *stage=NULL;
    scene_vehicle_physics_saved expected={0};scene_driller_runtime *r=NULL;
    uint32_t *a=rf_scene_active_vehicle_visibility_audit,raw_bytes=0,bytes=0,inner_bytes=0,row=0;
    uint32_t view_flags=0,driver=UINT32_MAX,session=0;int32_t occupant=-1,gunner=-1;
    int status=RF_OK;
    if(!frame)memset(a,0,sizeof(rf_scene_active_vehicle_visibility_audit));
    if(frame!=150||a[0]||!rf_scene_active_vehicle_visibility_fixture[6]||
       rf_scene_active_vehicle_visibility_fixture[1]!=1)return;
    ++a[0];a[1]=frame;
#define SCENE_VISIBILITY_AUDIT_CHECK(bit,condition) do { \
    if(condition)a[22]|=1u<<(bit); \
    else {a[23]=(uint32_t)(status?status:RF_FORMAT);goto done;} \
} while(0)
    SCENE_VISIBILITY_AUDIT_CHECK(0,campaign_spawn&&!strcmp(campaign_current_level,"L1S3.rfl")&&
        campaign_authored_vehicle_uid==9627&&rf_scene_vehicle_enabled==2&&
        !campaign_passive_vehicle_count&&scene_active_vehicle_visibility_fixture_empty_npcs()&&s&&(r=s->driller_runtime)&&
        !(r->damage.damage.object_flags&0x4000u)&&!r->entry.session.active&&
        r->entry.occupant==-1&&r->entry.gunner_occupant==-1&&r->entry.host.driver==UINT32_MAX);
    a[2]=(uint32_t)campaign_authored_vehicle_uid;a[3]=r->entry.host.handle;
    a[4]=r->damage.damage.object_flags;view_flags=r->entry.view.flags_7c;
    driver=r->entry.host.driver;session=r->entry.session.active;
    occupant=r->entry.occupant;gunner=r->entry.gunner_occupant;
    status=scene_vehicle_checkpoint_capture(s,captured,&raw_bytes);
    if(!status){vehicle.profile=2;status=rf_apc_checkpoint_decode(captured,raw_bytes,&vehicle.value.apc);}
    if(!status)status=scene_vehicle_physics_checkpoint_live(s,0,2,&expected);
    if(!status)status=scene_vehicle_physics_checkpoint_capture(s,captured,sizeof(captured),raw_bytes,&bytes);
    a[6]=raw_bytes;a[7]=bytes;a[10]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(1,!status&&raw_bytes==RF_APC_CHECKPOINT_BYTES&&
        bytes==SCENE_VEHICLE_PHYSICS_HEADER+raw_bytes+SCENE_VEHICLE_PHYSICS_ROW&&
        !memcmp(captured,"RFPV",4)&&scene_history_word(captured+4)==2&&
        scene_history_word(captured+8)==raw_bytes&&scene_history_word(captured+12)==1);
    row=SCENE_VEHICLE_PHYSICS_HEADER+raw_bytes;
    a[8]=scene_history_word(captured+4);a[9]=scene_history_word(captured+row+20);
    status=scene_vehicle_physics_checkpoint_prepare(s,captured,bytes,sizeof(scratch),&stage,&inner,&inner_bytes);
    a[11]=(uint32_t)status;
    a[13]=!status&&stage&&stage->version==2&&stage->count==1&&
        !memcmp(stage->rows,&expected,sizeof(expected))&&
        inner==captured+SCENE_VEHICLE_PHYSICS_HEADER&&inner_bytes==raw_bytes;
    SCENE_VISIBILITY_AUDIT_CHECK(2,a[13]);
    status=scene_vehicle_physics_checkpoint_admit(s,stage,&vehicle,NULL);a[12]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(3,!status&&stage->admitted&&!scene_vehicle_physics_checkpoint_active_hidden(stage));
    scene_vehicle_physics_checkpoint_close(&stage);
    /* Change only the wrapper version: the captured zero-hidden row already
     * obeys the original v1 contract. RFVC bytes/checksum remain exact. */
    memcpy(scratch,captured,bytes);scene_history_put(scratch+4,1);
    status=scene_vehicle_physics_checkpoint_prepare(s,scratch,bytes,sizeof(scratch),&stage,&inner,&inner_bytes);
    a[14]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(4,!status&&stage&&stage->version==1&&stage->count==1&&
        !memcmp(stage->rows,&expected,sizeof(expected))&&inner_bytes==raw_bytes&&
        !memcmp(inner,captured+SCENE_VEHICLE_PHYSICS_HEADER,raw_bytes));
    status=scene_vehicle_physics_checkpoint_admit(s,stage,&vehicle,NULL);a[15]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(5,!status&&stage->admitted&&!scene_vehicle_physics_checkpoint_active_hidden(stage));
    scene_vehicle_physics_checkpoint_close(&stage);
    scene_history_put(scratch+row+20,a[9]|0x4000u);
    status=scene_vehicle_physics_checkpoint_prepare(s,scratch,bytes,sizeof(scratch),&stage,&inner,&inner_bytes);
    a[16]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(6,status==RF_FORMAT&&!stage);
    memcpy(scratch,captured,bytes);scene_history_put(scratch+row,a[2]^1u);
    status=scene_vehicle_physics_checkpoint_prepare(s,scratch,bytes,sizeof(scratch),&stage,&inner,&inner_bytes);
    a[17]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(7,status==RF_FORMAT&&!stage);
    memcpy(scratch,captured,bytes);scene_history_put(scratch+row+20,a[9]|0x4000u);
    status=scene_vehicle_physics_checkpoint_prepare(s,scratch,bytes,sizeof(scratch),&stage,&inner,&inner_bytes);
    a[18]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(8,!status&&stage&&stage->version==2&&stage->count==1&&
        stage->rows[0].object_bits==(expected.object_bits|0x4000u));
    status=scene_vehicle_physics_checkpoint_admit(s,stage,&vehicle,NULL);a[19]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(9,!status&&stage->admitted&&scene_vehicle_physics_checkpoint_active_hidden(stage));
    occupied=vehicle;occupied.value.vehicle.player_occupied=1;
    status=rf_apc_checkpoint_validate(&occupied.value.apc);a[20]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(10,!status);
    status=scene_vehicle_physics_checkpoint_admit(s,stage,&occupied,NULL);a[21]=(uint32_t)status;
    SCENE_VISIBILITY_AUDIT_CHECK(11,status==RF_FORMAT&&!stage->admitted);
done:
    scene_vehicle_physics_checkpoint_close(&stage);
    if(!stage)a[22]|=1u<<13;
    if(r){
        a[5]=r->damage.damage.object_flags;
        if(a[4]==a[5]&&view_flags==r->entry.view.flags_7c&&a[3]==r->entry.host.handle&&
           a[2]==(uint32_t)campaign_authored_vehicle_uid&&driver==r->entry.host.driver&&
           session==r->entry.session.active&&occupant==r->entry.occupant&&gunner==r->entry.gunner_occupant&&
           rf_object_registry_lookup(&campaign_registry,a[3])==&r->entry.registration)
            a[22]|=1u<<12;
        else if(!a[23])a[23]=(uint32_t)RF_FORMAT;
    }
#undef SCENE_VISIBILITY_AUDIT_CHECK
}
#endif
