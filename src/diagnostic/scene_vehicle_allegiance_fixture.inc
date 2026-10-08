#ifndef RF_SCENE_VEHICLE_ALLEGIANCE_FIXTURE_INCLUDED
#define RF_SCENE_VEHICLE_ALLEGIANCE_FIXTURE_INCLUDED
/* Read-only opt-in observation of original APC26 event/source/save/load.
 * Setup runs ordinary authored event dispatch; no live owner state is edited. */
uint32_t rf_scene_vehicle_allegiance_fixture[8]; /* read,phase,samples,26,9627,enabled,reserved,status */
uint32_t rf_scene_vehicle_allegiance_samples[9][128];
static void scene_vehicle_allegiance_fixture_open(void)
{
    FILE *file;uint32_t config[4]={0};int status=RF_OK,trailing;
    memset(rf_scene_vehicle_allegiance_fixture,0,sizeof(rf_scene_vehicle_allegiance_fixture));
    memset(rf_scene_vehicle_allegiance_samples,0,sizeof(rf_scene_vehicle_allegiance_samples));
#ifdef RF_IMAGE_XBOX_NATIVE
    errno=0;file=fopen("D:\\campaign-vehicle-allegiance.bin","rb");
    if(!file){if(errno!=ENOENT)rf_scene_vehicle_allegiance_fixture[7]=(uint32_t)RF_IO;return;}
#else
    return;
#endif
    rf_scene_vehicle_allegiance_fixture[0]=1;
    if(fread(config,1,sizeof(config),file)!=sizeof(config))status=RF_FORMAT;
    trailing=fgetc(file);if(trailing!=EOF||ferror(file))status=RF_FORMAT;
    if(fclose(file))status=RF_IO;
    if(config[0]!=0x474c4156u||config[1]<1||config[1]>2||config[2]||config[3])status=RF_FORMAT;
    rf_scene_vehicle_allegiance_fixture[1]=config[1];rf_scene_vehicle_allegiance_fixture[3]=26;
    rf_scene_vehicle_allegiance_fixture[4]=9627;rf_scene_vehicle_allegiance_fixture[5]=!status;
    rf_scene_vehicle_allegiance_fixture[7]=(uint32_t)status;
}
static void scene_vehicle_allegiance_fixture_tick(scene_stream *s,uint32_t frame)
{
    static const uint32_t frames[9]={0,20,40,58,60,78,100,118,119};
    static const uint32_t events[5]={9619,9636,917000,917001,917002};
    uint32_t i,j,*p,status=RF_OK;scene_passive_vehicle *passive;
    if(!frame)scene_vehicle_allegiance_fixture_open();
    if(!rf_scene_vehicle_allegiance_fixture[5])return;
    for(i=0;i<9&&frames[i]!=frame;i++){}if(i==9)return;
    p=rf_scene_vehicle_allegiance_samples[i];p[0]=1;p[1]=frame;p[2]=rf_scene_vehicle_allegiance_fixture[1];
    ++rf_scene_vehicle_allegiance_fixture[2];
    if(!s||!s->driller_runtime||campaign_authored_vehicle_uid!=9627||campaign_passive_vehicle_count!=1||
       campaign_passive_vehicles[0].uid!=26||campaign_seeds.records.count!=2||campaign_events.count!=5||
       strcmp(campaign_current_level,"L1S3.rfl")){status=RF_FORMAT;goto done;}
    passive=campaign_passive_vehicles;
    p[4]=passive->uid;p[5]=passive->handle;p[6]=passive->damage.state.effects.affiliation;
    p[7]=rf_object_registry_lookup(&campaign_registry,p[5])==passive;
    memcpy(p+8,&passive->damage.state.effects.health,4);memcpy(p+9,&passive->damage.state.effects.armor,4);
    p[10]=passive->damage.object_flags;p[11]=passive->damage.destroyed;memcpy(p+12,passive->pose.position,12);
    p[15]=passive->attached;p[16]=campaign_authored_vehicle_uid;p[17]=campaign_authored_vehicle_handle;
    p[18]=s->driller_runtime->damage.damage.state.effects.affiliation;
    p[19]=rf_object_registry_lookup(&campaign_registry,p[17])==&s->driller_runtime->entry.registration;
    memcpy(p+20,&s->driller_runtime->damage.damage.state.effects.health,4);
    memcpy(p+21,&s->driller_runtime->damage.damage.state.effects.armor,4);
    p[22]=s->driller_runtime->damage.damage.object_flags;p[23]=s->driller_runtime->entry.session.active;
    memcpy(p+24,s->driller_runtime->physics.state.position,12);p[27]=campaign_player_object.handle;
    p[28]=campaign_secondary_vehicle_count;p[29]=scene_vehicle_allegiance_legacy_save_allowed();
    p[30]=scene_driller_damage_source(p[17],p+31);
    memcpy(p+32,rf_scene_vehicle_allegiance,sizeof(rf_scene_vehicle_allegiance));
    for(i=0;i<5;i++){
        const rf_runtime_event *e=NULL;uint32_t *v=p+40+i*12;
        for(j=0;j<campaign_events.count;j++)if(campaign_events.items[j].authored->record.uid==events[i]){
            if(e){status=RF_FORMAT;goto done;}e=campaign_events.items+j;}
        if(!e){status=RF_FORMAT;goto done;}
        v[0]=events[i];v[1]=e->handle;v[2]=e->state.type;memcpy(v+3,&e->state.delay,4);
        status=scene_secondary_fighter_remaining(e->state.deadline,v+4);if(status)goto done;
        v[5]=e->state.actor;v[6]=e->state.source;v[7]=e->state.flags;v[8]=e->state.mode;
        v[9]=e->authored->record.words[0];v[10]=e->authored->record.link_count;v[11]=e->retired;
    }
    p[100]=(uint32_t)campaign_blackout_now;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.view)++p[101];
done:p[3]=status;if(status)rf_scene_vehicle_allegiance_fixture[7]=status;
}
#endif
