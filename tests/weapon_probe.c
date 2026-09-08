#include "rf/weapon.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
int main(void)
{
    struct { int32_t weapon; rf_weapon_reset_state state; rf_weapon_descriptor descriptors[64];
        rf_weapon_reset_context context; rf_motion_playback_state playback;
        rf_motion_playback_resource resources[32]; } input;
    int32_t status; unsigned i;
    _Static_assert(sizeof(input)==2308,"Weapon reset wire layout");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while (fread(&input,sizeof(input),1,stdin)==1) {
        status=rf_weapon_reset(&input.state,input.weapon,input.descriptors,&input.context,
            &input.playback,input.resources,32,NULL,NULL);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,sizeof(input.state),1,stdout)!=1 ||
            fwrite(&input.playback,sizeof(input.playback),1,stdout)!=1) return 1;
        for (i=0;i<32;++i) if (fwrite(&input.resources[i].references,4,1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
