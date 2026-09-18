/* Real player contact response -> impact aggregation -> shared SP damage. */
#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Player impact scene line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int collide(uint32_t frame,float speed,uint32_t relocated)
{
    const float floor[3]={0,1,0},zero[3]={0};float impact;rf_physics_body_state next=scene_actor_body.state;
    next.velocity[1]=-speed;rf_scene_actor_landing[1]=3;
    scene_player_impact_frame_begin(frame,relocated);
    CHECK(!actor_contact(&next,floor,zero,zero,3,&impact));
    CHECK(fabsf(impact-speed)<.001f);
    CHECK(!scene_player_impact_contact(&next,3,2,impact));
    CHECK(!scene_player_impact_contact(&next,3,2,impact)); /* duplicate sphere */
    scene_actor_body.state=next;rf_scene_actor_landing[1]=1;
    CHECK(!scene_player_impact_frame_end(frame,(int32_t)(frame*16)));
    return 0;
}
int main(void)
{
    uint32_t i;float health;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_spawn=1;campaign_player_view.linked_handle=-1;campaign_player_view.flags_7c=8;
    campaign_force_class_kind=3;rf_scene_actor_eye_enabled=0; /* no audio device/model required */
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=100;
    campaign_player_damage.state.responsible_handle=UINT32_MAX;
    for(i=0;i<11;i++)campaign_player_damage.factors[i]=1;
    scene_actor_body.state.flags=0x80;
    scene_actor_body.state.orientation[0]=scene_actor_body.state.orientation[4]=scene_actor_body.state.orientation[8]=1;
    CHECK(!collide(0,13,0) && campaign_player_damage.state.effects.health==100);
    CHECK(!collide(1,13,0));health=campaign_player_damage.state.effects.health;
    CHECK(health==64 && rf_scene_player_impact[3]==1);
    CHECK(campaign_player_damage.state.responsible_handle==UINT32_MAX);
    CHECK(!scene_player_impact_frame_end(1,16) && campaign_player_damage.state.effects.health==health);
    for(i=2;i<12;i++)CHECK(!collide(i,1,0));
    CHECK(campaign_player_damage.state.effects.health==health && rf_scene_player_impact[3]==1);
    /* Same latch called by live teleport, respawn and checkpoint entry hooks. */
    scene_player_impact_relocated();CHECK(!collide(12,20,0));
    CHECK(campaign_player_damage.state.effects.health==health);
    CHECK(!collide(13,20,1) && campaign_player_damage.state.effects.health==health);
    campaign_player_view.flags_7c|=4;
    CHECK(!collide(14,20,0) && campaign_player_damage.state.effects.health==health);
    campaign_player_view.flags_7c&=~4u;
    CHECK(!collide(15,20,0) && campaign_player_damage.state.effects.health<=0);
    CHECK(rf_scene_player_impact[3]==2);
    puts("Player contact impact: real SP damage, duplicate/ground/relocation immunity passed");return 0;
}
