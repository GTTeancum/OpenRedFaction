#ifndef RF_SCENE_SECONDARY_VEHICLE_FIXTURE_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_FIXTURE_INCLUDED
/* Explicit opt-in read-only source/save/retirement evidence. Include after
 * secondary checkpoint and owner collision. Tick after the late frame0 load.
 * SAPC four words: magic,phase(1source/2retire/3retired),0,0.
 * All motion, retirement and reveal are genuine ordinary event effects. */
uint32_t rf_scene_secondary_vehicle_fixture[8]; /* read,phase,samples,UID,selected,enabled,audited,status */
uint32_t rf_scene_secondary_vehicle_samples[13][256];
uint32_t rf_scene_secondary_vehicle_clocks[13]; /* Exact remaining-timer query clock. */
/* One actual sphere from26 sweeps downward onto the visible9627 hull. This
 * isolated candidate query is NOT retired-owner movement or a scene mutation.
 * runs,frame,queryUID,handle,targetUID,handle,status,hit,hitHandle,fraction,
 * sourceIndex,targetIndex,sourceCount,targetCount,sourceRadius,targetRadius,
 * start3,end3,matrix9,sourceLocal3,targetWorld3,unchanged bits,world identity,
 * collision count,query flags,query limit,hit point3,normal3,selected visible. */
