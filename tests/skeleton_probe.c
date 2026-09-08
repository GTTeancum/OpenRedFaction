#include "rf/model.h"
#include "rf/model_file.h"
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    rf_vpp meshes,motions; rf_model_file model; rf_motion_file motion;
    rf_model_bone bones[256]; float matrices[256][12]; uint32_t count=0,i; int32_t tick;
    rf_model_attachment eye; int found=0;
    if (argc!=5) return 1;
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    if (rf_vpp_open(&meshes,argv[1])!=RF_OK || rf_vpp_open(&motions,argv[2])!=RF_OK) return 2;
    if (rf_model_file_open(&model,&meshes,argv[3])!=RF_OK || rf_motion_file_open(&motion,&motions,argv[4])!=RF_OK) return 3;
    for (i=0;i<model.section_count;++i) if (model.sections[i].type==0x424f4e45) {
        void *payload=malloc(model.sections[i].size); int status;
        if (!payload) return 4;
        status=rf_vpp_read(&meshes,&model.entry,model.sections[i].offset,payload,model.sections[i].size);
        if (status==RF_OK) status=rf_model_decode_bones(payload,model.sections[i].size,bones,256,&count);
        free(payload); if (status!=RF_OK) return 5;
    }
    if (!count) return 6;
    for (i=0;i<model.lods[0].attachment_count;++i) {
        if (rf_model_file_attachment(&model,0,i,&eye)!=RF_OK) return 6;
        if (!strcmp(eye.name,"eye")) { found=1; break; }
    }
    if (!found || eye.parent<0 || (uint32_t)eye.parent>=count) return 6;
    while (fread(&tick,4,1,stdin)==1) {
        float local[12], evaluated[12];
        if (rf_model_sample_single_motion(bones,count,&motion,tick,0,matrices,256)!=RF_OK) return 7;
        if (rf_model_attachment_transform(eye.rotation,eye.position,local)!=RF_OK ||
            rf_model_compose_transform(local,matrices[eye.parent],evaluated)!=RF_OK) return 7;
        if (fwrite(matrices,48,count,stdout)!=count) return 8;
        if (fwrite(evaluated,48,1,stdout)!=1) return 8;
    }
    rf_vpp_close(&meshes); rf_vpp_close(&motions); return ferror(stdin) ? 9 : 0;
}
