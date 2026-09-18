/* Installed APC secondary data and real gravity-flight/contact semantics.
 * Sweep provider is controlled; live blast/GeoMod belong to scene acceptance. */
#include <stdio.h>
#include "../src/diagnostic/scene_apc_secondary_state.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"APC secondary line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int secondary_sweep(void *context,const float *start,const float *delta,float radius,rf_weapon_flight_contact *hit,uint32_t *found)
{
    uint32_t i;(void)radius;*found=*(uint32_t*)context;memset(hit,0,sizeof(*hit));
    if(*found){hit->hit.fraction=.5f;hit->hit.normal[1]=1;hit->object=UINT32_MAX;
        for(i=0;i<3;i++)hit->hit.point[i]=start[i]+delta[i]*.5f;}
    return RF_OK;
}
int main(void)
{
    rf_vpp tables={0},meshes={0};rf_model_file model={0};rf_static_model_tags tags={0};scene_apc_secondary_state state;
    scene_apc_secondary_round round={0};rf_weapon_flight_event event;uint32_t i,hit=0;
    const float position[3]={0,10,0},direction[3]={0,0,1},gravity[3]={0,-9.8f,0};
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_model_file_open(&model,&meshes,"APC.v3m"));CHECK(!rf_static_model_tags_open(&model,65536,&tags));
    CHECK(!scene_apc_secondary_open(&tables,&tags,2*1024*1024,&state));
    CHECK(state.muzzle==16 && state.reserve==0 && state.definition.capacity==15);
    CHECK(state.definition.damage==500 && state.definition.blast==10 && state.definition.crater==8);
    CHECK(state.definition.speed==30 && state.definition.lifetime==5 && state.definition.fire_seconds==1.5f && state.definition.radius==.15f);
    CHECK(!rf_weapon_flight_launch(&round.flight,position,direction,state.definition.speed,state.definition.lifetime,state.definition.radius));
    for(i=0;i<60;i++)CHECK(!scene_apc_secondary_step(&round,1.f/60,gravity,secondary_sweep,&hit,&event));
    CHECK(round.flight.active && !event.kind && fabsf(round.flight.position[2]-30)<.001f);
    CHECK(fabsf(round.flight.position[1]-(10-9.8f*61/120))<.001f);
    CHECK(fabsf(round.flight.velocity[1]+9.8f)<.001f && fabs(round.flight.remaining-4)<.001);
    hit=1;CHECK(!scene_apc_secondary_step(&round,1.f/60,gravity,secondary_sweep,&hit,&event));
    CHECK(event.kind==1 && !round.flight.active && event.contact.hit.fraction==.5f);
    CHECK(!scene_apc_secondary_step(&round,1.f/60,gravity,secondary_sweep,&hit,&event) && !event.kind);
    rf_static_model_tags_close(&tags);rf_vpp_close(&meshes);rf_vpp_close(&tables);
    puts("PASS authored APC15-shell secondary, 30m/s gravity arc, immediate impact terminal and no repeat");return 0;
}
