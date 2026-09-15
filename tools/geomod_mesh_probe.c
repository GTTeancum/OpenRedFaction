#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Host-only measurement headroom; these are not runtime owner limits. */
#define PROBE_VERTICES 8192
#define PROBE_FACES 1536
static rf_geomod_vertex vertices[PROBE_VERTICES];
static rf_geomod_face faces[PROBE_FACES];
static float positions[PROBE_VERTICES][3];
static rf_collision_face bound[PROBE_FACES];
static rf_collision_face_filter filters[PROBE_FACES];
static rf_geomod_vertex output_vertices[PROBE_VERTICES];
static rf_geomod_face output_faces[PROBE_FACES];
static uint32_t output_nv,output_nf;
int main(int argc,char **argv)
{
    FILE *file;char magic[4];uint32_t counts[2],i,failures=0;rf_geomod_mesh_view mesh;int status;
    if((argc!=2 && argc!=3) || !(file=fopen(argv[1],"rb")))return 2;
    if(fread(magic,1,4,file)!=4 || memcmp(magic,"RGM1",4) || fread(counts,4,2,file)!=2 ||
       !counts[0] || counts[0]>PROBE_VERTICES || !counts[1] || counts[1]>PROBE_FACES ||
       fread(vertices,sizeof(*vertices),counts[0],file)!=counts[0] ||
       fread(faces,sizeof(*faces),counts[1],file)!=counts[1] || fgetc(file)!=EOF){fclose(file);return 2;}
    fclose(file);mesh=(rf_geomod_mesh_view){vertices,faces,counts[0],counts[1],0};
    status=rf_geomod_collision_faces(&mesh,filters,positions,PROBE_VERTICES,bound,PROBE_FACES);
    if(status)for(i=0;i<counts[1];i++) {
        rf_geomod_mesh_view one={vertices,faces+i,counts[0],1,0};
        int face_status=rf_geomod_collision_faces(&one,filters,positions,PROBE_VERTICES,bound,PROBE_FACES);
        if(face_status){printf("INVALID_FACE %u %d %u\n",i,face_status,faces[i].count);++failures;}
    }
    printf("MESH_STATUS %d VERTICES %u FACES %u INVALID %u\n",status,counts[0],counts[1],failures);
    if(argc==3) {
        rf_geomod_mesh_view result;
        status=rf_geomod_partition_mesh(&mesh,output_vertices,PROBE_VERTICES,output_faces,PROBE_FACES,&result);
        if(status){fprintf(stderr,"PARTITION_FAILED status%d\n",status);return 1;}
        mesh=result;output_nv=mesh.vertex_count;output_nf=mesh.face_count;
        status=rf_geomod_collision_faces(&mesh,filters,positions,PROBE_VERTICES,bound,PROBE_FACES);if(status)return 1;
        counts[0]=output_nv;counts[1]=output_nf;file=fopen(argv[2],"wb");if(!file)return 2;
        if(fwrite("RGM1",1,4,file)!=4 || fwrite(counts,4,2,file)!=2 ||
           fwrite(output_vertices,sizeof(*output_vertices),output_nv,file)!=output_nv ||
           fwrite(output_faces,sizeof(*output_faces),output_nf,file)!=output_nf){fclose(file);return 2;}
        if(fclose(file))return 2;
        printf("PARTITION_OK vertices%u faces%u\n",output_nv,output_nf);
        printf("RUNTIME_CAPACITY_FIT %u vertex_limit4096 face_limit768\n",output_nv<=4096 && output_nf<=768);return 0;
    }
    return status?1:0;
}
