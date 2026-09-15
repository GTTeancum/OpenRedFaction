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
static int valid_piece(const rf_geomod_vertex *v,uint32_t n,const double normal[3])
{
    rf_geomod_face f={0,n,0,UINT32_MAX};rf_geomod_mesh_view mesh={v,&f,n,1,0};
    int status=rf_geomod_collision_faces(&mesh,filters,positions,PROBE_VERTICES,bound,PROBE_FACES);
    if(status)return 0;
    return !normal || bound[0].plane[0]*normal[0]+bound[0].plane[1]*normal[1]+bound[0].plane[2]*normal[2]>0;
}
static int emit_piece(const rf_geomod_vertex *v,uint32_t n,const rf_geomod_face *source)
{
    if(n>PROBE_VERTICES-output_nv || output_nf==PROBE_FACES)return 0;
    memcpy(output_vertices+output_nv,v,n*sizeof(*v));
    output_faces[output_nf++]=(rf_geomod_face){output_nv,n,source->material,source->source_face};output_nv+=n;return 1;
}
static int partition(const rf_geomod_vertex *v,const rf_geomod_face *source)
{
    rf_geomod_vertex part[64],center={0};double normal[3]={0},sum[5]={0};uint32_t n=source->count,i,j,k,c,na,nb;
    if(n<3 || n>64)return 0;
    if(valid_piece(v,n,NULL))return emit_piece(v,n,source);
    for(i=0;i<n;i++)for(k=0;k<3;k++)normal[k]+=(double)v[i].position[(k+1)%3]*v[(i+1)%n].position[(k+2)%3]-(double)v[i].position[(k+2)%3]*v[(i+1)%n].position[(k+1)%3];
    for(i=0;i<n;i++)for(j=i+2;j<n;j++) {
        if(i==0 && j==n-1)continue;
        /* Do not turn a nearly collinear boundary chain into a thin extra
         * face: closure must distinguish the new diagonal from the boundary. */
        {
            double mid[3];uint32_t e;int separated=1;
            for(k=0;k<3;k++)mid[k]=((double)v[i].position[k]+v[j].position[k])*.5;
            for(e=0;e<n && separated;e++) {
                double d[3],length=0,t=0,error=0;
                for(k=0;k<3;k++){d[k]=(double)v[(e+1)%n].position[k]-v[e].position[k];length+=d[k]*d[k];t+=(mid[k]-v[e].position[k])*d[k];}
                if(length==0){separated=0;break;}t/=length;if(t<0)t=0;if(t>1)t=1;
                for(k=0;k<3;k++){double q=mid[k]-v[e].position[k]-t*d[k];error+=q*q;}
                if(error<=1e-12)separated=0;
            }
            if(separated) {
                double d[3],length=0;
                for(k=0;k<3;k++){d[k]=(double)v[j].position[k]-v[i].position[k];length+=d[k]*d[k];}
                if(length==0)separated=0;
                for(e=0;e<n && separated;e++)if(e!=i && e!=j) {
                    double t=0,error=0;
                    for(k=0;k<3;k++)t+=((double)v[e].position[k]-v[i].position[k])*d[k];
                    t/=length;if(t<=1e-8 || t>=1-1e-8)continue;
                    for(k=0;k<3;k++){double q=(double)v[e].position[k]-v[i].position[k]-t*d[k];error+=q*q;}
                    if(error<=1e-12)separated=0;
                }
            }
            if(!separated)continue;
        }
        na=j-i+1;nb=n-na+2;
        for(c=0;c<na;c++)part[c]=v[i+c];
        if(!valid_piece(part,na,normal))continue;
        for(c=0;c<nb;c++)part[c]=v[(j+c)%n];
        if(!valid_piece(part,nb,normal))continue;
        if(!emit_piece(part,nb,source))return 0;
        for(c=0;c<na;c++)part[c]=v[i+c];
        return emit_piece(part,na,source);
    }
    for(i=0;i<n;i++){for(k=0;k<3;k++)sum[k]+=v[i].position[k];for(k=0;k<2;k++)sum[k+3]+=v[i].uv[k];}
    for(k=0;k<3;k++)center.position[k]=(float)(sum[k]/n);
    for(k=0;k<2;k++)center.uv[k]=(float)(sum[k+3]/n);
    for(i=0;i<n;i++) {
        part[0]=center;part[1]=v[i];part[2]=v[(i+1)%n];
        if(!valid_piece(part,3,normal) || !emit_piece(part,3,source))return 0;
    }
    return 1;
}
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
        for(i=0;i<counts[1];i++) {
            if(faces[i].first>counts[0] || faces[i].count>counts[0]-faces[i].first || !partition(vertices+faces[i].first,faces+i)) {
                fprintf(stderr,"PARTITION_FAILED face%u\n",i);return 1;
            }
        }
        mesh=(rf_geomod_mesh_view){output_vertices,output_faces,output_nv,output_nf,0};
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
