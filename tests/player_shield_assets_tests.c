/* Installed-resource check; argv1 is the read-only Installed_Game directory.
 * Uses the ordinary1MiB per-view budget and reports actual retained/peak bytes.
 * Does not assert that table magazine32 represents ammunition or durability. */
#include "rf/player_weapon.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shield assets line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    const char *names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    rf_vpp tables={0},meshes={0},motions={0},maps[5]={{0}};rf_weapon_supply_catalog supply;
    rf_weapon_view_definition view;rf_weapon_primary_definition primary;rf_player_weapon *weapon=NULL;
    char path[1024];uint32_t i;int32_t id;int status;
    CHECK(argc==2);
    snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);CHECK(!rf_vpp_open(&tables,path));
    CHECK(!rf_weapon_supply_load(&tables,128*1024,&supply));id=rf_weapon_name_find(&supply.names,"riot shield");CHECK(id==13);
    CHECK(supply.definitions[id].ammo_type==-1 && supply.definitions[id].capacity==0 && supply.definitions[id].magazine==32);
    CHECK(!rf_weapon_view_load(&tables,"riot shield",128*1024,&view));
    printf("SHIELD_VIEW_FILES mesh=%s idle=%s fire=%s reload=%s alt=%s\n",view.mesh,view.clips[0],view.clips[1],view.clips[2],view.clips[3]);
    /* View loader resolves authored .vcm/.mvf names into the installed compiled
     * .v3c/.rfa payloads; it preserves stems, not source-format extensions. */
    CHECK(!strcmp(view.mesh,"fp_riotshield.v3c") && !strcmp(view.clips[0],"fp_riotshield_idle.rfa") &&
          !strcmp(view.clips[1],"fp_riotshield_attack.rfa") && !view.clips[2][0] && !strcmp(view.clips[3],"fp_riotshield_altfire.rfa"));
    CHECK(!rf_weapon_primary_load(&tables,"riot shield",128*1024,&primary));
    CHECK(primary.fire_seconds==.5f && primary.damage==10 && primary.magazine==32);
    snprintf(path,sizeof(path),"%s/meshes.vpp",argv[1]);CHECK(!rf_vpp_open(&meshes,path));
    snprintf(path,sizeof(path),"%s/motions.vpp",argv[1]);CHECK(!rf_vpp_open(&motions,path));
    for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);CHECK(!rf_vpp_open(maps+i,path));}
    status=rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,1024*1024,&weapon);
    printf("SHIELD_VIEW_LOAD status%d budget1048576\n",status);CHECK(!status && weapon);
    printf("SHIELD_VIEW_BYTES resident%u peak%u bones%u clips%u\n",weapon->resident_bytes,weapon->peak_bytes,weapon->bone_count,weapon->clip_count);
    CHECK(!rf_player_weapon_step(weapon,0,0));CHECK(!rf_player_weapon_step(weapon,1,1.f/60));CHECK(!rf_player_weapon_step(weapon,3,1.f/60));
    rf_player_weapon_close(&weapon);rf_vpp_close(&tables);rf_vpp_close(&meshes);rf_vpp_close(&motions);
    for(i=0;i<5;i++)rf_vpp_close(maps+i);
    puts("installed shield supply/view/action loading passed");return 0;
}
