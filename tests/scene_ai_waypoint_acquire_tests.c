#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_ai_waypoint_acquire.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI waypoint acquisition line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static campaign_npc_body owner,saved;
int main(void)
{
    unsigned char routes[]={1,0,0,0,6,0,'p','a','t','r','o','l',2,0,0,0,0,0,0,0,1,0,0,0};
    rf_level_navigation_node nodes[2]={0};rf_runtime_event events[2]={0};
    rf_level_owned_event authored[2]={0};rf_level_link_target links[2]={0};uint32_t i;
    campaign_npc_bodies=&owner;campaign_npc_body_count=1;
    rf_object_registry_init(&campaign_registry);
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration));
    owner.view.linked_handle=-1;owner.damage.effects.health=100;owner.combat_alert=1;
    campaign_navigation.nodes=nodes;campaign_navigation.count=2;
    nodes[0].candidate.position[0]=3;nodes[1].candidate.position[2]=7;
    campaign_waypoints=routes;campaign_waypoint_bytes=sizeof(routes);
    campaign_events.items=events;campaign_events.count=1;
    for(i=0;i<2;i++){
        events[i].authored=authored+i;events[i].links=links+i;
        authored[i].record.uid=900+i;authored[i].record.link_count=1;
        strcpy(authored[i].record.type,"Follow_Waypoints");
        strcpy(authored[i].record.texts[0],"patrol");strcpy(authored[i].record.texts[1],"Ping Pong");
        links[i].kind=1;links[i].value=owner.registration.handle;
    }
    CHECK(!campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,100));
    CHECK(owner.ai_mode.action_280==4 && owner.script_move.active && !owner.combat_alert);
    CHECK(owner.script_move.path.count==2 && owner.script_move.path_index==0 && owner.script_move.path_mode==2);
    CHECK(owner.script_move.event==900 && owner.script_move.target[0]==3 && !owner.script_move.follow);
    CHECK(!campaign_enemy_mode_admits(&owner,1,0));
    /* A paused route keeps its cursor and reverse direction when resumed. */
    owner.script_move.path_index=1;owner.script_move.path_reverse=1;
    CHECK(!campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,1,101));
    CHECK(!campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,102));
    CHECK(owner.script_move.path_index==1 && owner.script_move.path_reverse==1 && owner.script_move.target[2]==7);
    memset(&owner.script_move,0,sizeof(owner.script_move));owner.ai_mode.action_280=2;
    /* Duplicate identical authored bindings are unambiguous; differing modes
     * or names must preserve the entire owner rather than pick event order. */
    campaign_events.count=2;
    CHECK(!campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,103));
    memset(&owner.script_move,0,sizeof(owner.script_move));owner.ai_mode.action_280=2;
    strcpy(authored[1].record.texts[1],"Loop");saved=owner;
    CHECK(campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,104)==RF_NOT_FOUND);
    CHECK(!memcmp(&saved,&owner,sizeof(owner)));
    events[1].retired=1;nodes[1].candidate.position[2]=NAN;
    CHECK(campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,105)==RF_FORMAT);
    CHECK(!memcmp(&saved,&owner,sizeof(owner)));
    nodes[1].candidate.position[2]=7;links[0].value=UINT32_MAX;
    CHECK(campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,106)==RF_NOT_FOUND);
    CHECK(!memcmp(&saved,&owner,sizeof(owner)));
    links[0].value=owner.registration.handle;
    CHECK(!campaign_set_ai_mode_acquiring(NULL,owner.registration.handle,4,107));
    puts("AI waypoint acquisition: unique authored binding, route resume, ambiguity and invalid-node preservation passed; locomotion/rendering not exercised");
    return 0;
}
