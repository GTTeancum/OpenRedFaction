/* Actual installed APC weapon and first-LOD muzzle tag; callback captures
 * dispatch only. Real projectile/cover/damage integration remains parent-owned. */
#include <stdio.h>
#include "../src/diagnostic/scene_vehicle_primary.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle primary line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct shot_capture {uint32_t calls,accept,source;float pose[12],direction[3];} shot_capture;
static int shot(void *context,uint32_t source,const scene_vehicle_primary_definition *definition,const float pose[12],const float direction[3],uint32_t *accepted)
{
    shot_capture *c=context;(void)definition;++c->calls;c->source=source;
    memcpy(c->pose,pose,48);memcpy(c->direction,direction,12);*accepted=c->accept;return RF_OK;
}
int main(void)
{
    rf_vpp tables={0},meshes={0};rf_model_file model={0};rf_static_model_tags tags={0};
    scene_vehicle_primary_definition definition,jeep;scene_vehicle_primary_state state={0},saved;
    shot_capture capture={0};int32_t muzzle=-1,reserve=3;uint32_t fired,i;float expected[12];
    const float position[3]={30,10,-167},basis[9]={0,0,-1,0,1,0,1,0,0};
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_model_file_open(&model,&meshes,"APC.v3m"));CHECK(!rf_static_model_tags_open(&model,65536,&tags));
    CHECK(!scene_vehicle_primary_apc_open(&tables,&tags,2*1024*1024,&definition,&state,&muzzle));
    CHECK(muzzle==17 && definition.capacity==999 && definition.damage==150 && definition.damage_kind==2);
    CHECK(definition.fire_seconds==.08f && definition.start_seconds==0 && definition.speed==250 && definition.lifetime==.5f);
    CHECK(!rf_static_model_tag_place(&tags,muzzle,basis,position,expected));
    CHECK(!scene_vehicle_primary_tick(&state,&definition,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(!fired && reserve==3 && !state.shots && capture.calls==1);
    capture.accept=1;
    CHECK(!scene_vehicle_primary_tick(&state,&definition,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(fired && reserve==2 && capture.source==123 && !memcmp(capture.pose,expected,48));
    for(i=0;i<3;i++)CHECK(fabsf(capture.direction[i]-expected[6+i])<.0001f);
    for(i=0;i<60;i++)CHECK(!scene_vehicle_primary_tick(&state,&definition,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(reserve==0 && state.shots==3 && capture.calls==4);
    memset(&state,0,sizeof(state));reserve=999;capture.calls=0;
    for(i=0;i<60;i++)CHECK(!scene_vehicle_primary_tick(&state,&definition,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(state.shots==13 && reserve==986);
    saved=state;reserve=-1;
    CHECK(scene_vehicle_primary_tick(&state,&definition,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired)==RF_RANGE);
    CHECK(!memcmp(&state,&saved,sizeof(state)));
    CHECK(!scene_vehicle_primary_load(&tables,"Jeep Gun",2*1024*1024,&jeep));
    CHECK(jeep.damage==100 && jeep.fire_seconds==.12f && jeep.start_seconds==.1f && jeep.capacity==999);
    /* This exercises authored spin-up using the APC fixture tag only; it does
     * not claim a Jeep gun attachment or gunner has been integrated. */
    memset(&state,0,sizeof(state));reserve=2;capture.calls=0;
    for(i=0;i<5;i++)CHECK(!scene_vehicle_primary_tick(&state,&jeep,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(!capture.calls && reserve==2);
    CHECK(!scene_vehicle_primary_tick(&state,&jeep,&tags,muzzle,position,basis,NULL,123,1,1,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(fired && reserve==1);
    CHECK(!scene_vehicle_primary_tick(&state,&jeep,&tags,muzzle,position,basis,NULL,123,1,0,1.f/60,&reserve,shot,&capture,&fired));
    CHECK(!fired && !state.held && state.warmup==0);
    rf_static_model_tags_close(&tags);rf_vpp_close(&meshes);rf_vpp_close(&tables);
    puts("PASS installed APC finite clipless cadence, real tag transform, exhausted/pool-blocked debit, Jeep authored delay only");return 0;
}
