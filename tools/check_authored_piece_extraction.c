/* Installed-asset probe; no emulator or desktop interaction. */
#include "rf/geomod_authored_post.h"
#include "rf/geomod_piece_bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{int check_status=(x);if(check_status){fprintf(stderr,"FAIL line%d status%d: %s\n",__LINE__,check_status,#x);return 1;}}while(0)
typedef struct capture {uint32_t groups,pieces,resident,peak,digest;rf_random_state random;} capture;
static uint32_t hash(uint32_t h,const void *data,size_t bytes)
{const unsigned char *p=data;if(!h)h=2166136261u;while(bytes--)h=(h^*p++)*16777619u;return h;}
static int emit(const rf_geomod_mesh_view *mesh,const uint32_t *map,const rf_collision_face_filter *filters,
    uint32_t count,uint32_t prefix,uint32_t ordinal,void *opaque)
{
    capture *c=opaque;rf_geomod_piece_batch *batch=NULL;rf_collision_face_filter mapped[32],generated={0};int status;
    printf("EXTRACT prefix%u ordinal%u vertices%u faces%u\n",prefix,ordinal,mesh->vertex_count,mesh->face_count);
    if(mesh->face_count>32)return RF_RANGE;
    for(uint32_t i=0;i<mesh->face_count;i++){if(map[i]>=count)return RF_FORMAT;mapped[i]=filters[map[i]];}
    status=rf_geomod_piece_batch_open(mesh,mapped,&generated,0,2.5f,.5f,.25f,&c->random,2097152,&batch);
    if(status){printf("BATCH_REJECT %d\n",status);return status;}
    c->groups++;c->pieces+=rf_geomod_piece_batch_count(batch);
    c->resident+=rf_geomod_piece_batch_bytes(batch);
    if(c->peak<rf_geomod_piece_batch_peak_bytes(batch))c->peak=rf_geomod_piece_batch_peak_bytes(batch);
    for(uint32_t i=0;i<rf_geomod_piece_batch_count(batch);i++) {
        rf_geomod_owned_piece piece;rf_physics_body *body;
        status=rf_geomod_piece_batch_get(batch,i,&piece,&body);if(status){rf_geomod_piece_batch_close(&batch);return status;}
        c->digest=hash(c->digest,piece.mesh.vertices,piece.mesh.vertex_count*sizeof(*piece.mesh.vertices));
        c->digest=hash(c->digest,piece.mesh.faces,piece.mesh.face_count*sizeof(*piece.mesh.faces));
        c->digest=hash(c->digest,piece.filters,piece.mesh.face_count*sizeof(*piece.filters));
        c->digest=hash(c->digest,&body->state,sizeof(body->state));
        c->digest=hash(c->digest,body->spheres.items,body->spheres.count*sizeof(*body->spheres.items));
    }
    rf_geomod_piece_batch_close(&batch);return RF_OK;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};rf_level level;rf_geometry geometry={0};rf_geomod_authored_post *asset=NULL;
    rf_geomod_authored_post_view a;rf_geomod_template shape;rf_collision_face_filter generated;
    float lo[3]={1e20f,1e20f,1e20f},hi[3]={-1e20f,-1e20f,-1e20f},basis[9]={1,0,0,0,1,0,0,0,1};
    uint32_t failures=0,extracted=0;
    if(argc!=3)return 2;
    CHECK(rf_vpp_open(&archive,argv[1]));CHECK(rf_level_open(&level,&archive,"ctf06.rfl"));
    CHECK(rf_geometry_open(&geometry,&level,8388608));CHECK(rf_geomod_authored_post_open(&level,&geometry,2097152,&asset));
    CHECK(rf_geomod_authored_post_get(asset,&a));CHECK(rf_geomod_template_load(argv[2],&shape));
    for(uint32_t i=0;i<a.source.vertex_count;i++)for(uint32_t k=0;k<3;k++) {
        float x=a.source.vertices[i].position[k];if(x<lo[k])lo[k]=x;if(x>hi[k])hi[k]=x;
    }
    printf("SOURCE bounds %g %g %g / %g %g %g\n",lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
    generated=a.source_filters[0];generated.query_flags=0;generated.face_flags=256;
    for(uint32_t sample=0;sample<6;sample++) {
        rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view view;capture c={0};float center[3],radius=sample<3?1.05f:1.5f;int status;
        for(uint32_t k=0;k<3;k++)center[k]=(lo[k]+hi[k])*.5f;
        center[1]=lo[1]+(hi[1]-lo[1])*(.25f+.25f*(sample%3));
        CHECK(rf_geomod_terrain_open(&a.source,a.source_filters,&generated,0,4096,800,1179648,&terrain));
        CHECK(rf_geomod_terrain_set_mapping(terrain,256,256));CHECK(rf_geomod_terrain_set_extraction(terrain,emit,&c));
        status=rf_geomod_terrain_cut_template(terrain,&shape,center,basis,radius,0);
        CHECK(rf_geomod_terrain_get(terrain,&view));
        printf("CASE%u radius%g y%g status%d cuts%u groups%u pieces%u batch_resident%u batch_peak%u core_peak%u\n",
            sample,radius,center[1],status,view.cuts,c.groups,c.pieces,c.resident,c.peak,view.peak_bytes);
        failures+=status!=0;extracted+=c.groups;
        if(!status) {
            rf_geomod_terrain *reload=NULL;rf_geomod_terrain_view restored;capture r={0};
            unsigned char checkpoint[RF_GEOMOD_HISTORY_MAX_BYTES];uint32_t bytes;
            memset(&c,0,sizeof(c));center[1]+=.15f;
            status=rf_geomod_terrain_cut_template(terrain,&shape,center,basis,radius,0);
            printf("FOLLOWUP%u status%d groups%u pieces%u\n",sample,status,c.groups,c.pieces);
            failures+=status!=0;
            if(!status) {
                CHECK(rf_geomod_terrain_get(terrain,&view));
                CHECK(rf_geomod_terrain_history_size(terrain,&bytes));CHECK(rf_geomod_terrain_history_encode(terrain,checkpoint,bytes));
                CHECK(rf_geomod_terrain_open(&a.source,a.source_filters,&generated,0,4096,800,1179648,&reload));
                CHECK(rf_geomod_terrain_set_mapping(reload,256,256));CHECK(rf_geomod_terrain_set_extraction(reload,emit,&r));
                CHECK(rf_geomod_terrain_history_decode(reload,checkpoint,bytes));CHECK(rf_geomod_terrain_get(reload,&restored));
                if(restored.mesh.vertex_count!=view.mesh.vertex_count || restored.mesh.face_count!=view.mesh.face_count ||
                    memcmp(restored.mesh.vertices,view.mesh.vertices,view.mesh.vertex_count*sizeof(*view.mesh.vertices)) ||
                    memcmp(restored.mesh.faces,view.mesh.faces,view.mesh.face_count*sizeof(*view.mesh.faces)) ||
                    c.groups!=r.groups || c.pieces!=r.pieces || c.random.value!=r.random.value || c.digest!=r.digest) {
                    fprintf(stderr,"RELOAD_MISMATCH case%u\n",sample);failures++;
                } else printf("RELOAD%u exact terrain mesh and matching piece/body digest/counts/RNG\n",sample);
                rf_geomod_terrain_close(&reload);
            }
        }
        rf_geomod_terrain_close(&terrain);
    }
    rf_geomod_authored_post_close(&asset);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    printf("RESULT failed%u extracted%u\n",failures,extracted);return failures || !extracted;
}
