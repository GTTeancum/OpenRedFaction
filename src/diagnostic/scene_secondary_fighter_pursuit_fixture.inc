#ifndef RF_SCENE_SECONDARY_FIGHTER_PURSUIT_FIXTURE_INCLUDED
#define RF_SCENE_SECONDARY_FIGHTER_PURSUIT_FIXTURE_INCLUDED
#include "scene_secondary_vehicle_order_checks.inc"
/* Opt-in single original L20S2 UnHide18157 -> Goto_Player18156 -> Fighter18155.
 * An ordinary Delay begins the original chain; a retained Delay/Invert sends
 * OFF after fresh load. Only process-contained RFI6 inputs move the player.
 * This observer writes telemetry and temporary audit copies, never live state.
 * SFPL config: magic,phase(1source/2fresh),0,0. No pointer observations. */
uint32_t rf_scene_secondary_fighter_pursuit_fixture[8]; /* read,phase,samples,UID,event,enabled,audited,status */
uint32_t rf_scene_secondary_fighter_pursuit_samples[9][224];
uint32_t rf_scene_secondary_fighter_pursuit_history[120][48]; /* owner pose12,player3,last actual command32,steps */
uint32_t rf_scene_secondary_fighter_pursuit_clocks[9];
uint32_t rf_scene_secondary_fighter_pursuit_order_audit[16];
uint32_t rf_scene_secondary_fighter_pursuit_gate_audit[16]; /* attempted,passed,failed mask,line */
static int scene_secondary_fighter_pursuit_gates(scene_stream *s,const scene_secondary_vehicle_runtime *r,
    const scene_passive_vehicle *owner)
{
    scene_secondary_vehicle_runtime copy=*r;scene_passive_vehicle p=*owner;
    scene_secondary_vehicle_checkpoint_stage *stage=NULL;const unsigned char *inner;
    unsigned char wire[1024];uint32_t bytes,nested,inner_bytes,*out=rf_scene_secondary_fighter_pursuit_gate_audit;
    rf_vehicle_submersible_command command={0};int status;
    memset(out,0,16*sizeof(*out));
#define SF_PURSUIT_GATE(bit,condition) do{++out[0];if(condition)++out[1];else{out[2]|=1u<<(bit);if(!out[3])out[3]=__LINE__;}}while(0)
    SF_PURSUIT_GATE(0,!scene_secondary_vehicle_paused(&copy,&p));
    copy.retired=1;SF_PURSUIT_GATE(1,scene_secondary_vehicle_paused(&copy,&p));copy=*r;
    copy.scripted_physics=1;copy.physics.body_flags=0x18000000u;
    SF_PURSUIT_GATE(2,scene_secondary_vehicle_paused(&copy,&p));copy=*r;
    p.damage.object_flags|=0x4000u;SF_PURSUIT_GATE(3,scene_secondary_vehicle_paused(&copy,&p));p=*owner;
    p.damage.state.effects.health=0;SF_PURSUIT_GATE(4,scene_secondary_vehicle_paused(&copy,&p));p=*owner;
    p.seat_driver=123;SF_PURSUIT_GATE(5,scene_secondary_vehicle_paused(&copy,&p));p=*owner;
    p.attached=1;SF_PURSUIT_GATE(6,scene_secondary_vehicle_paused(&copy,&p));p=*owner;
    copy.route.active=0;status=scene_secondary_vehicle_fighter_command(&copy,&command);
    SF_PURSUIT_GATE(7,!status&&!copy.route.active&&copy.route.kind==SCENE_SECONDARY_ORDER_PLAYER&&
        command.controlled&&!command.throttle&&!command.yaw&&!command.rise);copy=*r;
    status=scene_passive_vehicle_checkpoint_capture(wire,sizeof(wire),0,&nested);
    if(!status)status=scene_secondary_vehicle_checkpoint_capture(s,wire,sizeof(wire),nested,&bytes);
    if(!status)status=scene_secondary_vehicle_checkpoint_prepare(s,wire,bytes,65536,&stage,&inner,&inner_bytes);
    SF_PURSUIT_GATE(8,!status&&stage&&stage->count==1&&stage->version==2&&
        stage->entries[0].runtime->route.kind==SCENE_SECONDARY_ORDER_PLAYER&&
        !memcmp(stage->entries[0].runtime->route.target,r->route.target,12));
    if(!status&&stage){
        scene_secondary_vehicle_checkpoint_entry *e=stage->entries;
        e->runtime->route.kind=SCENE_SECONDARY_ORDER_POINT;
        status=scene_secondary_vehicle_checkpoint_valid(e->runtime,&e->passive,2);
        SF_PURSUIT_GATE(9,status==RF_FORMAT);
    }else SF_PURSUIT_GATE(9,0);
    scene_secondary_vehicle_checkpoint_close(&stage);
#undef SF_PURSUIT_GATE
    return out[2]?RF_FORMAT:RF_OK;
}
static void scene_secondary_fighter_pursuit_open(void)
{
    FILE *file;uint32_t config[4]={0};int status=RF_OK,trailing;
    memset(rf_scene_secondary_fighter_pursuit_fixture,0,sizeof(rf_scene_secondary_fighter_pursuit_fixture));
    memset(rf_scene_secondary_fighter_pursuit_samples,0,sizeof(rf_scene_secondary_fighter_pursuit_samples));
    memset(rf_scene_secondary_fighter_pursuit_history,0,sizeof(rf_scene_secondary_fighter_pursuit_history));
    memset(rf_scene_secondary_fighter_pursuit_clocks,0,sizeof(rf_scene_secondary_fighter_pursuit_clocks));
    memset(rf_scene_secondary_fighter_pursuit_order_audit,0,sizeof(rf_scene_secondary_fighter_pursuit_order_audit));
    memset(rf_scene_secondary_fighter_pursuit_gate_audit,0,sizeof(rf_scene_secondary_fighter_pursuit_gate_audit));
#ifdef RF_IMAGE_XBOX_NATIVE
    errno=0;file=fopen("D:\\campaign-fighter-pursuit.bin","rb");
    if(!file){if(errno!=ENOENT)rf_scene_secondary_fighter_pursuit_fixture[7]=(uint32_t)RF_IO;return;}
#else
    return;
#endif
    rf_scene_secondary_fighter_pursuit_fixture[0]=1;
    if(fread(config,1,sizeof(config),file)!=sizeof(config))status=RF_FORMAT;
    trailing=fgetc(file);if(trailing!=EOF||ferror(file))status=RF_FORMAT;
    if(fclose(file))status=RF_IO;
    if(config[0]!=0x4c504653u||config[1]<1||config[1]>2||config[2]||config[3])status=RF_FORMAT;
    rf_scene_secondary_fighter_pursuit_fixture[1]=config[1];rf_scene_secondary_fighter_pursuit_fixture[3]=18155;
    rf_scene_secondary_fighter_pursuit_fixture[4]=18156;rf_scene_secondary_fighter_pursuit_fixture[5]=!status;
    rf_scene_secondary_fighter_pursuit_fixture[7]=(uint32_t)status;
}
/* Header16,exact wire64,rigid34,player3,kind/steps/last frame3,command32,
 * five event rows12,owner/registry/seat gates12. */
