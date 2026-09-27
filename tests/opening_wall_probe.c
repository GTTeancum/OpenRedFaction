#include "rf/editor_brush.h"
#include "rf/entity_assets.h"
#include "rf/geomod_campaign_room.h"
#include "rf/geomod_campaign_wall.h"
#include <stdio.h>
#include <stdlib.h>

/* Process-local CSG feasibility check for the first walked L1S1 wall.
 * This only stages candidate geometry; it never changes the running game. */
static rf_geomod_vertex clip_vertices[2][4096],aggregate_vertices[2][4096];
static rf_geomod_fragment clip_fragments[2][1024],aggregate_fragments[2][1024];
static rf_geomod_vertex cutter_vertices[RF_GEOMOD_STAR_VERTEX_LIMIT],wall_vertices[4096];
static rf_geomod_face cutter_faces[RF_GEOMOD_STAR_FACE_LIMIT],wall_faces[1024];
static rf_collision_face cutter_collision[RF_GEOMOD_STAR_FACE_LIMIT];
static rf_collision_face_filter cutter_filters[RF_GEOMOD_STAR_FACE_LIMIT];
static float cutter_positions[RF_GEOMOD_STAR_VERTEX_LIMIT][3];
static rf_geomod_vertex retained_vertices[8192],merged_vertices[4096];
static rf_geomod_face retained_faces[2048],merged_faces[1024];
static uint8_t retained_unchanged[2048];
static rf_collision_face_filter filters[1024];
static rf_collision_face bound[1024];
static float positions[4096][3];
static uint32_t face_ids[1024];
static rf_geometry_portal portals[256];

int main(int argc,char **argv)
{
    static const uint32_t room[2]={40,27},uid[2]={8715,8747};
    static const float center[3]={-21.8000622f,-0.7422277f,29.5635757f};
    static const float basis[9]={1,0,0,0,1,0,0,0,1};
    float radii[4]={0,1.0f,1.5f,2.5f};
    rf_vpp archive={0},tables={0};rf_level level={0};rf_geometry geometry={0};
    rf_weapon_explosive_definition remote={0};
    rf_editor_brush brushes[2]={{0}};
    rf_geomod_template shape;rf_geomod_solid_union_work work={0};
    rf_geomod_solid_clip_source sources[2];
    uint32_t *slots=NULL,i,j;int status=0;
    if(argc!=4)return 2;
    if((status=rf_vpp_open(&archive,argv[1])) ||
       (status=rf_level_open(&level,&archive,"L1S1.rfl")) ||
       (status=rf_geometry_open(&geometry,&level,8*1024*1024)) ||
       (status=rf_vpp_open(&tables,argv[3])) ||
       (status=rf_weapon_explosive_load(&tables,"Remote Charge",128*1024,&remote)) ||
       (status=rf_geomod_template_load(argv[2],&shape)))goto done;
    radii[0]=remote.crater_radius;
    printf("authored remote charge crater radius=%.3f\n",radii[0]);
    slots=calloc(geometry.textures,sizeof(*slots));if(!slots){status=RF_IO;goto done;}
    for(i=0;i<geometry.textures;i++)slots[i]=i;
    for(i=0;i<2;i++){
        uint32_t count=0;
        if((status=rf_editor_brush_open(&level,uid[i],512*1024,brushes+i)) ||
           brushes[i].operation!=2){if(!status)status=RF_FORMAT;goto done;}
        status=rf_editor_brush_compiled_faces(brushes+i,&geometry,room[i],NULL,0,&count);
        if(status)goto done;
        printf("owner uid=%u room=%u brush_faces=%u compiled_faces=%u\n",
            uid[i],room[i],brushes[i].face_count,count);
        sources[i]=(rf_geomod_solid_clip_source){brushes[i].faces,brushes[i].face_count};
        work.clip.vertices[i]=clip_vertices[i];work.clip.fragments[i]=clip_fragments[i];
        work.vertices[i]=aggregate_vertices[i];work.fragments[i]=aggregate_fragments[i];
    }
    work.clip.vertex_capacity=work.vertex_capacity=4096;
    work.clip.fragment_capacity=work.fragment_capacity=1024;
    for(j=0;j<sizeof(radii)/sizeof(radii[0]);j++){
        rf_geomod_mesh_view cutter,wall;float kernel[3];
        status=rf_geomod_template_mesh(&shape,center,basis,radii[j],7,
            cutter_vertices,cutter_faces,kernel,&cutter);
        if(status)goto done;
        status=rf_geomod_collision_faces(&cutter,cutter_filters,cutter_positions,
            RF_GEOMOD_STAR_VERTEX_LIMIT,cutter_collision,RF_GEOMOD_STAR_FACE_LIMIT);
        if(status)goto done;
        status=rf_geomod_campaign_wall_build(&cutter,sources,2,&work,
            wall_vertices,4096,wall_faces,1024,&wall);
        printf("radius=%.3f wall status=%d faces=%u vertices=%u\n",
            radii[j],status,status?0:wall.face_count,status?0:wall.vertex_count);
        if(status){if(!j)goto done;continue;}
        {
            rf_geomod_campaign_room_scope scope;
            status=rf_geomod_campaign_room_scope_check_group(&geometry,room,2,
                cutter_collision,cutter.face_count,portals,256,&scope);
            if(status)goto done;
            printf(" grouped foreign_solid=%u detail=%u portals=%u\n",
                scope.other_solid_faces,scope.other_detail_faces,scope.touching_portals);
            if(!j && (scope.other_solid_faces || scope.touching_portals)){
                status=RF_FORMAT;goto done;
            }
        }
        for(i=0;i<2;i++){
            rf_geomod_campaign_room_scope scope;rf_geomod_mesh_view retained,merged;
            rf_geomod_campaign_room_stage_work stage={merged_vertices,merged_faces,filters,
                bound,positions,face_ids,4096,1024};
            rf_collision_tree tree={0};
            status=rf_geomod_campaign_room_scope_check(&geometry,room[i],cutter_collision,
                cutter.face_count,portals,256,&scope);
            if(status)goto done;
            printf(" room=%u foreign_solid=%u detail=%u portals=%u",
                room[i],scope.other_solid_faces,scope.other_detail_faces,scope.touching_portals);
            status=rf_geomod_campaign_room_retain(&geometry,room[i],cutter_collision,
                cutter.face_count,&work.clip,retained_vertices,8192,retained_faces,
                retained_unchanged,2048,&retained);
            if(status){printf(" retain_status=%d\n",status);continue;}
            status=rf_geomod_campaign_room_stage(&geometry,&retained,retained_unchanged,
                &wall,slots,geometry.textures,geometry.textures,geometry.textures+1,
                (rf_collision_face_filter){0,256,0,0,0,0},3460,&stage,
                2*1024*1024,&merged,&tree);
            printf(" retain=%u/%u stage_status=%d merged=%u/%u tree=%u\n",
                retained.face_count,retained.vertex_count,status,
                status?0:merged.face_count,status?0:merged.vertex_count,
                status?0:tree.allocated_bytes);
            rf_collision_tree_close(&tree);
            if(!j && status)goto done;
        }
    }
    status=RF_OK;
done:
    if(status)fprintf(stderr,"opening wall probe status=%d\n",status);
    free(slots);rf_editor_brush_close(brushes+1);rf_editor_brush_close(brushes);
    rf_geometry_close(&geometry);rf_vpp_close(&tables);rf_vpp_close(&archive);
    return status?1:0;
}
