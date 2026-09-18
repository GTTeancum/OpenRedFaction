#include "../src/diagnostic/scene_fighter_weapon.inc"
#include <stdio.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line%d: %s\n",__LINE__,#c);return 1;}}while(0)
static int shot(void *context,uint32_t source,const scene_vehicle_primary_definition *d,const float pose[12],const float direction[3],uint32_t *accepted)
{(void)source;(void)d;(void)pose;(void)direction;++*(uint32_t*)context;*accepted=1;return RF_OK;}
static int clear_sweep(void *context,const float start[3],const float delta[3],float radius,uint32_t flags,
    rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{(void)context;(void)start;(void)delta;(void)radius;(void)flags;(void)out;*liquid=*matched=0;return RF_OK;}
static int candidate(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{(void)context;(void)index;memset(out,0,sizeof(*out));out->handle=99;out->alive=out->hostile=1;out->position[0]=5;out->position[2]=10;return RF_OK;}
static int los(void *context,const float a[3],const float b[3],uint32_t target,uint32_t source,uint32_t driver,uint32_t *clear)
{(void)context;(void)a;(void)b;(void)target;(void)source;(void)driver;*clear=1;return RF_OK;}
int main(void)
{
    rf_vpp tables={0},meshes={0};rf_model_file *model=malloc(sizeof(*model));rf_static_model_tags tags={0};
    scene_fighter_weapon_state state;uint32_t fired,calls=0,i,target;rf_weapon_flight_liquid_event event;
    float position[3]={0},basis[9]={1,0,0,0,1,0,0,0,1},aim[3]={0,0,1};
    CHECK(model);CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    /* Actual installed Fighter chassis launch tags. */
    CHECK(!rf_model_file_open(model,&meshes,"Fighter01.v3m"));CHECK(!rf_static_model_tags_open(model,65536,&tags));free(model);
    CHECK(!scene_fighter_weapon_open(&tables,&tags,"muzzle_1","secondary_1",2*1024*1024,&state));
    CHECK(state.primary.capacity==900&&state.primary.damage==100&&state.primary.damage_kind==2);
    CHECK(state.primary.fire_seconds==.05f&&state.primary.start_seconds==.1f&&state.primary.speed==275);
    CHECK(state.rocket.capacity==20&&state.rocket.damage==200&&state.rocket.fire_seconds==3&&state.rocket.speed==25);
    CHECK(state.rocket.lifetime==5&&state.rocket.blast==15&&state.rocket.crater==8);
    CHECK(state.rocket.turn_seconds==8&&state.rocket.view_degrees==110&&state.rocket.scan_range==30&&state.rocket.wakeup==.25f);
    state.primary_reserve=900;state.rocket_reserve=20;
    for(i=0;i<5;i++)CHECK(!scene_fighter_weapon_fire(&state,&tags,position,basis,aim,1,2,0,1,1,1.f/60,shot,&calls,&fired));
    CHECK(calls==0&&state.primary_reserve==900);
    CHECK(!scene_fighter_weapon_fire(&state,&tags,position,basis,aim,1,2,0,1,1,1.f/60,shot,&calls,&fired));
    CHECK(fired&&calls==1&&state.primary_reserve==899);
    CHECK(!scene_fighter_weapon_fire(&state,&tags,position,basis,aim,1,2,1,1,1,1.f/60,NULL,NULL,&fired));
    CHECK(fired&&state.rocket_reserve==19&&state.rockets[0].driver==2&&state.rockets[0].flight.velocity[2]==25);
    CHECK(!scene_fighter_weapon_fire(&state,&tags,position,basis,aim,1,2,1,1,1,1.f/60,NULL,NULL,&fired));
    CHECK(!fired&&state.rocket_reserve==19);
    memset(state.rockets[0].flight.position,0,12);state.rockets[0].flight.remaining=4;
    CHECK(!scene_fighter_rocket_step(state.rockets,&state.rocket,.05f,candidate,NULL,1,los,NULL,clear_sweep,NULL,&event,&target));
    CHECK(target==99&&state.rockets[0].flight.velocity[0]>0&&event.terminal.kind==0);
    CHECK(fabsf(state.rockets[0].flight.velocity[0]*state.rockets[0].flight.velocity[0]+
        state.rockets[0].flight.velocity[2]*state.rockets[0].flight.velocity[2]-625)<.001f);
    printf("Fighter minigun900/100AP/0.05/275 and Rocket20/200/3/25 model DrillMissile01.VFX admitted\n");
    rf_static_model_tags_close(&tags);rf_vpp_close(&meshes);rf_vpp_close(&tables);return 0;
}
