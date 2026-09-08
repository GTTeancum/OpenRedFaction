#include "rf/model.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    uint32_t count; float q[16][4],p[16][3],w[16];
    struct { int32_t status; float matrix[12]; } output;
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(&count,4,1,stdin)==1) {
        if (count>16 || fread(q,16,count,stdin)!=count || fread(p,12,count,stdin)!=count || fread(w,4,count,stdin)!=count) return 2;
        output.status=rf_model_blend_pose(q,p,w,count,output.matrix);
        if (fwrite(&output,52,1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
