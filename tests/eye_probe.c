#include "rf/eye.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    rf_eye_input input;
    struct { int32_t status; float position[3]; } output;
    _Static_assert(sizeof(input) == 96, "Probe wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    while (fread(&input, sizeof(input), 1, stdin) == 1) {
        output.position[0] = output.position[1] = output.position[2] = 0;
        output.status = rf_eye_position(&input, output.position);
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
