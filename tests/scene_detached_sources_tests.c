#include "rf/geomod_piece_bank.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int cube(float x,float extent,rf_geomod_piece_registry **out) {
    static const float p[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    static const uint32_t indices[6][4]={{1,2,6,5},{0,4,7,3},{3,7,6,2},{0,1,5,4},{4,5,6,7},{0,3,2,1}};
    rf_geomod_vertex vertices[24]={0};rf_geomod_face faces[6];rf_collision_face_filter filters[6],generated={0,256,-1,1,0,0};
    uint32_t map[6],i,j,k;rf_geomod_mesh_view mesh;
    for(i=0;i<6;i++) {
        faces[i]=(rf_geomod_face){4*i,4,0,i};map[i]=i;filters[i]=generated;
        for(j=0;j<4;j++)for(k=0;k<3;k++)vertices[4*i+j].position[k]=p[indices[i][j]][k]*extent+(k?0:x);
    }
    mesh=(rf_geomod_mesh_view){vertices,faces,24,6,0};
    CHECK(!rf_geomod_piece_registry_open(&generated,0,2.5f,.5f,.25f,1,2*1024*1024,out));
    CHECK(!rf_geomod_piece_registry_begin(*out,0));
    CHECK(!rf_geomod_piece_registry_emit(&mesh,map,filters,6,1,0,*out));
    rf_geomod_piece_registry_commit(*out);return 0;
}
static int notify_sources(void) {
    scene_stream scene={0};scene_terrain_source_owner sources[2]={{0}};rf_geomod_piece_registry *r[2]={0};
    rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;rf_physics_body *body[2];
    uint32_t prior[4],i,woken=777;float center[3]={2,0,0};rf_geomod_changed_box original;
    CHECK(!cube(0,.4f,r));CHECK(!cube(4,.4f,r+1));
    for(i=0;i<2;i++) {
        sources[i].pieces=r[i];CHECK(!rf_geomod_piece_registry_get(r[i],0,&batch));
        CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,body+i));body[i]->state.flags&=~0x80000000u;
    }
    scene.terrain_sources=sources;scene.terrain_source_count=2;
    CHECK(!scene_detached_sources_counts(&scene,prior));CHECK(prior[0]==1 && prior[1]==1 && !prior[2] && !prior[3]);
    original=(rf_geomod_changed_box){{0},{0}};
    memcpy(original.minimum,body[1]->state.bounds.minimum,12);memcpy(original.maximum,body[1]->state.bounds.maximum,12);
    /* Only source0 has a new changed box; source1's sleeping body overlaps it. */
    memcpy(body[1]->state.bounds.minimum,body[0]->state.bounds.minimum,12);
    memcpy(body[1]->state.bounds.maximum,body[0]->state.bounds.maximum,12);prior[0]=0;
    CHECK(!scene_detached_sources_notify(&scene,prior,center,0,&woken));CHECK(woken==2);
    for(i=0;i<2;i++)CHECK(body[i]->state.flags&0x80000000u);
    CHECK(!scene_detached_sources_notify(&scene,prior,center,0,&woken) && !woken);
    for(i=0;i<2;i++)body[i]->state.flags&=~0x80000000u;
    memcpy(body[1]->state.bounds.minimum,original.minimum,12);memcpy(body[1]->state.bounds.maximum,original.maximum,12);
    prior[0]=1; /* No new boxes: original radial fallback still visits both owners. */
    body[1]->state.bounds.minimum[0]=NAN;woken=777;
    CHECK(scene_detached_sources_notify(&scene,prior,center,3,&woken)==RF_FORMAT);
    CHECK(woken==777);for(i=0;i<2;i++)CHECK(!(body[i]->state.flags&0x80000000u));
    body[1]->state.bounds.minimum[0]=original.minimum[0];
    prior[1]=2;CHECK(scene_detached_sources_notify(&scene,prior,center,3,&woken)==RF_RANGE);
    CHECK(woken==777);for(i=0;i<2;i++)CHECK(!(body[i]->state.flags&0x80000000u));
    prior[1]=1;CHECK(!scene_detached_sources_notify(&scene,prior,center,3,&woken) && woken==2);
    for(i=0;i<2;i++)rf_geomod_piece_registry_close(r+i);
    puts("PASS collection wake: cross-source changed box, radial fallback, repeated wake and late-owner error without partial writes");return 0;
}
static int player_sources(void) {
    scene_stream scene={0};scene_terrain_source_owner sources[2]={{0}};rf_geomod_piece_registry *r[2]={0};
    rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;rf_physics_body *body[2];
    rf_collision_actor_general_response actor={0};rf_physics_sphere sphere={0};
    rf_collision_body_sphere query_sphere={0};rf_collision_body_query query={0};
    rf_geomod_registry_body_hit hit,kept;rf_collision_actor_contact npc={0},saved_npc;
    uint32_t i,k,motion,found,b=777,p=777;float radius;
    CHECK(!cube(0,.4f,r));CHECK(!cube(4,.4f,r+1));
    for(i=0;i<2;i++) {
        sources[i].pieces=r[i];CHECK(!rf_geomod_piece_registry_get(r[i],0,&batch));
        CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,body+i));
        CHECK(body[i]->state.bounds.radius>.5f && body[i]->state.bounds.radius<=1);
    }
    scene.terrain_sources=sources;scene.terrain_source_count=2;
    /* Original ground selection uses raw normal.y < -.5 before normalization. */
    sphere.radius=.6f;actor.actor.sphere_count=1;actor.actor.spheres=&sphere;
    actor.actor.mass=10;actor.actor.body_flags=0x8000003f;actor.actor.contact.time=1;actor.extent=.6f;
    query_sphere.radius=.6f;query.spheres=&query_sphere;query.count=1;query.limit=1;query.radius=.6f;
    for(k=0;k<3;k++) {
        actor.actor.minimum[k]=-100;actor.actor.maximum[k]=100;
        actor.actor.position[k]=actor.actor.next_position[k]=body[1]->state.position[k]+body[1]->spheres.items[0].center[k];
        actor.orientation[k*3+k]=actor.next_orientation[k*3+k]=1;query.matrix[k][k]=1;
    }
    actor.actor.position[1]+=20;actor.actor.next_position[1]-=20;
    memcpy(query.start,actor.actor.position,12);memcpy(query.end,actor.actor.next_position,12);
    for(motion=0;motion<2;motion++) {
        CHECK(!scene_detached_sources_player(&scene,&actor,&query,1,motion,&hit,&found));
        CHECK(found && hit.batch==16 && hit.face==UINT32_MAX && hit.contact.normal[1]>0);
    }
    {
        rf_checkpoint_placement placement={0};rf_physics_ground_probe probe={0};rf_checkpoint_support_hit support,expected,old;
        scene_collection_support_context context={&scene,&placement};rf_geomod_player_support_context single={r[1],&placement};
        uint32_t selected=0;
        placement.spheres=&sphere;placement.count=1;memcpy(placement.basis,actor.orientation,36);
        memcpy(probe.start,query.start,12);memcpy(probe.end,query.end,12);probe.bounds.radius=.6f;
        for(i=0;i<2;i++){body[i]->state.flags&=~0x80000000u;memset(body[i]->state.velocity,0,12);memset(body[i]->state.vector_c8,0,12);}
        CHECK(!rf_geomod_piece_registry_player_support(&single,&probe,1,&expected,&selected) && selected && expected.stable);
        CHECK(!scene_collection_player_support(&context,&probe,1,&support,&selected) && selected && support.stable);
        CHECK(!memcmp(&support,&expected,sizeof(support)));
        body[1]->state.velocity[0]=1;
        CHECK(!scene_collection_player_support(&context,&probe,1,&support,&selected) && selected && !support.stable);
        body[1]->state.velocity[0]=0;
        memcpy(placement.position,body[1]->state.position,12);
        CHECK(scene_collection_player_placement(&scene,&placement)==RF_NOT_FOUND);
        placement.position[0]=20;CHECK(!scene_collection_player_placement(&scene,&placement));
        old=support;selected=777;CHECK(!rf_geomod_piece_registry_begin(r[1],0));
        CHECK(scene_collection_player_support(&context,&probe,1,&support,&selected)!=RF_OK);
        CHECK(selected==777 && !memcmp(&support,&old,sizeof(old)));rf_geomod_piece_registry_abort(r[1]);
        puts("PASS collection checkpoint support: second source, stable/moving body distinction, overlap rejection and atomic later-registry failure");
    }
    CHECK(!scene_detached_sources_npc(&scene,&actor,1,1,&npc,&b,&p,&found));CHECK(found && b==16 && p==0);
    saved_npc=npc;b=p=777;
    CHECK(!scene_detached_sources_npc(&scene,&actor,9,1,&npc,&b,&p,&found));
    CHECK(!found && b==777 && p==777 && !memcmp(&npc,&saved_npc,sizeof(npc)));
    memcpy(body[0]->state.position,body[1]->state.position,12);memcpy(body[0]->state.next_position,body[0]->state.position,12);
    CHECK(!rf_physics_body_prepare_sweep(&body[0]->state));
    for(motion=0;motion<2;motion++) {
        CHECK(!scene_detached_sources_player(&scene,&actor,&query,1,motion,&hit,&found));CHECK(found && hit.batch==0);
    }
    radius=body[0]->state.bounds.radius;body[0]->state.bounds.radius=.5f;
    for(motion=0;motion<2;motion++) {
        CHECK(!scene_detached_sources_player(&scene,&actor,&query,1,motion,&hit,&found));CHECK(found && hit.batch==16);
    }
    body[0]->state.bounds.radius=radius;
    memset(&kept,0xa5,sizeof(kept));hit=kept;found=777;query.limit=NAN;
    CHECK(scene_detached_sources_player(&scene,&actor,&query,1,1,&hit,&found)!=RF_OK);
    CHECK(found==777 && !memcmp(&hit,&kept,sizeof(hit)));
    for(i=0;i<2;i++)rf_geomod_piece_registry_close(r+i);
    puts("PASS collection player motion/ground: later source, stable equal hits, original small-body exclusion; vehicle admission retained");return 0;
}
int main(void) {
    rf_geomod_piece_registry *registries[2]={0};scene_terrain_authored_assets assets[2]={{0}};
    scene_terrain_source_owner sources[2];scene_stream scene={0};rf_geomod_registry_hit hit,sentinel;
    rf_geomod_piece_batch *batches[2];rf_geomod_owned_piece piece;rf_physics_body *bodies[2];
    float start[3]={6,0,0},delta[3]={-8,0,0};uint32_t found,i,size;unsigned char *before,*after;
    CHECK(!cube(0,.25f,registries));CHECK(!cube(4,.25f,registries+1));
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
    CHECK(!player_sources());CHECK(!notify_sources());
    puts("PASS multi-source weapon queries: nearer later source, stable ties, selected alias, isolated damage and atomic misses/errors");return 0;
}
