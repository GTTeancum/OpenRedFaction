#ifndef SCENE_PASSIVE_JEEP_DRIVER_FIXTURE_INCLUDED
#define SCENE_PASSIVE_JEEP_DRIVER_FIXTURE_INCLUDED
/* Read-only exact-frame evidence for the bounded living passive Jeep driver.
 * Include after the runtime, switch, wreck exit and collision definitions;
 * tick after the late frame-zero ordinary world load. No state transitions,
 * event dispatches, seat fabrication or checkpoint mutation occur here.
 * campaign-passive-jeep-driver.bin: PJPD, phase(1source/2return/3wreck),0,0.
 * The same ordinary source payload supplies both fresh-load continuations. */
uint32_t rf_scene_passive_jeep_driver_fixture[8]; /* read,phase,samples,A,B,NPC,enabled,status */
uint32_t rf_scene_passive_jeep_driver_samples[14][152];
static void scene_passive_jeep_driver_fixture_open(void)
{
    FILE *file;uint32_t config[4]={0};int status=RF_OK,trailing;
    memset(rf_scene_passive_jeep_driver_fixture,0,sizeof(rf_scene_passive_jeep_driver_fixture));
    memset(rf_scene_passive_jeep_driver_samples,0,sizeof(rf_scene_passive_jeep_driver_samples));
#ifdef RF_IMAGE_XBOX_NATIVE
    errno=0;file=fopen("D:\\campaign-passive-jeep-driver.bin","rb");
    if(!file){if(errno!=ENOENT)rf_scene_passive_jeep_driver_fixture[7]=(uint32_t)RF_IO;return;}
#else
    return;
#endif
    rf_scene_passive_jeep_driver_fixture[0]=1;
    if(fread(config,1,sizeof(config),file)!=sizeof(config))status=RF_FORMAT;
    trailing=fgetc(file);if(trailing!=EOF||ferror(file))status=RF_FORMAT;
    if(fclose(file))status=RF_IO;
    if(config[0]!=0x44504a50u||config[1]<1||config[1]>3||config[2]||config[3])status=RF_FORMAT;
    rf_scene_passive_jeep_driver_fixture[1]=config[1];
    rf_scene_passive_jeep_driver_fixture[3]=7629;rf_scene_passive_jeep_driver_fixture[4]=915200;
    rf_scene_passive_jeep_driver_fixture[5]=7646;rf_scene_passive_jeep_driver_fixture[6]=!status;
    rf_scene_passive_jeep_driver_fixture[7]=(uint32_t)status;
}
/* Host block24 words: selected,kind,exact registry,entity-view present,
 * driver,seat0,seat1,alive,health,armor,flags,destroyed,position3,basis9.
 * Bank block3words: valid,primary reserve,RNG. */
static int scene_passive_jeep_driver_host(scene_stream *s,uint32_t uid,uint32_t *handle,
    uint32_t *row,uint32_t *bank,uint32_t *profile,uint32_t *role)
{
    scene_driller_runtime *r=s->driller_runtime;scene_passive_vehicle *found=NULL;
    if((uint32_t)campaign_authored_vehicle_uid==uid){
        scene_driller_entry_exit *e=&r->entry;*handle=e->host.handle;
        row[0]=1;row[1]=e->registration.object_kind;
        row[2]=rf_object_registry_lookup(&campaign_registry,*handle)==&e->registration;
        row[3]=rf_entity_lookup(&campaign_entities,(int32_t)*handle)==&e->view;
        row[4]=e->host.driver;row[5]=(uint32_t)e->occupant;row[6]=(uint32_t)e->gunner_occupant;
        row[7]=e->host.alive;memcpy(row+8,&r->damage.damage.state.effects.health,4);
        memcpy(row+9,&r->damage.damage.state.effects.armor,4);row[10]=r->damage.damage.object_flags;
        row[11]=r->damage.damage.destroyed;memcpy(row+12,r->physics.state.position,12);
        memcpy(row+15,r->physics.state.orientation,36);
        bank[0]=1;bank[1]=(uint32_t)s->apc_primary.reserve;bank[2]=s->apc_primary.random.value;
        *profile=rf_scene_vehicle_enabled;*role=r->jeep_seat.role;return RF_OK;
    }
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(p->uid!=uid)continue;if(found)return RF_FORMAT;found=p;
        *handle=p->handle;row[1]=p->object_kind;
        row[2]=rf_object_registry_lookup(&campaign_registry,*handle)==p;
        row[3]=rf_entity_lookup(&campaign_entities,(int32_t)*handle)!=NULL;
        row[4]=p->seat_driver;row[5]=(uint32_t)p->seat_occupants[0];row[6]=(uint32_t)p->seat_occupants[1];
        row[7]=!p->damage.destroyed&&p->damage.state.effects.health>0;
        memcpy(row+8,&p->damage.state.effects.health,4);memcpy(row+9,&p->damage.state.effects.armor,4);
        row[10]=p->damage.object_flags;row[11]=p->damage.destroyed;
        memcpy(row+12,p->pose.position,12);memcpy(row+15,p->pose.input_matrix,36);
        if(scene_vehicle_parked&&i<scene_vehicle_parked_count){
            const scene_vehicle_parked_state *v=scene_vehicle_parked+i;
            if(v->valid){if(v->uid!=uid||v->handle!=p->handle)return RF_FORMAT;
                bank[0]=v->valid;bank[1]=(uint32_t)v->primary_reserve;bank[2]=v->random.value;
                *profile=v->profile;*role=v->jeep_role;}
        }
    }
    return found?RF_OK:RF_NOT_FOUND;
}
/* Independent actual-sphere surface separation for the released NPC and
 * each Jeep, including the dead host. Negative values mean overlap. */
