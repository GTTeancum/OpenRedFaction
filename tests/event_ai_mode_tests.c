#include "rf/event.h"
#include "rf/physics.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static rf_entity_ai_transition_state ai;
static uint32_t entity_handle,calls,propagations;
static uint32_t form_calls,form_variant,form_enabled;static int32_t form_now;
static int set_form(void *context,uint32_t variant,uint32_t enabled,int32_t now)
{
    if(context!=&ai)return RF_FORMAT;
    ++form_calls;form_variant=variant;form_enabled=enabled;form_now=now;return RF_OK;
}
static int set_mode(void *context,uint32_t handle,int32_t action,int32_t now)
{
    if(context!=&ai)return RF_FORMAT;
    if(handle!=entity_handle)return RF_NOT_FOUND;
    ++calls;return rf_entity_ai_set_action(&ai,action,UINT32_MAX,UINT32_MAX,(float)now,0,0);
}
static int propagated(void *context,const rf_level_event *event)
{(void)context;(void)event;++propagations;return RF_OK;}
typedef struct npc_teleport_test_context {
    uint32_t actors[2],trace[12],count,attempts,enabled[12];int failure;
    const rf_level_event *destination;
} npc_teleport_test_context;
static int teleport_link(void *context,uint32_t handle,const rf_level_event *event)
{
    npc_teleport_test_context *c=context;
    if(event!=c->destination)return RF_FORMAT;
    ++c->attempts;
    if(handle!=c->actors[0] && handle!=c->actors[1])return RF_NOT_FOUND;
    if(c->failure)return c->failure;
    if(c->count>=12)return RF_RANGE;
    c->trace[c->count++]=handle;return RF_OK;
}
static int teleport_downstream(void *context,const rf_level_event *event)
{
    npc_teleport_test_context *c=context;(void)event;
    if(c->count>=12)return RF_RANGE;
    c->trace[c->count++]=UINT32_MAX;return RF_OK;
}
static int scripted_actor_link(void *context,uint32_t handle)
{
    npc_teleport_test_context *c=context;
    return teleport_link(context,handle,c->destination);
}
static int physics_actor_link(void *context,uint32_t handle,uint32_t enabled)
{
    npc_teleport_test_context *c=context;uint32_t before=c->count;
    int status=scripted_actor_link(context,handle);
    if(!status)c->enabled[before]=enabled;
    return status;
}
static void scripted_actor_backend(rf_runtime_triggers *triggers,uint32_t type,
    npc_teleport_test_context *c,uint32_t enabled)
{
    if(type==4){triggers->teleport_npc=enabled?teleport_link:NULL;triggers->npc_teleport_context=c;}
    else if(type==62){triggers->set_physics_enabled=enabled?physics_actor_link:NULL;triggers->physics_state_context=c;}
    else if(type==81){triggers->drop_npc_weapon=enabled?scripted_actor_link:NULL;triggers->drop_weapon_context=c;}
    else {triggers->ignite_npc=enabled?scripted_actor_link:NULL;triggers->ignite_context=c;}
}
/* Ordered generation-valid links for Teleport4, Drop_Weapon81, Ignite_Entity82
 * and Turn_Off_Physics62. The latter also acts on OFF, requesting wake. */
