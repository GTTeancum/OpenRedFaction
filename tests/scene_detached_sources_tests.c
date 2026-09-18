#include "rf/geomod_piece_bank.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static int moving_surface_material(void *context,uint32_t solid,uint32_t face,uint32_t *texture,uint32_t *material)
{if(context)return RF_IO;if(solid || face)return RF_FORMAT;*texture=7;*material=3;return RF_OK;}
typedef struct support_fixture {const rf_geomod_mesh_view *mesh;const rf_geometry_collision_movers *movers;int error;} support_fixture;
static int current_support_query(const rf_physics_body_state *body,rf_physics_solid_hit *out,uint32_t *found,void *opaque)
{
    support_fixture *fixture=opaque;rf_geometry_body_hit hit;int status;
    if(fixture->error)return RF_IO;
    status=scene_fragment_shape_mover_sweep(fixture->mesh,body,fixture->movers,1,&hit,found);if(status)return status;
    if(*found){memset(out,0,sizeof(*out));out->fraction=hit.contact.fraction;memcpy(out->point,hit.contact.point,12);memcpy(out->normal,hit.contact.normal,12);}
    return RF_OK;
}
static int moving_surface_contacts(void)
{
    rf_geomod_vertex vertices[4]={{{-.5f,-.25f,-.5f},{0,0}},{{.5f,-.25f,-.5f},{0,0}},
        {{.5f,-.25f,.5f},{0,0}},{{-.5f,-.25f,.5f},{0,0}}};
    rf_geomod_face mesh_face={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&mesh_face,4,1,0};
    float points[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    rf_collision_face face={0};rf_geometry_collision_flat flat={0};rf_group_attached_pose pose={0};
    rf_collision_solid_view view={0};rf_geometry_collision_movers movers={0};scene_mover_interval interval;
    rf_physics_body_state body={0};rf_geometry_body_hit hit={0},saved;uint32_t i,found=0;
    face.vertices=points;face.count=4;face.plane[1]=1;
    face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
    flat.faces=&face;flat.count=1;view.object_id=77;
    movers.owned=&flat;movers.poses=&pose;movers.views=&view;movers.count=1;
    for(i=0;i<3;i++)pose.input_matrix[i*4]=body.orientation[i*4]=body.next_orientation[i*4]=1;
    CHECK(!scene_mover_intervals_capture(&movers,&interval,1,0));pose.position[1]=2;
    for(i=0;i<3;i++){pose.minimum[i]=-3;pose.maximum[i]=3;}
    pose.minimum[1]=1.999f;pose.maximum[1]=2.001f;pose.velocity[1]=16;
    CHECK(!scene_mover_intervals_capture(&movers,&interval,1,1));
    body.position[1]=body.next_position[1]=1;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL));
    CHECK(found && hit.contact.fraction==.375f && hit.contact.point[1]==.75f && hit.contact.normal[1]==1);
    CHECK(hit.solid==0 && hit.contact.object_id==77 && hit.contact.material==3 && hit.contact.texture==7 && hit.contact.velocity[1]==16);
    saved=hit;found=0;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.0625f,&hit,&found,moving_surface_material,NULL));
    CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));
    hit.contact.fraction=.2f;saved=hit;found=1;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL));
    CHECK(found && !memcmp(&hit,&saved,sizeof(hit)));
    found=0;CHECK(scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,&body)==RF_IO);
    CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));
    interval.handle=78;CHECK(scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL)==RF_FORMAT);
    CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));interval.handle=77;
    interval.end[1]=pose.position[1]=-2;pose.minimum[1]=-2.001f;pose.maximum[1]=-1.999f;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL));
    CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));
    interval.end[1]=pose.position[1]=2;pose.minimum[1]=1.999f;pose.maximum[1]=2.001f;
    /* Narrow surface: no fragment corner enters it; reciprocal face contact. */
    for(i=0;i<4;i++){points[i][0]*=.01f;points[i][2]*=.01f;}
    face.minimum[0]=face.minimum[2]=-.02f;face.maximum[0]=face.maximum[2]=.02f;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL));
    CHECK(found && hit.contact.fraction==.375f && hit.contact.point[1]==.75f);
    /* Crossed rectangles: only the swept edge route intersects. */
    for(i=0;i<4;i++) {
        points[i][0]=points[i][0]<0?-.25f:.25f;points[i][2]=points[i][2]<0?-1:1;
        vertices[i].position[0]*=2;vertices[i].position[2]*=.5f;
    }
    face.minimum[0]=-.25f;face.maximum[0]=.25f;face.minimum[2]=-1;face.maximum[2]=1;found=0;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL));
    CHECK(found && hit.contact.fraction==.375f && hit.contact.point[1]==.75f && hit.contact.normal[1]==1);
    saved=hit;pose.flags|=0x40000u;found=0;
    CHECK(!scene_fragment_moving_sweep(&mesh,&body,&movers,&interval,.125f,.125f,&hit,&found,moving_surface_material,NULL));
    CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));
    {
        rf_physics_body_state before;rf_group_attached_pose floor;rf_geometry_collision_movers current=movers;
        support_fixture fixture={&mesh,&current,0};uint32_t wake=77,flags=0;float position[3],basis[9];rf_physics_solid_step_report report;
        pose.flags=0;interval.end[1]=pose.position[1]=-2;pose.minimum[1]=-2.001f;pose.maximum[1]=-1.999f;
        body.position[1]=body.next_position[1]=.25f;body.flags=0x1800003fu;body.mass=1;body.coefficients[0]=.8f;
        for(i=0;i<3;i++)body.local_tensor[i*4]=body.world_tensor[i*4]=1;
        floor=pose;current.poses=&floor;before=body;
        CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));
        CHECK(wake==1 && !memcmp(&body,&before,sizeof(body)));
        /* Another current floor at the old height prevents an unnecessary wake. */
        floor.position[1]=0;floor.minimum[1]=-.001f;floor.maximum[1]=.001f;
        CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));CHECK(!wake);
        wake=77;fixture.error=1;
        CHECK(scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake)==RF_IO);
        CHECK(wake==77 && !memcmp(&body,&before,sizeof(body)));fixture.error=0;floor=pose;
        body.flags|=0x80000000u;memcpy(position,body.position,12);memcpy(basis,body.orientation,36);
        CHECK(!rf_physics_fragment_step(&body,1.f/60,9.8f,&flags,position,basis,current_support_query,&fixture,&report));
        CHECK(body.position[1]<before.position[1] && body.velocity[1]<0 && !report.contacts);
        /* Sideways withdrawal uses the retained old pose, not the final AABB. */
        body=before;pose.position[1]=interval.end[1]=0;
        pose.minimum[1]=-.001f;pose.maximum[1]=.001f;
        for(i=0;i<4;i++) {
            static const float translations[4]={.1f,.5f,2.f,-2.f};
            float x=translations[i];
            pose.position[0]=interval.end[0]=x;
            pose.minimum[0]=x-.25f;pose.maximum[0]=x+.25f;
            pose.minimum[2]=-1;pose.maximum[2]=1;floor=pose;
            CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));
            CHECK(wake==(i>=2) && !memcmp(&body,&before,sizeof(body)));
        }
        /* Small frame-by-frame motion must still cross the wake boundary. */
        for(uint32_t axis=0;axis<2;axis++) {
            uint32_t woke=0;
            memset(interval.start,0,12);memset(interval.end,0,12);memset(pose.position,0,12);
            for(uint32_t frame=1;frame<=2000;frame++) {
                float displacement=axis?-(float)frame*.001f:(float)frame*.001f;
                interval.start[axis]=pose.position[axis];
                interval.end[axis]=pose.position[axis]=displacement;
                pose.minimum[0]=pose.position[0]-.25f;pose.maximum[0]=pose.position[0]+.25f;
                pose.minimum[1]=pose.position[1]-.001f;pose.maximum[1]=pose.position[1]+.001f;
                floor=pose;
                CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));
                CHECK(!memcmp(&body,&before,sizeof(body)));
                if(wake){woke=frame;break;}
            }
            CHECK(axis?(woke>=4 && woke<=6):(woke>=1249 && woke<=1251));
        }
        memset(interval.start,0,12);memset(interval.end,0,12);memset(pose.position,0,12);
        pose.minimum[1]=-.001f;pose.maximum[1]=.001f;
        /* A distant mover must not wake this body or even call the world query. */
        interval.start[0]=20;interval.end[0]=pose.position[0]=22;
        pose.minimum[0]=21.75f;pose.maximum[0]=22.25f;floor=pose;fixture.error=1;
        CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));CHECK(!wake);
        /* Excluded, unchanged and rotating intervals do not enter translation wake. */
        interval.start[0]=0;pose.flags=0x40000u;
        CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));CHECK(!wake);
        pose.flags=0;interval.changed=0;
        CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));CHECK(!wake);
        interval.changed=2;
        CHECK(!scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake));CHECK(!wake);
        interval.changed=1;interval.handle=78;wake=77;
        CHECK(scene_fragment_support_loss(&mesh,&body,&movers,&interval,current_support_query,&fixture,&wake)==RF_FORMAT);
        CHECK(wake==77 && !memcmp(&body,&before,sizeof(body)));
    }
    return 0;
}
static int mover_intervals(void)
{
    rf_group_attached_pose poses[2]={{0}};rf_collision_solid_view views[2]={{0}};
    rf_geometry_collision_movers movers={0};scene_mover_interval items[2],saved[2];uint32_t i,j;
    movers.count=2;movers.poses=poses;movers.views=views;
    for(i=0;i<2;i++) {
        views[i].object_id=77+i;poses[i].position[0]=(float)i;
        for(j=0;j<3;j++)poses[i].input_matrix[j*4]=1;
        poses[i].pending[0]=999;poses[i].public_position[0]=-999;
    }
    CHECK(!scene_mover_intervals_capture(&movers,items,2,0));
    CHECK(items[0].handle==77 && items[1].handle==78 && !items[0].changed && !items[1].changed);
    poses[0].position[0]=2;poses[1].input_matrix[0]=poses[1].input_matrix[4]=0;
    poses[1].input_matrix[1]=1;poses[1].input_matrix[3]=-1;
    CHECK(!scene_mover_intervals_capture(&movers,items,2,1));
    CHECK(items[0].start[0]==0 && items[0].end[0]==2 && items[0].changed==1);
    CHECK(items[1].changed==2 && items[1].matrix[0]==1 && items[1].end_matrix[0]==0);
    {
        rf_collision_mover_motion motion={0};rf_collision_mover_relative relative;
        memcpy(motion.mover_start,items[0].start,12);memcpy(motion.mover_end,items[0].end,12);
        memcpy(motion.mover_matrix,items[0].matrix,36);
        for(j=0;j<3;j++)motion.body_matrix[j][j]=motion.next_body_matrix[j][j]=1;
        motion.body_remaining=.0625f;motion.mover_remaining=.125f;motion.start[0]=motion.end[0]=3;
        CHECK(!rf_collision_mover_relative_sphere(&motion,&relative));
        CHECK(relative.origin[0]==1 && relative.start[0]==2 && relative.delta[0]==-1);
    }
    memcpy(saved,items,sizeof(items));views[1].object_id=79;poses[0].position[0]=3;
    CHECK(scene_mover_intervals_capture(&movers,items,2,1)==RF_FORMAT && !memcmp(saved,items,sizeof(items)));
    views[1].object_id=78;poses[1].input_matrix[2]=NAN;
    CHECK(scene_mover_intervals_capture(&movers,items,2,0)==RF_FORMAT && !memcmp(saved,items,sizeof(items)));
    poses[1].input_matrix[2]=0;
    CHECK(scene_mover_intervals_capture(&movers,items,1,0)==RF_RANGE && !memcmp(saved,items,sizeof(items)));
    CHECK(!scene_mover_intervals_capture(&movers,items,2,0));poses[0].position[0]=2;
    CHECK(!scene_mover_intervals_capture(&movers,items,2,1));
    CHECK(items[0].start[0]==3 && items[0].end[0]==2 && items[0].changed==1);
    CHECK(!scene_mover_intervals_capture(&movers,items,2,0));
    CHECK(!scene_mover_intervals_capture(&movers,items,2,1) && !items[0].changed);
    movers.count=0;CHECK(!scene_mover_intervals_capture(&movers,NULL,0,0));return 0;
}
/* Standalone numerical fixture entry; gameplay shares preparation across all
 * reciprocal vertices in each world/mover query. */
