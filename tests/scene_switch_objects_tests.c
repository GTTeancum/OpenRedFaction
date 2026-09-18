#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_switch_objects.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static int objects_lookup(void *context,uint32_t family,uint32_t handle,rf_switch_target *target)
{(void)context;return campaign_switch_object_lookup(family,handle,target);}
static int objects_dispatch(void *context,const rf_switch_request *request)
{(void)context;return campaign_switch_object_dispatch(request);}
int main(void)
{
    campaign_npc_body owner={0};rf_switch_target target;rf_switch_request request={0};
    rf_switch_state state={0};rf_event_state event={0};rf_event_links links={0};
    uint32_t handle,other,unrelated=8;
    rf_object_registry_init(&campaign_registry);
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    owner.registration.view=&owner.view;owner.damage.effects.health=100;
    owner.object_flags=4;
    CHECK(rf_object_registry_insert(&campaign_registry,&owner.registration,&handle)==RF_OK);
    owner.registration.handle=handle;
    CHECK(campaign_switch_object_lookup(RF_SWITCH_OBJECT,handle,&target)==RF_OK);
    CHECK(target.token==handle && target.renderable);
    CHECK(campaign_switch_object_lookup(RF_SWITCH_CONTROLLER,handle,&target)==RF_NOT_FOUND);
    /* Exercise the actual core Switch family selection for disable/enable. */
    links.handles=&handle;links.count=1;state.disabled=1;
    CHECK(rf_event_switch_links(&state,&event,&links,0,objects_lookup,objects_dispatch,NULL)==RF_OK);
    CHECK(owner.object_flags==0x4004 && owner.view.flags_7c==0x4004 && owner.room.flags==0x4004);
    state.disabled=0;
    CHECK(rf_event_switch_links(&state,&event,&links,0,objects_lookup,objects_dispatch,NULL)==RF_OK);
    CHECK(owner.object_flags==4 && owner.view.flags_7c==4 && owner.room.flags==4);
    /* Dead actors are never resurrected by a visibility switch. */
    owner.object_flags=0x4006;owner.damage.effects.health=0;
    CHECK(rf_event_switch_links(&state,&event,&links,0,objects_lookup,objects_dispatch,NULL)==RF_OK);
    CHECK(owner.object_flags==0x4006 && owner.damage.effects.health==0);
    CHECK(rf_object_registry_insert(&campaign_registry,&unrelated,&other)==RF_OK);
    CHECK(campaign_switch_object_lookup(RF_SWITCH_OBJECT,other,&target)==RF_NOT_FOUND);
    CHECK(rf_object_registry_remove(&campaign_registry,handle)==RF_OK);
    CHECK(campaign_switch_object_lookup(RF_SWITCH_OBJECT,handle,&target)==RF_NOT_FOUND);
    request.family=RF_SWITCH_OBJECT;request.token=handle;request.enabled=1;
    CHECK(campaign_switch_object_dispatch(&request)==RF_OK);
    CHECK(owner.object_flags==0x4006);
    puts("Switch NPC hide/show, shared flags, dead target and stale handle checks passed");return 0;
}
