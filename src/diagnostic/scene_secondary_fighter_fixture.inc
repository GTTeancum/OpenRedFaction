#ifndef RF_SCENE_SECONDARY_FIGHTER_FIXTURE_INCLUDED
#define RF_SCENE_SECONDARY_FIGHTER_FIXTURE_INCLUDED
/* Opt-in, read-only bounded two-Fighter evidence. Original UnHide/Goto and
 * ordinary Delay/Turn_Off_Physics/Invert do all scene mutation. Include after
 * secondary checkpoint support; tick after frame0 world-load publication.
 * SFTR config: magic,phase(1source/2fresh),0,0. No pointers in observations. */
uint32_t rf_scene_secondary_fighter_fixture[8]; /* read,phase,samples,A,B,enabled,audit,status */
uint32_t rf_scene_secondary_fighter_samples[9][384];
uint32_t rf_scene_secondary_fighter_clocks[9];
#include "scene_vehicle_static_edges_checks.inc"
uint32_t rf_scene_secondary_fighter_edge_audit[16];
/* Isolated dispatcher regression: all objects, registrations and callbacks are
 * private heap state. No scene event, owner, timer or registry is mutated. */
uint32_t rf_scene_secondary_fighter_event_audit[24];
typedef struct scene_secondary_fighter_event_check {
    rf_object_registry registry;
    rf_runtime_triggers triggers;rf_runtime_events events;
    rf_runtime_event items[3];rf_level_owned_event authored[3];
    rf_level_link_target links[3][2];uint32_t uids[3][2];
    rf_runtime_trigger visibility_target;
    struct {uint32_t kind,handle;} move_target;
    rf_physics_gravity gravity;rf_startup_events_report report;
    uint32_t pending,moves,move_on,move_off,visible,visible_on,visible_off,misses;
    uint32_t move_log[3][3];
} scene_secondary_fighter_event_check;
static int scene_secondary_fighter_event_move(void *context,uint32_t handle,const rf_level_event *event,uint32_t on)
{
    scene_secondary_fighter_event_check *c=context;uint32_t *log;
    if(!c||handle!=c->move_target.handle||event!=&c->authored[1].record||on>1||c->moves>=3)return RF_FORMAT;
    log=c->move_log[c->moves++];log[0]=on;log[1]=c->items[1].state.source;log[2]=c->items[1].state.actor;
    if(on)++c->move_on;else ++c->move_off;return RF_OK;
}
static int scene_secondary_fighter_event_visible(void *context,uint32_t handle,uint32_t visible)
{
    scene_secondary_fighter_event_check *c=context;
    if(!c||visible>1)return RF_FORMAT;
    if(handle==c->items[1].handle){++c->misses;return RF_NOT_FOUND;}
    if(handle!=c->visibility_target.handle)return RF_FORMAT;
    ++c->visible;if(visible)++c->visible_on;else ++c->visible_off;return RF_OK;
}
/* runs,frame,checks,passed,failed,status,cycle status/depth,moves/ON/OFF,
 * visibility/ON/OFF,event visibility misses,non-event flags,ON source/actor,
 * OFF source/actor,delayed source/actor,heap bytes,private-registry guard. */
