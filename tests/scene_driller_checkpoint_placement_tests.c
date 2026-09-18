/* Actual Driller spheres against a complete pending closed-room composition. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_checkpoint_placement.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller placement line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void room_cube(rf_geomod_vertex *v,rf_geomod_face *f,rf_collision_face_filter *filters)
{
    uint32_t axis,side,j;const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++){
        uint32_t n=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        f[n]=(rf_geomod_face){n*4,4,0,n};filters[n].face_flags=256;
        for(j=0;j<4;j++){
            uint32_t k=side!=1?j:3-j;rf_geomod_vertex *p=v+n*4+j;
            p->position[axis]=side?20.f:-20.f;p->position[a]=u[k]*20.f;p->position[b]=w[k]*20.f;
        }
    }
}
int main(void)
{
    static scene_authored_collection_stage stage;static scene_terrain_lighting_stage lighting;
    static scene_terrain_publication_owner publication;scene_driller_physics physics,kept;
    scene_driller_resources *resources=NULL;rf_vpp meshes={0},maps[4]={{0}};
    rf_geomod_vertex vertices[24]={{0}};rf_geomod_face faces[6];rf_collision_face_filter filters[6]={{0}},generated={0};
    rf_geomod_mesh_view mesh;rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view view;
    rf_collision_room_view room={0};rf_geometry_collision_world world={0};rf_collision_composition_view pending;
    rf_vehicle_checkpoint record={0},saved;uint32_t ids[6]={0,1,2,3,4,5},root=0,i;
    float basis[9]={0,0,-1,0,1,0,1,0,0},position[3]={0};char path[128];
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resources));
    CHECK(!scene_driller_physics_initialize(resources,position,basis,9.8f,&physics));CHECK(physics.sphere_count==8);kept=physics;
    room_cube(vertices,faces,filters);mesh=(rf_geomod_mesh_view){vertices,faces,24,6,0};generated.face_flags=256;
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,1,4096,800,1024*1024,&terrain));
    CHECK(!rf_geomod_terrain_get(terrain,&view));
    CHECK(!rf_collision_composition_open(view.tree,ids,ids,6,32,1024*1024,NULL,&publication.composition));
    CHECK(!rf_collision_composition_prepare(publication.composition,view.faces,ids,6));
    CHECK(!rf_collision_composition_pending(publication.composition,&pending));publication.has_pending=1;
    room.tree=view.tree;world.views=&room;world.primary=&root;world.primary_count=world.room_count=1;
    stage.candidate.collision=&world;stage.candidate.terrain_publication=&publication;
    stage.candidate.terrain_sources=stage.owners;stage.candidate.terrain_source_count=2;
    stage.shared.owns_pending=1;stage.shared.lighting=&lighting;lighting.draw_ready=1;stage.shared.publication=view;
    /* Deliberately empty per-source view: only the full pending room is valid. */
    stage.shared.publication.mesh.face_count=0;stage.shared.publication.faces=NULL;stage.shared.publication.tree=NULL;
    record.health=900;record.alive=1;memcpy(record.orientation,basis,36);saved=record;
    CHECK(!scene_driller_checkpoint_placement(&stage,&physics,&record)); /* Floating 20m above floor. */
    record.position[0]=19;CHECK(scene_driller_checkpoint_placement(&stage,&physics,&record)==RF_NOT_FOUND);
    record=saved;record.position[0]=30;CHECK(scene_driller_checkpoint_placement(&stage,&physics,&record)==RF_NOT_FOUND);
    record=saved;CHECK(!scene_driller_checkpoint_placement(&stage,&physics,&record));
    stage.shared.scene=&stage.candidate;stage.candidate.terrain_source_count=1;
    CHECK(!scene_driller_checkpoint_placement_single(&stage.shared,&physics,&record));
    record.position[0]=19;CHECK(scene_driller_checkpoint_placement_single(&stage.shared,&physics,&record)==RF_NOT_FOUND);
    record=saved;stage.candidate.terrain_source_count=2;
    CHECK(!memcmp(&physics,&kept,sizeof(physics)) && !memcmp(&record,&saved,sizeof(record)));
    CHECK(!rf_collision_composition_pending(publication.composition,&pending)); /* Validation never commits. */
    publication.has_pending=0;CHECK(scene_driller_checkpoint_placement(&stage,&physics,&record)==RF_RANGE);
    rf_collision_composition_close(&publication.composition);rf_geomod_terrain_close(&terrain);
    scene_driller_resources_close(&resources);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("PASS actual eight-sphere Driller pending-room fit, airborne admission, wall/solid rejection, no mutation");return 0;
}
