#include "rf/motion.h"
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
int main(void)
{
    uint32_t count;
    int32_t tick;
    rf_motion_position_key keys[512];
    struct { int32_t status; float value[3]; } output;
    _Static_assert(sizeof(rf_motion_position_key) == 40, "Key wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    while (fread(&count, 4, 1, stdin) == 1) {
        if (count > 512 || fread(&tick, 4, 1, stdin) != 1 || fread(keys, 40, count, stdin) != count) return 2;
        output.value[0] = output.value[1] = output.value[2] = 0;
        output.status = rf_motion_sample_position(keys, count, tick, output.value);
        if (fwrite(&output, 16, 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
