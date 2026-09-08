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
