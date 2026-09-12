#ifndef RF_MODEL_SKIN_FIXTURE_H
#define RF_MODEL_SKIN_FIXTURE_H
#include "rf/model_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* Stream independently original-verified queries/answers; no host input. */
static void rf_model_skin_fixture(const char *plan_path,const char *archive_prefix,volatile uint32_t state[8])
{
    FILE *plan=fopen(plan_path,"rb");uint32_t header[2],model_index;int status=RF_OK;
    state[0]=0x52465354u;state[1]=1;state[2]=state[3]=state[4]=state[6]=state[7]=0;state[5]=2166136261u;
    if(!plan){state[7]=(uint32_t)RF_IO;state[1]=0x80000000u;return;}
    if(fread(header,8,1,plan)!=1 || header[0]!=0x52465354u || header[1]>1024){status=RF_FORMAT;goto done;}
    for(model_index=0;model_index<header[1];++model_index) {
        char archive_name[64],model_name[128],path[512];uint32_t count,i;rf_vpp archive={0};rf_model_file model;rf_model_skin_geometry owner={0};float matrices[256][12],(*scratch)[3]=NULL;uint32_t selected=UINT32_MAX;
        if(fread(archive_name,64,1,plan)!=1 || fread(model_name,128,1,plan)!=1 || fread(&count,4,1,plan)!=1 ||
            !memchr(archive_name,0,64) || !memchr(model_name,0,128) || count>4096){status=RF_FORMAT;break;}
        if(snprintf(path,sizeof(path),"%s%s",archive_prefix,archive_name)<0){status=RF_FORMAT;break;}
        status=rf_vpp_open(&archive,path);if(status)break;
        status=rf_model_file_open(&model,&archive,model_name);

        for(i=0;!status && i<count;++i) {
            struct {rf_collision_model_part_query query;rf_collision_model_response_hit hit;uint32_t reset,lod,mode;} input;
            struct {uint32_t accepted;rf_collision_model_part_query query;rf_collision_model_response_hit hit;uint32_t scratch_hash;} output,expected;
            uint32_t j;
            _Static_assert(sizeof(input)==148 && sizeof(output)==144,"native collision wire");
            if(fread(&input,148,1,plan)!=1 || fread(&expected,144,1,plan)!=1){status=RF_FORMAT;break;}
            if(input.mode>1){status=RF_FORMAT;break;}
            if(input.lod!=selected) {
                free(scratch);scratch=NULL;rf_model_skin_geometry_close(&owner);
                status=rf_model_skin_geometry_open(&owner,&model,input.lod,256,256*1024);if(status)break;
                scratch=owner.max_vertices?malloc((size_t)owner.max_vertices*12):NULL;
                if(owner.max_vertices && !scratch){status=RF_IO;break;}selected=input.lod;
                if(owner.accounted_bytes+(uint32_t)owner.max_vertices*12+sizeof(matrices)>state[6])
                    state[6]=owner.accounted_bytes+(uint32_t)owner.max_vertices*12+sizeof(matrices);
            }
            memset(matrices,0,sizeof(matrices));
            for(j=0;j<256;++j){matrices[j][0]=matrices[j][4]=matrices[j][8]=1;matrices[j][11]=input.mode?j*.03125f:0;}
            if(owner.max_vertices)memset(scratch,0xa5,(size_t)owner.max_vertices*12);
            output.accepted=rf_collision_model_pose_query(owner.batches,owner.batch_count,matrices,256,&input.query,&input.hit,scratch,input.reset);
            output.scratch_hash=2166136261u;
            for(j=0;j<(uint32_t)owner.max_vertices*12;++j)output.scratch_hash=(output.scratch_hash^((unsigned char *)scratch)[j])*16777619u;
            output.query=input.query;output.hit=input.hit;
            if(memcmp(&output,&expected,144)){status=RF_FORMAT;break;}
            for(j=0;j<144;++j)state[5]=(state[5]^((unsigned char *)&output)[j])*16777619u;
            ++state[3];state[4]+=output.accepted;
        }
        free(scratch);rf_model_skin_geometry_close(&owner);rf_vpp_close(&archive);
        if(status)break;++state[2];
    }
 done:
    fclose(plan);state[7]=(uint32_t)status;state[1]=status?0x80000000u:2u;
}
#endif
