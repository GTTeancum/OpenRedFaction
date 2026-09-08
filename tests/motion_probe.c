#include "rf/motion.h"
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc, char **argv)
{
    uint32_t count;
    int32_t tick;
    rf_motion_position_key keys[512];
    struct { int32_t status; float value[3]; } output;
    _Static_assert(sizeof(rf_motion_position_key) == 40, "Key wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    if (argc == 2 && strcmp(argv[1], "--movement") == 0) {
        struct { rf_motion_controller controller; int32_t motions[23]; rf_motion_movement movement; } input;
        int32_t status;
        _Static_assert(sizeof(input)==148,"Movement selector wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            status=rf_motion_select_movement(&input.controller,input.motions,&input.movement);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.controller,24,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--request-state") == 0) {
        struct { rf_motion_controller controller; int32_t motions[23],requested; float duration; } input;
        int32_t status,matched;
        _Static_assert(sizeof(input)==124,"State request wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            matched=rf_motion_has_state(&input.controller,input.requested);
            status=rf_motion_request_state(&input.controller,input.motions,input.requested,input.duration);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&matched,4,1,stdout)!=1 ||
                fwrite(&input.controller,24,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--controller") == 0) {
        struct { rf_motion_playback_state state; rf_motion_playback_resource resources[32];
                 rf_motion_controller controller; int32_t motions[23]; float elapsed; } input;
        int32_t status; unsigned i;
        _Static_assert(sizeof(input)==1532,"Controller wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            status=rf_motion_apply_controller(&input.controller,input.motions,input.elapsed,&input.state,input.resources,32);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,260,1,stdout)!=1 ||
                fwrite(&input.controller,24,1,stdout)!=1) return 1;
            for (i=0;i<32;++i) if (fwrite(&input.resources[i].references,4,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--control") == 0) {
        struct { rf_motion_playback_state state; rf_motion_playback_resource resources[32];
                 int32_t motion; float weight; int32_t restart,freeze; } input;
        int32_t status; unsigned i;
        _Static_assert(sizeof(input)==1428,"Control wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            if (input.restart==2) status=rf_motion_stop_looping(&input.state,input.resources,32);
            else if (input.restart==3) status=rf_motion_stop_nonlooping(&input.state,input.resources,32);
            else if (input.restart==4) status=rf_motion_stop_slot(&input.state,input.motion);
            else status=input.restart ? rf_motion_start(&input.state,input.resources,32,input.motion,input.weight,input.freeze) :
                                   rf_motion_set_weight(&input.state,input.resources,32,input.motion,input.weight);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,260,1,stdout)!=1) return 1;
            for (i=0;i<32;++i) if (fwrite(&input.resources[i].references,4,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--update") == 0) {
        rf_motion_playback_state state;
        rf_motion_playback_resource resources[32];
        uint32_t frames,i,j; float elapsed; int32_t status;
        _Static_assert(sizeof(state)==260,"Playback state wire layout");
        _Static_assert(sizeof(resources[0])==36,"Playback resource wire layout");
        while (fread(&state,sizeof(state),1,stdin)==1) {
            if (fread(resources,sizeof(resources),1,stdin)!=1 || fread(&frames,4,1,stdin)!=1) return 2;
            for (i=0;i<frames;++i) {
                if (fread(&elapsed,4,1,stdin)!=1) return 2;
                status=rf_motion_update(&state,resources,32,elapsed);
                if (fwrite(&status,4,1,stdout)!=1 || fwrite(&state,sizeof(state),1,stdout)!=1) return 1;
                for (j=0;j<32;++j) if (fwrite(&resources[j].references,4,1,stdout)!=1) return 1;
            }
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--advance-candidate") == 0) {
        struct { rf_motion_completion_state state; uint32_t index; int32_t delta;
                 rf_motion_weight_envelope candidate,primary; int32_t candidate_bypass,primary_bypass; } input;
        int32_t status;
        _Static_assert(sizeof(input)==304,"Candidate wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            status=rf_motion_advance_candidate(&input.state,input.index,input.delta,&input.candidate,input.candidate_bypass,&input.primary,input.primary_bypass);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,248,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--complete-slots") == 0) {
        struct { rf_motion_completion_state state; int32_t ends[16]; uint32_t looping; } input;
        int32_t status;
        _Static_assert(sizeof(rf_motion_completion_state)==248,"Completion wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            status=rf_motion_complete_slots(&input.state,input.ends,input.looping);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,sizeof(input.state),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--remove-slot") == 0) {
        struct { rf_motion_slot_state state; int32_t motion,references; } input;
        int32_t status;
        _Static_assert(sizeof(rf_motion_slot_state)==208,"Slot state wire layout");
        while (fread(&input,sizeof(input),1,stdin)==1) {
            status=rf_motion_remove_slot(&input.state,input.motion,&input.references);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input,sizeof(input),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--map-loop") == 0) {
        struct { int32_t start,end; float phase; int32_t previous,markers[2],wrapped; } input;
        struct { int32_t status; rf_motion_loop_result result; } output;
        while (fread(&input,28,1,stdin)==1) {
            memset(&output,0,sizeof(output));
            output.status=rf_motion_map_loop(input.start,input.end,input.phase,input.previous,input.markers,input.wrapped,&output.result);
            if (fwrite(&output,12,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--advance-phase") == 0) {
        struct { uint32_t count; float phase; int32_t delta; } input;
        rf_motion_phase_slot slots[16];
        struct { int32_t status; rf_motion_phase_result result; } output;
        while (fread(&input,12,1,stdin)==1) {
            if (input.count>16 || fread(slots,12,input.count,stdin)!=input.count) return 2;
            memset(&output,0,sizeof(output));
            output.status=rf_motion_advance_phase(slots,input.count,input.phase,input.delta,&output.result);
            if (fwrite(&output,16,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--elapsed-ticks") == 0) {
        float input;
        struct { int32_t status, ticks; } output;
        while (fread(&input,4,1,stdin)==1) {
            output.ticks=0; output.status=rf_motion_elapsed_ticks(input,&output.ticks);
            if (fwrite(&output,8,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--sample-weight") == 0) {
        struct { rf_motion_weight_envelope envelope; int32_t tick, bypass; } input;
        struct { int32_t status; float value; } sampled;
        size_t n;
        _Static_assert(sizeof(input)==28,"Envelope wire layout");
        while ((n=fread(&input,1,sizeof(input),stdin))!=0) {
            if (n!=sizeof(input)) return 2;
            sampled.value=0;
            sampled.status=rf_motion_sample_weight(&input.envelope,input.tick,input.bypass,&sampled.value);
            if (fwrite(&sampled,sizeof(sampled),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--sample-rotation") == 0) {
        static rf_motion_rotation_key rotations[32767]; /* PC oracle transport only. */
        struct { int32_t status; float value[4]; } sampled;
        _Static_assert(sizeof(rf_motion_rotation_key)==16,"Rotation key wire layout");
        while (fread(&count,4,1,stdin)==1) {
            if (count>32767 || fread(&tick,4,1,stdin)!=1 || fread(rotations,16,count,stdin)!=count) return 2;
            memset(&sampled,0,sizeof(sampled));
            sampled.status=rf_motion_sample_rotation(rotations,count,tick,sampled.value);
            if (fwrite(&sampled,20,1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--interpolate-rotation") == 0) {
        struct { int16_t a[4], b[4]; float t; } input;
        struct { int32_t status; int16_t value[4]; } interpolated;
        size_t n;
        while ((n = fread(&input, 1, 20, stdin)) != 0) {
            if (n != 20) return 2;
            memset(&interpolated,0,sizeof(interpolated));
            interpolated.status = rf_motion_interpolate_rotation(input.a,input.b,input.t,interpolated.value);
            if (fwrite(&interpolated, 12, 1, stdout) != 1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc == 2 && strcmp(argv[1], "--decode-rotation") == 0) {
        unsigned char packed[8];
        struct { int32_t status; float value[4]; } decoded;
        size_t n;
        while ((n = fread(packed, 1, 8, stdin)) != 0) {
            if (n != 8) return 2;
            decoded.status = rf_motion_decode_rotation(packed, 8, decoded.value);
            if (fwrite(&decoded, 20, 1, stdout) != 1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    while (fread(&count, 4, 1, stdin) == 1) {
        if (count > 512 || fread(&tick, 4, 1, stdin) != 1 || fread(keys, 40, count, stdin) != count) return 2;
        output.value[0] = output.value[1] = output.value[2] = 0;
        output.status = rf_motion_sample_position(keys, count, tick, output.value);
        if (fwrite(&output, 16, 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