static void scene_secondary_fighter_pursuit_tick(scene_stream *s,uint32_t frame)
{
    static const uint32_t frames[9]={0,20,40,58,60,78,100,118,119};
    static const uint32_t events[5]={18157,18156,916900,916901,916902};
    scene_secondary_vehicle_runtime *r;scene_passive_vehicle *owner;scene_passive_vehicle_saved saved;
    uint32_t i,j,*p,status=RF_OK;
    if(!frame)scene_secondary_fighter_pursuit_open();
    if(!rf_scene_secondary_fighter_pursuit_fixture[5])return;
    r=scene_secondary_vehicle_find_uid(18155);owner=scene_secondary_vehicle_owner(r);
    if(!s||!campaign_spawn||strcmp(campaign_current_level,"L20S2.rfl")||!r||!owner||frame>=120||
       campaign_secondary_vehicle_count!=1||campaign_passive_vehicle_count!=1||campaign_authored_vehicle_uid||
       s->driller_runtime||campaign_seeds.records.count!=1||campaign_events.count!=5||
       sizeof(r->physics.state)!=34*4){rf_scene_secondary_fighter_pursuit_fixture[7]=(uint32_t)RF_FORMAT;return;}
    p=rf_scene_secondary_fighter_pursuit_history[frame];
    memcpy(p,r->physics.state.position,12);memcpy(p+3,r->physics.state.orientation,36);
    memcpy(p+12,scene_actor_body.state.position,12);memcpy(p+15,rf_scene_secondary_vehicle_pursuit,32*4);p[47]=r->steps;
    for(i=0;i<9&&frames[i]!=frame;i++){}if(i==9)return;
    rf_scene_secondary_fighter_pursuit_clocks[i]=(uint32_t)campaign_blackout_now;
    p=rf_scene_secondary_fighter_pursuit_samples[i];p[0]=1;p[1]=frame;p[2]=rf_scene_secondary_fighter_pursuit_fixture[1];
    ++rf_scene_secondary_fighter_pursuit_fixture[2];p[4]=r->uid;p[5]=r->handle;p[6]=r->slot;p[7]=r->profile;
    p[8]=campaign_player_object.handle;p[9]=(uint32_t)campaign_player_view.linked_handle;
    p[10]=campaign_secondary_vehicle_capacity;p[11]=campaign_events.count;p[12]=rf_scene_secondary_vehicle[7];
    p[13]=rf_scene_secondary_vehicle[13];p[14]=scene_secondary_vehicle_has_rider(r->handle);
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.view)++p[15];
    scene_secondary_vehicle_checkpoint_passive(r,&saved);
    scene_secondary_vehicle_checkpoint_encode((unsigned char *)(p+16),r,&saved,owner->velocity);
    memcpy(p+80,&r->physics.state,34*4);memcpy(p+114,scene_actor_body.state.position,12);
    p[117]=r->route.kind;p[118]=r->steps;p[119]=r->last_frame;memcpy(p+120,rf_scene_secondary_vehicle_pursuit,32*4);
    for(i=0;i<5;i++){
        const rf_runtime_event *e=NULL;uint32_t *v=p+152+i*12;
        for(j=0;j<campaign_events.count;j++)if(campaign_events.items[j].authored->record.uid==events[i]){
            if(e){status=RF_FORMAT;goto done;}e=campaign_events.items+j;}
        if(!e||rf_object_registry_lookup(&campaign_registry,e->handle)!=e){status=RF_FORMAT;goto done;}
        v[0]=events[i];v[1]=e->handle;v[2]=e->state.type;memcpy(v+3,&e->state.delay,4);
        status=scene_secondary_fighter_remaining(e->state.deadline,v+4);if(status)goto done;
        v[5]=e->state.actor;v[6]=e->state.source;v[7]=e->state.flags;v[8]=e->state.mode;v[9]=UINT32_MAX;
        if(e->state.type==50){status=scene_secondary_fighter_remaining(e->unhide.deadline,v+9);if(status)goto done;}
        v[10]=e->unhide.on;v[11]=e->unhide.off;
    }
    p[212]=rf_object_registry_lookup(&campaign_registry,r->handle)==owner;
    p[213]=!memcmp(owner->pose.position,r->physics.state.position,12)&&!memcmp(owner->pose.input_matrix,r->physics.state.orientation,36);
    p[214]=owner->object_kind;p[215]=r->physics.sphere_count;p[216]=owner->seat_driver;
    p[217]=(uint32_t)owner->seat_occupants[0];p[218]=(uint32_t)owner->seat_occupants[1];
    p[219]=owner->attached;p[220]=owner->group_owned;p[221]=owner->damage.destroyed;p[222]=owner->pose.flags;
    p[223]=r->collision.world==s->collision&&r->query.stream==s&&r->collision.query_context==&r->query;
    if(frame==40&&rf_scene_secondary_fighter_pursuit_fixture[1]==1){
        status=scene_secondary_vehicle_order_checks(rf_scene_secondary_fighter_pursuit_order_audit);
        if(!status)status=scene_secondary_fighter_pursuit_gates(s,r,owner);
        if(!status)status=scene_secondary_vehicle_checkpoint_audit(s);
        rf_scene_secondary_fighter_pursuit_fixture[6]=1;
    }
done:p[3]=status;if(status)rf_scene_secondary_fighter_pursuit_fixture[7]=status;
}
#endif
