#include "rf/turn.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
static int count_reset(void *user) { ++*(uint32_t *)user; return RF_OK; }
struct prepare_observation { int32_t *deadline,observed; uint32_t count; int status; };
static int observe_prepare(void *user) {
    struct prepare_observation *o=user; ++o->count; o->observed=*o->deadline; return o->status;
}
int main(int argc, char **argv)
{
    struct { rf_motion_playback_state playback; rf_turn_effects effects; rf_turn_context context;
             float local_x; rf_motion_playback_resource resources[32]; int32_t actions[45],sounds[45]; } input;
    int32_t status,sound; unsigned i;
    int update=argc==2 && (!strcmp(argv[1],"--update") || !strcmp(argv[1],"--update-no-reset"));
    int choose=argc==2 && !strcmp(argv[1],"--candidates");
    _Static_assert(sizeof(input)==1864,"Turn effects wire layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    if (argc==2 && !strcmp(argv[1],"--prepare")) {
        struct { int32_t pending; rf_turn_context context; rf_turn_actor actor;
            uint32_t count,flags[64]; int32_t deadline,callback_status; uint32_t available; } p;
        _Static_assert(sizeof(p)==440,"Prepare wire layout");
        while (fread(&p,sizeof(p),1,stdin)==1) {
            struct prepare_observation observation={&p.deadline,-99,0,p.callback_status};
            int32_t result[4];
            result[0]=rf_locomotion_prepare(&p.deadline,p.pending,&p.context,&p.actor,p.flags,p.count,
                p.available ? observe_prepare : NULL,&observation);
            result[1]=p.deadline; result[2]=(int32_t)observation.count; result[3]=observation.observed;
            if (fwrite(result,sizeof(result),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
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
        rf_locomotion_candidates candidates={0};
        sound=-99;
        if (update || choose) {
            rf_turn_actor actor;
            if (fread(&actor,sizeof(actor),1,stdin)!=1) return 2;
            if (choose) {
                rf_locomotion_candidate_input selection; int32_t motions[23];
                if (fread(&selection,sizeof(selection),1,stdin)!=1 || fread(motions,sizeof(motions),1,stdin)!=1) return 2;
                status=rf_locomotion_choose_candidates(&candidates,&selection,motions,&input.effects,&input.playback,input.resources,32,
                    input.actions,input.sounds,&input.context,&actor,count_reset,&resets,&sound);
            } else status=rf_turn_update(&input.effects,&input.playback,input.resources,32,input.actions,input.sounds,
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
        if (choose && (fwrite(&resets,4,1,stdout)!=1 || fwrite(&candidates,sizeof(candidates),1,stdout)!=1)) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
