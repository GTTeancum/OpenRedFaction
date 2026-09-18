#include <stdio.h>
#include "rf/flame_stream.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"flame line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int cover(void *context,const float *a,const float *b,uint32_t *blocked)
{(void)a;(void)b;*blocked=*(uint32_t *)context;return RF_OK;}
int main(void)
{
    rf_vpp tables={0};rf_weapon_primary_definition definition;rf_flame_cadence cadence={0};
    rf_damage_request request;uint32_t pulse,hit,blocked=0;float origin[3]={0},forward[3]={0,0,1},center[3]={.7f,0,4};
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_primary_load(&tables,"Flamethrower",128*1024,&definition));rf_vpp_close(&tables);
    CHECK(!rf_flame_pulse(&cadence,&definition,0,1,42,&pulse,&request) && pulse);
    CHECK(request.amount==20 && request.kind==definition.damage_kind && request.source==42);
    CHECK(!rf_flame_pulse(&cadence,&definition,1,1,42,&pulse,&request) && !pulse);
    CHECK(!rf_flame_pulse(&cadence,&definition,6,1,42,&pulse,&request) && pulse);
    CHECK(!rf_flame_pulse(&cadence,&definition,12,0,42,&pulse,&request) && !pulse);
    CHECK(!rf_flame_contact(origin,forward,5,1,center,.2f,cover,&blocked,&hit) && hit);
    blocked=1;CHECK(!rf_flame_contact(origin,forward,5,1,center,.2f,cover,&blocked,&hit) && !hit);
    blocked=0;center[0]=2;CHECK(!rf_flame_contact(origin,forward,5,1,center,.2f,cover,&blocked,&hit) && !hit);
    center[0]=0;center[2]=-1;CHECK(!rf_flame_contact(origin,forward,5,1,center,.2f,cover,&blocked,&hit) && !hit);
    puts("Flame authored pulse cadence and covered expanding volume admission");return 0;
}
