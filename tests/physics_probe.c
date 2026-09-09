#include "rf/physics.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(void)
{
    float in[3];struct {rf_physics_fallback value;int32_t status;} out;
    _Static_assert(sizeof(out)==28,"Physics probe wire format");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(in,sizeof(in),1,stdin)==1) {
        memset(&out,0xa5,sizeof(out));
        out.status=rf_physics_fallback_prepare(in[0],in[1],in[2],&out.value);
        if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
    }
    return ferror(stdin)?1:0;
}
