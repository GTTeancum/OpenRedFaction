#include "rf/model.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    uint32_t count, i;
    rf_model_bone bones[256];
    uint8_t order[256];
    int32_t status;
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    while (fread(&count, 4, 1, stdin) == 1) {
        if (count > 256) return 2;
        for (i = 0; i < count; ++i) {
            if (fread(&bones[i].parent, 4, 1, stdin) != 1) return 2;
            order[i] = 0;
        }
        status = rf_model_bone_order(bones, count, order, 256);
        if (fwrite(&status, 4, 1, stdout) != 1 || fwrite(order, 1, count, stdout) != count) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
