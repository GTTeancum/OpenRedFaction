#include "rf/editor_brush.h"
#include "rf/geomod_solid_clip.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level={0};
    rf_geometry geometry={0};
    rf_editor_brush owned[2]={{0}};
    static const uint32_t uids[2]={8755,7778},counts[2]={44,488};
    uint32_t i,j,k;int status;
    if(argc!=2)return 2;
    status=rf_vpp_open(&archive,argv[1]);if(status)return 3;
    status=rf_level_open(&level,&archive,"L1S1.rfl");if(status)return 4;
    status=rf_geometry_open(&geometry,&level,8*1024*1024);if(status)return 13;
    for(i=0;i<2;i++) {
        rf_editor_brush brush={0},kept={0};
        status=rf_editor_brush_open(&level,uids[i],512*1024,&brush);
        if(status){fprintf(stderr,"brush %u read status %d\n",uids[i],status);return 5;}
        uint32_t source4395=0;float lo[3]={INFINITY,INFINITY,INFINITY};
        float hi[3]={-INFINITY,-INFINITY,-INFINITY};
        if(brush.uid!=uids[i] || brush.operation!=2 || brush.face_count!=counts[i] ||
           !brush.corner_count || brush.resident_bytes>512*1024)return 6;
        for(j=0;j<brush.face_count;j++) {
            const rf_collision_face *f=brush.faces+j;
            source4395+=brush.source_words[j]==4395;
            if(f->count<3 || f->count>64 || !brush.source_words[j])return 7;
            for(k=0;k<f->count;k++) {
                double d=f->plane[3];uint32_t axis;
                for(axis=0;axis<3;axis++){
                    float v=f->vertices[k][axis];
                    d+=(double)f->plane[axis]*v;
                    if(v<lo[axis])lo[axis]=v;if(v>hi[axis])hi[axis]=v;
                }
                if(fabs(d)>1e-4)return 8;
            }
        }
        if(i==0 && (source4395!=1 || fabs(lo[0]-34.5715)>0.001 ||
                    fabs(hi[2]-47.1228)>0.001))return 10;
        kept=brush;
        status=rf_editor_brush_open(&level,uids[i],1,&brush);
        if(status!=RF_RANGE || memcmp(&brush,&kept,sizeof(brush)))return 9;
        printf("PASS L1S1 editor brush uid=%u faces=%u corners=%u bytes=%u\n",
            brush.uid,brush.face_count,brush.corner_count,brush.resident_bytes);
        owned[i]=brush;
    }
    for(i=0;i<2;i++){
        uint32_t count=0,ids[128],found3766=0;
        status=rf_editor_brush_compiled_faces(owned+i,&geometry,28,NULL,0,&count);
        if(status || count!=(i?109u:40u))return 14;
        status=rf_editor_brush_compiled_faces(owned+i,&geometry,28,ids,128,&count);
        if(status || count!=(i?109u:40u))return 15;
        for(j=0;j<count;j++){
            rf_geometry_face face;
            if(j && ids[j]<=ids[j-1])return 16;
            if(rf_geometry_get_face(&geometry,ids[j],&face) || face.room!=28)return 17;
            found3766+=ids[j]==3766;
        }
        if(found3766!=(i?0u:1u))return 18;
        printf("PASS L1S1 compiled room28 ownership uid=%u faces=%u\n",uids[i],count);
    }
    {
        static rf_geomod_vertex clip_vertices[2][4096],aggregate_vertices[2][4096];
        static rf_geomod_fragment clip_fragments[2][1024],aggregate_fragments[2][1024];
        rf_geomod_solid_union_work work={0};rf_geomod_solid_clip_result result;
        rf_geomod_solid_clip_source sources[2];rf_geomod_vertex patch[4]={0};
        const float xy[4][2]={{31.7616f,-.9939f},{41.7616f,-.9939f},
                               {41.7616f,9.0061f},{31.7616f,9.0061f}};
        double area=0;
        for(i=0;i<2;i++){
            sources[i]=(rf_geomod_solid_clip_source){owned[i].faces,owned[i].face_count};
            work.clip.vertices[i]=clip_vertices[i];work.clip.fragments[i]=clip_fragments[i];
            work.vertices[i]=aggregate_vertices[i];work.fragments[i]=aggregate_fragments[i];
        }
        work.clip.vertex_capacity=work.vertex_capacity=4096;
        work.clip.fragment_capacity=work.fragment_capacity=1024;
        for(i=0;i<4;i++){
            patch[i].position[0]=patch[i].uv[0]=xy[i][0];
            patch[i].position[1]=patch[i].uv[1]=xy[i][1];
            patch[i].position[2]=44.425f;
        }
        status=rf_geomod_polygon_clip_outside_union(patch,4,sources,2,&work,&result);
        if(status){fprintf(stderr,"L1S1 air union status %d\n",status);return 11;}
        for(i=0;i<result.fragment_count;i++){
            const rf_geomod_fragment *f=result.fragments+i;
            const rf_geomod_vertex *v=result.vertices+f->first;
            for(j=1;j+1<f->count;j++)area+=fabs(((double)v[j].position[0]-v[0].position[0])*
                (v[j+1].position[1]-v[0].position[1])-((double)v[j].position[1]-v[0].position[1])*
                (v[j+1].position[0]-v[0].position[0]))*.5;
        }
        if(!isfinite(area) || fabs(area-67.6862)>0.02)return 12;
        printf("PASS L1S1 10x10 patch outside air union: area %.4f, fragments %u, vertices %u\n",
            area,result.fragment_count,result.vertex_count);
    }
    for(i=0;i<2;i++)rf_editor_brush_close(owned+i);
    rf_geometry_close(&geometry);
    rf_vpp_close(&archive);return 0;
}
