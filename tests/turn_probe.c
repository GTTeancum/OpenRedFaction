#include "rf/turn.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    struct { rf_motion_playback_state playback; rf_turn_effects effects; rf_turn_context context;
             float local_x; rf_motion_playback_resource resources[32]; int32_t actions[45],sounds[45]; } input;
    int32_t status,sound; unsigned i;
    _Static_assert(sizeof(input)==1864,"Turn effects wire layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(&input,sizeof(input),1,stdin)==1) {
        sound=-99;
        status=rf_turn_apply_selected(&input.effects,&input.playback,input.resources,32,input.actions,input.sounds,
                                      &input.context,input.local_x,&sound);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.playback,260,1,stdout)!=1 ||
            fwrite(&input.effects,44,1,stdout)!=1 || fwrite(&sound,4,1,stdout)!=1) return 1;
        for (i=0;i<32;++i) if (fwrite(&input.resources[i].references,4,1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
