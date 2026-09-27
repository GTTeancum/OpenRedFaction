#include "rf/editor_brush.h"
#include "rf/geomod_solid_clip.h"
#include "rf/geomod_campaign_wall.h"
#include "rf/geomod_campaign_room.h"
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
    if(argc!=3)return 2;
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
        {
            static rf_geomod_template shape;
            static rf_geomod_vertex cutter_vertices[RF_GEOMOD_STAR_VERTEX_LIMIT],wall_vertices[4096];
            static rf_geomod_face cutter_faces[RF_GEOMOD_STAR_FACE_LIMIT],wall_faces[1024];
            static const float center[3]={36.7616f,4.0061f,44.425f};
            static const float basis[9]={1,0,0,0,1,0,0,0,1};
            rf_geomod_mesh_view cutter,wall;float kernel[3];
            status=rf_geomod_template_load(argv[2],&shape);if(status)return 19;
            status=rf_geomod_template_mesh(&shape,center,basis,5.f,7,
                cutter_vertices,cutter_faces,kernel,&cutter);if(status)return 20;
            status=rf_geomod_campaign_wall_build(&cutter,sources,2,&work,
                wall_vertices,4096,wall_faces,1024,&wall);
            if(status){fprintf(stderr,"L1S1 wall build status %d\n",status);return 21;}
            if(wall.face_count!=368 || wall.vertex_count!=1447)return 22;
            for(i=0;i<wall.face_count;i++){
                const rf_geomod_face *f=wall.faces+i;
                const rf_geomod_vertex *v=wall.vertices+f->first;
                double normal[3]={0},toward=0;uint32_t corner,axis;
                if(f->material!=7 || f->source_face!=UINT32_MAX ||
                   f->count<3 || f->first+f->count>wall.vertex_count)return 23;
                for(corner=1;corner+1<f->count;corner++){
                    double a[3],b[3];
                    for(axis=0;axis<3;axis++){
                        a[axis]=(double)v[corner].position[axis]-v[0].position[axis];
                        b[axis]=(double)v[corner+1].position[axis]-v[0].position[axis];
                    }
                    for(axis=0;axis<3;axis++)normal[axis]+=
                        a[(axis+1)%3]*b[(axis+2)%3]-a[(axis+2)%3]*b[(axis+1)%3];
                }
                for(axis=0;axis<3;axis++)toward+=normal[axis]*(kernel[axis]-v[0].position[axis]);
                if(!isfinite(toward) || toward<=1e-10)return 24;
            }
            printf("PASS L1S1 authored crater wall faces=%u vertices=%u radius=5\n",
                wall.face_count,wall.vertex_count);
            {
                static rf_collision_face cutter_collision[RF_GEOMOD_STAR_FACE_LIMIT];
                static rf_collision_face_filter cutter_filters[RF_GEOMOD_STAR_FACE_LIMIT];
                static float cutter_positions[RF_GEOMOD_STAR_VERTEX_LIMIT][3];
                static rf_geomod_vertex retained_vertices[8192];
                static rf_geomod_face retained_faces[2048];
                static uint8_t retained_unchanged[2048];
                rf_geomod_mesh_view retained;uint32_t changed=0;
                status=rf_geomod_collision_faces(&cutter,cutter_filters,cutter_positions,
                    RF_GEOMOD_STAR_VERTEX_LIMIT,cutter_collision,RF_GEOMOD_STAR_FACE_LIMIT);
                if(status)return 27;
                status=rf_geomod_campaign_room_retain(&geometry,28,cutter_collision,
                    cutter.face_count,&work.clip,retained_vertices,8192,retained_faces,
                    retained_unchanged,2048,&retained);
                if(status){fprintf(stderr,"L1S1 retained room status %d\n",status);return 28;}
                for(i=0;i<retained.face_count;i++){
                    const rf_geomod_face *f=retained.faces+i;
                    if(f->source_face==3766)changed++;
                    if(f->first+f->count>retained.vertex_count || f->count<3)return 29;
                }
                printf("PASS L1S1 room28 retained faces=%u vertices=%u source3766=%u\n",
                    retained.face_count,retained.vertex_count,changed);
                {
                    static rf_geomod_vertex merged_vertices[4096];
                    static rf_geomod_face merged_faces[1024];
                    static rf_collision_face collision_faces[1024];
                    static rf_collision_face_filter filters[1024];
                    static float positions[4096][3];
                    rf_geomod_mesh_view merged;rf_collision_tree tree={0};
                    if(retained.vertex_count+wall.vertex_count>4096 ||
                       retained.face_count+wall.face_count>1024)return 30;
                    memcpy(merged_vertices,retained.vertices,retained.vertex_count*sizeof(*merged_vertices));
                    memcpy(merged_vertices+retained.vertex_count,wall.vertices,wall.vertex_count*sizeof(*merged_vertices));
                    memcpy(merged_faces,retained.faces,retained.face_count*sizeof(*merged_faces));
                    for(i=0;i<wall.face_count;i++){
                        merged_faces[retained.face_count+i]=wall.faces[i];
                        merged_faces[retained.face_count+i].first+=retained.vertex_count;
                    }
                    merged=(rf_geomod_mesh_view){merged_vertices,merged_faces,
                        retained.vertex_count+wall.vertex_count,
                        retained.face_count+wall.face_count,0};
                    for(i=0;i<merged.face_count;i++){
                        uint32_t source=merged.faces[i].source_face;
                        if(source==UINT32_MAX)filters[i]=(rf_collision_face_filter){0,256,0,0,0,0};
                        else if(rf_geometry_initial_collision_filter(&geometry,source,0,filters+i))return 31;
                    }
                    for(i=0;i<merged.face_count;i++){
                        const rf_geomod_face *f=merged.faces+i;
                        if(i<retained.face_count && retained_unchanged[i]){
                            status=rf_geometry_collision_face(&geometry,f->source_face,filters+i,
                                positions+f->first,f->count,collision_faces+i);
                        } else {
                            rf_geomod_face local=*f;
                            rf_geomod_mesh_view one;
                            local.first=0;
                            one=(rf_geomod_mesh_view){merged.vertices+f->first,&local,f->count,1,0};
                            status=rf_geomod_collision_faces(&one,filters+i,positions+f->first,
                                f->count,collision_faces+i,1);
                        }
                        if(status){fprintf(stderr,"L1S1 merged face %u source %u status %d\n",
                            i,f->source_face,status);return 32;}
                    }
                    status=rf_collision_tree_open(collision_faces,merged.face_count,2*1024*1024,&tree);
                    if(status){fprintf(stderr,"L1S1 merged tree status %d\n",status);return 33;}
                    printf("PASS L1S1 staged room28 tree faces=%u vertices=%u treebytes=%u\n",
                        merged.face_count,merged.vertex_count,tree.allocated_bytes);
                    rf_collision_tree_close(&tree);
                }
            }
            {
                static rf_collision_face collision_faces[1024];
                static rf_collision_face_filter filters[1024];
                static float positions[4096][3];rf_collision_tree tree={0};
                for(i=0;i<wall.face_count;i++)filters[i].face_flags=256;
                status=rf_geomod_collision_faces(&wall,filters,positions,4096,
                    collision_faces,1024);
                if(status){fprintf(stderr,"L1S1 wall collision status %d\n",status);return 25;}
                status=rf_collision_tree_open(collision_faces,wall.face_count,1024*1024,&tree);
                if(status){fprintf(stderr,"L1S1 wall tree status %d\n",status);return 26;}
                printf("PASS L1S1 staged wall collision tree bytes=%u\n",tree.allocated_bytes);
                rf_collision_tree_close(&tree);
            }
        }
    }
    for(i=0;i<2;i++)rf_editor_brush_close(owned+i);
    rf_geometry_close(&geometry);
    rf_vpp_close(&archive);return 0;
}
