#include "rf/animation_check.h"
#include <fcntl.h>
#include <io.h>
int main(int argc, char **argv)
{
    uint32_t output[8]; int status;
    if (argc!=3) return 1;
    _setmode(_fileno(stdout),_O_BINARY);
    status=rf_animation_check(argv[1],argv[2],output);
    if (fwrite(output,sizeof(output),1,stdout)!=1) return 2;
    return status==RF_OK ? 0 : 3;
}
