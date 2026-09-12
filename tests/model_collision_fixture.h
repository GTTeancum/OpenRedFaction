#ifndef RF_MODEL_COLLISION_FIXTURE_H
#define RF_MODEL_COLLISION_FIXTURE_H
#include "rf/model_file.h"
#include <stdio.h>
#include <string.h>
/* Stream independently original-verified queries/answers; no host input. */
static void rf_model_collision_fixture(const char *plan_path,const char *archive_prefix,volatile uint32_t state[8])
{
    FILE *plan=fopen(plan_path,"rb");uint32_t header[2],model_index;int status=RF_OK;
    state[0]=0x52464354u;state[1]=1;state[2]=state[3]=state[4]=state[6]=state[7]=0;state[5]=2166136261u;
    if(!plan){state[7]=(uint32_t)RF_IO;state[1]=0x80000000u;return;}
    if(fread(header,8,1,plan)!=1 || header[0]!=0x52464354u || header[1]>1024){status=RF_FORMAT;goto done;}
    for(model_index=0;model_index<header[1];++model_index) {
        char archive_name[64],model_name[128],path[512];uint32_t count,i;rf_vpp archive={0};rf_model_file model;rf_model_collision_resource owner={0};
        if(fread(archive_name,64,1,plan)!=1 || fread(model_name,128,1,plan)!=1 || fread(&count,4,1,plan)!=1 ||
            !memchr(archive_name,0,64) || !memchr(model_name,0,128) || count>4096){status=RF_FORMAT;break;}
        if(snprintf(path,sizeof(path),"%s%s",archive_prefix,archive_name)<0){status=RF_FORMAT;break;}
        status=rf_vpp_open(&archive,path);if(status)break;
        status=rf_model_file_open(&model,&archive,model_name);
        if(!status)status=rf_model_collision_resource_open(&owner,&model,256*1024);
        if(owner.accounted_bytes>state[6])state[6]=owner.accounted_bytes;
        for(i=0;!status && i<count;++i) {
            struct {rf_collision_model_part_query query;rf_collision_model_response_hit hit;uint32_t reset;} input;
            struct {uint32_t accepted;rf_collision_model_part_query query;rf_collision_model_response_hit hit;} output,expected;
            uint32_t j;
            _Static_assert(sizeof(input)==140 && sizeof(output)==140,"native collision wire");
            if(fread(&input,140,1,plan)!=1 || fread(&expected,140,1,plan)!=1){status=RF_FORMAT;break;}
            output.accepted=rf_collision_model_trace(owner.parts,&owner.part_count,&input.query,&input.hit,input.reset);
            output.query=input.query;output.hit=input.hit;
            if(memcmp(&output,&expected,140)){status=RF_FORMAT;break;}
            for(j=0;j<140;++j)state[5]=(state[5]^((unsigned char *)&output)[j])*16777619u;
            ++state[3];state[4]+=output.accepted;
        }
        rf_model_collision_resource_close(&owner);rf_vpp_close(&archive);
        if(status)break;++state[2];
    }
 done:
    fclose(plan);state[7]=(uint32_t)status;state[1]=status?0x80000000u:2u;
}
#endif