static int scene_secondary_fighter_event_audit(uint32_t frame)
{
    scene_secondary_fighter_event_check *c=calloc(1,sizeof(*c));
    uint32_t *p=rf_scene_secondary_fighter_event_audit,i;int status=RF_OK,cycle;
    memset(p,0,sizeof(rf_scene_secondary_fighter_event_audit));p[0]=1;p[1]=frame;p[22]=sizeof(*c);
    if(!c){p[5]=(uint32_t)RF_IO;return RF_IO;}
#define SF_EVENT_CALL(call) do{status=(call);if(status)goto done;}while(0)
#define SF_EVENT_CHECK(test) do{uint32_t bit=1u<<p[2]++;if(test)p[3]|=bit;else{p[4]|=bit;status=RF_FORMAT;goto done;}}while(0)
    rf_object_registry_init(&c->registry);c->events.registry=c->triggers.registry=&c->registry;
    c->events.items=c->items;c->events.count=3;
    for(i=0;i<3;i++){
        c->items[i].object_kind=6;c->items[i].authored=c->authored+i;c->items[i].links=c->links[i];
        c->items[i].state.deadline=-1;c->authored[i].links=c->uids[i];c->authored[i].record.uid=916890+i;
        SF_EVENT_CALL(rf_object_registry_insert(&c->registry,c->items+i,&c->items[i].handle));
    }
    c->items[0].state.type=50;strcpy(c->authored[0].record.type,"UnHide");
    c->items[1].state.type=5;strcpy(c->authored[1].record.type,"Goto");
    c->items[2].state.type=3;strcpy(c->authored[2].record.type,"Invert");
    c->visibility_target.object_kind=5;c->visibility_target.state.flags=16;
    c->move_target.kind=11;
    SF_EVENT_CALL(rf_object_registry_insert(&c->registry,&c->visibility_target,&c->visibility_target.handle));
    SF_EVENT_CALL(rf_object_registry_insert(&c->registry,&c->move_target,&c->move_target.handle));
    c->authored[0].record.link_count=2;c->uids[0][0]=916893;c->uids[0][1]=916891;
    c->links[0][0].kind=c->links[0][1].kind=1;
    c->links[0][0].value=c->visibility_target.handle;c->links[0][1].value=c->items[1].handle;
    c->authored[1].record.link_count=1;c->uids[1][0]=916894;
    c->links[1][0].kind=1;c->links[1][0].value=c->move_target.handle;
    c->authored[2].record.link_count=1;c->uids[2][0]=916890;
    c->links[2][0].kind=1;c->links[2][0].value=c->items[0].handle;
    c->triggers.move_npc=scene_secondary_fighter_event_move;c->triggers.move_context=c;
    c->triggers.set_visible=scene_secondary_fighter_event_visible;c->triggers.visibility_context=c;
    p[23]=c->events.registry==c->triggers.registry&&c->events.registry!=&campaign_registry;
    SF_EVENT_CALL(rf_unhide_init(&c->items[0].unhide,0));
    SF_EVENT_CALL(rf_runtime_event_fire(&c->triggers,c->items[0].handle,7,8,100,&c->gravity,NULL,NULL,&c->report));
    SF_EVENT_CHECK(c->moves==1&&c->move_on==1&&!c->visible&&c->items[0].unhide.on==1&&c->report.events==2);
    SF_EVENT_CHECK(c->move_log[0][1]==7&&c->move_log[0][2]==8);
    SF_EVENT_CHECK(c->visibility_target.state.flags==16&&c->report.other_targets==1&&p[23]);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,100,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==1&&c->visible==1&&c->visible_on==1&&c->misses==1&&
        !c->items[0].unhide.on&&c->items[0].unhide.deadline==600&&!c->pending);
    SF_EVENT_CALL(rf_runtime_event_fire(&c->triggers,c->items[2].handle,9,10,200,&c->gravity,NULL,NULL,&c->report));
    SF_EVENT_CHECK(c->moves==2&&c->move_off==1&&c->visible==1&&c->items[0].unhide.off==1&&c->report.events==3);
    SF_EVENT_CHECK(c->move_log[1][1]==UINT32_MAX&&c->move_log[1][2]==UINT32_MAX&&
        c->items[0].state.source==UINT32_MAX&&c->items[0].state.actor==UINT32_MAX);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,599,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==2&&c->visible==1&&c->items[0].unhide.off==1);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,600,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==2&&c->visible==2&&c->visible_off==1&&!c->items[0].unhide.off&&
        c->items[0].unhide.deadline==1100&&c->misses==2);
    c->items[0].state.delay=.25f;
    SF_EVENT_CALL(rf_runtime_event_fire(&c->triggers,c->items[0].handle,17,18,700,&c->gravity,NULL,NULL,&c->report));
    SF_EVENT_CHECK(c->moves==2&&c->visible==2&&c->items[0].state.deadline==950&&!c->items[0].unhide.on);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,949,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==2&&c->visible==2&&c->items[0].state.deadline==950);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,950,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==3&&c->move_on==2&&c->visible==2&&c->items[0].state.deadline==-1&&
        c->items[0].unhide.on==1&&c->move_log[2][1]==17&&c->move_log[2][2]==18);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,1099,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==3&&c->visible==2&&c->items[0].unhide.on==1);
    SF_EVENT_CALL(rf_runtime_events_tick(&c->events,&c->triggers,&c->gravity,1100,NULL,NULL,&c->report,&c->pending));
    SF_EVENT_CHECK(c->moves==3&&c->visible==3&&c->visible_on==2&&!c->items[0].unhide.on&&c->misses==3);
    SF_EVENT_CHECK(c->visibility_target.state.flags==16&&c->move_target.kind==11&&!c->pending);
    c->items[0].state.delay=0;c->authored[0].record.link_count=1;
    c->uids[0][0]=c->authored[0].record.uid;c->links[0][0].value=c->items[0].handle;
    cycle=rf_runtime_event_fire(&c->triggers,c->items[0].handle,21,22,1200,&c->gravity,NULL,NULL,&c->report);
    p[6]=(uint32_t)cycle;p[7]=c->report.events;
    SF_EVENT_CHECK(cycle==RF_RANGE&&c->report.events==64&&c->moves==3&&c->visible==3);
