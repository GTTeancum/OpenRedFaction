#include "rf/turn.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
static int count_reset(void *user) { ++*(uint32_t *)user; return RF_OK; }
int main(int argc, char **argv)
{
    struct { rf_motion_playback_state playback; rf_turn_effects effects; rf_turn_context context;
             float local_x; rf_motion_playback_resource resources[32]; int32_t actions[45],sounds[45]; } input;
    int32_t status,sound; unsigned i;
    int update=argc==2 && (!strcmp(argv[1],"--update") || !strcmp(argv[1],"--update-no-reset"));
    _Static_assert(sizeof(input)==1864,"Turn effects wire layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    if (argc==2 && !strcmp(argv[1],"--direction")) {
        rf_turn_direction_input direction;
        struct { int32_t status; rf_turn_direction_result result; } output;
        while (fread(&direction,sizeof(direction),1,stdin)==1) {
            memset(&output,0,sizeof(output));
            output.status=rf_turn_direction(&direction,&output.result);
            if (fwrite(&output,sizeof(output),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    while (fread(&input,sizeof(input),1,stdin)==1) {
        uint32_t resets=0;
        sound=-99;
        if (update) {
            rf_turn_actor actor;
            if (fread(&actor,sizeof(actor),1,stdin)!=1) return 2;
            status=rf_turn_update(&input.effects,&input.playback,input.resources,32,input.actions,input.sounds,
                                  &input.context,&actor,!strcmp(argv[1],"--update-no-reset") ? NULL : count_reset,&resets,&sound);
        } else if (argc==2 && !strcmp(argv[1],"--finish")) {
            rf_turn_finish_input finish;
            finish.local_x=input.local_x;
            if (fread(&finish.eligible,20,1,stdin)!=1) return 2;
            status=rf_turn_finish_candidates(&input.effects,&input.playback,input.resources,32,input.actions,input.sounds,
                                             &input.context,&finish,&sound);
        } else status=rf_turn_apply_selected(&input.effects,&input.playback,input.resources,32,input.actions,input.sounds,
                                              &input.context,input.local_x,&sound);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.playback,260,1,stdout)!=1 ||
            fwrite(&input.effects,44,1,stdout)!=1 || fwrite(&sound,4,1,stdout)!=1) return 1;
        for (i=0;i<32;++i) if (fwrite(&input.resources[i].references,4,1,stdout)!=1) return 1;
        if (update && fwrite(&resets,4,1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
