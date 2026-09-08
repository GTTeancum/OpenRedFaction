#include "rf/model.h"
#include <fcntl.h>
#include <io.h>
int main(void)
{
    float input[24]; struct { int32_t status; float matrix[12]; } output;
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(input,sizeof(input),1,stdin)==1) {
        output.status=rf_model_place_tag(input,input+12,input+21,output.matrix);
        if (fwrite(&output,52,1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