done:
    p[5]=(uint32_t)status;p[8]=c->moves;p[9]=c->move_on;p[10]=c->move_off;
    p[11]=c->visible;p[12]=c->visible_on;p[13]=c->visible_off;p[14]=c->misses;
    p[15]=c->visibility_target.state.flags;
    for(i=0;i<3;i++){p[16+i*2]=c->move_log[i][1];p[17+i*2]=c->move_log[i][2];}
    free(c);return status;
#undef SF_EVENT_CALL
#undef SF_EVENT_CHECK
}
/* Per-frame actual-hull separation, including the frame0 late restore and
 * final presentation-only frame. Summary stores the global minimum's complete
 * owner poses and actual collision sphere records for independent decoding. */
uint32_t rf_scene_secondary_fighter_gap_frames[120]; /* float minimum gap/frame */
uint32_t rf_scene_secondary_fighter_gap[64];
/* Every actual late-load/world pose: A position3+basis9, then B position3+basis9. */
uint32_t rf_scene_secondary_fighter_pose_history[120][24];
/* count,last,status,minframe,mingap,Ahandle,Bhandle,Aindex,Bindex,Acount,Bcount,
 * nonpositive count,first nonpositive; padding3; Apose12,Bpose12,Asphere4,
 * Bsphere4,Aworld3,Bworld3,Auid,Buid; padding8. */
