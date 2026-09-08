#include "rf/model.h"
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(void)
{
    struct { uint32_t counts[3]; char names[3][16][32]; char query[32]; } input;
    struct { int32_t status, index; } output;
    rf_model_name names[3][16];
    rf_model_name_group groups[3];
    uint32_t g, n;
    _Static_assert(sizeof(input) == 1580, "Probe wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    while (fread(&input, sizeof(input), 1, stdin) == 1) {
        for (g = 0; g < 3; ++g) {
            if (input.counts[g] > 16) return 2;
            groups[g].names = names[g]; groups[g].count = input.counts[g];
            for (n = 0; n < input.counts[g]; ++n) {
                if (!memchr(input.names[g][n], 0, 32)) return 2;
                names[g][n].data = input.names[g][n];
                names[g][n].length = strlen(input.names[g][n]);
            }
        }
        if (!memchr(input.query, 0, 32)) return 2;
        {
            rf_model_name query = {input.query, strlen(input.query)};
            output.index = -1;
            output.status = rf_model_find_tag(groups, query, &output.index);
        }
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
