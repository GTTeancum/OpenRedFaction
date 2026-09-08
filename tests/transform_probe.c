#include "rf/model.h"
#include <fcntl.h>
#include <io.h>
int main(int argc, char **argv)
{
    float input[7];
    struct { int32_t status; float transform[12]; } output;
    _Static_assert(sizeof(output) == 52, "Probe wire layout");
    (void)argv;
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    while (fread(input, sizeof(input), 1, stdin) == 1) {
        unsigned int i;
        for (i = 0; i < 12; ++i) output.transform[i] = 0;
        output.status = argc > 1 ? rf_model_attachment_transform(input, input + 4, output.transform)
                                 : rf_model_bone_transform(input, input + 4, output.transform);
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
