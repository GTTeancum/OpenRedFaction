#include "rf/vpp.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
typedef struct scene_stream {int unused;} scene_stream;
static struct {float fraction,last_limit,damage;uint32_t world,rubble,world_calls,rubble_calls,hits;int32_t kind;} probe;
static int campaign_clutter_firearm_select(const float a[3],const float b[3],float limit,uint32_t *slot,float *fraction)
{(void)a;(void)b;probe.last_limit=limit;*slot=probe.fraction<limit?3:UINT32_MAX;*fraction=probe.fraction;return RF_OK;}
static int combat_shot_obstructed(scene_stream *s,const float a[3],const float b[3],float limit,uint32_t *blocked)
{(void)s;(void)a;(void)b;probe.last_limit=limit;++probe.world_calls;*blocked=probe.world;return RF_OK;}
static int combat_enemy_fragment_shot(scene_stream *s,const float a[3],const float b[3],float limit,float damage,uint32_t *blocked)
{(void)s;(void)a;(void)b;(void)damage;probe.last_limit=limit;++probe.rubble_calls;*blocked=probe.rubble;return RF_OK;}
static int campaign_clutter_firearm_damage(uint32_t slot,float damage,int32_t kind)
{if(slot!=3)return RF_RANGE;++probe.hits;probe.damage=damage;probe.kind=kind;return RF_OK;}
#include "../src/diagnostic/scene_clutter_enemy_fire.inc"
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line%d %s\n",__LINE__,#c);return 1;}}while(0)
int main(void)
{
    scene_stream stream={0};float start[3]={0},delta[3]={0,0,10};uint32_t consumed;
    probe.fraction=.5f;
    CHECK(!campaign_clutter_enemy_fire(&stream,start,delta,.4f,10,2,0,&consumed));
    CHECK(!consumed&&!probe.hits&&!probe.world_calls); /* Earlier actor/vehicle wins. */
    CHECK(!campaign_clutter_enemy_fire(&stream,start,delta,.5f,10,2,0,&consumed)&&!consumed); /* Tie preserved. */
    probe.world=1;
    CHECK(!campaign_clutter_enemy_fire(&stream,start,delta,1,10,2,0,&consumed));
    CHECK(consumed&&!probe.hits&&!probe.rubble_calls&&probe.last_limit==.5f);
    probe.world=0;probe.rubble=1;
    CHECK(!campaign_clutter_enemy_fire(&stream,start,delta,1,10,2,0,&consumed));
    CHECK(consumed&&!probe.hits&&probe.rubble_calls==1);
    probe.rubble=0;
    CHECK(!campaign_clutter_enemy_fire(&stream,start,delta,1,10,2,0,&consumed));
    CHECK(consumed&&probe.hits==1&&probe.damage==10&&probe.kind==2);
    probe.world=probe.rubble=1;
    CHECK(!campaign_clutter_enemy_fire(&stream,start,delta,1,10,2,1,&consumed));
    CHECK(consumed&&probe.hits==2&&probe.rubble_calls==2);
    puts("Enemy clutter nearest target, world/rubble cover and explicit penetration policy passed");return 0;
}
