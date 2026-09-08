#include "rf/model.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    float input[24];
    struct { int32_t status; float transform[12]; } output;
    _Static_assert(sizeof(output) == 52, "Probe wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    while (fread(input, sizeof(input), 1, stdin) == 1) {
        unsigned int i;
        for (i = 0; i < 12; ++i) output.transform[i] = 0;
        output.status = rf_model_compose_transform(input, input + 12, output.transform);
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
