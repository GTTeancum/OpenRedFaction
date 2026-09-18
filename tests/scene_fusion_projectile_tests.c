#include <stdio.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_fusion_projectile.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fusion line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int empty_sweep(void *c,const float *p,const float *d,float r,rf_weapon_flight_contact *h,uint32_t *hit)
{(void)c;(void)p;(void)d;(void)r;(void)h;*hit=0;return RF_OK;}
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_weapon_explosive_definition definition;rf_weapon_primary_definition primary;
    float position[3]={1,2,3},direction[3]={0,0,2};uint32_t i,spawned;rf_weapon_flight_event event;
    CHECK(argc==2 && !rf_vpp_open(&tables,argv[1]));
    CHECK(!rf_weapon_explosive_load(&tables,"shoulder_cannon",128*1024,&definition));
    CHECK(!rf_weapon_primary_load(&tables,"shoulder_cannon",128*1024,&primary));rf_vpp_close(&tables);
    CHECK(definition.speed==20 && definition.lifetime==15 && definition.collision_radius==.051f);
    CHECK(definition.damage_radius==25 && definition.crater_radius==7 && definition.impact_radius[0]==9 && primary.damage==1000);
    scene_fusion_projectile_reset();CHECK(!scene_fusion_launch(position,direction,123,primary.damage,&definition,&spawned) && spawned);
    CHECK(scene_fusion_projectile_pending() && scene_fusion_projectiles[0].source==123 && scene_fusion_projectiles[0].flight.velocity[2]==20);
    CHECK(scene_fusion_projectiles[0].basis[0]==1 && scene_fusion_projectiles[0].basis[4]==1 && scene_fusion_projectiles[0].basis[8]==1);
    CHECK(!rf_weapon_flight_step(&scene_fusion_projectiles[0].flight,.1f,empty_sweep,NULL,&event));
    CHECK(!event.kind && fabsf(scene_fusion_projectiles[0].flight.position[2]-5)<.0001f);
    for(i=1;i<SCENE_FUSION_CAPACITY;i++)CHECK(!scene_fusion_launch(position,direction,123,primary.damage,&definition,&spawned) && spawned);
    CHECK(!scene_fusion_launch(position,direction,123,primary.damage,&definition,&spawned) && !spawned && rf_scene_fusion_projectiles[4]==1);
    scene_fusion_projectile_reset();CHECK(!scene_fusion_projectile_pending());
    puts("Fusion installed flight/blast metadata, real travel, source and bounded admission passed");return 0;
}
