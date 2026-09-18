/* Installed remaining hitscan view owners: authored camera/action fixtures,
 * bounded residency reports, and evaluated playback using real meshes/motions.
 * Camera fields are table facts below, not output of the current view parser. */
#include "rf/player_weapon.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"remaining view line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct view_case {
    const char *name,*mesh,*idle,*fire,*reload;
    float fov,camera[3];uint32_t continuous,alternate_loop;
} view_case;
static const view_case cases[]={
 {"Machine Pistol","fp_mp.v3c","fp_mp_idle.rfa","fp_mp_fireslow.rfa","fp_mp_reloadtop.rfa",40,{.294f,-.08f,-.056f},1,0},
 {"heavy_machine_gun","fp_hmac.v3c","fp_hmac_idle.rfa","fp_hmac_fire.rfa","fp_hmac_reload.rfa",65,{-.168f,0,-.085f},1,1},
 {"scope_assault_rifle","fp_ass2.v3c","fp_ass2_idle.rfa","fp_ass2_fire.rfa","fp_ass2_reload.rfa",85,{.130f,.080f,.150f},0,0},
 {"Undercover 12mm handgun","fp_ugun.v3c","fp_glock_idle.rfa","fp_glock_fire.rfa","fp_glock_reload.rfa",65,{.110f,.140f,.342f},0,0}
};
int main(void)
{
 rf_vpp tables={0},meshes={0},motions={0},maps[5]={{0}};
 const char *map_names[]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
 uint32_t i,k,action,tick;char path[128];
 CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
 CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
 CHECK(!rf_vpp_open(&motions,"Installed_Game/motions.vpp"));
 for(i=0;i<5;i++){snprintf(path,sizeof(path),"Installed_Game/%s",map_names[i]);CHECK(!rf_vpp_open(maps+i,path));}
 for(k=0;k<sizeof(cases)/sizeof(cases[0]);k++) {
  const view_case *c=cases+k;rf_weapon_view_definition view={0};rf_weapon_primary_definition primary;
  rf_player_weapon *w=NULL;int status=rf_weapon_view_load(&tables,c->name,128*1024,&view);
  printf("VIEW_PARSER name=\"%s\" status=%d\n",c->name,status);
  CHECK(!status); /* Dual-continuous HMG must use the actual parser. */
  CHECK(!strcmp(view.mesh,c->mesh) && !strcmp(view.clips[0],c->idle));
  CHECK(!strcmp(view.clips[1],c->fire) && !strcmp(view.clips[2],c->reload));
  CHECK(c->alternate_loop?(!strcmp(view.clips[3],c->fire) && view.alt_loop):!view.clips[3][0]);
  CHECK(!rf_weapon_primary_load(&tables,c->name,128*1024,&primary));
  CHECK(!rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,2*1024*1024,&w));
  w->resources[1].looping=c->continuous;
  CHECK(!rf_player_weapon_step(w,0,1.f/60));
  for(action=1;action<4;action++) {
   if(!view.clips[action][0]) {
    uint32_t previous=w->current;CHECK(rf_player_weapon_step(w,(int32_t)action,1.f/60)==RF_RANGE);CHECK(w->current==previous);continue;
   }
   CHECK(!rf_player_weapon_step(w,(int32_t)action,1.f/60));CHECK(w->current==action);
   if(w->resources[action].looping) {
    for(tick=0;tick<120;tick++)CHECK(!rf_player_weapon_step(w,-1,1.f/60));
    CHECK(w->current==action);CHECK(!rf_player_weapon_step(w,0,1.f/60));
   } else {
    for(tick=0;tick<360 && w->current;tick++)CHECK(!rf_player_weapon_step(w,-1,1.f/60));
    CHECK(!w->current);
   }
  }
  printf("VIEW_OWNER name=\"%s\" resident=%u peak=%u bones=%u vertices=%u materials=%u clips=%u budget64k=%u\n",
   c->name,w->resident_bytes,w->peak_bytes,w->bone_count,w->geometry.vertex_count,w->materials.count,w->clip_count,(w->peak_bytes+65535u)&~65535u);
  printf("VIEW_CAMERA name=\"%s\" fov=%g camera=%g,%g,%g fire=%g alt=%g reload=%g magazine=%u\n",
   c->name,c->fov,c->camera[0],c->camera[1],c->camera[2],primary.fire_seconds,primary.alt_fire_seconds,primary.reload_seconds,primary.magazine);
  rf_player_weapon_close(&w);CHECK(!w);
 }
 rf_vpp_close(&tables);rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<5;i++)rf_vpp_close(maps+i);
 return 0;
}
