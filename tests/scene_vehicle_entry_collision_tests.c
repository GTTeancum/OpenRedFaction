/* Production entry adapter + real swept sphere composition/registry ownership.
 * Only material metadata and the final floor-classification probe are supplied
 * by this fixture; no installed assets, scene boot or gameplay replay. */
#define rf_geometry_body_surface entry_test_surface
#include "../src/diagnostic/scene.c"
#undef rf_geometry_body_surface
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"entry collision line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int entry_test_surface(void *context,uint32_t solid,uint32_t face,uint32_t *texture,uint32_t *material)
{(void)context;(void)solid;(void)face;*texture=0;*material=3;return RF_OK;}
static const float unit_basis[9]={1,0,0,0,1,0,0,0,1};
static void entry_cube(rf_geomod_vertex *v,rf_geomod_face *f,rf_collision_face_filter *filters)
{
    const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t side=0;side<2;side++){
        uint32_t n=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        f[n]=(rf_geomod_face){n*4,4,0,n};filters[n].face_flags=256;
        for(uint32_t j=0;j<4;j++){uint32_t k=side!=1?j:3-j;
            v[n*4+j].position[axis]=side?10.f:-10.f;
            v[n*4+j].position[a]=u[k]*10.f;v[n*4+j].position[b]=w[k]*10.f;}
    }
}
static uint32_t floor_dynamic,floor_calls;
static int entry_floor(void *context,const rf_collision_body_query *q,rf_geometry_body_hit *h,uint32_t *found)
{
    (void)context;memset(h,0,sizeof(*h));*found=0;
    if(q->end[1]<q->start[1]-.01f && q->end[0]==q->start[0] && q->end[2]==q->start[2]){
        ++floor_calls;*found=1;h->contact.fraction=.1f;h->contact.normal[1]=1;
        h->solid=h->room=floor_dynamic?UINT32_MAX:0;h->contact.object_id=123;
    }
    return RF_OK;
}
int main(void)
{
    static scene_stream stream;static scene_driller_runtime runtime;static scene_driller_resources resource;
    static scene_passive_vehicle passive;static campaign_npc_body npc;
    static rf_scene_world_geometry follows;static rf_geometry geometry;static rf_surface_materials palette;
    const rf_geometry *sources[1]={&geometry};uint32_t offsets[1]={0},slots[1]={0};
    rf_geomod_vertex vertices[24]={{0}};rf_geomod_face faces[6];rf_collision_face_filter filters[6]={{0}},generated={0};
    rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view terrain_view;rf_geomod_mesh_view mesh;
    rf_geometry_collision_world world={0};rf_geometry_collision_room owned={0};rf_collision_room_view room={0};
    uint32_t primary=0,indices[6]={0,1,2,3,4,5},found,baseline_found,clear,host;
    rf_collision_body_sphere query_sphere={{0,0,0},.5f};rf_physics_sphere actor_sphere={{0,0,0},.5f,0,0};
    rf_collision_body_query q={0};rf_geometry_body_hit hit={0},baseline;scene_vehicle_pose pose={0};
    rf_model_attachment tag={0};scene_driller_entry_exit *entry=&runtime.entry;
    entry_cube(vertices,faces,filters);mesh=(rf_geomod_mesh_view){vertices,faces,24,6,0};generated.face_flags=256;
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,1,4096,800,1024*1024,&terrain));
    CHECK(!rf_geomod_terrain_get(terrain,&terrain_view));owned.tree=*terrain_view.tree;owned.tree.source_indices=indices;
    room.tree=&owned.tree;world.rooms=&owned;world.views=&room;world.primary=&primary;world.primary_count=world.room_count=1;
    for(uint32_t i=0;i<3;i++){world.minimum[i]=room.minimum[i]=-10;world.maximum[i]=room.maximum[i]=10;}
    geometry.faces=6;geometry.textures=1;follows.geometry_count=follows.material_count=1;follows.offsets=offsets;follows.slots=slots;
    actor_follow_world=&follows;campaign_surface_sources=sources;campaign_surface_palette=&palette;
    stream.collision=&world;stream.driller_runtime=&runtime;stream.driller=&resource;
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&entry->view,&entry->registration));host=entry->registration.handle;
    entry->entities=&campaign_entities;entry->host.handle=host;entry->player.handle=campaign_player_object.handle;
    entry->player_body=&scene_actor_body;entry->resource=&resource;entry->physics=&runtime.physics;
    runtime.damage.active=1;runtime.damage.entry=entry;runtime.damage.damage.state.effects.handle=host;
    scene_actor_body.allocated_bytes=sizeof(actor_sphere);scene_actor_body.spheres.items=&actor_sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.orientation,unit_basis,36);memcpy(runtime.physics.state.orientation,unit_basis,36);
    /* A live active hull and the player's own body lie on the swept segment;
     * neither is an exit obstacle. The closed world wall still wins. */
    scene_actor_body.state.position[0]=2;runtime.physics.sphere_count=1;runtime.physics.collision[0].radius=1;
    runtime.physics.state.position[0]=3;runtime.physics.radius=1;
    q.spheres=&query_sphere;q.count=1;q.radius=.5f;q.flags=4;q.limit=1;q.end[0]=20;memcpy(q.matrix,unit_basis,36);
    CHECK(!campaign_body_query_batch_for(&world,&q,&baseline,&baseline_found,NULL,&campaign_movers)&&baseline_found);
    scene_vehicle_entry_collision_bind(&stream);CHECK(entry->collision.query==scene_vehicle_entry_query&&entry->exit_clear==scene_vehicle_entry_exit_clear);
    CHECK(!entry->collision.query(entry->collision.query_context,&q,&hit,&found)&&found&&!memcmp(&hit,&baseline,sizeof(hit)));
    /* Another real passive owner blocks; hidden and stale owners do not. */
    CHECK(!rf_object_registry_insert(&campaign_registry,&passive,&passive.handle));passive.resource_kind=0;passive.pose.position[0]=4;
    memcpy(passive.pose.input_matrix,unit_basis,36);stream.passive_vehicle_sphere_count[0]=1;stream.passive_vehicle_spheres[0][0].radius=1;
    campaign_passive_vehicles=&passive;campaign_passive_vehicle_count=1;
    CHECK(!scene_vehicle_entry_query(&stream,&q,&hit,&found)&&found&&hit.contact.object_id==passive.handle&&fabsf(hit.contact.fraction-.125f)<1e-6f);
    passive.damage.object_flags=2;
    CHECK(!scene_vehicle_entry_query(&stream,&q,&hit,&found)&&found&&!memcmp(&hit,&baseline,sizeof(hit)));passive.damage.object_flags=0;
    CHECK(!rf_object_registry_remove(&campaign_registry,passive.handle));
    CHECK(!scene_vehicle_entry_query(&stream,&q,&hit,&found)&&found&&!memcmp(&hit,&baseline,sizeof(hit)));campaign_passive_vehicle_count=0;
    /* NPC actual spheres block; a nearer wall must retain all world metadata. */
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&npc.view,&npc.registration));
    npc.body.allocated_bytes=sizeof(actor_sphere);npc.body.spheres.items=&actor_sphere;npc.body.spheres.count=1;
    memcpy(npc.body.state.orientation,unit_basis,36);npc.body.state.position[0]=2;
    campaign_npc_bodies=&npc;campaign_npc_body_count=1;
    CHECK(!scene_vehicle_entry_query(&stream,&q,&hit,&found)&&found&&hit.contact.object_id==npc.registration.handle&&fabsf(hit.contact.fraction-.05f)<1e-6f);
    npc.body.state.position[0]=14;
    CHECK(!scene_vehicle_entry_query(&stream,&q,&hit,&found)&&found&&!memcmp(&hit,&baseline,sizeof(hit)));
    npc.body.state.position[0]=2;npc.object_flags=0x4000;
    CHECK(!scene_vehicle_entry_query(&stream,&q,&hit,&found)&&found&&!memcmp(&hit,&baseline,sizeof(hit)));
    npc.object_flags=0;memcpy(pose.basis,unit_basis,36);pose.position[0]=2;
    CHECK(!scene_vehicle_entry_exit_clear(&stream,&pose,&clear)&&!clear); /* endpoint overlap */
    pose.position[0]=0;CHECK(!scene_vehicle_entry_exit_clear(&stream,&pose,&clear)&&clear);
    campaign_npc_body_count=0;campaign_passive_vehicle_count=0;
    /* Exercise the actual exit routine: an upward dynamic contact is not a
     * floor. Identical static support is admitted, so rejection is specific. */
    strcpy(resource.model,"entry-fixture");strcpy(tag.name,"interface_1");tag.rotation[3]=1;tag.position[1]=2;tag.parent=-1;
    resource.tags.items=&tag;resource.tags.count=1;resource.seat=0;resource.physics.authored.movement_index=1;
    memset(runtime.physics.state.position,0,12);memcpy(entry->session.saved.pose.basis,unit_basis,36);
    entry->collision.query=entry_floor;entry->collision.query_context=NULL;entry->exit_clear=NULL;
    floor_dynamic=1;floor_calls=0;
    CHECK(!scene_driller_exit_pose(entry,entry->player.handle,host,&pose,&clear)&&!clear&&floor_calls==4);
    floor_dynamic=0;floor_calls=0;
    CHECK(!scene_driller_exit_pose(entry,entry->player.handle,host,&pose,&clear)&&clear&&floor_calls==1);
    rf_geomod_terrain_close(&terrain);
    puts("PASS vehicle entry composition: world metadata, own-owner exclusion, visible/live passive and NPC blocking, final fit and dynamic-floor rejection");return 0;
}