uint32_t rf_scene_secondary_vehicle_collision_audit[56];
static void scene_secondary_vehicle_fixture_open(void)
{
    FILE *file;uint32_t config[4]={0};int status=RF_OK,trailing;
    memset(rf_scene_secondary_vehicle_fixture,0,sizeof(rf_scene_secondary_vehicle_fixture));
    memset(rf_scene_secondary_vehicle_samples,0,sizeof(rf_scene_secondary_vehicle_samples));
    memset(rf_scene_secondary_vehicle_clocks,0,sizeof(rf_scene_secondary_vehicle_clocks));
    memset(rf_scene_secondary_vehicle_collision_audit,0,sizeof(rf_scene_secondary_vehicle_collision_audit));
#ifdef RF_IMAGE_XBOX_NATIVE
    errno=0;file=fopen("D:\\campaign-secondary-apc.bin","rb");
    if(!file){if(errno!=ENOENT)rf_scene_secondary_vehicle_fixture[7]=(uint32_t)RF_IO;return;}
#else
    return;
#endif
    rf_scene_secondary_vehicle_fixture[0]=1;
    if(fread(config,1,sizeof(config),file)!=sizeof(config))status=RF_FORMAT;
    trailing=fgetc(file);if(trailing!=EOF||ferror(file))status=RF_FORMAT;
    if(fclose(file))status=RF_IO;
    if(config[0]!=0x43504153u||config[1]<1||config[1]>3||config[2]||config[3])status=RF_FORMAT;
    rf_scene_secondary_vehicle_fixture[1]=config[1];rf_scene_secondary_vehicle_fixture[3]=26;
    rf_scene_secondary_vehicle_fixture[4]=9627;rf_scene_secondary_vehicle_fixture[5]=!status;
    rf_scene_secondary_vehicle_fixture[7]=(uint32_t)status;
}
static int scene_secondary_vehicle_fixture_remaining(int32_t deadline,uint32_t *out)
{
    int32_t value=-1;int status=RF_OK;
    if(deadline>=0)status=rf_timer_remaining(deadline,campaign_blackout_now,&value);
    if(!status)*out=(uint32_t)value;return status;
}
static void scene_secondary_vehicle_fixture_collision(scene_stream *s,uint32_t frame)
{
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle,before;
    scene_driller_runtime *selected=s?s->driller_runtime:NULL;scene_passive_vehicle *owner;
    scene_passive_vehicle owner_before;rf_vehicle_rigid_state selected_before;
    scene_driller_entry_exit entry_before;scene_driller_damage damage_before;
    rf_collision_body_query q={0};rf_geometry_body_hit hit={0};uint32_t *a=rf_scene_secondary_vehicle_collision_audit;
    uint32_t i,source=0,target=0,found=0;float center[3],offset[3],zero[3]={0},distance;int status;
    if(frame!=138||rf_scene_secondary_vehicle_fixture[1]!=2||a[0]||!r||!selected)return;
    a[0]=1;a[1]=frame;a[2]=r->uid;a[3]=r->handle;a[4]=9627;a[5]=selected->entry.host.handle;
    owner=scene_secondary_vehicle_owner(r);
    if(!owner||!r->physics.sphere_count||!selected->physics.sphere_count||
       (selected->damage.damage.object_flags&0x4000u)){a[6]=(uint32_t)RF_FORMAT;return;}
    before=*r;owner_before=*owner;selected_before=selected->physics.state;
    entry_before=selected->entry;damage_before=selected->damage.damage;
    for(i=1;i<r->physics.sphere_count;i++)
        if(r->physics.collision[i].radius>r->physics.collision[source].radius)source=i;
    for(i=1;i<selected->physics.sphere_count;i++)
        if(selected->physics.collision[i].radius>selected->physics.collision[target].radius)target=i;
    scene_driller_point(selected->physics.state.position,selected->physics.state.orientation,
        selected->physics.collision[target].center,center);
    scene_driller_point(zero,selected->physics.state.orientation,r->physics.collision[source].center,offset);
    distance=r->physics.collision[source].radius+selected->physics.collision[target].radius+1;
    q.spheres=r->physics.collision+source;q.count=1;q.flags=4;q.limit=1;
    q.radius=q.spheres[0].radius;
    memcpy(q.matrix,selected->physics.state.orientation,36);
    for(i=0;i<3;i++)q.start[i]=q.end[i]=center[i]-offset[i];
    q.start[1]+=distance;
    a[10]=source;a[11]=target;a[12]=r->physics.sphere_count;a[13]=selected->physics.sphere_count;
    memcpy(a+14,&q.spheres[0].radius,4);memcpy(a+15,&selected->physics.collision[target].radius,4);
    memcpy(a+16,q.start,12);memcpy(a+19,q.end,12);memcpy(a+22,q.matrix,36);
    memcpy(a+31,q.spheres[0].center,12);memcpy(a+34,center,12);
    a[38]=r->collision.world==s->collision&&selected->collision.world==s->collision;
    a[39]=q.count;a[40]=q.flags;memcpy(a+41,&q.limit,4);
    status=scene_vehicle_owner_collision_query(s,r->handle,&q,&hit,&found);
    a[6]=(uint32_t)status;a[7]=found;a[8]=hit.contact.object_id;memcpy(a+9,&hit.contact.fraction,4);
    memcpy(a+42,hit.contact.point,12);memcpy(a+45,hit.contact.normal,12);
    a[48]=!(selected->damage.damage.object_flags&0x4000u);
    if(!memcmp(&before,r,sizeof(before)))a[37]|=1;
    if(!memcmp(&owner_before,owner,sizeof(owner_before)))a[37]|=2;
    if(!memcmp(&selected_before,&selected->physics.state,sizeof(selected_before)))a[37]|=4;
    if(!memcmp(&entry_before,&selected->entry,sizeof(entry_before)))a[37]|=8;
    if(!memcmp(&damage_before,&selected->damage.damage,sizeof(damage_before)))a[37]|=16;
}
/* Row: header16, exact RFSV wire64, rigid34, selected rigid34, selected/player16,
 * seven event rows12, and telemetry12. No raw pointers enter observed rows. */
