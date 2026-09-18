#include "rf/entity_assets.h"
#include "rf/grenade_flight.h"
#include "rf/collision.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct scene_stream {void *collision;} scene_stream;
typedef struct scene_grenade_projectile {rf_grenade_flight flight;rf_weapon_flight_contact last_contact;uint32_t has_contact;} scene_grenade_projectile;
static struct {float vector[3];} scene_gravity={{0,-9.8f,0}};
static struct {struct {float velocity[3];} state;} scene_actor_body;
static struct {int32_t loaded[64],reserve[32];uint32_t owned[64];} campaign_player_inventory;
static struct {rf_weapon_acquire_definition definitions[64];} campaign_weapon_supply;
static float scene_flame_gas_remainder;
static uint32_t rf_scene_combat[8],campaign_last_alt,rf_scene_combat_trace,blasts,impacts;
static int collide;
static void campaign_ammo_publish(void){}
static void combat_sound(const char *s,const float *p){(void)s;(void)p;}
static void scene_impact_sound(const float *p,uint32_t f){(void)p;(void)f;}
static int scene_impact_start_sized(scene_stream *s,const rf_weapon_flight_contact *c,uint32_t f,float radius)
{(void)s;(void)c;(void)f;if(radius!=2)return RF_RANGE;++impacts;return RF_OK;}
static int scene_explosion_blast(scene_stream *s,uint32_t f,const float *p,float damage,float radius)
{(void)s;(void)f;(void)p;if(damage!=100 || radius!=7)return RF_RANGE;++blasts;return RF_OK;}
static int scene_grenade_sweep(void *c,const float *p,const float *d,float r,rf_weapon_flight_contact *h,uint32_t *hit)
{uint32_t i;(void)c;(void)r;*hit=collide;if(collide){memset(h,0,sizeof(*h));h->hit.fraction=.5f;h->hit.normal[1]=1;
 for(i=0;i<3;i++)h->hit.point[i]=p[i]+d[i]*.5f;}return RF_OK;}
static int test_locate(const void *w,const float *p,rf_collision_room_location *r)
{(void)w;(void)p;memset(r,0,sizeof(*r));return RF_OK;}
#define rf_geometry_collision_world_locate test_locate
#include "../src/diagnostic/scene_flame_canister.inc"
int main(void)
{
    scene_stream s={0};rf_weapon_primary_definition d={0};float p[3]={0,3,0},f[3]={0,0,1};uint32_t frame;
    d.alt_damage=100;d.alt_fire_seconds=4;campaign_player_inventory.owned[2]=1;campaign_player_inventory.loaded[2]=37;
    campaign_weapon_supply.definitions[2].ammo_type=3;campaign_weapon_supply.definitions[2].magazine=100;
    campaign_player_inventory.reserve[3]=250;
    scene_flame_canister_reset();
    CHECK(!scene_flame_canister_tick(&s,0,p,f,2,&d,1,0,1));
    CHECK(scene_flame_canister_busy() && campaign_last_alt && rf_scene_combat[0]==1);
    CHECK(scene_flame_canister_save_pending());
    for(frame=1;frame<108;frame++)CHECK(!scene_flame_canister_tick(&s,frame,p,f,2,&d,1,0,1));
    CHECK(campaign_player_inventory.loaded[2]==37 && !rf_scene_flame_canister[1]);
    CHECK(!scene_flame_canister_tick(&s,108,p,f,2,&d,1,0,1));
    CHECK(campaign_player_inventory.loaded[2]==100 && campaign_player_inventory.reserve[3]==150 && rf_scene_flame_canister[1]==1 && rf_scene_flame_canister[3]==1);
    CHECK(scene_flame_canister_save_pending());
    CHECK(scene_flame_canisters[0].flight.velocity[2]==10 && scene_flame_canisters[0].flight.radius==.051f);
    collide=1;CHECK(!scene_flame_canister_tick(&s,109,p,f,2,&d,0,0,0));CHECK(!blasts);
    CHECK(!scene_flame_canister_tick(&s,110,p,f,2,&d,0,0,0));CHECK(blasts==1 && impacts==1 && !rf_scene_flame_canister[3]);
    CHECK(!scene_flame_canister_save_pending());
    CHECK(!scene_flame_canister_tick(&s,111,p,f,2,&d,0,0,0));CHECK(blasts==1);
    /* Switch cancellation preserves ammo; holding alternate across cooldown does not retrigger. */
    scene_flame_canister_reset();collide=0;campaign_player_inventory.loaded[2]=100;
    CHECK(!scene_flame_canister_tick(&s,0,p,f,2,&d,1,0,1));
    CHECK(!scene_flame_canister_tick(&s,1,p,f,2,&d,0,0,1));
    for(frame=2;frame<300;frame++)CHECK(!scene_flame_canister_tick(&s,frame,p,f,2,&d,1,0,1));
    CHECK(campaign_player_inventory.loaded[2]==100 && !rf_scene_flame_canister[1]);
    /* Low and zero reserve use the explicit partial-refill port policy. */
    {uint32_t run;for(run=0;run<2;run++){
        scene_flame_canister_reset();campaign_player_inventory.loaded[2]=37;campaign_player_inventory.reserve[3]=run?0:29;
        CHECK(!scene_flame_canister_tick(&s,0,p,f,2,&d,1,0,1));
        for(frame=1;frame<=108;frame++)CHECK(!scene_flame_canister_tick(&s,frame,p,f,2,&d,1,0,1));
        CHECK(campaign_player_inventory.loaded[2]==(run?0:29) && !campaign_player_inventory.reserve[3]);
        CHECK(rf_scene_flame_canister[1]==1);
    }}
    /* A bounded pool refusal preserves both loaded and reserve ammunition. */
    scene_flame_canister_reset();campaign_player_inventory.loaded[2]=37;campaign_player_inventory.reserve[3]=250;
    CHECK(!scene_flame_canister_tick(&s,0,p,f,2,&d,1,0,1));
    for(frame=1;frame<108;frame++)CHECK(!scene_flame_canister_tick(&s,frame,p,f,2,&d,1,0,1));
    for(frame=0;frame<SCENE_FLAME_CANISTER_CAPACITY;frame++)
        CHECK(!rf_grenade_flight_launch(&scene_flame_canisters[frame].flight,p,f,10,.051f,10,0,0x10));
    CHECK(!scene_flame_canister_tick(&s,108,p,f,2,&d,1,0,1));
    CHECK(campaign_player_inventory.loaded[2]==37 && campaign_player_inventory.reserve[3]==250 && rf_scene_flame_canister[4]==1);
    puts("flame canister release, ammo, contact explosion, deselection and debounce passed");return 0;
}