static int scene_detached_vertex_sweep(const rf_geomod_mesh_view *mesh,
    const rf_physics_body_state *body,const float point[3],float limit,
    rf_collision_ray_hit *out,uint32_t *matched)
{
    scene_fragment_path path;int status;
    status=scene_fragment_path_prepare(body,&path);if(status)return status;
    status=scene_fragment_shape_prepare(mesh,&scene_fragment_shape_scratch);if(status)return status;
    return scene_detached_vertex_sweep_prepared(mesh,body,point,limit,out,matched,&path,&scene_fragment_shape_scratch);
}
static int fragment_test_material(void *context,uint32_t solid,uint32_t face,uint32_t *texture,uint32_t *material) {
    if(context)return RF_IO;
    if(solid!=UINT32_MAX || face!=17)return RF_FORMAT;
    *texture=9;*material=3;return RF_OK;
}
static int fragment_thin_obstacle(void) {
    rf_geomod_vertex vertices[4]={{{-.5f,-.5f,-.5f},{0,0}},{{.5f,-.5f,-.5f},{0,0}},
        {{.5f,-.5f,.5f},{0,0}},{{-.5f,-.5f,.5f},{0,0}}};
    rf_geomod_face face={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&face,4,1,0};
    float patch[4][3]={{-.02f,0,-.02f},{-.02f,0,.02f},{.02f,0,.02f},{.02f,0,-.02f}};
    rf_collision_face obstacle={0};rf_physics_body_state body={0};
    rf_collision_ray_hit contact;uint32_t i,j,hit;float delta[3]={0,-2,0};
    obstacle.vertices=patch;obstacle.count=4;obstacle.plane[1]=1;
    for(i=0;i<3;i++){obstacle.minimum[i]=-.0201f;obstacle.maximum[i]=.0201f;}
    body.position[1]=1;body.next_position[1]=-1;
    for(i=0;i<3;i++)body.orientation[i*4]=body.next_orientation[i*4]=1;
    /* Eight fully occupied half-unit grid spheres still have gaps between
     * them. A 0.04-wide finite patch is inside the face, outside every sweep. */
    for(i=0;i<8;i++) {
        float start[3];rf_collision_sweep_hit sphere;
        for(j=0;j<3;j++)start[j]=((i>>j)&1)?.25f:-.25f;start[1]+=1;
        CHECK(!rf_collision_sweep_face(&obstacle,start,delta,delta,.25f,1,&sphere,&hit));CHECK(!hit);
    }
    for(i=0;i<4;i++) {
        float start[3];for(j=0;j<3;j++)start[j]=vertices[i].position[j]+body.position[j];
        CHECK(!rf_collision_thin_face(&obstacle,start,delta,1,&contact,&hit));CHECK(!hit);
    }
    for(i=0;i<4;i++) {
        CHECK(!scene_detached_vertex_sweep(&mesh,&body,patch[i],1,&contact,&hit));
        CHECK(hit && fabsf(contact.fraction-.25f)<1e-6f && contact.normal[1]>.999f);
        CHECK(!memcmp(contact.point,patch[i],12));
    }
    CHECK(!scene_detached_vertex_sweep(&mesh,&body,patch[0],.2f,&contact,&hit));CHECK(!hit);
    {float outside[3]={2,0,0};CHECK(!scene_detached_vertex_sweep(&mesh,&body,outside,1,&contact,&hit));CHECK(!hit);}
    {
        rf_geometry_collision_world world={0};rf_geometry_collision_room room={0};rf_collision_room_view view={0};
        scene_stream scene={0};scene_detached_query_context query={0};rf_geometry_body_hit result={0},saved;
        uint32_t primary=0,found=0;
        CHECK(!rf_collision_tree_open(&obstacle,1,65536,&room.tree));room.tree.source_indices[0]=17;
        view.tree=&room.tree;for(i=0;i<3;i++){view.minimum[i]=-3;view.maximum[i]=3;}
        world.rooms=&room;world.views=&view;world.room_count=1;world.primary=&primary;world.primary_count=1;
        scene.collision=&world;query.scene=&scene;query.mesh=&mesh;body.bounds.radius=.001f; /* Proxy bound deliberately smaller than the mesh. */
        CHECK(!rf_physics_body_prepare_sweep(&body));
        CHECK(!scene_detached_world_vertex_sweep(&query,&body,&result,&found,fragment_test_material,NULL));
        CHECK(found && fabsf(result.contact.fraction-.25f)<1e-6f && result.contact.normal[1]>.999f);
        CHECK(result.face==17 && result.room==0 && result.contact.texture==9 && result.contact.material==3);
        /* Room suppression and authored non-solid flags must match body casts. */
        found=0;view.skip=1;
        CHECK(!scene_detached_world_vertex_sweep(&query,&body,&result,&found,fragment_test_material,NULL));CHECK(!found);
        view.skip=0;room.tree.faces[0].filter.face_flags=0x40;
        CHECK(!scene_detached_world_vertex_sweep(&query,&body,&result,&found,fragment_test_material,NULL));CHECK(!found);
        room.tree.faces[0].filter.face_flags=0;saved=result;
        CHECK(scene_detached_world_vertex_sweep(&query,&body,&result,&found,fragment_test_material,&found)==RF_IO);
        CHECK(!found && !memcmp(&saved,&result,sizeof(result)));
        rf_collision_tree_close(&room.tree);
    }
    body.next_position[1]=2;
    CHECK(!scene_detached_vertex_sweep(&mesh,&body,patch[0],1,&contact,&hit));CHECK(!hit);
    /* Stationary center, 60-degree roll: the obstacle enters the face interior
     * in the third angular interval, not through a translation-only ray. */
    body.position[1]=body.next_position[1]=.6f;
    body.next_orientation[0]=body.next_orientation[4]=.5f;
    body.next_orientation[1]=.866025404f;body.next_orientation[3]=-.866025404f;
    {float origin[3]={0,0,0};
     CHECK(!scene_detached_vertex_sweep(&mesh,&body,origin,1,&contact,&hit));
     CHECK(hit && contact.fraction>.5f && contact.fraction<.6f && contact.normal[1]>.8f);}
    return 0;
}
static int fragment_crossed_edges(void) {
    rf_geomod_vertex vertices[4]={{{-1,-.5f,-.25f},{0,0}},{{1,-.5f,-.25f},{0,0}},{{1,-.5f,.25f},{0,0}},{{-1,-.5f,.25f},{0,0}}};
    rf_geomod_face polygon={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&polygon,4,1,0};
    float patch[4][3]={{-.25f,0,-1},{-.25f,0,1},{.25f,0,1},{.25f,0,-1}};
    rf_collision_face face={0};rf_geometry_collision_room room={0};rf_collision_room_view view={0};
    rf_geometry_collision_world world={0};rf_physics_body_state body={0};scene_detached_query_context query={0};
    rf_geometry_body_hit hit={0},saved;scene_stream *scene=calloc(1,sizeof(*scene));uint32_t i,primary=0,found=0;
    CHECK(scene);face.vertices=patch;face.count=4;face.plane[1]=1;
    face.minimum[0]=-.25f;face.maximum[0]=.25f;face.minimum[2]=-1;face.maximum[2]=1;
    for(i=0;i<3;i++){view.minimum[i]=-5;view.maximum[i]=5;body.orientation[i*4]=body.next_orientation[i*4]=1;}
    body.position[1]=1;body.next_position[1]=-1;body.bounds.radius=.001f;
    CHECK(!rf_collision_tree_open(&face,1,4096,&room.tree));room.tree.source_indices[0]=17;view.tree=&room.tree;
    world.rooms=&room;world.views=&view;world.primary=&primary;world.primary_count=world.room_count=1;
    scene->collision=&world;query.scene=scene;query.mesh=&mesh;
    CHECK(!scene_detached_world_vertex_sweep(&query,&body,&hit,&found,fragment_test_material,NULL));
    CHECK(found && hit.contact.fraction==.25f && hit.contact.point[1]==0 && hit.contact.normal[1]==1 && hit.face==17 && hit.contact.material==3);
    hit.contact.fraction=.2f;saved=hit;
    CHECK(!scene_detached_world_vertex_sweep(&query,&body,&hit,&found,fragment_test_material,NULL));CHECK(found && !memcmp(&hit,&saved,sizeof(hit)));
    found=0;view.skip=1;
    CHECK(!scene_detached_world_vertex_sweep(&query,&body,&hit,&found,fragment_test_material,NULL));CHECK(!found);view.skip=0;
    CHECK(scene_detached_world_vertex_sweep(&query,&body,&hit,&found,fragment_test_material,&found)==RF_IO);
    CHECK(!found && !memcmp(&hit,&saved,sizeof(hit)));
    rf_collision_tree_close(&room.tree);free(scene);return 0;
}
static int fragment_mover_material(void *context,uint32_t solid,uint32_t face,uint32_t *texture,uint32_t *material) {
    if(context)return RF_IO;
    if(solid!=0 || face!=0)return RF_FORMAT;
    *texture=11;*material=7;return RF_OK;
}
static int fragment_mover_contact(void) {
    rf_geomod_vertex vertices[4]={{{-.5f,-.5f,-.5f},{0,0}},{{.5f,-.5f,-.5f},{0,0}},
        {{.5f,-.5f,.5f},{0,0}},{{-.5f,-.5f,.5f},{0,0}}};
    rf_geomod_face polygon={0,4,0,0};rf_geomod_mesh_view mesh={vertices,&polygon,4,1,0};
    float patch[4][3]={{-.02f,0,-.02f},{-.02f,0,.02f},{.02f,0,.02f},{.02f,0,-.02f}};
    rf_collision_face face={0};rf_geometry_collision_flat flat={0};rf_group_attached_pose pose={0};
    rf_collision_solid_view view={0};rf_geometry_collision_movers movers={0};rf_physics_body_state body={0};
    rf_geometry_body_hit result={0},saved;uint32_t i,mode,found=0;
    face.vertices=patch;face.count=4;face.plane[1]=1;flat.faces=&face;flat.count=1;
    movers.count=1;movers.owned=&flat;movers.views=&view;movers.poses=&pose;view.object_id=77;
    pose.position[0]=3;pose.position[1]=2;
    for(i=0;i<3;i++){pose.minimum[i]=-10;pose.maximum[i]=10;pose.velocity[i]=(float)(i+1);pose.public_position[i]=99;pose.output_matrix[i*4]=-1;}
    for(mode=0;mode<2;mode++) {
        memset(pose.input_matrix,0,36);
        if(!mode)for(i=0;i<3;i++)pose.input_matrix[i*4]=1;
        else {pose.input_matrix[1]=1;pose.input_matrix[3]=-1;pose.input_matrix[8]=1;}
        memcpy(body.orientation,pose.input_matrix,36);memcpy(body.next_orientation,pose.input_matrix,36);
        for(i=0;i<3;i++){body.position[i]=pose.position[i]+pose.input_matrix[3+i];body.next_position[i]=pose.position[i]-pose.input_matrix[3+i];}
        found=0;
        CHECK(!scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&result,&found,fragment_mover_material,NULL));
        CHECK(found && fabsf(result.contact.fraction-.25f)<1e-6f && result.solid==0 && result.room==UINT32_MAX);
        CHECK(result.face==0 && result.contact.object_id==77 && result.contact.texture==11 && result.contact.material==7);
        CHECK(!memcmp(result.contact.velocity,pose.velocity,12));
        CHECK(mode?result.contact.normal[0]<-.999f:result.contact.normal[1]>.999f);
        CHECK(fabsf(result.contact.point[mode?0:1]-pose.position[mode?0:1])<1e-6f);
        saved=result;found=0;pose.flags=0x40000;
        CHECK(!scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&result,&found,fragment_mover_material,NULL));
        CHECK(!found && !memcmp(&saved,&result,sizeof(result)));pose.flags=0;
        face.filter.face_flags=0x40;
        CHECK(!scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&result,&found,fragment_mover_material,NULL));CHECK(!found);
        face.filter.face_flags=0;
        CHECK(scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&result,&found,fragment_mover_material,&found)==RF_IO);
        CHECK(!found && !memcmp(&saved,&result,sizeof(result)));
        /* A preexisting nearer contact wins without changing its identity. */
        result.contact.fraction=.1f;saved=result;found=1;
        CHECK(!scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&result,&found,fragment_mover_material,NULL));
        CHECK(found && !memcmp(&saved,&result,sizeof(result)));
        /* Updating the committed pose changes contact time; unrelated ray pose
         * and velocity values must not be used to extrapolate geometry. */
        for(i=0;i<3;i++)pose.position[i]+=.25f*pose.input_matrix[3+i];found=0;
        CHECK(!scene_detached_mover_vertex_sweep(&mesh,&body,&movers,&result,&found,fragment_mover_material,NULL));
        CHECK(found && fabsf(result.contact.fraction-.125f)<1e-6f);
        for(i=0;i<3;i++)pose.position[i]-=.25f*pose.input_matrix[3+i];
    }
    return 0;
}
static int fragment_cache_fallback(void) {
    rf_geomod_vertex vertices[129*3]={0};rf_geomod_face faces[129];
    rf_geomod_mesh_view mesh={vertices,faces,129*3,129,0};rf_physics_body_state body={0};
    rf_collision_ray_hit hit;uint32_t i,j,found;
    const float triangle[3][3]={{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{0,-.5f,.5f}};
    const float point[3]={0,0,0};
    for(i=0;i<129;i++) {
        faces[i]=(rf_geomod_face){i*3,3,0,i};
        for(j=0;j<3;j++){memcpy(vertices[i*3+j].position,triangle[j],12);if(i<128)vertices[i*3+j].position[0]+=10+i*2;}
    }
    for(i=0;i<3;i++)body.orientation[i*4]=body.next_orientation[i*4]=1;
    body.position[1]=1;body.next_position[1]=-1;
    CHECK(!scene_detached_vertex_sweep(&mesh,&body,point,1,&hit,&found));
    CHECK(!scene_fragment_shape_scratch.complete && found && fabsf(hit.fraction-.25f)<1e-6f);
    {scene_fragment_path path;float a[3]={-.25f,0,-1},b[3]={-.25f,0,1},normal[3]={0,1,0};
     CHECK(!scene_fragment_path_prepare(&body,&path));CHECK(!scene_fragment_edges_prepare(&mesh,&path));CHECK(!scene_fragment_edges.complete);
     CHECK(!scene_detached_edge_sweep(&mesh,&path,a,b,normal,1,&hit,&found));CHECK(found && hit.fraction==.25f);}
    /* Only the uncached 129th triangle can hit. Reusing storage after an edit
     * must not retain the previous prepared geometry. */
    for(j=0;j<3;j++)vertices[128*3+j].position[0]+=20;
    CHECK(!scene_detached_vertex_sweep(&mesh,&body,point,1,&hit,&found));CHECK(!found);
    {scene_fragment_path path;float a[3]={-.25f,0,-1},b[3]={-.25f,0,1},normal[3]={0,1,0};
     CHECK(!scene_fragment_path_prepare(&body,&path));CHECK(!scene_fragment_edges_prepare(&mesh,&path));
     CHECK(!scene_detached_edge_sweep(&mesh,&path,a,b,normal,1,&hit,&found));CHECK(!found);}
    return 0;
}
static int fragment_plane_side(void) {
    rf_geomod_vertex vertices[3]={{{0,0,0},{0,0}},{{1,-1,0},{0,0}},{{0,-1,1},{0,0}}};
    rf_geomod_mesh_view mesh={0};float position[3]={0,0,0},point[3]={0,0,0},normal[3]={0,1,0};
    float basis[9]={1,0,0,0,1,0,0,0,1};mesh.vertices=vertices;mesh.vertex_count=3;
    /* A parent top face touched by the back of the newborn piece cannot
     * support it; the floor beneath the piece must still be admitted. */
    CHECK(scene_detached_plane_extent(&mesh,position,basis,point,normal)==0);
    point[1]=-2;CHECK(scene_detached_plane_extent(&mesh,position,basis,point,normal)==2);
    point[1]=0;position[1]=.5f;CHECK(scene_detached_plane_extent(&mesh,position,basis,point,normal)==.5f);
    position[1]=-1;CHECK(scene_detached_plane_extent(&mesh,position,basis,point,normal)==-1);
    return 0;
}
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
static int support_loss_scene_tick(void) {
    scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_geomod_piece_registry *registry=NULL;rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;rf_physics_body *body;
    float points[4][3]={{-2,0,-2},{-2,0,2},{2,0,2},{2,0,-2}};
    rf_collision_face face={0};rf_geometry_collision_flat flat={0};rf_collision_solid_view view={0};
    rf_group_attached_pose pose={0},filtered;scene_mover_interval interval;
    rf_geometry_collision_movers saved_movers=campaign_movers;
    scene_mover_interval *saved_intervals=campaign_mover_intervals;
    rf_group_attached_pose *saved_filtered=campaign_fragment_mover_poses;
    float saved_duration=campaign_mover_interval_seconds,saved_gravity=scene_gravity.acceleration;
    uint32_t saved_count=campaign_mover_translation_count;rf_physics_body_state before;
    rf_scene_world_geometry empty_render={0};rf_surface_materials empty_palette={0};const rf_geometry *empty_geometry=NULL;
    const rf_scene_world_geometry *saved_render=actor_follow_world;
    rf_surface_materials *saved_palette=campaign_surface_palette;const rf_geometry **saved_sources=campaign_surface_sources;
    rf_collision_body_mover scratch;rf_collision_body_mover *saved_scratch=campaign_sweep_scratch;
    actor_follow_world=&empty_render;campaign_surface_palette=&empty_palette;campaign_surface_sources=&empty_geometry;campaign_sweep_scratch=&scratch;
    CHECK(!cube(0,.25f,&registry));scene.detached_pieces=registry;scene.collision=&world;
    CHECK(!rf_geomod_piece_registry_get(registry,0,&batch));CHECK(!rf_geomod_piece_batch_get(batch,0,&piece,&body));
    body->state.flags&=~0x80000000u;before=body->state;
    face.vertices=points;face.count=4;face.plane[1]=1;
    face.minimum[0]=face.minimum[2]=-2;face.maximum[0]=face.maximum[2]=2;
    flat.faces=&face;flat.count=1;view.object_id=77;
    memset(&campaign_movers,0,sizeof(campaign_movers));campaign_movers.count=1;
    campaign_movers.poses=&pose;campaign_movers.views=&view;campaign_movers.owned=&flat;
    for(uint32_t k=0;k<3;k++){pose.input_matrix[k*4]=1;pose.minimum[k]=-2;pose.maximum[k]=2;}
    pose.position[1]=-.25f;
    CHECK(!scene_mover_intervals_capture(&campaign_movers,&interval,1,0));
    pose.position[1]=-2;pose.minimum[1]=-2.001f;pose.maximum[1]=-1.999f;
    CHECK(!scene_mover_intervals_capture(&campaign_movers,&interval,1,1));
    filtered=pose;filtered.flags|=0x40000u;campaign_fragment_mover_poses=&filtered;
    campaign_mover_intervals=&interval;campaign_mover_interval_seconds=1.f/60;campaign_mover_translation_count=1;
    scene_gravity.acceleration=9.8f;
    interval.handle=78;
    CHECK(scene_detached_tick(&scene,1.f/60)==RF_FORMAT);
    CHECK(!memcmp(&body->state,&before,sizeof(before)));interval.handle=77;
    CHECK(!scene_detached_tick(&scene,1.f/60));
    CHECK((body->state.flags&0x80000000u) && body->state.position[1]<before.position[1] && body->state.velocity[1]<0);
    CHECK(rf_scene_detached_motion[0]==1 && rf_scene_detached_motion[2]==0 && rf_scene_detached_motion[3]==0);
    /* A later ordinary frame must continue falling rather than re-settling. */
    before=body->state;campaign_mover_interval_seconds=0;campaign_mover_translation_count=0;
    CHECK(!scene_detached_tick(&scene,1.f/60));CHECK(body->state.position[1]<before.position[1]);
    campaign_movers=saved_movers;campaign_mover_intervals=saved_intervals;campaign_fragment_mover_poses=saved_filtered;
    campaign_mover_interval_seconds=saved_duration;campaign_mover_translation_count=saved_count;scene_gravity.acceleration=saved_gravity;
    actor_follow_world=saved_render;campaign_surface_palette=saved_palette;campaign_surface_sources=saved_sources;campaign_sweep_scratch=saved_scratch;
    rf_geomod_piece_registry_close(&registry);return 0;
}
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
static int rocket_object_contacts(void) {
    scene_stream scene={0};rf_geometry_collision_world world={0};campaign_npc_body npc={0};
    rf_collision_room_liquid_view water={0};world.liquids=&water;
    rf_physics_sphere sphere={0};rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{1,-2,-2},{1,2,-2},{1,2,2},{1,-2,2}};
    float start[3]={3,0,0},delta[3]={-6,0,0};uint32_t matched,liquid;
    rf_weapon_flight_contact contact={0};rf_collision_ray_hit hit;
    rf_geomod_piece_registry *pieces=NULL;
    scene.collision=&world;npc.body.allocated_bytes=1;npc.body.spheres.items=&sphere;npc.body.spheres.count=1;
    sphere.radius=.5f;npc.body.state.orientation[0]=npc.body.state.orientation[4]=npc.body.state.orientation[8]=1;
    npc.registration.view=&npc.view;npc.damage.effects.health=100;
    campaign_npc_bodies=&npc;campaign_npc_body_count=1;
    CHECK(scene_rocket_body_hit(&npc.body,start,delta,.2f,1,&hit));
    CHECK(fabsf(hit.fraction-(3-.7f)/6)<.000001f && fabsf(hit.point[0]-.5f)<.000001f && hit.normal[0]==1);
    start[1]=.6f;CHECK(!rf_physics_body_segment(&npc.body,start,delta,1,&hit.fraction));
    CHECK(scene_rocket_body_hit(&npc.body,start,delta,.2f,1,&hit));start[1]=0;
    CHECK(!scene_rocket_sweep(&scene,start,delta,.2f,0x1004,&contact,&liquid,&matched));
    CHECK(matched && !liquid && contact.object==SCENE_ACTOR_ROCKET_OWNER);
    for(uint32_t i=0;i<3;i++) {
        npc.object_flags=i==0?2:i==1?0x4000:0;npc.damage.effects.health=i==2?0:100;
        CHECK(!scene_rocket_sweep(&scene,start,delta,.2f,0x1004,&contact,&liquid,&matched) && !matched);
    }
    npc.object_flags=0;npc.damage.effects.health=100;
    face.vertices=vertices;face.count=4;face.plane[0]=1;face.plane[3]=-1;
    face.minimum[0]=face.maximum[0]=1;face.minimum[1]=face.minimum[2]=-2;face.maximum[1]=face.maximum[2]=2;
    mover.flat_faces=&face;mover.flat_count=1;
    for(uint32_t k=0;k<3;k++){mover.input_matrix[k][k]=mover.output_matrix[k][k]=1;mover.minimum[k]=-2;mover.maximum[k]=2;}
    campaign_movers.views=&mover;campaign_movers.count=1;
    CHECK(!scene_rocket_sweep(&scene,start,delta,.2f,0x1004,&contact,&liquid,&matched));
    CHECK(matched && contact.object==SCENE_MOVER_ROCKET_OWNER && fabsf(contact.hit.point[0]-1)<.000001f);
    mover.input_origin[1]=mover.output_origin[1]=5;
    CHECK(!scene_rocket_sweep(&scene,start,delta,.2f,0x1004,&contact,&liquid,&matched));
    CHECK(matched && contact.object==SCENE_ACTOR_ROCKET_OWNER); /* Raised door no longer blocks. */
    mover.input_origin[1]=mover.output_origin[1]=0;
    /* Actual rocket flags1004 mean WORLD coordinates. The mover adapter must
     * clear bit4 before querying local geometry, then transform contact back. */
    for(uint32_t angle=0;angle<16;angle++) {
        float radians=(float)(angle*6.283185307179586/16),c=cosf(radians),sn=sinf(radians);
        float rotated_start[3]={7+3*c,-2,3-3*sn},rotated_delta[3]={-6*c,0,6*sn};
        memset(mover.input_matrix,0,36);mover.input_matrix[0][0]=c;mover.input_matrix[0][2]=-sn;
        mover.input_matrix[1][1]=1;mover.input_matrix[2][0]=sn;mover.input_matrix[2][2]=c;
        memcpy(mover.output_matrix,mover.input_matrix,36);
        mover.input_origin[0]=mover.output_origin[0]=7;mover.input_origin[1]=mover.output_origin[1]=-2;
        mover.input_origin[2]=mover.output_origin[2]=3;
        CHECK(!scene_rocket_sweep(&scene,rotated_start,rotated_delta,.2f,0x1004,&contact,&liquid,&matched));
        CHECK(matched && contact.object==SCENE_MOVER_ROCKET_OWNER && !liquid);
        CHECK(fabsf(contact.hit.fraction-.3f)<.00001f);
        CHECK(fabsf(contact.hit.point[0]-(7+c))<.00001f && fabsf(contact.hit.point[1]+2)<.00001f && fabsf(contact.hit.point[2]-(3-sn))<.00001f);
        CHECK(fabsf(contact.hit.normal[0]-c)<.00001f && fabsf(contact.hit.normal[1])<.00001f && fabsf(contact.hit.normal[2]+sn)<.00001f);
    }
    memset(mover.input_origin,0,12);memset(mover.output_origin,0,12);memset(mover.input_matrix,0,36);
    for(uint32_t k=0;k<3;k++)mover.input_matrix[k][k]=1;memcpy(mover.output_matrix,mover.input_matrix,36);
    CHECK(!cube(1.8f,.25f,&pieces));scene.detached_pieces=pieces;
    CHECK(!scene_rocket_sweep(&scene,start,delta,.2f,0x1004,&contact,&liquid,&matched));
    CHECK(matched && contact.object==SCENE_DETACHED_ROCKET_OWNER); /* Rubble precedes door/actor. */
    {rf_geometry_collision_room room={0};rf_collision_room_view view={0};uint32_t primary=0;
     float wall_vertices[4][3]={{2.5f,-2,-2},{2.5f,2,-2},{2.5f,2,2},{2.5f,-2,2}};
     rf_collision_face wall=face;wall.vertices=wall_vertices;wall.plane[3]=-2.5f;
     wall.minimum[0]=wall.maximum[0]=2.5f;
     CHECK(!rf_collision_tree_open(&wall,1,65536,&room.tree));view.tree=&room.tree;
     memcpy(view.minimum,room.tree.nodes[0].minimum,12);memcpy(view.maximum,room.tree.nodes[0].maximum,12);
     world.rooms=&room;world.views=&view;world.room_count=world.primary_count=1;world.primary=&primary;
     CHECK(!scene_rocket_sweep(&scene,start,delta,.2f,0x1004,&contact,&liquid,&matched));
     CHECK(matched && contact.object==UINT32_MAX && fabsf(contact.hit.point[0]-2.5f)<.000001f);
     rf_collision_tree_close(&room.tree);}
    rf_geomod_piece_registry_close(&pieces);campaign_npc_bodies=NULL;campaign_npc_body_count=0;
    memset(&campaign_movers,0,sizeof(campaign_movers));return 0;
}
static int enemy_fragment_shots(void) {
    scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_geomod_piece_registry *registry=NULL;rf_geomod_piece_batch *batch;
    float start[3]={2,0,0},delta[3]={-4,0,0};uint32_t blocked,size;unsigned char *before,*after;
    rf_weapon_primary_definition rifle={0},heavy={0},pistol={0},baton={0};
    rifle.damage=60;rifle.ai_damage_scale[0]=.4f;rifle.ai_damage_scale[1]=2;
    pistol.damage=40;pistol.ai_damage_scale[0]=1;
    baton.damage=60;baton.ai_damage_scale[0]=.1f;
    heavy.damage=800;heavy.ai_damage_scale[0]=.5f;
    CHECK(combat_enemy_primary_damage(NULL)==10);
    CHECK(combat_enemy_primary_damage(&rifle)==24);
    CHECK(combat_enemy_primary_damage(&pistol)==40);
    CHECK(combat_enemy_primary_damage(&baton)==6);
    CHECK(combat_enemy_primary_damage(&heavy)==400);
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
        CHECK(!combat_enemy_fragment_shot(&scene,start,delta,1,combat_enemy_primary_damage(&rifle),&blocked) && blocked);
        CHECK(!rf_geomod_piece_registry_state_encode(registry,after,size));
        CHECK(rear?memcmp(before,after,size)!=0:memcmp(before,after,size)==0);
        rf_collision_tree_close(&room.tree);memset(&world,0,sizeof(world));
    }

    CHECK(rf_geomod_piece_batch_alive(batch,0));
    CHECK(!rf_geomod_piece_registry_state_encode(registry,after,size));CHECK(memcmp(before,after,size));
    /* A high direct hit retires; the next ray passes through the removed piece. */
    CHECK(!combat_enemy_fragment_shot(&scene,start,delta,1,combat_enemy_primary_damage(&heavy),&blocked) && blocked);
    CHECK(!rf_geomod_piece_batch_alive(batch,0));
    CHECK(!combat_enemy_fragment_shot(&scene,start,delta,1,combat_enemy_primary_damage(&heavy),&blocked) && !blocked);
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
static int rotated_support_clearance(void) {
    rf_physics_body_state actor={0};rf_physics_body piece={0};
    rf_physics_sphere a={{0,0,0},.6f},b={{.25f,0,0},.4f};
    rf_physics_spheres spheres={0};float height;uint32_t k;
    spheres.items=&a;spheres.count=1;piece.spheres.items=&b;piece.spheres.count=1;
    for(k=0;k<3;k++)actor.orientation[k*3+k]=piece.state.orientation[k*3+k]=1;
    piece.state.bounds.radius=.8f;actor.position[1]=.9f;
    height=actor_piece_support_height(&actor,&spheres,&piece);
    CHECK(fabsf(height-(sqrtf(1-.25f*.25f)+.0001f))<.00001f);
    /* Rotate the offset sphere from X+.25 to Y+.25: top is now exactly1.25. */
    piece.state.orientation[0]=piece.state.orientation[4]=0;
    piece.state.orientation[1]=1;piece.state.orientation[3]=-1;
    height=actor_piece_support_height(&actor,&spheres,&piece);CHECK(fabsf(height-1.2501f)<.00001f);
    actor.position[1]=1.4f;CHECK(actor_piece_support_height(&actor,&spheres,&piece)==1.4f);
    actor.position[1]=-.5f;CHECK(actor_piece_support_height(&actor,&spheres,&piece)==-.5f);
    actor.position[1]=.9f;piece.state.bounds.radius=.5f;CHECK(actor_piece_support_height(&actor,&spheres,&piece)==.9f);
    piece.state.bounds.radius=1.01f;CHECK(actor_piece_support_height(&actor,&spheres,&piece)==.9f);
    puts("PASS rotated sphere support clearance and nonoverlap/underside/admission controls");return 0;
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
    {
        const uint32_t beams[2]={92,108},high[2]={79,103},low[2]={75,99};uint32_t i,posts[2];
        for(i=0;i<2;i++) {
            CHECK(rf_geomod_authored_beam_posts(beams[i],posts) && posts[0]==low[i] && posts[1]==high[i]);
            CHECK(!rf_scene_authored_post_place_group(&level,beams[i],3));
            CHECK(scene_authored_source_uid==beams[i] && scene_authored_source_count==3);
            CHECK(rf_scene_authored_post_place_group(&level,high[i],2)==RF_RANGE);
            CHECK(scene_authored_source_uid==beams[i] && scene_authored_source_count==3);
        }
    }
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
    CHECK(!moving_surface_contacts());
    CHECK(!support_loss_scene_tick());
    CHECK(!mover_intervals());
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
    CHECK(!rf_scene_fragment_contact_check());CHECK(rf_scene_fragment_contact_audit[1]==8 && rf_scene_fragment_contact_audit[3]==1);CHECK(!fragment_crossed_edges());CHECK(!fragment_cache_fallback());CHECK(!fragment_mover_contact());CHECK(!fragment_thin_obstacle());CHECK(!fragment_plane_side());CHECK(!inspection_camera());CHECK(!extended_batches());CHECK(!enemy_fragment_shots());CHECK(!rocket_object_contacts());CHECK(!beam_selection());CHECK(!runtime_surfaces());CHECK(!player_sources());CHECK(!notify_sources());CHECK(!rotated_support_clearance());CHECK(!large_support_snap());CHECK(!moving_piece_support());
    puts("PASS multi-source weapon queries: nearer later source, stable ties, selected alias, isolated damage and atomic misses/errors");return 0;
}
