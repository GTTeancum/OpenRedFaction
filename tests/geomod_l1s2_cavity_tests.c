/* Installed L1S2 source identity, bounded terrain admission and publication. */
#include "rf/geomod_authored_post.h"
#include "rf/authored_identity_capture.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(call) do { int result=(call); if(result) { fprintf(stderr,"FAIL line%d status%d %s\n",__LINE__,result,#call); return 1; } } while(0)
static rf_geomod_publication_work publication_work;
static rf_geomod_vertex published_vertices[RF_GEOMOD_PUBLICATION_VERTICES];
static rf_geomod_face published_faces[RF_GEOMOD_PUBLICATION_FACES];
static rf_geomod_publication_origin published_origins[RF_GEOMOD_PUBLICATION_FACES];
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};
    rf_vpp maps[6]={{0}};rf_lightmap_rgb_owner rgb={0};
    rf_geomod_authored_post *owner=NULL;rf_geomod_authored_post_view view;
    rf_geomod_authored_identity_manifest manifest={0};unsigned char identity[32];uint32_t identity_peak=0;
    rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view terrain_view;
    rf_collision_face_filter generated;rf_geomod_template shape;
    rf_geomod_publication_job job={0};rf_geomod_publication_cut cut={0};rf_geomod_mesh_view published;
    const float center[3]={121.208061f,-2.12633848f,-17.f};
    const float basis[9]={1,0,0,0,1,0,0,0,1};int cut_status;uint32_t i,crater_faces=0,retained_faces=0;
    const char *map_names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
    char map_path[512];
    if(argc!=4)return 2;
    CHECK(rf_vpp_open(&archive,argv[1]));
    CHECK(rf_level_open(&level,&archive,"L1S2.rfl"));
    CHECK(rf_geometry_open(&geometry,&level,8u*1024u*1024u));
    CHECK(rf_geomod_authored_cavity_open_source(&level,&geometry,8123,2u*1024u*1024u,&owner));
    CHECK(rf_geomod_authored_post_get(owner,&view));
    if(view.source_uid!=8123 || view.room!=8 || view.source.face_count!=48 ||
       view.windows.face_count!=83 || view.replaced_count!=83 || view.solid_count) {
        fprintf(stderr,"FAIL source %u room %u faces %u windows %u replaced %u solids %u\n",
            view.source_uid,view.room,view.source.face_count,view.windows.face_count,
            view.replaced_count,view.solid_count);return 1;
    }
    CHECK(rf_lightmap_rgb_open(&rgb,&level,16u*1024u*1024u));
    for(i=0;i<6;i++) {
        snprintf(map_path,sizeof(map_path),"%s/%s",argv[3],map_names[i]);
        CHECK(rf_vpp_open(maps+i,map_path));
    }
    manifest.reference_capacity=view.source.face_count+view.windows.face_count+view.neighbors.face_count;
    manifest.material_capacity=manifest.reference_capacity>128?128:manifest.reference_capacity;
    manifest.materials=calloc(manifest.material_capacity,sizeof(*manifest.materials));
    manifest.references=calloc(manifest.reference_capacity,sizeof(*manifest.references));
    manifest.substrate=calloc(1,sizeof(*manifest.substrate));
    if(!manifest.materials || !manifest.references || !manifest.substrate)return 1;
    CHECK(rf_geomod_authored_identity_capture_manifest(&level,&geometry,&view,maps,6,&rgb,
        3u*1024u*1024u,identity,&identity_peak,&manifest));
    if(!manifest.material_count || !manifest.reference_count || identity_peak>3u*1024u*1024u)return 1;
    generated=view.source_filters[0];generated.query_flags=0;generated.face_flags=256;
    CHECK(rf_geomod_terrain_open(&view.source,view.source_filters,&generated,1,
        4096,1024,1152u*1024u,&terrain));
    CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
    if(terrain_view.mesh.face_count!=48 || terrain_view.cuts!=0) {
        fprintf(stderr,"FAIL terrain faces %u cuts %u\n",terrain_view.mesh.face_count,terrain_view.cuts);
        return 1;
    }
    CHECK(rf_geomod_template_load(argv[2],&shape));
    {
        float minimum[3],maximum[3];uint32_t reference=UINT32_MAX;
        CHECK(rf_geomod_template_bounds(&shape,center,basis,1.f,NULL,0,minimum,maximum));
        cut_status=rf_geomod_authored_cavity_admit(owner,minimum,maximum,&reference);
        if(cut_status || reference!=768) {
            fprintf(stderr,"FAIL cutter admission status %d reference %u\n",cut_status,reference);
            return 1;
        }
    }
    CHECK(rf_geomod_terrain_cut_template(terrain,&shape,center,basis,10.f,0));
    CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
    if(terrain_view.cuts!=1 || terrain_view.mesh.face_count<=48 ||
       terrain_view.resident_bytes>1152u*1024u || terrain_view.peak_bytes>1152u*1024u) {
        fprintf(stderr,"FAIL cut faces %u cuts %u resident %u peak %u\n",
            terrain_view.mesh.face_count,terrain_view.cuts,
            terrain_view.resident_bytes,terrain_view.peak_bytes);return 1;
    }
    CHECK(rf_geomod_terrain_cutter_get(terrain,0,&cut.mesh,cut.kernel,&cut.star));
    job.terrain=terrain_view.mesh;job.windows=view.windows;job.neighbors=view.neighbors;
    job.window_origins=view.window_origins;job.neighbor_origins=view.neighbor_origins;
    job.source_planes=view.source_planes;job.source_plane_count=view.source.face_count;
    job.cuts=&cut;job.cut_count=1;
    job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,8123,UINT32_MAX,768};
    cut_status=rf_geomod_publication_build_cavity(&job,&publication_work,
        published_vertices,RF_GEOMOD_PUBLICATION_VERTICES,
        published_faces,RF_GEOMOD_PUBLICATION_FACES,published_origins,&published);
    CHECK(cut_status);
    for(i=0;i<published.face_count;i++) {
        crater_faces+=published_origins[i].kind==RF_GEOMOD_PUBLICATION_CRATER;
        retained_faces+=published_origins[i].kind==RF_GEOMOD_PUBLICATION_RETAINED;
    }
    if(!crater_faces || !retained_faces || published.face_count<view.windows.face_count) {
        fprintf(stderr,"FAIL publication faces %u crater %u retained %u\n",
            published.face_count,crater_faces,retained_faces);return 1;
    }
    printf("L1S2 cut core faces %u publication faces %u crater %u retained %u resident %u peak %u\n",
        terrain_view.mesh.face_count,published.face_count,crater_faces,retained_faces,
        terrain_view.resident_bytes,terrain_view.peak_bytes);
    CHECK(rf_geomod_terrain_reset(terrain));CHECK(rf_geomod_terrain_get(terrain,&terrain_view));
    if(terrain_view.cuts || terrain_view.mesh.face_count!=48)return 1;
    printf("PASS L1S2 cavity source %u room %u faces %u windows %u resident %u peak %u\n",
        view.source_uid,view.room,view.source.face_count,view.windows.face_count,
        view.resident_bytes,view.peak_bytes);
    printf("L1S2 identity materials %u references %u peak %u\n",
        manifest.material_count,manifest.reference_count,identity_peak);
    free(manifest.materials);free(manifest.references);free(manifest.substrate);
    for(i=0;i<6;i++)rf_vpp_close(maps+i);rf_lightmap_rgb_close(&rgb);
    rf_geomod_terrain_close(&terrain);rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    return 0;
}
