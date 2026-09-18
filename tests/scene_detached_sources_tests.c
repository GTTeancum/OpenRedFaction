#include "rf/geomod_piece_bank.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int runtime_surfaces(void) {
    scene_terrain_publication_owner *p=calloc(1,sizeof(*p));scene_stream scene={0};
    rf_geometry geometry={0};rf_geomod_authored_post_view asset={0};
    rf_geomod_vertex vertices[3]={{{0,0,1},{0,0}},{{1,0,1},{1,0}},{{0,1,1},{0,1}}};
    rf_geomod_face face={0,3,1,549};rf_geomod_publication_origin origin={2,94,549,UINT32_MAX};
    rf_collision_face_filter filter={0,256,-1,1,0,0};
    uint32_t offsets[2]={0,2},slots[2]={1,0},texture=99,material=99,count;
    unsigned char indices[2]={7,3};rf_material items[2]={0};rf_materials materials={0};
    rf_geometry_materials mapping={0};const rf_geometry *geometries[1]={&geometry};
    rf_geometry_body_surfaces body={0};const rf_geometry_runtime_surface *rows=NULL;
    scene_stream *old=scene_actor_collision_owner;
    CHECK(p);geometry.faces=10;geometry.textures=2;
    asset.neighbors=(rf_geomod_mesh_view){vertices,&face,3,1,0};asset.neighbor_origins=&origin;asset.neighbor_filters=&filter;
    CHECK(!scene_publication_runtime_import(p,&geometry,&asset));CHECK(p->runtime_count==1 && p->runtime_vertices==3);
    CHECK(!scene_publication_runtime_import(p,&geometry,&asset));CHECK(p->runtime_count==1);
    origin.owner=93;CHECK(scene_publication_runtime_import(p,&geometry,&asset)==RF_FORMAT);origin.owner=94;
    scene.terrain_publication=p;scene.geometry=&geometry;scene.surface_indices=indices;
    materials.items=items;materials.count=2;scene.materials=&materials;scene_actor_collision_owner=&scene;
    mapping.offsets=offsets;mapping.slots=slots;mapping.count=1;mapping.textures.count=2;
    body.geometries=geometries;body.count=1;body.mapping=&mapping;
    CHECK(!scene_runtime_material_rows(&scene,&rows,&count) && count==1 && rows[0].id==559);
    CHECK(!campaign_body_surface(&body,UINT32_MAX,559,&texture,&material));CHECK(texture==0 && material==3);
    texture=material=99;CHECK(campaign_body_surface(&body,UINT32_MAX,560,&texture,&material)==RF_NOT_FOUND);
    CHECK(texture==99 && material==99);
    vertices[0].position[0]=7;vertices[0].uv[0]=8;CHECK(rows[0].vertices[0][0]==0 && rows[0].uv[0][0]==0);
    CHECK(p->runtime_filters[0].face_flags==256 && p->runtime_filters[0].property_34==-1);
    scene_actor_collision_owner=old;free(p);return 0;
}
static int cube_batches(float x,float extent,uint32_t batches,rf_geomod_piece_registry **out) {
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
    for(i=0;i<batches;i++) {
        uint32_t released;
        CHECK(!rf_geomod_piece_registry_begin(*out,0));
        for(j=0;j<=i;j++)CHECK(!rf_geomod_piece_registry_emit(&mesh,map,filters,6,1,j,*out));
        if(i==31)CHECK(rf_geomod_piece_registry_emit(&mesh,map,filters,6,2,0,*out)==RF_RANGE);
        rf_geomod_piece_registry_commit(*out);
        /* Exercise historical growth under the existing2MiB cap, retaining
         * batches16/31 for the two extended-range targeting controls. */
        if(i && i-1!=16) {
            CHECK(!rf_geomod_piece_registry_damage(*out,i-1,0,400));
            CHECK(!rf_geomod_piece_registry_collect_retired(*out,&released));
        }
    }
    return 0;
}
static int cube(float x,float extent,rf_geomod_piece_registry **out)
{return cube_batches(x,extent,1,out);}
static int extended_batches(void) {
    rf_geomod_piece_registry *r=NULL;scene_stream scene={0};scene_terrain_source_owner sources[2]={{0}};
    rf_geomod_registry_hit hit;float start[3]={2,0,0},delta[3]={-4,0,0};uint32_t found,i;
    CHECK(!cube_batches(0,.25f,32,&r));sources[1].pieces=r;scene.terrain_sources=sources;scene.terrain_source_count=2;
    CHECK(rf_geomod_piece_registry_count(r)==32);
    {
        uint32_t size,count=99;unsigned char *snapshot;
        rf_geomod_changed_box boxes[33],sentinel;
        memset(&sentinel,0xa5,sizeof(sentinel));boxes[32]=sentinel;
        CHECK(!rf_geomod_piece_registry_changed_boxes(r,0,boxes,&count) && count==32);
        CHECK(!memcmp(boxes+32,&sentinel,sizeof(sentinel)));
        CHECK(!rf_geomod_piece_registry_state_size(r,&size));snapshot=malloc(size);CHECK(snapshot);
        CHECK(!rf_geomod_piece_registry_state_encode(r,snapshot,size));
        CHECK(!rf_geomod_piece_registry_state_decode(r,snapshot,size));free(snapshot);
    }
    for(i=0;i<16;i++)CHECK(!rf_geomod_piece_registry_damage(r,i,0,400));
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found));CHECK(found && hit.batch==80);
    CHECK(!scene_detached_sources_damage(&scene,hit.batch,0,400));
    for(i=17;i<31;i++)CHECK(!rf_geomod_piece_registry_damage(r,i,0,400));
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found));CHECK(found && hit.batch==95);
    CHECK(!scene_detached_sources_damage(&scene,hit.batch,0,400));
    CHECK(!scene_detached_sources_sweep(&scene,4,start,delta,0,1,&hit,&found) && !found);
    CHECK(scene_detached_sources_damage(&scene,128,0,400)==RF_RANGE);
    for(i=0;i<4;i++)for(uint32_t b=0;b<32;b++) {
        uint32_t tag=scene_detached_batch_tag(i,b);
        CHECK(scene_detached_tag_source(tag)==i && scene_detached_tag_batch(tag)==b);
        if(b<16)CHECK(tag==i*16+b);
    }
    rf_geomod_piece_registry_close(&r);return 0;
}
static int enemy_fragment_shots(void) {
    scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_geomod_piece_registry *registry=NULL;rf_geomod_piece_batch *batch;
    float start[3]={2,0,0},delta[3]={-4,0,0};uint32_t blocked,size;unsigned char *before,*after;
    CHECK(!cube(0,.25f,&registry));scene.detached_pieces=registry;scene.collision=&world;
    CHECK(!rf_geomod_piece_registry_get(registry,0,&batch));
    CHECK(!rf_geomod_piece_registry_state_size(registry,&size));before=malloc(size);after=malloc(size);CHECK(before && after);
    CHECK(!rf_geomod_piece_registry_state_encode(registry,before,size));
    /* Visibility and a nearer actor must never damage the chunk. */
    CHECK(!combat_shot_obstructed(&scene,start,delta,1,&blocked) && blocked);
    CHECK(!combat_enemy_fragment_shot(&scene,start,delta,.1f,400,&blocked) && !blocked);
    CHECK(!rf_geomod_piece_registry_state_encode(registry,after,size));CHECK(!memcmp(before,after,size));
    /* A front wall protects the fragment; a rear wall must not protect it. */
    for(uint32_t rear=0;rear<2;rear++) {
        float x=rear?-1:1,vertices[4][3]={{x,-2,-2},{x,2,-2},{x,2,2},{x,-2,2}};
        rf_collision_face face={0};rf_geometry_collision_room room={0};rf_collision_room_view view={0};uint32_t primary=0;
        face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-x;
        face.minimum[0]=face.maximum[0]=x;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
        CHECK(!rf_collision_tree_open(&face,1,65536,&room.tree));view.tree=&room.tree;
        memcpy(view.minimum,room.tree.nodes[0].minimum,12);memcpy(view.maximum,room.tree.nodes[0].maximum,12);
        world.rooms=&room;world.views=&view;world.room_count=world.primary_count=1;world.primary=&primary;
        CHECK(!combat_enemy_fragment_shot(&scene,start,delta,1,10,&blocked) && blocked);
        CHECK(!rf_geomod_piece_registry_state_encode(registry,after,size));
        CHECK(rear?memcmp(before,after,size)!=0:memcmp(before,after,size)==0);
        rf_collision_tree_close(&room.tree);memset(&world,0,sizeof(world));
    }

    CHECK(rf_geomod_piece_batch_alive(batch,0));
    CHECK(!rf_geomod_piece_registry_state_encode(registry,after,size));CHECK(memcmp(before,after,size));
    /* A high direct hit retires; the next ray passes through the removed piece. */
    CHECK(!combat_enemy_fragment_shot(&scene,start,delta,1,400,&blocked) && blocked);
    CHECK(!rf_geomod_piece_batch_alive(batch,0));
    CHECK(!combat_enemy_fragment_shot(&scene,start,delta,1,400,&blocked) && !blocked);
    free(before);free(after);rf_geomod_piece_registry_close(&registry);return 0;
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
/* Deliberately synthetic large fragments: validates polygon-path support
 * publication, not a claim that the live post replay extracts these cubes. */
static int moving_piece_support(void) {
    scene_stream scene={0};scene_terrain_source_owner sources[2]={{0}};
    rf_geomod_piece_registry *registry=NULL;rf_geomod_piece_batch *batch;
    rf_geomod_owned_piece piece;rf_physics_body *body;scene_piece_support support;
    rf_physics_body_state state={0};actor_ground_record ground={0};rf_geometry_body_hit contact={0};
    CHECK(!cube(4,.8f,&registry));sources[1].pieces=registry;
    scene.terrain_sources=sources;scene.terrain_source_count=2;scene.terrain_publication_serial=7;
    scene_actor_collision_owner=&scene;campaign_spawn=1;
    CHECK(!rf_geomod_piece_registry_get(registry,0,&batch));
    CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,&body));
    {
        rf_geometry_collision_world world={0};rf_collision_body_query query={0};
        rf_collision_body_sphere query_sphere={{0,0,0},.6f};rf_physics_sphere sphere={{0,0,0},.6f};uint32_t k,found=0;
        scene.collision=&world;memset(&scene_actor_body,0,sizeof(scene_actor_body));
        scene_actor_body.allocated_bytes=sizeof(scene_actor_body);scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
        scene_actor_body.state.mass=10;scene_actor_body.state.bounds.radius=.6f;scene_actor_body.state.flags=0x8000003f;
        query.start[0]=query.end[0]=4;query.start[1]=2;query.end[1]=-2;
        query.spheres=&query_sphere;query.count=1;query.limit=1;query.radius=.6f;
        for(k=0;k<3;k++)scene_actor_body.state.orientation[k*3+k]=scene_actor_body.state.next_orientation[k*3+k]=query.matrix[k][k]=1;
        memcpy(scene_actor_body.state.position,query.start,12);memcpy(scene_actor_body.state.next_position,query.end,12);
        CHECK(!rf_physics_body_prepare_sweep(&scene_actor_body.state));
        memset(&support,0,sizeof(support));
        CHECK(!campaign_player_piece_query(&world,&query,NULL,&contact,&found,&support));
        CHECK(found && support.tag==17 && support.piece==0 && support.serial==7 && support.registry==registry);
        memset(&scene_actor_body,0,sizeof(scene_actor_body));scene.collision=NULL;
    }
    CHECK(campaign_piece_support_body(&support)==body);
    body->state.velocity[0]=.25f;body->state.velocity[1]=.5f;body->state.velocity[2]=-.125f;
    state.position[0]=state.next_position[0]=4;state.position[1]=state.next_position[1]=-1;state.bounds.radius=.6f;
    ground.probe.start[1]=2;ground.probe.end[1]=-2;ground.hit.hit.fraction=.5f;
    contact.solid=UINT32_MAX;memcpy(contact.contact.velocity,body->state.velocity,12);
    CHECK(!actor_support_commit(&state,&ground,&contact,0,&support));
    CHECK(state.position[1]==.05f && (state.flags&0x400000u));
    CHECK(campaign_piece_support.tag==17 && campaign_support_handle==0);
    body->state.velocity[0]=1.25f;body->state.velocity[1]=-.25f;
    campaign_player_support_refresh(&state,1);
    CHECK(!memcmp(campaign_support_velocity,body->state.velocity,12));
    CHECK(state.flags&0x80000000u);
    {
        rf_physics_body_state carried=state,stationary=state;float zero[3]={0},normal[3]={0,1,0};uint32_t k;
        carried.mass=stationary.mass=10;
        CHECK(!rf_physics_run_propose(&carried,.125f,5,10,1,zero,normal,campaign_support_velocity));
        CHECK(!rf_physics_run_propose(&stationary,.125f,5,10,1,zero,normal,zero));
        for(k=0;k<3;k++)CHECK(fabsf(carried.next_position[k]-stationary.next_position[k]-.125f*campaign_support_velocity[k])<.000001f);
    }
    memset(body->state.velocity,0,12);campaign_player_support_refresh(&state,1);
    CHECK(campaign_support_velocity[0]==0 && campaign_support_velocity[1]==0 && campaign_support_velocity[2]==0);
    /* Unrelated edits/owner replacement cannot revive a borrowed support ID. */
    scene.terrain_publication_serial=8;campaign_player_support_refresh(&state,1);
    CHECK(!campaign_piece_support.tag && !campaign_piece_support_body(&support));
    scene.terrain_publication_serial=7;support.registry=NULL;CHECK(!campaign_piece_support_body(&support));
    support.registry=registry;campaign_piece_support=support;
    CHECK(!rf_geomod_piece_registry_damage(registry,0,0,400));
    campaign_player_support_refresh(&state,1);CHECK(!campaign_piece_support.tag);
    CHECK(!campaign_piece_support_body(&support));
    scene_actor_collision_owner=NULL;campaign_spawn=0;memset(&campaign_piece_support,0,sizeof(campaign_piece_support));
    memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));rf_geomod_piece_registry_close(&registry);
    puts("PASS moving rubble support: rising commit, refreshed carry, stopped velocity, retired and replaced identity");return 0;
}
static int large_support_snap(void) {
    scene_stream scene={0};scene_terrain_source_owner sources[2]={{0}};
    rf_geomod_piece_registry *r[2]={0};rf_geomod_piece_batch *batch;
    rf_geomod_owned_piece piece;rf_physics_body *body;
    rf_physics_sphere sphere={{0,0,0},.6f};
    actor_ground_record ground={0};rf_geometry_body_hit contact={0};
    uint32_t slot,landing,clear,k;rf_physics_body_state state;
    CHECK(!cube(0,.8f,r));CHECK(!cube(4,.8f,r+1));
    for(slot=0;slot<2;slot++) {
        sources[slot].pieces=r[slot];CHECK(!rf_geomod_piece_registry_get(r[slot],0,&batch));
        CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,&body));
        CHECK(body->state.bounds.radius>1); /* Actual polygon dispatch boundary. */
        body->state.flags&=~0x80000000u;
    }
    scene.terrain_sources=sources;scene.terrain_source_count=2;
    campaign_spawn=1;scene_actor_collision_owner=&scene;
    memset(&scene_actor_body,0,sizeof(scene_actor_body));
    scene_actor_body.allocated_bytes=sizeof(scene_actor_body);
    scene_actor_body.spheres.items=&sphere;scene_actor_body.spheres.count=1;
    ground.probe.start[1]=2;ground.probe.end[1]=-2;ground.hit.hit.fraction=.5f;
    contact.solid=UINT32_MAX;
    for(slot=0;slot<2;slot++)for(landing=0;landing<2;landing++)for(clear=0;clear<2;clear++) {
        memset(&state,0,sizeof(state));state.mass=10;state.flags=0x8000003f;
        state.position[0]=state.next_position[0]=4.0f*slot;
        state.position[1]=state.next_position[1]=2;
        state.position[2]=state.next_position[2]=clear?2:0;
        state.bounds.radius=.6f;
        for(k=0;k<3;k++)state.orientation[k*3+k]=state.next_orientation[k*3+k]=1;
        CHECK(!rf_physics_body_prepare_sweep(&state));
        CHECK(!actor_support_commit(&state,&ground,&contact,landing,NULL));
        /* Exact flat surface height + player sphere radius, or unobstructed
         * original support proposal. Bounds and pending pose must agree. */
        CHECK(fabsf(state.position[1]-(clear?.05f:1.4f))<.0001f);
        CHECK(state.next_position[1]==state.position[1]);
        CHECK(fabsf(state.bounds.minimum[1]-(state.position[1]-.6f))<.0001f);
        CHECK(fabsf(state.bounds.maximum[1]-(state.position[1]+.6f))<.0001f);
    }
    scene_actor_collision_owner=NULL;campaign_spawn=0;memset(&scene_actor_body,0,sizeof(scene_actor_body));
    for(slot=0;slot<2;slot++)rf_geomod_piece_registry_close(r+slot);
    puts("PASS large-fragment polygon support snap: both source slots, landing/support and unobstructed descent controls");return 0;
}
static int beam_selection(void) {
    rf_level level={0};float before[3];
    strcpy(level.entry.name,"ctf06.rfl");
    CHECK(!rf_scene_authored_post_place_group(&level,95,1));
    CHECK(scene_authored_source_uid==95 && scene_authored_source_count==1);
    memcpy(before,level.player_position,sizeof(before));
    CHECK(!rf_scene_authored_post_place_group(&level,95,2));
    CHECK(scene_authored_source_uid==95 && scene_authored_source_count==2);
    CHECK(!rf_scene_authored_post_place_group(&level,95,3));
    CHECK(scene_authored_source_uid==95 && scene_authored_source_count==3);
    CHECK(rf_scene_authored_post_place_group(&level,94,3)==RF_RANGE);
    CHECK(rf_scene_authored_post_place_group(&level,95,4)==RF_RANGE);
    CHECK(!memcmp(before,level.player_position,sizeof(before)) && scene_authored_source_count==3);
    CHECK(!rf_scene_authored_post_place(&level));
    CHECK(scene_authored_source_uid==94 && scene_authored_source_count==1);
    return 0;
}
static int inspection_camera(void) {
    float eye[3]={-3,2.25f,0},target[3]={-5,1.5f,2.5f},saved[3][3];
    CHECK(!rf_scene_inspection_camera(eye,target) && scene_inspection_enabled);
    memcpy(saved,scene_inspection_basis,sizeof(saved));
    for(uint32_t i=0;i<3;i++)for(uint32_t j=0;j<3;j++) {
        float dot=0;for(uint32_t k=0;k<3;k++)dot+=saved[i][k]*saved[j][k];
        CHECK(fabsf(dot-(i==j?1.0f:0.0f))<1e-6f);
    }
    CHECK(rf_scene_inspection_camera(eye,eye)==RF_RANGE);
    CHECK(rf_scene_inspection_camera(NULL,target)==RF_RANGE);
    target[0]=NAN;CHECK(rf_scene_inspection_camera(eye,target)==RF_RANGE);
    CHECK(!memcmp(saved,scene_inspection_basis,sizeof(saved)) && scene_inspection_enabled);
    CHECK(!rf_scene_inspection_camera(NULL,NULL) && !scene_inspection_enabled);return 0;
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
    CHECK(!scene_detached_tick(&scene,scene_step_seconds));CHECK(rf_scene_detached_motion[0]==2 && rf_scene_detached_motion[3]==2);
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
    CHECK(!inspection_camera());CHECK(!extended_batches());CHECK(!enemy_fragment_shots());CHECK(!beam_selection());CHECK(!runtime_surfaces());CHECK(!player_sources());CHECK(!notify_sources());CHECK(!large_support_snap());CHECK(!moving_piece_support());
    puts("PASS multi-source weapon queries: nearer later source, stable ties, selected alias, isolated damage and atomic misses/errors");return 0;
}