static int scene_secondary_fighter_gap_tick(uint32_t frame)
{
    const scene_secondary_vehicle_runtime *a=scene_secondary_vehicle_find_uid(4801);
    const scene_secondary_vehicle_runtime *b=scene_secondary_vehicle_find_uid(18353);
    uint32_t *p=rf_scene_secondary_fighter_gap,i,j,ia=0,ib=0;float minimum=0,old,ca[3],cb[3],wa[3]={0},wb[3]={0};
    if(frame>=120||!a||!b||!a->physics.sphere_count||!b->physics.sphere_count||a==b||a->handle==b->handle){
        p[2]=(uint32_t)RF_FORMAT;return RF_FORMAT;}
    memcpy(rf_scene_secondary_fighter_pose_history[frame],a->physics.state.position,12);
    memcpy(rf_scene_secondary_fighter_pose_history[frame]+3,a->physics.state.orientation,36);
    memcpy(rf_scene_secondary_fighter_pose_history[frame]+12,b->physics.state.position,12);
    memcpy(rf_scene_secondary_fighter_pose_history[frame]+15,b->physics.state.orientation,36);
    for(i=0;i<a->physics.sphere_count;i++)for(j=0;j<b->physics.sphere_count;j++){
        double distance=0;float gap;
        scene_driller_point(a->physics.state.position,a->physics.state.orientation,a->physics.collision[i].center,ca);
        scene_driller_point(b->physics.state.position,b->physics.state.orientation,b->physics.collision[j].center,cb);
        for(uint32_t k=0;k<3;k++){double delta=(double)ca[k]-cb[k];distance+=delta*delta;}
        gap=(float)(sqrt(distance)-a->physics.collision[i].radius-b->physics.collision[j].radius);
        if(!isfinite(gap)){p[2]=(uint32_t)RF_FORMAT;return RF_FORMAT;}
        if((!i&&!j)||gap<minimum){minimum=gap;ia=i;ib=j;memcpy(wa,ca,12);memcpy(wb,cb,12);}
    }
    memcpy(rf_scene_secondary_fighter_gap_frames+frame,&minimum,4);memcpy(&old,p+4,4);
    if(!p[0]||minimum<old){
        p[3]=frame;memcpy(p+4,&minimum,4);p[5]=a->handle;p[6]=b->handle;p[7]=ia;p[8]=ib;
        p[9]=a->physics.sphere_count;p[10]=b->physics.sphere_count;
        memcpy(p+16,a->physics.state.position,12);memcpy(p+19,a->physics.state.orientation,36);
        memcpy(p+28,b->physics.state.position,12);memcpy(p+31,b->physics.state.orientation,36);
        memcpy(p+40,a->physics.collision[ia].center,12);memcpy(p+43,&a->physics.collision[ia].radius,4);
        memcpy(p+44,b->physics.collision[ib].center,12);memcpy(p+47,&b->physics.collision[ib].radius,4);
        memcpy(p+48,wa,12);memcpy(p+51,wb,12);p[54]=a->uid;p[55]=b->uid;
    }
    if(minimum<=0){if(!p[11])p[12]=frame;++p[11];}
    ++p[0];p[1]=frame;return RF_OK;
}
static void scene_secondary_fighter_fixture_open(void)
{
    FILE *file;uint32_t config[4]={0};int status=RF_OK,trailing;
    memset(rf_scene_secondary_fighter_fixture,0,sizeof(rf_scene_secondary_fighter_fixture));
    memset(rf_scene_secondary_fighter_samples,0,sizeof(rf_scene_secondary_fighter_samples));
    memset(rf_scene_secondary_fighter_clocks,0,sizeof(rf_scene_secondary_fighter_clocks));
    memset(rf_scene_secondary_fighter_event_audit,0,sizeof(rf_scene_secondary_fighter_event_audit));
    memset(rf_scene_secondary_fighter_edge_audit,0,sizeof(rf_scene_secondary_fighter_edge_audit));
    memset(rf_scene_secondary_fighter_gap_frames,0,sizeof(rf_scene_secondary_fighter_gap_frames));
    memset(rf_scene_secondary_fighter_gap,0,sizeof(rf_scene_secondary_fighter_gap));
    memset(rf_scene_secondary_fighter_pose_history,0,sizeof(rf_scene_secondary_fighter_pose_history));
    rf_scene_secondary_fighter_gap[12]=UINT32_MAX;
#ifdef RF_IMAGE_XBOX_NATIVE
    errno=0;file=fopen("D:\\campaign-secondary-fighters.bin","rb");
    if(!file){if(errno!=ENOENT)rf_scene_secondary_fighter_fixture[7]=(uint32_t)RF_IO;return;}
#else
    return;
#endif
    rf_scene_secondary_fighter_fixture[0]=1;
    if(fread(config,1,sizeof(config),file)!=sizeof(config))status=RF_FORMAT;
    trailing=fgetc(file);if(trailing!=EOF||ferror(file))status=RF_FORMAT;
    if(fclose(file))status=RF_IO;
    if(config[0]!=0x52544653u||config[1]<1||config[1]>2||config[2]||config[3])status=RF_FORMAT;
    rf_scene_secondary_fighter_fixture[1]=config[1];rf_scene_secondary_fighter_fixture[3]=4801;
    rf_scene_secondary_fighter_fixture[4]=18353;rf_scene_secondary_fighter_fixture[5]=!status;
    rf_scene_secondary_fighter_fixture[7]=(uint32_t)status;
}
static int scene_secondary_fighter_remaining(int32_t deadline,uint32_t *out)
{
    int32_t value=-1;int status=RF_OK;
    if(deadline>=0)status=rf_timer_remaining(deadline,campaign_blackout_now,&value);
    if(!status)*out=(uint32_t)value;return status;
}
/* Header16; two(owner16 + wire64 + rigid34 + publication14); seven events12;
 * ordinary physics telemetry16; owner-clearance telemetry6; padding6. */
