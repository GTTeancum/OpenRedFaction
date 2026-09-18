#include "rf/player_weapon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/diagnostic/scene_weapon_custom_actions.inc"
#include "../src/diagnostic/scene_undercover_mode.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"undercover mode line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
 rf_vpp tables={0},meshes={0},motions={0},maps[4]={{0}};rf_weapon_view_definition definition;
 rf_player_weapon *w=NULL;scene_undercover_resources *o=NULL;uint32_t i,tick,started,changed;char path[128];float pose[12];
 CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
 CHECK(!rf_vpp_open(&motions,"Installed_Game/motions.vpp"));
 for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
 CHECK(!rf_weapon_view_load(&tables,"Undercover 12mm handgun",128*1024,&definition));
 CHECK(!rf_player_weapon_open_view(&meshes,&motions,maps,4,&definition,2*1024*1024,&w));
 CHECK(!rf_player_weapon_step(w,0,0));
 CHECK(!scene_undercover_resources_open(w,&meshes,&motions,maps,4,0,512*1024,&o));
 CHECK(o->render.part_count && o->materials.count);
 printf("UNDERCOVER_PROP resident=%u peak=%u geometry=%u materials=%u motion=%u parent=%s\n",o->resident_bytes,o->peak_bytes,o->render.allocated_bytes,o->materials.resident_bytes,o->actions->resident_bytes,w->bones[o->mode.attachment.parent].name);
 CHECK(!scene_undercover_mode_visible(&o->mode));CHECK(scene_undercover_mode_can_fire(&o->mode));
 CHECK(!strcmp(scene_undercover_mode_launch(&o->mode),"Glock Launch"));
 for(i=0;i<2;i++) {
  CHECK(!scene_undercover_mode_toggle(&o->mode,o->actions,w,&started) && started);
  CHECK(scene_undercover_mode_visible(&o->mode) && !scene_undercover_mode_can_fire(&o->mode));
  CHECK(!strcmp(scene_undercover_mode_transition_sound(&o->mode),i?"Silencer Off":"Silencer On"));
  CHECK(!scene_undercover_mode_toggle(&o->mode,o->actions,w,&started) && !started);
  changed=0;
  for(tick=0;tick<360 && !changed;tick++) {
   CHECK(!rf_player_weapon_step(w,-1,1.f/60));CHECK(!scene_undercover_mode_pose(&o->mode,w,pose));
   {uint32_t j;for(j=0;j<12;j++)CHECK(isfinite(pose[j]));}
   CHECK(!scene_undercover_mode_poll(&o->mode,o->actions,w,&changed));
  }
  CHECK(changed && !o->mode.pending && o->mode.attached==(i?0u:1u));
  CHECK(scene_undercover_mode_visible(&o->mode)==o->mode.attached);
  CHECK(scene_undercover_mode_can_fire(&o->mode) && !w->payloads[3]);
  CHECK(!strcmp(scene_undercover_mode_launch(&o->mode),i?"Glock Launch":"Silencer Launch"));
  printf("UNDERCOVER_TRANSITION attach=%u frames=%u\n",!i,tick);
 }
 CHECK(!scene_undercover_mode_toggle(&o->mode,o->actions,w,&started) && started);
 CHECK(!scene_undercover_mode_cancel(&o->mode,o->actions,w));CHECK(!o->mode.pending && !o->mode.attached && !w->payloads[3]);
 CHECK(!scene_undercover_resources_close(&o,w));CHECK(!o);rf_player_weapon_close(&w);
 rf_vpp_close(&tables);rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<4;i++)rf_vpp_close(maps+i);return 0;
}
