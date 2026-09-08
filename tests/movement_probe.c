#include "rf/movement.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    struct { rf_movement_settings state; rf_movement_config config;
             int32_t requested,forced; float scale; uint32_t override; } input;
    int32_t status;
    _Static_assert(sizeof(input)==56,"Movement settings wire layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(&input,sizeof(input),1,stdin)==1) {
        status=rf_movement_set_mode(&input.state,&input.config,input.requested,input.forced,input.scale,(uint8_t)input.override);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,sizeof(input.state),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
