#include "rf/event_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"event checkpoint line%d\n",__LINE__);return 1;}}while(0)
static int export_ref(void *context,uint32_t handle,uint32_t *uid)
{(void)context;if(handle!=1234)return RF_NOT_FOUND;*uid=8456;return RF_OK;}
static int import_ref(void *context,uint32_t uid,uint32_t *handle)
{(void)context;if(uid!=8456)return RF_NOT_FOUND;*handle=5678;return RF_OK;}
static int missing_ref(void *context,uint32_t uid,uint32_t *handle)
{(void)context;(void)uid;(void)handle;return RF_NOT_FOUND;}
static void switch_effect(void *context,const rf_switch_state *s,uint32_t effect)
{(void)context;(void)s;(void)effect;}
static int visible(void *context,uint32_t uid,int on)
{if(context)++*(uint32_t *)context;(void)uid;(void)on;return 1;}
typedef struct dispatched {uint32_t count,source,actor;} dispatched;
static void action(void *context,rf_event_state *s,uint32_t kind,uint32_t source,uint32_t actor,uint32_t mode)
{dispatched *d=context;++d->count;d->source=source;d->actor=actor;(void)s;(void)kind;(void)mode;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void checksum(unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<192;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}put(p+12,h);}
int main(void)
{
    unsigned char identity[32]={3},wire[192],saved[192];rf_runtime_event live={0},fresh={0},before;
    rf_level_owned_event authored={0};uint32_t pulse;int32_t remaining;
    authored.record.uid=100;live.authored=&authored;live.object_kind=6;live.handle=10;
    live.state.type=20;live.state.deadline=-1;live.state.source=live.state.actor=UINT32_MAX;
    CHECK(!rf_event_cycle_init(&live.cycle,.1f,4,0,0));CHECK(!rf_event_cycle_enable(&live.cycle,1));
    CHECK(!rf_event_cycle_tick(&live.cycle,1,&pulse)&&pulse&&live.cycle.count==1);
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,sizeof(wire)));memcpy(saved,wire,192);
    fresh=live;fresh.handle=90;CHECK(!rf_event_cycle_init(&fresh.cycle,.1f,4,0,1000));
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000));
    CHECK(fresh.handle==90&&fresh.cycle.count==1&&fresh.cycle.enabled==1);
    CHECK(!rf_timer_remaining(fresh.cycle.deadline,1000,&remaining)&&remaining==81);
    CHECK(!rf_event_cycle_tick(&fresh.cycle,1082,&pulse)&&pulse&&fresh.cycle.count==2);
    before=fresh;wire[92]^=1;
    CHECK(rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)==RF_FORMAT&&!memcmp(&fresh,&before,sizeof(fresh)));
    memcpy(wire,saved,192);authored.record.uid=101;
    CHECK(rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)==RF_FORMAT&&!memcmp(&fresh,&before,sizeof(fresh)));
    authored.record.uid=100;live.state.source=22;
    CHECK(rf_event_checkpoint_encode(identity,&live,20,wire,192)==RF_NOT_FOUND&&!memcmp(wire,saved,192));
    live.state.source=UINT32_MAX;live.state.deadline=30;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000));
    CHECK(!rf_timer_remaining(fresh.state.deadline,1000,&remaining)&&remaining==10);
    live.state.deadline=-1;live.state.type=89;
    CHECK(rf_event_checkpoint_encode(identity,&live,20,wire,192)==RF_NOT_FOUND);
    live.state.type=16;live.death_fired=1;live.death_time=19;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;fresh.death_fired=fresh.death_time=0;
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)&&fresh.death_fired==1&&fresh.death_time==19);
    live.state.type=87;live.threshold.threshold=20;live.threshold.fired=1;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;fresh.threshold.fired=0;
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)&&fresh.threshold.fired==1);
    live.state.type=88;
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;fresh.threshold.fired=0;
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)&&fresh.threshold.fired==1);
    fresh.threshold.threshold=21;before=fresh;
    CHECK(rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)==RF_FORMAT&&!memcmp(&fresh,&before,sizeof(fresh)));
    {
        static const uint32_t types[]={0,1,2,3,5,6,7,10,11,12,13,14,15,17,19,22,24,28,30,34,35,36,37,38,39,41,42,44,46,48,51,52,56,61,64,65,81};
        uint32_t i;rf_event_checkpoint_refs refs={export_ref,import_ref,NULL},missing={export_ref,missing_ref,NULL};
        live.state.source=0;live.state.actor=UINT32_MAX;live.state.flags=5;live.state.mode=7;
        for(i=0;i<sizeof(types)/sizeof(types[0]);i++){
            live.state.type=types[i];CHECK(rf_event_checkpoint_type_supported(types[i]));
            CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;
            fresh.state.flags=fresh.state.mode=0;fresh.state.actor=0;fresh.state.source=UINT32_MAX;
            CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000));
            CHECK(fresh.state.flags==5&&fresh.state.mode==7&&!fresh.state.source&&fresh.state.actor==UINT32_MAX);
        }
        live.state.type=5;live.state.source=1234;
        CHECK(!rf_event_checkpoint_encode_mapped(identity,&live,20,&refs,wire,192));fresh=live;fresh.state.source=0;before=fresh;
        CHECK(rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)==RF_NOT_FOUND&&!memcmp(&fresh,&before,sizeof(fresh)));
        CHECK(rf_event_checkpoint_restore_mapped(wire,192,identity,&fresh,1000,&missing)==RF_NOT_FOUND&&!memcmp(&fresh,&before,sizeof(fresh)));
        CHECK(!rf_event_checkpoint_restore_mapped(wire,192,identity,&fresh,1000,&refs)&&fresh.state.source==5678);
        memcpy(saved,wire,192);live.state.deadline=100;
        CHECK(!rf_event_checkpoint_encode_mapped(identity,&live,20,&refs,wire,192));
        CHECK(!rf_event_checkpoint_restore_mapped(wire,192,identity,&fresh,1000,&refs)&&fresh.state.source==5678);
        CHECK(!rf_timer_remaining(fresh.state.deadline,1000,&remaining)&&remaining==80);
        live.state.deadline=-1;live.state.source=0;
    }
    {
        rf_switch_state switched,fresh_switch;
        CHECK(!rf_event_switch_init(&switched,0,10,0,0));
        CHECK(!rf_event_switch_on(&switched,switch_effect,NULL)&&switched.activations==1);
        live.state.type=32;live.switch_state=&switched;
        CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));
        CHECK(!rf_event_switch_init(&fresh_switch,0,10,0,0));fresh=live;fresh.switch_state=&fresh_switch;
        CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000));
        CHECK(fresh_switch.disabled==switched.disabled&&fresh_switch.activations==1&&fresh.switch_state==&fresh_switch);
        CHECK(!rf_event_switch_on(&fresh_switch,switch_effect,NULL)&&fresh_switch.activations==2);
        live.switch_state=NULL;
    }
    live.state.type=50;CHECK(!rf_unhide_init(&live.unhide,0));CHECK(!rf_unhide_request(&live.unhide,1));
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)&&fresh.unhide.on==1);
    {uint32_t uid=42,seen=0;CHECK(!rf_unhide_tick(&fresh.unhide,1001,&uid,1,visible,&seen)&&seen==1&&!fresh.unhide.on);}
    CHECK(!rf_unhide_tick(&live.unhide,1,NULL,0,visible,NULL)&&!live.unhide.on);
    CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;fresh.unhide.deadline=-1;
    CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000));
    CHECK(!rf_timer_remaining(fresh.unhide.deadline,1000,&remaining)&&remaining==481);
    /* Composer removes restored retired owners before gameplay resumes. */
    {
        static rf_object_registry registry;
        live.state.type=15;live.state.flags|=1;live.retired=1;live.death_fired=1;
        CHECK(!rf_event_checkpoint_encode(identity,&live,20,wire,192));fresh=live;fresh.retired=0;fresh.death_fired=0;
        rf_object_registry_init(&registry);CHECK(!rf_object_registry_insert(&registry,&fresh,&fresh.handle));
        CHECK(!rf_event_checkpoint_preflight(wire,192,identity,&fresh,1000));
        CHECK(!rf_event_checkpoint_restore(wire,192,identity,&fresh,1000)&&fresh.retired&&fresh.death_fired);
        CHECK(!rf_object_registry_remove(&registry,fresh.handle)&&!rf_object_registry_lookup(&registry,fresh.handle));
    }
    {
        rf_runtime_event queued={0},resumed;dispatched d={0};rf_event_checkpoint_refs refs={export_ref,import_ref,NULL};
        queued.object_kind=6;queued.authored=&authored;queued.state.type=48;queued.state.deadline=-1;queued.state.delay=.5f;
        CHECK(!rf_event_activate(&queued.state,100,1234,UINT32_MAX,1,action,&d)&&!d.count);
        CHECK(!rf_event_checkpoint_encode_mapped(identity,&queued,200,&refs,wire,192));resumed=queued;
        CHECK(!rf_event_checkpoint_restore_mapped(wire,192,identity,&resumed,1000,&refs));
        CHECK(!rf_event_tick(&resumed.state,1399,action,&d)&&!d.count);
        CHECK(!rf_event_tick(&resumed.state,1401,action,&d)&&d.count==2&&d.source==5678&&d.actor==UINT32_MAX&&resumed.state.deadline==-1);
        CHECK(!rf_event_checkpoint_encode_mapped(identity,&queued,200,&refs,wire,192));
        put(wire+160,(uint32_t)-2);checksum(wire);before=resumed;
        CHECK(rf_event_checkpoint_restore_mapped(wire,192,identity,&resumed,1000,&refs)==RF_FORMAT&&!memcmp(&resumed,&before,sizeof(resumed)));
        queued.state.deadline=-1;CHECK(!rf_event_checkpoint_encode_mapped(identity,&queued,200,&refs,wire,192));
        put(wire+4,2);put(wire+160,0);checksum(wire);
        CHECK(!rf_event_checkpoint_restore_mapped(wire,192,identity,&resumed,1000,&refs)&&resumed.state.deadline==-1);
    }
    CHECK(rf_event_checkpoint_external_requirements(0)==RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO);
    CHECK(rf_event_checkpoint_external_requirements(17)==RF_EVENT_CHECKPOINT_EXTERNAL_DAMAGE);
    puts("PASS L1S1 event fields, delayed dispatch, UnHide requests, UID remapping and RFEC2 compatibility");return 0;
}
