#include "rf/model.h"
#include "rf/burn.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
int main(int argc, char **argv)
{
    unsigned char *data;
    rf_model_bone *bones;
    uint32_t count = 0, i;
    long bytes;
    FILE *file;
    int status;
    if ((argc != 2 && argc != 3) || !(file = fopen(argv[1], "rb"))) return 2;
    if (fseek(file, 0, SEEK_END) || (bytes = ftell(file)) < 0 || bytes > 1048576 || fseek(file, 0, SEEK_SET)) return 2;
    data = malloc((size_t)bytes + 1);
    bones = calloc(16384, sizeof(*bones));
    if (!data || !bones || fread(data, 1, (size_t)bytes, file) != (size_t)bytes) return 2;
    fclose(file);
    status = rf_model_decode_bones(data, (size_t)bytes, bones, 16384, &count);
    if (status) { free(data); free(bones); return 3; }
    _setmode(_fileno(stdout), _O_BINARY);
    if(argc==3 && !strcmp(argv[2],"--burn-bones")) {
        rf_model_name *names=calloc(count?count:1,sizeof(*names));int32_t result[5]={0,-9,-9,-9,-9};
        if(!names){free(data);free(bones);return 3;}
        for(i=0;i<count;++i){names[i].data=bones[i].name;names[i].length=strlen(bones[i].name);}
        result[0]=rf_burn_resolve_bones(names,count,result+1);
        int written=fwrite(result,sizeof(result),1,stdout)==1;free(names);free(data);free(bones);return written?0:2;
    }
    for (i = 0; i < count; ++i) {
        if (fwrite(bones[i].name, 1, 24, stdout) != 24 ||
            fwrite(bones[i].rotation, 4, 4, stdout) != 4 ||
            fwrite(bones[i].position, 4, 3, stdout) != 3 ||
            fwrite(&bones[i].parent, 4, 1, stdout) != 1) return 2;
    }
    free(data); free(bones);
    return 0;
}
