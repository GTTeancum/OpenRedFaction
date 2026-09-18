/* Actual scene pool/launch and core flight; controlled no-contact sweep.
 * Does not claim installed-world cover, NPC death, or rendered tracer evidence. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"APC flight line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int apc_empty_sweep(void *context,const float *start,const float *delta,float radius,rf_weapon_flight_contact *hit,uint32_t *found)
{(void)context;(void)start;(void)delta;(void)radius;memset(hit,0,sizeof(*hit));*found=0;return RF_OK;}
int main(void)
{
    static scene_stream stream;rf_vpp tables={0};scene_apc_primary_launch_context launch={&stream,77};
    scene_vehicle_primary_definition definition;float pose[12]={1,0,0,0,1,0,0,0,1,10,20,30},direction[3]={0,0,1};
    uint32_t i,accepted,random;rf_weapon_flight_event event;scene_apc_primary_round *round;
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!scene_vehicle_primary_load(&tables,"APC Minigun",2*1024*1024,&definition));
    stream.apc_primary.random.value=1;
    for(i=0;i<8;i++)CHECK(!scene_apc_primary_launch(&launch,123,&definition,pose,direction,&accepted)&&accepted);
    CHECK(scene_apc_primary_pending(&stream));random=stream.apc_primary.random.value;
    CHECK(!scene_apc_primary_launch(&launch,123,&definition,pose,direction,&accepted)&&!accepted);
    CHECK(stream.apc_primary.random.value==random && rf_scene_apc_primary[1]==8 && rf_scene_apc_primary[5]==1);
    round=stream.apc_primary.rounds;
    CHECK(round->source==123 && round->driver==77 && round->kind==2 && round->damage==150);
    CHECK(!memcmp(round->flight.position,pose+9,12));
    for(i=0;i<30;i++)CHECK(!rf_weapon_flight_step(&round->flight,1.f/60,apc_empty_sweep,NULL,&event));
    CHECK(!round->flight.active && event.kind==2);
    CHECK(fabsf(sqrtf((round->flight.position[0]-10)*(round->flight.position[0]-10)+
        (round->flight.position[1]-20)*(round->flight.position[1]-20)+
        (round->flight.position[2]-30)*(round->flight.position[2]-30))-125)<.001f);
    CHECK(!scene_apc_primary_launch(&launch,123,&definition,pose,direction,&accepted)&&accepted);
    CHECK(round->flight.active && round->flight.remaining==.5);
    rf_vpp_close(&tables);puts("PASS APC real finite flight pool, source/driver retention, spread, 125m expiry and reuse");return 0;
}
