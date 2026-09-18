#include "rf/entity.h"
#include "rf/collision.h"
#include "rf/liquid_damage.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct campaign_npc_body {
    struct {void *view;uint32_t handle;} registration;
    rf_entity_damage_state damage;uint32_t object_flags;
    struct {uint32_t flags_810;} view;
    struct {struct {float position[3];} state;} body;
} campaign_npc_body;
typedef struct scene_stream {void *collision;rf_liquid_room *liquid_rooms;uint32_t swim_room_count;} scene_stream;
static campaign_npc_body actors[2],*campaign_npc_bodies=actors;
static uint32_t campaign_npc_body_count=2,campaign_registry,retired,deaths;
typedef struct combat_feedback {int32_t now;int status;} combat_feedback;
static void *test_lookup(void *r,uint32_t handle)
{uint32_t i;(void)r;if(retired)return NULL;for(i=0;i<2;i++)if(actors[i].registration.handle==handle)return &actors[i].registration;return NULL;}
#define rf_object_registry_lookup test_lookup
static uint32_t scene_burning_create(uint32_t,uint32_t);
static uint32_t combat_predicate(void *c,uint32_t kind,uint32_t h){(void)c;return kind==RF_DAMAGE_PLAYER && h==99;}
static uint32_t combat_uid(void *c,int32_t u){(void)c;(void)u;return UINT32_MAX;}
static int combat_source(void *c,uint32_t h,uint32_t *a){(void)c;(void)h;*a=1;return 1;}
static uint32_t combat_burn(void *c,uint32_t t,uint32_t s){(void)c;return scene_burning_create(t,s);}
static float combat_random(void *c,float a,float b){(void)c;return (a+b)*.5f;}
static void combat_notify(void *c,uint32_t n,uint32_t t,float v,uint32_t s){(void)c;(void)n;(void)t;(void)v;(void)s;}
static uint32_t combat_playing(void *c,uint32_t v){(void)c;(void)v;return 0;}
static uint32_t combat_play(void *c,uint32_t t){(void)c;(void)t;return 0;}
static int rf_scene_npc_damage(uint32_t h,const rf_damage_request *r,float d,uint32_t clock,const rf_damage_effect_backend *be,float *out)
{uint32_t i;for(i=0;i<2;i++)if(actors[i].registration.handle==h)return rf_entity_damage_sp(&actors[i].damage,r->amount,r->kind,r->source,-1,d,clock,be,out);return RF_NOT_FOUND;}
static int rf_scene_npc_death_entry(uint32_t h,uint32_t *entered){(void)h;*entered=1;return RF_OK;}
static int combat_death_start(uint32_t i){(void)i;++deaths;return RF_OK;}
static int test_locate(void *w,const float *p,rf_collision_room_location *l){(void)w;(void)p;memset(l,0,sizeof(*l));return RF_OK;}
#define rf_geometry_collision_world_locate test_locate
#include "../src/diagnostic/scene_burning.inc"
static void prepare(void)
{
    uint32_t i;scene_burning_reset();memset(actors,0,sizeof(actors));retired=deaths=0;
    for(i=0;i<2;i++){actors[i].registration.view=&actors[i].view;actors[i].registration.handle=i+1;
        actors[i].damage.effects.handle=i+1;actors[i].damage.effects.class_health=actors[i].damage.effects.health=100;}
}
static int ignite(uint32_t index)
{
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,NULL};
    rf_damage_effect_input input={1,1,100,4,99,-1};
    return rf_entity_damage_effects(&actors[index].damage.effects,&input,&effects);
}
int main(void)
{
    scene_stream stream={0};uint32_t frame;float health;
    prepare();CHECK(!ignite(0) && actors[0].damage.effects.burn && scene_burning_save_pending());
    CHECK(!ignite(0) && rf_scene_burning[0]==1);health=actors[0].damage.effects.health;
    for(frame=0;frame<15;frame++)CHECK(!scene_burning_tick(&stream,frame));
    CHECK(fabsf(actors[0].damage.effects.health-(health-100*.25f/6.5f))<.001f);
    for(;frame<300;frame++)CHECK(!scene_burning_tick(&stream,frame));
    CHECK(!scene_burning_save_pending() && !actors[0].damage.effects.burn && rf_scene_burning[1]==20);
    CHECK(!ignite(0));scene_burning_extinguish(1);CHECK(!scene_burning_save_pending());
    CHECK(!ignite(0));actors[0].damage.effects.health=1;
    for(frame=0;frame<15;frame++)CHECK(!scene_burning_tick(&stream,frame));
    CHECK(deaths==1 && !scene_burning_save_pending() && !actors[0].damage.effects.burn);
    prepare();CHECK(!ignite(0));{rf_liquid_room water={0,2,0};stream.collision=&stream;stream.liquid_rooms=&water;stream.swim_room_count=1;
        CHECK(!scene_burning_tick(&stream,0));CHECK(!scene_burning_save_pending() && actors[0].damage.effects.health==100);}
    stream=(scene_stream){0};prepare();CHECK(!ignite(0));retired=1;CHECK(!scene_burning_tick(&stream,0) && !scene_burning_save_pending());
    puts("burn ignition, timed damage, expiry, explicit/water cleanup, death and stale identity passed");return 0;
}