static void scene_secondary_vehicle_fixture_tick(scene_stream *s,uint32_t frame)
{
    static const uint32_t frames[13]={0,20,40,60,78,80,100,118,119,120,138,158,159};
    static const uint32_t events[7]={9517,9626,9628,916700,916701,916702,916703};
    scene_secondary_vehicle_runtime *r;scene_driller_runtime *selected;scene_passive_vehicle *owner;
    scene_passive_vehicle_saved saved;uint32_t i,j,*p,status=RF_OK;
    if(!frame)scene_secondary_vehicle_fixture_open();
    if(!rf_scene_secondary_vehicle_fixture[5])return;
    for(i=0;i<13&&frames[i]!=frame;i++){}if(i==13)return;
    rf_scene_secondary_vehicle_clocks[i]=(uint32_t)campaign_blackout_now;
    p=rf_scene_secondary_vehicle_samples[i];p[0]=1;p[1]=frame;p[2]=rf_scene_secondary_vehicle_fixture[1];
    ++rf_scene_secondary_vehicle_fixture[2];
    r=campaign_secondary_vehicle;selected=s?s->driller_runtime:NULL;
    if(!campaign_spawn||strcmp(campaign_current_level,"L1S3.rfl")||campaign_authored_vehicle_uid!=9627||
       campaign_seeds.records.count!=2||campaign_passive_vehicle_count!=1||campaign_events.count!=7||
       !r||!selected||r->uid!=26||sizeof(r->physics.state)!=34*4){status=RF_FORMAT;goto done;}
    owner=scene_secondary_vehicle_owner(r);if(!owner||
        rf_object_registry_lookup(&campaign_registry,selected->entry.host.handle)!=&selected->entry.registration||
        rf_entity_lookup(&campaign_entities,(int32_t)selected->entry.host.handle)!=&selected->entry.view){status=RF_FORMAT;goto done;}
    p[4]=r->uid;p[5]=r->handle;p[6]=(uint32_t)campaign_authored_vehicle_uid;p[7]=selected->entry.host.handle;
    p[8]=rf_object_registry_lookup(&campaign_registry,r->handle)==owner;p[9]=owner->object_kind;
    p[10]=!memcmp(owner->pose.position,r->physics.state.position,12)&&!memcmp(owner->pose.input_matrix,r->physics.state.orientation,36);
    p[12]=r->retired;
    p[13]=owner->damage.object_flags;p[14]=owner->pose.flags;p[15]=owner->damage.destroyed;
    scene_secondary_vehicle_checkpoint_passive(r,&saved);
    scene_secondary_vehicle_checkpoint_encode((unsigned char *)(p+16),r,&saved,owner->velocity);
    p[11]=!memcmp(owner->velocity,p+72,12)&&isfinite(owner->velocity[0])&&
        isfinite(owner->velocity[1])&&isfinite(owner->velocity[2]);
    memcpy(p+80,&r->physics.state,34*4);memcpy(p+114,&selected->physics.state,34*4);
    memcpy(p+148,&selected->damage.damage.state.effects.health,4);memcpy(p+149,&selected->damage.damage.state.effects.armor,4);
    p[150]=selected->damage.damage.object_flags;p[151]=selected->entry.view.flags_7c;
    p[152]=selected->damage.damage.destroyed;p[153]=selected->entry.host.alive;p[154]=selected->entry.host.driver;
    p[155]=selected->entry.session.active;p[156]=selected->entry.session.host;p[157]=selected->entry.session.driver;
    p[158]=campaign_player_object.handle;p[159]=(uint32_t)campaign_player_view.linked_handle;
    for(i=0;i<7;i++){
        const rf_runtime_event *e=NULL;uint32_t *v=p+160+i*12;
        for(j=0;j<campaign_events.count;j++)if(campaign_events.items[j].authored->record.uid==events[i]){
            if(e){status=RF_FORMAT;goto done;}e=campaign_events.items+j;}
        if(!e||rf_object_registry_lookup(&campaign_registry,e->handle)!=e){status=RF_FORMAT;goto done;}
        v[0]=events[i];v[1]=e->handle;v[2]=e->state.type;memcpy(v+3,&e->state.delay,4);
        status=scene_secondary_vehicle_fixture_remaining(e->state.deadline,v+4);if(status)goto done;
        v[5]=e->state.actor;v[6]=e->state.source;v[7]=e->state.flags;v[8]=e->state.mode;
        v[9]=UINT32_MAX;
        if(e->state.type==50){status=scene_secondary_vehicle_fixture_remaining(e->unhide.deadline,v+9);if(status)goto done;}
        v[10]=e->unhide.on;v[11]=e->unhide.off;
    }
    memcpy(p+244,rf_scene_secondary_vehicle+1,5*4);p[249]=rf_scene_secondary_vehicle[9];
    p[250]=r->last_frame;p[251]=r->physics.body_flags;p[252]=r->physics.sphere_count;p[253]=selected->physics.sphere_count;
    memcpy(p+254,&owner->damage.state.effects.health,4);memcpy(p+255,&owner->damage.state.effects.armor,4);
    if(frame==60&&rf_scene_secondary_vehicle_fixture[1]==1){
        status=scene_secondary_vehicle_checkpoint_audit(s);rf_scene_secondary_vehicle_fixture[6]=1;
    }
    scene_secondary_vehicle_fixture_collision(s,frame);
done:p[3]=status;if(status)rf_scene_secondary_vehicle_fixture[7]=status;
}
#endif
