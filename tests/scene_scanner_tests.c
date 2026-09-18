#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_scanner_select.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scene scanner line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static campaign_npc_body actors[34];scene_stream scene={0};rf_weapon_scanner_result out,before;uint32_t i,k;
    scene.npc_view.perspective=1;scene.npc_view.rotation[0]=scene.npc_view.rotation[4]=scene.npc_view.rotation[8]=1;
    scene.npc_view.screen[0]=320;scene.npc_view.screen[1]=-320;scene.npc_view.screen[2]=320;scene.npc_view.screen[3]=240;
    campaign_npc_bodies=actors;campaign_npc_body_count=34;rf_object_registry_init(&campaign_registry);
    for(i=0;i<34;i++){
        actors[i].registration.view=&actors[i].view;actors[i].body.allocated_bytes=1;actors[i].damage.effects.health=100;
        for(k=0;k<3;k++){float c=k==2?(float)(34-i):0;actors[i].body.state.bounds.minimum[k]=c-.5f;actors[i].body.state.bounds.maximum[k]=c+.5f;}
        CHECK(!rf_object_registry_insert(&campaign_registry,&actors[i].registration,&actors[i].registration.handle));
    }
    /* No collision world is present: markers must not depend on LOS. */
    CHECK(!scene_scanner_collect(&scene,&out));CHECK(out.count==32 && out.truncated);
    CHECK(out.markers[0].index==33 && out.markers[31].index==2 && out.markers[0].screen[0]==320);
    actors[33].object_flags=2;actors[32].view.flags_810=1;actors[31].damage.effects.health=0;
    CHECK(!rf_object_registry_remove(&campaign_registry,actors[30].registration.handle));actors[30].body.state.bounds.minimum[0]=NAN;
    CHECK(!scene_scanner_collect(&scene,&out));CHECK(out.count==30 && !out.truncated && out.markers[0].index==29);
    before=out;actors[29].body.state.bounds.minimum[0]=NAN;
    CHECK(scene_scanner_collect(&scene,&out)==RF_RANGE && !memcmp(&before,&out,sizeof(out)));
    campaign_npc_body_count=0;CHECK(!scene_scanner_collect(&scene,&out) && !out.count);
    puts("scene scanner merges nearest32, filters hidden/dead/stale, and preserves result on invalid live bounds");return 0;
}
