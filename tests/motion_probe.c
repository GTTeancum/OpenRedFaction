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
