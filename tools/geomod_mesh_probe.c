#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static rf_geomod_vertex vertices[4096];
static rf_geomod_face faces[768];
static float positions[4096][3];
static rf_collision_face bound[768];
static rf_collision_face_filter filters[768];
int main(int argc,char **argv)
{
    FILE *file;char magic[4];uint32_t counts[2],i,failures=0;rf_geomod_mesh_view mesh;int status;
    if(argc!=2 || !(file=fopen(argv[1],"rb")))return 2;
    if(fread(magic,1,4,file)!=4 || memcmp(magic,"RGM1",4) || fread(counts,4,2,file)!=2 ||
       !counts[0] || counts[0]>4096 || !counts[1] || counts[1]>768 ||
       fread(vertices,sizeof(*vertices),counts[0],file)!=counts[0] ||
       fread(faces,sizeof(*faces),counts[1],file)!=counts[1] || fgetc(file)!=EOF){fclose(file);return 2;}
    fclose(file);mesh=(rf_geomod_mesh_view){vertices,faces,counts[0],counts[1],0};
    status=rf_geomod_collision_faces(&mesh,filters,positions,4096,bound,768);
    if(status)for(i=0;i<counts[1];i++) {
        rf_geomod_mesh_view one={vertices,faces+i,counts[0],1,0};
        int face_status=rf_geomod_collision_faces(&one,filters,positions,4096,bound,768);
        if(face_status){printf("INVALID_FACE %u %d %u\n",i,face_status,faces[i].count);++failures;}
    }
    printf("MESH_STATUS %d VERTICES %u FACES %u INVALID %u\n",status,counts[0],counts[1],failures);
    return status?1:0;
}
