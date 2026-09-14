#include "rf/player_weapon.h"
#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"weapon line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp meshes={0},motions={0},maps[5]={{0}};rf_player_weapon *w=NULL,*other=NULL;
    const char *map_names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    char path[1024];uint32_t i,j;rf_motion_sample sample;
    {
        const char *valid="$Name: \"pistol\" $Flags: (\"semi_automatic\") $Clip Size: 16 8 $Clip Reload Time: 1.1 $Fire Wait: .5 $Damage: 40 $Damage Type: \"bullet\" $Damage Multi: 25 #End";
        const char *duplicate="$Name: \"pistol\" $Clip Size: 16 8 $Clip Size: 8 8";
        const char *bad="$Name: \"pistol\" $Clip Size: -1 8";
        rf_weapon_primary_definition d={0},saved;
        CHECK(rf_weapon_primary_read(valid,(uint32_t)strlen(valid),"pistol",&d)==RF_OK);
        CHECK(d.damage_kind==1 && d.magazine==16 && d.semi_automatic==1 && d.damage==40 && d.fire_seconds==.5f && d.reload_seconds==1.1f);saved=d;
        CHECK(rf_weapon_primary_read(duplicate,(uint32_t)strlen(duplicate),"pistol",&d)==RF_FORMAT && !memcmp(&d,&saved,sizeof(d)));
        CHECK(rf_weapon_primary_read(bad,(uint32_t)strlen(bad),"pistol",&d)==RF_RANGE && !memcmp(&d,&saved,sizeof(d)));
        CHECK(rf_weapon_primary_read(valid,(uint32_t)strlen(valid),"missing",&d)==RF_NOT_FOUND && !memcmp(&d,&saved,sizeof(d)));
    }
    {
        rf_weapon_inventory inv={0},saved;rf_weapon_acquire_definition d={0,125,16};uint32_t moved=999;
        inv.owned[3]=1;inv.loaded[3]=10;inv.reserve[0]=3;
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==3 && inv.loaded[3]==13 && inv.reserve[0]==0);
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==0 && inv.loaded[3]==13);
        inv.reserve[0]=10;CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==3 && inv.loaded[3]==16 && inv.reserve[0]==7);
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==0 && inv.reserve[0]==7);
        inv.loaded[3]=17;saved=inv;moved=999;
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_RANGE && moved==999 && !memcmp(&saved,&inv,sizeof(inv)));
        inv.loaded[3]=0;inv.reserve[0]=-1;saved=inv;
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_RANGE && !memcmp(&saved,&inv,sizeof(inv)));
    }
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/meshes.vpp",argv[1]);CHECK(rf_vpp_open(&meshes,path)==RF_OK);
    snprintf(path,sizeof(path),"%s/motions.vpp",argv[1]);CHECK(rf_vpp_open(&motions,path)==RF_OK);
    for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],map_names[i]);CHECK(rf_vpp_open(maps+i,path)==RF_OK);}
    CHECK(rf_player_weapon_open(&meshes,&motions,maps,5,1024*1024,&w)==RF_OK);
    {
        rf_vpp tables={0};rf_weapon_primary_definition d;
        snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);CHECK(rf_vpp_open(&tables,path)==RF_OK);
        CHECK(rf_weapon_primary_load(&tables,"12mm handgun",128*1024,&d)==RF_OK);rf_vpp_close(&tables);
        CHECK(d.damage_kind==1 && d.magazine==16 && d.semi_automatic==1 && d.damage==40 && d.fire_seconds==.5f && d.reload_seconds==1.1f);
        printf("Primary definition PASS magazine=%u semi=%u damage=%g reload=%g fire=%g\n",d.magazine,d.semi_automatic,d.damage,d.reload_seconds,d.fire_seconds);
    }
    CHECK(w->bone_count && w->geometry.vertex_count && w->materials.count && w->peak_bytes<=1024*1024);
    CHECK(rf_player_weapon_open(&meshes,&motions,maps,5,w->resident_bytes-1,&other)==RF_RANGE && !other);
    rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<5;i++)rf_vpp_close(maps+i);
    for(i=0;i<3;i++)for(j=0;j<w->bone_count;j++) {
        rf_motion_track t;CHECK(rf_motion_file_track(w->clips+i,j,&t)==RF_OK);
        CHECK(rf_motion_file_sample(w->clips+i,j,t.envelope.start_tick,1,&sample)==RF_OK);
        CHECK(rf_motion_file_sample(w->clips+i,j,t.envelope.end_tick,1,&sample)==RF_OK);
    }
    {
        float idle[50][12],fire[50][12];uint32_t ticks;
        CHECK(rf_player_weapon_step(w,0,0)==RF_OK);memcpy(idle,w->pose,sizeof(idle));
        CHECK(rf_player_weapon_step(w,1,.08f)==RF_OK && w->current==1);memcpy(fire,w->pose,sizeof(fire));
        CHECK(memcmp(idle,fire,w->bone_count*48));
        CHECK(rf_player_weapon_step(w,2,.2f)==RF_OK && w->current==2);
        CHECK(memcmp(fire,w->pose,w->bone_count*48));
        for(ticks=0;ticks<600 && w->current!=0;ticks++)CHECK(rf_player_weapon_step(w,-1,1.0f/60)==RF_OK);
        CHECK(w->current==0 && ticks<600);
        for(i=0;i<50;i++){CHECK(rf_player_weapon_step(w,1,0)==RF_OK);CHECK(rf_player_weapon_step(w,2,0)==RF_OK);}
        CHECK(w->resources[0].references==0 && w->resources[1].references==0 && w->resources[2].references==1);
        CHECK(rf_player_weapon_step(w,3,0)==RF_RANGE);
        printf("Playback PASS reload-return=%u frames repeated-actions=100\n",ticks);
    }
    printf("PASS bones=%u vertices=%u materials=%u resident=%u peak=%u\n",w->bone_count,w->geometry.vertex_count,w->materials.count,w->resident_bytes,w->peak_bytes);
    rf_player_weapon_close(&w);rf_player_weapon_close(&w);CHECK(!w);return 0;
}