static void scene_secondary_fighter_fixture_tick(scene_stream *s,uint32_t frame)
{
    static const uint32_t frames[9]={0,20,40,58,60,78,100,118,119};
    static const uint32_t owners[2]={4801,18353};
    static const uint32_t events[7]={18456,18457,916800,916801,916802,916803,916804};
    uint32_t i,j,*p,status=RF_OK;
    if(!frame)scene_secondary_fighter_fixture_open();
    if(!rf_scene_secondary_fighter_fixture[5])return;
    status=scene_secondary_fighter_gap_tick(frame);
    if(status){rf_scene_secondary_fighter_fixture[7]=(uint32_t)status;return;}
    for(i=0;i<9&&frames[i]!=frame;i++){}if(i==9)return;
    rf_scene_secondary_fighter_clocks[i]=(uint32_t)campaign_blackout_now;
    p=rf_scene_secondary_fighter_samples[i];p[0]=1;p[1]=frame;p[2]=rf_scene_secondary_fighter_fixture[1];
    ++rf_scene_secondary_fighter_fixture[2];
    p[4]=campaign_secondary_vehicle_count;p[5]=campaign_secondary_vehicle_capacity;
    p[6]=campaign_passive_vehicle_count;p[7]=(uint32_t)campaign_authored_vehicle_uid;
    p[8]=campaign_authored_vehicle_handle;p[9]=s&&s->driller_runtime!=NULL;
    p[10]=campaign_seeds.records.count;p[11]=campaign_events.count;
    p[12]=campaign_player_object.handle;p[13]=(uint32_t)campaign_player_view.linked_handle;
    p[14]=rf_scene_secondary_vehicle[1];
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.view)++p[15];
    if(!s||!campaign_spawn||strcmp(campaign_current_level,"L20S2.rfl")||p[4]!=2||p[5]!=2||
       p[6]!=2||p[7]||p[9]||p[10]!=2||p[11]!=7||p[15]||sizeof(rf_vehicle_rigid_state)!=34*4){status=RF_FORMAT;goto done;}
    for(i=0;i<2;i++){
        scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find_uid(owners[i]);
        scene_passive_vehicle *owner;scene_passive_vehicle_saved saved;uint32_t *v=p+16+i*128;
        if(!r||(owner=scene_secondary_vehicle_owner(r))==NULL){status=RF_NOT_FOUND;goto done;}
        v[0]=r->uid;v[1]=r->handle;v[2]=r->slot;v[3]=r->profile;v[4]=owner->object_kind;
        v[5]=rf_object_registry_lookup(&campaign_registry,r->handle)==owner;
        v[6]=!memcmp(owner->pose.position,r->physics.state.position,12)&&!memcmp(owner->pose.input_matrix,r->physics.state.orientation,36);
        v[8]=r->steps;v[9]=r->last_frame;v[10]=r->scripted_physics;v[11]=r->physics.body_flags;
        v[12]=r->physics.sphere_count;v[13]=owner->attached;v[14]=owner->group_owned;v[15]=owner->seat_driver;
        scene_secondary_vehicle_checkpoint_passive(r,&saved);
        scene_secondary_vehicle_checkpoint_encode((unsigned char *)(v+16),r,&saved,owner->velocity);
        v[7]=!memcmp(owner->velocity,v+72,12);
        memcpy(v+80,&r->physics.state,34*4);
        memcpy(v+114,&owner->damage.state.effects.health,4);memcpy(v+115,&owner->damage.state.effects.armor,4);
        v[116]=owner->damage.object_flags;v[117]=owner->pose.flags;v[118]=owner->damage.destroyed;
        v[119]=scene_secondary_vehicle_has_rider(r->handle);v[120]=(uint32_t)owner->seat_occupants[0];
        v[121]=(uint32_t)owner->seat_occupants[1];v[122]=r->collision.world==s->collision;
        v[123]=r->query.stream==s&&r->query.handle==r->handle&&r->collision.query_context==&r->query;
        v[124]=owner->scripted_physics;v[125]=owner->physics_body_flags;
        v[126]=r->collision.spheres==r->physics.collision&&r->collision.count==r->physics.sphere_count;
        v[127]=r!=scene_secondary_vehicle_find_uid(owners[1-i]);
    }
    for(i=0;i<7;i++){
        const rf_runtime_event *e=NULL;uint32_t *v=p+272+i*12;
        for(j=0;j<campaign_events.count;j++)if(campaign_events.items[j].authored->record.uid==events[i]){
            if(e){status=RF_FORMAT;goto done;}e=campaign_events.items+j;}
        if(!e||rf_object_registry_lookup(&campaign_registry,e->handle)!=e){status=RF_FORMAT;goto done;}
        v[0]=events[i];v[1]=e->handle;v[2]=e->state.type;memcpy(v+3,&e->state.delay,4);
        status=scene_secondary_fighter_remaining(e->state.deadline,v+4);if(status)goto done;
        v[5]=e->state.actor;v[6]=e->state.source;v[7]=e->state.flags;v[8]=e->state.mode;v[9]=UINT32_MAX;
        if(e->state.type==50){status=scene_secondary_fighter_remaining(e->unhide.deadline,v+9);if(status)goto done;}
        v[10]=e->unhide.on;v[11]=e->unhide.off;
    }
    memcpy(p+356,rf_scene_vehicle_physics,16*4);memcpy(p+372,rf_scene_secondary_vehicle_clearance,6*4);
    if(frame==40&&rf_scene_secondary_fighter_fixture[1]==1){
        status=scene_secondary_vehicle_checkpoint_audit(s);rf_scene_secondary_fighter_fixture[6]=1;
    }
    if(frame==100&&rf_scene_secondary_fighter_fixture[1]==1){
        status=scene_secondary_fighter_event_audit(frame);
        if(!status)status=scene_vehicle_static_edges_checks(rf_scene_secondary_fighter_edge_audit);
    }
done:p[3]=status;if(status)rf_scene_secondary_fighter_fixture[7]=status;
}
#endif