static int npc_linked_action_dispatch_test(uint32_t type)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[5]={{0}},off={0};npc_teleport_test_context c={0};
    uint32_t objects[3]={0},stale,pending,i,before,attempts;
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    for(i=0;i<2;i++)CHECK(rf_object_registry_insert(&registry,objects+i,c.actors+i)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects+2,&stale)==RF_OK);
    CHECK(rf_object_registry_remove(&registry,stale)==RF_OK);
    items[0].state.type=type;items[0].links=links;authored[0].record.link_count=5;
    authored[0].record.has_orientation=1;authored[0].record.position[0]=49.284691f;c.destination=&authored[0].record;
    items[1].state.type=63;
    items[2].state.type=3;items[2].links=&off;authored[2].record.link_count=1;off.kind=1;off.value=items[0].handle;
    for(i=0;i<5;i++)links[i].kind=i==2?2:1;
    links[0].value=c.actors[0];links[1].value=stale;links[2].value=c.actors[1];
    links[3].value=c.actors[0];links[4].value=items[1].handle;
    scripted_actor_backend(&triggers,type,&c,1);
    triggers.teleport_player=teleport_downstream;triggers.teleport_context=&c;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(c.count==4 && c.attempts==4 && c.trace[0]==c.actors[0] && c.trace[1]==c.actors[1] &&
        c.trace[2]==c.actors[0] && c.trace[3]==UINT32_MAX && report.other_targets>=1);
    if(type==62)CHECK(!c.enabled[0] && !c.enabled[1] && !c.enabled[2]);
    /* Invert emits OFF: only physics62 applies its reverse effect. */
    CHECK(rf_runtime_event_fire(&triggers,items[2].handle,7,8,110,&gravity,NULL,NULL,&report)==RF_OK);
    if(type==62) {
        CHECK(c.count==7 && c.attempts==8 && c.trace[4]==c.actors[0] && c.trace[5]==c.actors[1] && c.trace[6]==c.actors[0]);
        CHECK(c.enabled[4]==1 && c.enabled[5]==1 && c.enabled[6]==1);
    } else CHECK(c.count==4 && c.attempts==4);
    before=c.count;attempts=c.attempts;
    items[0].state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,200,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(items[0].state.deadline==450 && c.count==before);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,449,NULL,NULL,&report,&pending)==RF_OK && c.count==before);
    scripted_actor_backend(&triggers,type,&c,0);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,450,NULL,NULL,&report,&pending)==RF_OK && pending==1 && c.count==before);
    scripted_actor_backend(&triggers,type,&c,1);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,450,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(c.count==before+4 && c.attempts==attempts+4 && c.trace[before]==c.actors[0] && c.trace[before+1]==c.actors[1] &&
        c.trace[before+2]==c.actors[0] && c.trace[before+3]==UINT32_MAX && items[0].state.deadline==-1);
    if(type==62)CHECK(!c.enabled[before] && !c.enabled[before+1] && !c.enabled[before+2]);
    items[0].state.delay=0;c.failure=RF_IO;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,500,&gravity,NULL,NULL,&report)==RF_IO && c.count==before+4);
    return 0;
}
typedef struct pickup_state_test_context {
    uint32_t flags[2],trace[24],enabled[24],count,attempts;int failure;
} pickup_state_test_context;
static int pickup_state_link(void *context,uint32_t uid,uint32_t enabled)
{
    pickup_state_test_context *c=context;uint32_t slot;
    ++c->attempts;
    if(uid!=4037 && uid!=4038)return RF_NOT_FOUND;
    if(c->failure)return c->failure;
    if(c->count>=24 || enabled>1)return RF_RANGE;
    slot=uid-4037;
    if(enabled)c->flags[slot]&=~1u;else c->flags[slot]|=1u;
    c->trace[c->count]=uid;c->enabled[c->count++]=enabled;return RF_OK;
}
static int pickup_state_downstream(void *context,const rf_level_event *event)
{
    pickup_state_test_context *c=context;(void)event;
    if(c->count>=24)return RF_RANGE;
    c->trace[c->count]=UINT32_MAX;c->enabled[c->count++]=1;return RF_OK;
}
static int pickup_state_dispatch_test(void)
{
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[5]={{0}},off={0};uint32_t uids[]={4037,4040,4038,4037,9000},pending,i;
    pickup_state_test_context c={{0xa5,0x22}};
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    /* Real placed pickups have no registry entry: resolved kind0 must work.
     * Missing UID4040 and linked event9000 are rejected by the item backend. */
    items[0].state.type=54;items[0].links=links;authored[0].links=uids;authored[0].record.link_count=5;
    items[1].state.type=63;links[4].kind=1;links[4].value=items[1].handle;
    items[2].state.type=3;items[2].links=&off;authored[2].record.link_count=1;off.kind=1;off.value=items[0].handle;
    triggers.set_item_pickup_state=pickup_state_link;triggers.pickup_state_context=&c;
    triggers.teleport_player=pickup_state_downstream;triggers.teleport_context=&c;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(c.count==4 && c.attempts==5 && c.flags[0]==0xa4 && c.flags[1]==0x22);
    CHECK(c.trace[0]==4037 && c.trace[1]==4038 && c.trace[2]==4037 && c.trace[3]==UINT32_MAX);
    CHECK(c.enabled[0]==1 && c.enabled[1]==1 && c.enabled[2]==1);
    CHECK(rf_runtime_event_fire(&triggers,items[2].handle,7,8,110,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(c.count==7 && c.attempts==10 && c.flags[0]==0xa5 && c.flags[1]==0x23);
    CHECK(c.trace[4]==4037 && c.trace[5]==4038 && c.trace[6]==4037 && !c.enabled[4] && !c.enabled[5] && !c.enabled[6]);
    items[0].state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,200,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(items[0].state.deadline==450 && c.count==7);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,449,NULL,NULL,&report,&pending)==RF_OK && c.count==7);
    triggers.set_item_pickup_state=NULL;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,450,NULL,NULL,&report,&pending)==RF_OK && pending==1 && c.count==7);
    triggers.set_item_pickup_state=pickup_state_link;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,450,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(c.count==11 && c.attempts==15 && c.flags[0]==0xa4 && c.flags[1]==0x22 && c.trace[10]==UINT32_MAX);
    /* OFF retains its own ordinary delayed mode, instead of becoming ON. */
    CHECK(rf_runtime_event_fire(&triggers,items[2].handle,7,8,500,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(items[0].state.deadline==750 && c.count==11);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,750,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(c.count==14 && c.attempts==20 && c.flags[0]==0xa5 && c.flags[1]==0x23 && !c.enabled[11]);
    items[0].state.delay=0;c.failure=RF_IO;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,800,&gravity,NULL,NULL,&report)==RF_IO && c.count==14);
    return 0;
}
static int physics_enable_primitive_test(void)
{
    static const uint32_t flags[][3]={
        {0,0x18000000u,0x98000000u},{UINT32_MAX,0x7fffffffu,UINT32_MAX},
        {0x98000000u,0x18000000u,0x98000000u},{0x04000001u,0x1c000001u,0x9c000001u}};
    rf_physics_body_state actual,expected,before;uint32_t object_flags,i;
    for(i=0;i<sizeof(flags)/sizeof(flags[0]);i++) {
        memset(&actual,0x5a,sizeof(actual));actual.flags=flags[i][0];expected=actual;
        expected.flags=flags[i][1];object_flags=0x42;
        memset(expected.velocity,0,sizeof(expected.velocity));
        memset(expected.vector_c8,0,sizeof(expected.vector_c8));
        memset(expected.mass_vector_d4,0,sizeof(expected.mass_vector_d4));
        CHECK(rf_physics_set_enabled(&actual,&object_flags,0)==RF_OK);
        CHECK(!memcmp(&actual,&expected,sizeof(actual)) && object_flags==0x42);
        /* All other fields, including e0/ec force vectors and pose, survive. */
        expected.flags=flags[i][2];
        CHECK(rf_physics_set_enabled(&actual,&object_flags,1)==RF_OK);
        CHECK(!memcmp(&actual,&expected,sizeof(actual)) && object_flags==0x06000042u);
    }
    memset(&actual,0x5a,sizeof(actual));actual.flags=0;before=actual;object_flags=0x08000042u;
    CHECK(rf_physics_set_enabled(&actual,&object_flags,0)==RF_OK);
    CHECK(!memcmp(&actual,&before,sizeof(actual)) && object_flags==0x08000042u);
    expected=before;expected.flags=0x80000000u;
    CHECK(rf_physics_set_enabled(&actual,&object_flags,1)==RF_OK);
    CHECK(!memcmp(&actual,&expected,sizeof(actual)) && object_flags==0x0e000042u);
    before=actual;
    CHECK(rf_physics_set_enabled(&actual,&object_flags,2)==RF_RANGE);
    CHECK(rf_physics_set_enabled(NULL,&object_flags,0)==RF_RANGE);
    CHECK(rf_physics_set_enabled(&actual,NULL,1)==RF_RANGE);
    CHECK(!memcmp(&actual,&before,sizeof(actual)) && object_flags==0x0e000042u);
    return 0;
}
int main(void)
{
    CHECK(npc_linked_action_dispatch_test(4)==0);
    CHECK(npc_linked_action_dispatch_test(81)==0);
    CHECK(npc_linked_action_dispatch_test(82)==0);
    CHECK(npc_linked_action_dispatch_test(62)==0);
    CHECK(physics_enable_primitive_test()==0);
    CHECK(pickup_state_dispatch_test()==0);
    rf_runtime_event items[3]={{0}};rf_level_owned_event authored[3]={{0}};
    rf_runtime_events events={0};rf_runtime_triggers triggers={0};rf_object_registry registry;
    rf_physics_gravity gravity={0};rf_startup_events_report report;
    rf_level_link_target links[4]={{0}},off={0};uint32_t entity=0,pending,i,before;
    static const int32_t expected[]={1,2,4,5,11,-1};
    rf_object_registry_init(&registry);events.registry=triggers.registry=&registry;events.items=items;events.count=3;
    for(i=0;i<3;i++) {
        items[i].object_kind=6;items[i].authored=authored+i;items[i].state.deadline=-1;
        CHECK(rf_object_registry_insert(&registry,items+i,&items[i].handle)==RF_OK);
    }
    CHECK(rf_object_registry_insert(&registry,&entity,&entity_handle)==RF_OK);
    items[0].state.type=34;items[0].links=links;authored[0].record.link_count=4;
    items[1].state.type=3;items[1].links=&off;authored[1].record.link_count=1;
    items[2].state.type=63;off.kind=1;off.value=items[0].handle;
    for(i=0;i<4;i++)links[i].kind=1;
    links[0].value=links[2].value=entity_handle;links[1].value=UINT32_MAX;links[3].value=items[2].handle;
    triggers.set_ai_mode=set_mode;triggers.ai_mode_context=&ai;triggers.teleport_player=propagated;
    for(i=0;i<6;i++) {
        authored[0].record.words[0]=i;
        CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1234,&gravity,NULL,NULL,&report)==RF_OK);
        CHECK(ai.action_280==expected[i] && ai.clock_288==1234 && ai.argument_28c==UINT32_MAX && ai.argument_290==UINT32_MAX);
    }
    CHECK(calls==12 && propagations==6);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,1234,&gravity,NULL,NULL,&report)==RF_OK && calls==12);
    authored[0].record.words[0]=1;items[0].state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1300,&gravity,NULL,NULL,&report)==RF_OK && calls==12);
    triggers.set_ai_mode=NULL;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1550,NULL,NULL,&report,&pending)==RF_OK && pending==1);
    triggers.set_ai_mode=set_mode;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,1550,NULL,NULL,&report,&pending)==RF_OK && !pending);
    CHECK(calls==14 && ai.action_280==2 && ai.clock_288==1550 && propagations==7);
    items[0].state.delay=0;items[0].state.flags=1;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1600,&gravity,NULL,NULL,&report)==RF_OK && calls==14);
    items[0].state.flags=0;authored[0].record.words[0]=6;before=calls;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1600,&gravity,NULL,NULL,&report)==RF_RANGE && calls==before);
    items[0].state.type=47;authored[0].record.words[0]=1;
    triggers.set_player_form=set_form;triggers.player_form_context=&ai;
    CHECK(rf_runtime_event_fire(&triggers,items[0].handle,7,8,1700,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(form_calls==1 && form_variant==1 && form_enabled==1 && form_now==1700);
    CHECK(rf_runtime_event_fire(&triggers,items[1].handle,7,8,1800,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(form_calls==2 && form_variant==1 && form_enabled==0 && form_now==1800);
    puts("NPC teleport/disarm/ignition, physics/pickup-state ordering/delay and AI mode/undercover ON/OFF dispatch passed");return 0;
}