static int scene_passive_jeep_driver_gap(scene_stream *s,const campaign_npc_body *actor,
    uint32_t uid,float *gap,uint32_t *count)
{
    const float *position=NULL,*basis=NULL;rf_physics_sphere copy[8]={{0}};
    const rf_physics_sphere *spheres=NULL;uint32_t n=0;double nearest=DBL_MAX;
    if((uint32_t)campaign_authored_vehicle_uid==uid){scene_driller_physics *p=&s->driller_runtime->physics;
        position=p->state.position;basis=p->state.orientation;spheres=p->spheres;n=p->sphere_count;
    }else for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(p->uid!=uid)continue;if(position||p->resource_kind>=6)return RF_FORMAT;
        position=p->pose.position;basis=p->pose.input_matrix;n=s->passive_vehicle_sphere_count[p->resource_kind];
        if(n>8)return RF_RANGE;
        for(uint32_t j=0;j<n;j++){memcpy(copy[j].center,s->passive_vehicle_spheres[p->resource_kind][j].center,12);
            copy[j].radius=s->passive_vehicle_spheres[p->resource_kind][j].radius;}
        spheres=copy;
    }
    if(!position||!spheres||!n||n>8||!actor->body.spheres.items||!actor->body.spheres.count)return RF_NOT_FOUND;
    for(uint32_t i=0;i<actor->body.spheres.count;i++)for(uint32_t j=0;j<n;j++){
        float a[3],b[3];double d2=0,d;
        scene_driller_point(actor->body.state.position,actor->body.state.orientation,actor->body.spheres.items[i].center,a);
        scene_driller_point(position,basis,spheres[j].center,b);
        for(uint32_t k=0;k<3;k++){d=(double)a[k]-b[k];d2+=d*d;}
        d=sqrt(d2)-actor->body.spheres.items[i].radius-spheres[j].radius;if(d<nearest)nearest=d;
    }
    *gap=(float)nearest;*count=n;return RF_OK;
}
static void scene_passive_jeep_driver_fixture_tick(scene_stream *s,uint32_t frame)
{
    static const uint32_t frames[14]={0,40,80,100,118,138,190,230,248,278,310,318,348,349};
    scene_npc_seat_binding *b=NULL;campaign_npc_body *o=NULL;scene_npc_seat_host host;
    scene_driller_runtime *r;uint32_t *p,i,status=RF_OK;float tag[12],gap;
    if(!frame)scene_passive_jeep_driver_fixture_open();
    if(!rf_scene_passive_jeep_driver_fixture[6])return;
    for(i=0;i<14&&frames[i]!=frame;i++){}if(i==14)return;
    p=rf_scene_passive_jeep_driver_samples[i];p[0]=1;p[1]=frame;p[2]=rf_scene_passive_jeep_driver_fixture[1];
    ++rf_scene_passive_jeep_driver_fixture[2];
    if(!s||!(r=s->driller_runtime)||!campaign_spawn||strcmp(campaign_current_level,"ctf06.rfl")||
       campaign_seeds.records.count!=3||campaign_passive_vehicle_count!=1||scene_npc_seat_count!=1){status=RF_FORMAT;goto done;}
    b=scene_npc_seats;o=scene_npc_seat_actor(b);
    if(!o||b->host_uid!=7629||b->slot>=campaign_seeds.records.count||
       campaign_seeds.records.items[b->slot].record.uid!=7646){status=RF_FORMAT;goto done;}
    p[4]=(uint32_t)campaign_authored_vehicle_uid;p[5]=r->entry.host.handle;p[6]=campaign_player_object.handle;
    p[7]=b->actor;p[10]=rf_object_registry_lookup(&campaign_registry,b->actor)==&o->registration;
    p[11]=rf_entity_lookup(&campaign_entities,(int32_t)b->actor)==&o->view;
    p[12]=b->active;p[13]=b->kind;p[14]=(uint32_t)b->tag;p[15]=(uint32_t)o->view.linked_handle;
    p[16]=(uint32_t)o->ai_mode.action_280;p[17]=(uint32_t)o->view.action_520;
    memcpy(p+18,&o->damage.effects.health,4);memcpy(p+19,&o->damage.effects.armor,4);
    status=scene_passive_jeep_driver_host(s,7629,p+8,p+20,p+109,p+138,p+139);if(status)goto done;
    status=scene_passive_jeep_driver_host(s,915200,p+9,p+44,p+112,p+140,p+141);if(status)goto done;
    memcpy(p+68,o->body.state.position,12);memcpy(p+71,o->body.state.orientation,36);
    status=scene_npc_seat_host_read(b,&host);p[130]=status;if(status)goto done;
    status=rf_static_model_tag_place(host.tags,b->tag,host.basis,host.position,tag);p[130]=status;if(status)goto done;
    memcpy(p+80,tag+9,12);memcpy(p+83,tag,36);memcpy(p+92,o->published,12);memcpy(p+95,o->body.state.velocity,12);
    p[98]=o->support.handle;p[99]=(uint32_t)b->occupant;p[100]=r->entry.session.active;
    p[101]=r->entry.session.host;p[102]=r->entry.session.driver;p[103]=r->jeep_seat.role;
    p[104]=(uint32_t)campaign_player_view.linked_handle;p[105]=r->entry.player.control;
    memcpy(p+106,scene_actor_body.state.position,12);p[142]=b->last_frame;
    p[131]=!memcmp(p+68,p+80,48);p[132]=!memcmp(o->published,o->body.state.position,12);
    p[133]=o->body.spheres.count;p[134]=scene_actor_body.spheres.count;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *event=campaign_events.items+i;uint32_t uid=event->authored->record.uid;
        if(uid==916600){int32_t remaining=-1;p[115]=uid;p[116]=event->handle;p[117]=event->state.type;p[118]=(uint32_t)event->state.deadline;
            if(event->state.deadline>=0){status=rf_timer_remaining(event->state.deadline,campaign_blackout_now,&remaining);if(status)goto done;}
            p[119]=(uint32_t)remaining;}
        if(uid==916601){p[120]=uid;p[121]=event->handle;p[122]=event->state.type;p[123]=(uint32_t)event->state.deadline;}
    }
    p[124]=rf_scene_script_slays[0];p[125]=rf_scene_script_slays[2];
    memcpy(p+126,rf_scene_vehicle_wreck_exit,16);
    if(!b->active&&p[31]&&o->damage.effects.health>0){
        scene_vehicle_pose pose;rf_physics_body_state before=o->body.state;scene_npc_seat_binding binding=*b;
        memcpy(pose.position,o->body.state.position,12);memcpy(pose.basis,o->body.state.orientation,36);
        p[135]=1;p[136]=(uint32_t)scene_vehicle_wreck_destination(s,o,&pose);p[137]=!p[136];
        status=scene_passive_jeep_driver_gap(s,o,7629,&gap,p+147);if(status)goto done;memcpy(p+144,&gap,4);
        status=scene_passive_jeep_driver_gap(s,o,915200,&gap,p+148);if(status)goto done;memcpy(p+145,&gap,4);
        p[149]=(!memcmp(&before,&o->body.state,sizeof(before))?1u:0u)|(!memcmp(&binding,b,sizeof(binding))?2u:0u);
    }else if(!r->entry.session.active&&(frame==230||frame==80)){
        scene_vehicle_pose pose;rf_physics_body_state before=scene_actor_body.state;
        rf_checkpoint_placement placement={0};scene_driller_physics *physical=&r->physics;
        memcpy(pose.position,before.position,12);memcpy(pose.basis,before.orientation,36);
        p[135]=2;p[136]=(uint32_t)scene_vehicle_entry_exit_clear(s,&pose,p+137);
        placement.spheres=scene_actor_body.spheres.items;placement.count=scene_actor_body.spheres.count;
        memcpy(placement.position,pose.position,12);memcpy(placement.basis,pose.basis,36);
        if(!p[136]&&p[137])p[136]=(uint32_t)scene_vehicle_wreck_pair(&placement,physical->spheres,physical->sphere_count,
            physical->state.position,physical->state.orientation);
        if(p[136])p[137]=0;p[149]=!memcmp(&before,&scene_actor_body.state,sizeof(before));
    }
done:p[3]=status;if(status)rf_scene_passive_jeep_driver_fixture[7]=status;
}
#endif
