#include "../src/diagnostic/scene_submarine_homing.inc"
#include <stdio.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1;}}while(0)
typedef struct los_state {uint32_t blocked,calls;int status;} los_state;
typedef struct reader_state {const scene_submarine_homing_candidate *items;uint32_t calls;int status;} reader_state;
static int read_candidate(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{
    reader_state *reader=context;++reader->calls;
    if(reader->status)return reader->status;
    *out=reader->items[index];return RF_OK;
}
static int visible(void *context,const float start[3],const float end[3],uint32_t target,uint32_t source,uint32_t driver,uint32_t *clear)
{
    los_state *s=context;(void)start;(void)end;
    if(source!=10||driver!=11)return RF_FORMAT;
    ++s->calls;if(s->status)return s->status;*clear=target!=s->blocked;return RF_OK;
}
int main(void)
{
    scene_submarine_weapon_definition definition={0};scene_submarine_weapon_round round={0},initial,saved;
    scene_submarine_homing_candidate candidates[]={
        {10,1,1,1,{0,0,5}}, {11,1,1,1,{0,0,5}}, {12,0,1,1,{0,0,5}},
        {13,1,0,1,{0,0,5}}, {14,1,1,0,{0,0,5}}, {15,1,1,1,{0,0,31}},
        {16,1,1,1,{10,0,0}}, {21,1,1,1,{5,0,10}}, {20,1,1,1,{5,0,10}}};
    los_state los={UINT32_MAX,0,0};reader_state reader={candidates,0,0};uint32_t target=123;float speed,angle;
    definition.turn_seconds=12;definition.view_degrees=110;definition.scan_range=30;definition.wakeup=.1f;definition.lifetime=10;
    round.source=10;round.driver=11;round.flight.active=1;round.flight.remaining=10;round.flight.velocity[2]=7;initial=round;
    CHECK(!scene_submarine_homing_step(&round,&definition,.05f,candidates,9,visible,&los,&target));
    CHECK(target==UINT32_MAX&&!los.calls&&!memcmp(&round,&initial,sizeof(round)));
    round.flight.remaining=9;initial=round;
    CHECK(!scene_submarine_homing_step(&round,&definition,.05f,candidates,9,visible,&los,&target));
    CHECK(target==20&&los.calls==2);
    speed=sqrtf(round.flight.velocity[0]*round.flight.velocity[0]+round.flight.velocity[2]*round.flight.velocity[2]);
    angle=atan2f(round.flight.velocity[0],round.flight.velocity[2]);
    CHECK(fabsf(speed-7)<1e-5f&&fabsf(angle-6.283185307179586f*.05f/12)<1e-6f);
    CHECK(!memcmp(round.flight.position,initial.flight.position,12)&&round.flight.remaining==initial.flight.remaining);
    round=initial;los.blocked=20;
    CHECK(!scene_submarine_homing_step(&round,&definition,.05f,candidates,9,visible,&los,&target)&&target==21);
    round=initial;saved=round;los.status=RF_RANGE;target=123;
    CHECK(scene_submarine_homing_step(&round,&definition,.05f,candidates,9,visible,&los,&target)==RF_RANGE);
    CHECK(target==123&&!memcmp(&round,&saved,sizeof(round)));
    los.status=0;los.blocked=UINT32_MAX;round=initial;
    CHECK(!scene_submarine_homing_scan(&round,&definition,.05f,read_candidate,&reader,9,visible,&los,&target));
    CHECK(target==20&&reader.calls==9);
    saved=round;reader.status=RF_RANGE;target=123;
    CHECK(scene_submarine_homing_scan(&round,&definition,.05f,read_candidate,&reader,9,visible,&los,&target)==RF_RANGE);
    CHECK(target==123&&!memcmp(&round,&saved,sizeof(round)));
    puts("submarine guidance admission, turn bound, speed and LOS passed");return 0;
}
