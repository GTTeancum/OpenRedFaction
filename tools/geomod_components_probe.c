#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Read-only RGM1 geometry adapter; snapshots do not carry eligibility flags. */
int main(int argc,char **argv)
{
    FILE *file=NULL;uint32_t header[3],words,count,largest,i,*work=NULL,*labels=NULL,*sizes=NULL;
    rf_geomod_vertex *vertices=NULL;rf_geomod_face *faces=NULL;rf_geomod_mesh_view mesh={0};int status=1;
    if(argc!=2){fprintf(stderr,"usage: geomod_components_probe physical.mesh\n");return 1;}
    file=fopen(argv[1],"rb");if(!file)goto done;
    if(fread(header,4,3,file)!=3 || memcmp(header,"RGM1",4) || !header[1] || !header[2] || header[1]>1000000 || header[2]>1000000)goto done;
    vertices=malloc(header[1]*sizeof(*vertices));faces=malloc(header[2]*sizeof(*faces));labels=malloc(header[2]*4);sizes=calloc(header[2],4);
    if(!vertices || !faces || !labels || !sizes)goto done;
    if(fread(vertices,sizeof(*vertices),header[1],file)!=header[1] || fread(faces,sizeof(*faces),header[2],file)!=header[2] || fgetc(file)!=EOF)goto done;
    mesh=(rf_geomod_mesh_view){vertices,faces,header[1],header[2],0};
    if(rf_geomod_component_work_size(&mesh,&words))goto done;
    work=malloc((size_t)words*4);if(!work)goto done;
    if(rf_geomod_mesh_components(&mesh,NULL,work,words,labels,&count,&largest))goto done;
    for(i=0;i<mesh.face_count;i++)if(labels[i]!=UINT32_MAX)++sizes[labels[i]];
    printf("{\"vertices\":%u,\"faces\":%u,\"scratch_bytes\":%u,\"components\":%u,\"largest\":%u,\"face_counts\":[",mesh.vertex_count,mesh.face_count,words*4,count,largest);
    for(i=0;i<count;i++)printf("%s%u",i?",":"",sizes[i]);puts("]}");status=0;
done:
    if(file)fclose(file);free(vertices);free(faces);free(labels);free(sizes);free(work);
    if(status)fprintf(stderr,"Invalid mesh or component analysis failure\n");return status;
}
