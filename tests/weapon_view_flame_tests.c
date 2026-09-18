#include <stdio.h>
#include <string.h>
#include "rf/entity_assets.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"flame view line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp tables={0};rf_weapon_view_definition view,saved;
    const char *loop_only="$Name: \"test\" $Flags: (\"continuous_fire\") $1st Person Mesh: \"a.v3d\" +State: \"idle\" \"idle.mvf\" +State: \"loop_fire\" \"loop.mvf\" +Action: \"alt_fire\" \"alt.mvf\"";
    const char *missing="$Name: \"test\" $1st Person Mesh: \"a.v3d\" +State: \"idle\" \"idle.mvf\" +State: \"loop_fire\" \"loop.mvf\"";
    const char *both="$Name: \"test\" $Flags: (\"continuous_fire\") $1st Person Mesh: \"a.v3d\" +State: \"idle\" \"idle.mvf\" +State: \"loop_fire\" \"loop.mvf\" +Action: \"fire\" \"fire.mvf\"";
    CHECK(!rf_weapon_view_read(loop_only,(uint32_t)strlen(loop_only),"test",&view));
    CHECK(!strcmp(view.clips[1],"loop.rfa") && !strcmp(view.clips[3],"alt.rfa") && !view.alt_loop);
    saved=view;CHECK(rf_weapon_view_read(missing,(uint32_t)strlen(missing),"test",&view)==RF_FORMAT);
    CHECK(!memcmp(&view,&saved,sizeof(view)));
    CHECK(!rf_weapon_view_read(both,(uint32_t)strlen(both),"test",&view));
    CHECK(!strcmp(view.clips[1],"fire.rfa") && !view.clips[3][0]);
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!rf_weapon_view_load(&tables,"Flamethrower",128*1024,&view));rf_vpp_close(&tables);
    CHECK(!strcmp(view.clips[1],"fp_flame_fire.rfa") && !strcmp(view.clips[2],"fp_flame_reload.rfa"));
    CHECK(!strcmp(view.clips[3],"fp_flame_altfire.rfa") && !view.alt_loop);
    puts("Flame primary loop binding preserves action and alternate semantics");return 0;
}
