#include "rf/movement.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    struct { rf_movement_settings state; rf_movement_config config;
             int32_t requested,forced; float scale; uint32_t override; } input;
    int32_t status;
    _Static_assert(sizeof(input)==56,"Movement settings wire layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    if(argc==4 && !strcmp(argv[1],"--descriptor")) {
        rf_vpp archive;rf_movement_descriptor value;int result;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        result=rf_movement_descriptor_load(&archive,(uint32_t)strtoul(argv[3],NULL,10),65536,&value);
        rf_vpp_close(&archive);if(result)return 3;
        return fwrite(&value,sizeof(value),1,stdout)==1?0:1;
    }
    if(argc==2 && !strcmp(argv[1],"--transform")) {
        struct {uint32_t reference[3];float input[3],eye[9],body[9],parent[9];} v;float result[3];
        while(fread(&v,sizeof(v),1,stdin)==1) {
            if(rf_movement_transform(v.reference,v.input,v.eye,v.body,v.parent,result))return 3;
            if(fwrite(result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    while (fread(&input,sizeof(input),1,stdin)==1) {
        status=rf_movement_set_mode(&input.state,&input.config,input.requested,input.forced,input.scale,(uint8_t)input.override);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,sizeof(input.state),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
