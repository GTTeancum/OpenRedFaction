#include "rf/geomod_piece_bank.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int cube(float x,rf_geomod_piece_registry **out) {
    static const float p[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    static const uint32_t indices[6][4]={{1,2,6,5},{0,4,7,3},{3,7,6,2},{0,1,5,4},{4,5,6,7},{0,3,2,1}};
    rf_geomod_vertex vertices[24]={0};rf_geomod_face faces[6];rf_collision_face_filter filters[6],generated={0,256,-1,1,0,0};
    uint32_t map[6],i,j,k;rf_geomod_mesh_view mesh;
    for(i=0;i<6;i++) {
        faces[i]=(rf_geomod_face){4*i,4,0,i};map[i]=i;filters[i]=generated;
        for(j=0;j<4;j++)for(k=0;k<3;k++)vertices[4*i+j].position[k]=p[indices[i][j]][k]*.25f+(k?0:x);
    }
    mesh=(rf_geomod_mesh_view){vertices,faces,24,6,0};
    CHECK(!rf_geomod_piece_registry_open(&generated,0,2.5f,.5f,.25f,1,2*1024*1024,out));
    CHECK(!rf_geomod_piece_registry_begin(*out,0));
    CHECK(!rf_geomod_piece_registry_emit(&mesh,map,filters,6,1,0,*out));
    rf_geomod_piece_registry_commit(*out);return 0;
}
int main(void) {
    rf_geomod_piece_registry *registries[2]={0};scene_terrain_authored_assets assets[2]={{0}};
    scene_terrain_source_owner sources[2];scene_stream scene={0};rf_geomod_registry_hit hit,sentinel;
    rf_geomod_piece_batch *batches[2];rf_geomod_owned_piece piece;rf_physics_body *bodies[2];
    float start[3]={6,0,0},delta[3]={-8,0,0};uint32_t found,i,size;unsigned char *before,*after;
    CHECK(!cube(0,registries));CHECK(!cube(4,registries+1));
    for(i=0;i<2;i++) {
        sources[i]=(scene_terrain_source_owner){assets+i,NULL,registries[i]};
        CHECK(!rf_geomod_piece_registry_get(registries[i],0,batches+i));
        CHECK(rf_geomod_piece_batch_count(batches[i])==1);
        CHECK(!rf_geomod_piece_batch_get(batches[i],0,&piece,bodies+i));
    }
    scene.terrain_sources=sources;scene.terrain_source_count=2;scene.terrain_authored=assets;scene.detached_pieces=registries[0];
    sources[0].pieces=NULL; /* selected live alias wins over a stale collection entry */
    for(i=0;i<2;i++)bodies[i]->state.flags&=~0x80000000u;
    CHECK(!scene_detached_tick(&scene));CHECK(rf_scene_detached_motion[0]==2 && rf_scene_detached_motion[3]==2);
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found));
    CHECK(found && hit.batch==16 && hit.piece.piece==0);
    bodies[0]->state.position[0]=4;
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found));
    CHECK(found && hit.batch==0); /* equal contacts retain the first source */
    bodies[0]->state.position[0]=0;
    CHECK(!rf_geomod_piece_registry_state_size(registries[0],&size));before=malloc(size);after=malloc(size);CHECK(before && after);
    CHECK(!rf_geomod_piece_registry_state_encode(registries[0],before,size));
    CHECK(!scene_detached_sources_damage(&scene,16,0,100000));
    CHECK(!rf_geomod_piece_batch_alive(batches[1],0) && rf_geomod_piece_batch_alive(batches[0],0));
    CHECK(!rf_geomod_piece_registry_state_encode(registries[0],after,size));CHECK(!memcmp(before,after,size));
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found));CHECK(found && hit.batch==0);
    CHECK(scene_detached_sources_damage(&scene,64,0,100000)==RF_RANGE);
    memset(&sentinel,0xa5,sizeof(sentinel));hit=sentinel;start[1]=5;
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found));CHECK(!found && !memcmp(&hit,&sentinel,sizeof(hit)));
    found=77;CHECK(scene_detached_sources_sweep(&scene,4,start,delta,0,NAN,&hit,&found)!=RF_OK);
    CHECK(found==77 && !memcmp(&hit,&sentinel,sizeof(hit)));
    free(before);free(after);for(i=0;i<2;i++)rf_geomod_piece_registry_close(registries+i);
    puts("PASS multi-source weapon queries: nearer later source, stable ties, selected alias, isolated damage and atomic misses/errors");return 0;
}
