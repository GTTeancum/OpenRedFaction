#define main checkpoint_world_reference_main
#include "scene_checkpoint_placement_probe.c"
#undef main
#include "../src/diagnostic/scene_checkpoint_world.inc"
static int material(void *context,uint32_t solid,uint32_t face,uint32_t *texture,uint32_t *value)
{(void)context;(void)solid;(void)face;*texture=0;*value=7;return RF_OK;}
int main(void)
{
    const float zero[3]={0};rf_geomod_mesh_view mesh;rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view view;
    rf_geometry_collision_world world={0};rf_geometry_collision_room owned={0};rf_collision_room_view room_view={0};
    uint32_t primary=0,indices[6]={0,1,2,3,4,5},i;rf_geometry_collision_movers movers={0};
    rf_checkpoint_placement p={0};rf_physics_sphere sphere={{0},.5f,0,0};scene_checkpoint_world_context context={0};
    rf_entity_room_state room,saved_room;rf_physics_support_contact support,saved_support;
    rf_clutter_base_owner prop={0},*props[1]={&prop};rf_clutter_class prop_class={0};rf_physics_sphere prop_sphere={{0},1,0,0};
    rf_geometry_collision_flat moving={0};rf_collision_solid_view moving_view={0};rf_group_attached_pose pose={0};rf_collision_body_mover scratch;
    cube(vertices,faces,zero,10,1);mesh=(rf_geomod_mesh_view){vertices,faces,24,6,0};generated.face_flags=256;
    CHECK(!rf_geomod_terrain_open(&mesh,filters,&generated,1,4096,800,1024*1024,&terrain));
    CHECK(!rf_geomod_terrain_get(terrain,&view));owned.tree=*view.tree;owned.tree.source_indices=indices;
    room_view.tree=&owned.tree;world.rooms=&owned;world.views=&room_view;world.room_count=world.primary_count=1;world.primary=&primary;
    for(i=0;i<3;i++){world.minimum[i]=room_view.minimum[i]=-10;world.maximum[i]=room_view.maximum[i]=10;p.basis[i*3+i]=1;}
    p.spheres=&sphere;p.count=1;p.query_flags=4;p.position[1]=-9.5f;
    context.world=&world;context.movers=&movers;context.metadata=material;context.dt=1.0f/60;context.class_speed=5;
    CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
    CHECK(room.room==1&&!memcmp(room.query_position,p.position,12)&&support.handle==0&&support.material==7);
    saved_room=room;saved_support=support;p.position[1]=0;
    CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
    CHECK(!memcmp(&room,&saved_room,sizeof(room))&&!memcmp(&support,&saved_support,sizeof(support)));
    context.allow_no_contact=1;
    CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
    CHECK(room.room==1&&support.handle==0&&support.material==-1&&room.query_position[1]==0);
    p.position[0]=9.75f;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
    {float authored[3]={9.75f,0,0};
     context.authored_static_unchanged=1;context.authored_position=authored;context.authored_basis=p.basis;
     CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
     CHECK(support.material==-1&&room.query_position[0]==9.75f);
     p.position[0]=9.76f;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
     p.position[0]=9.75f;context.authored_static_unchanged=0;
     CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
     context.authored_position=NULL;context.authored_basis=NULL;}
    p.position[0]=0;context.allow_no_contact=0;p.position[1]=-9.5f;
    prop.state.definition=&prop_class;prop_class.flags=2;memcpy(prop.state.position,p.position,12);
    prop.body.spheres.items=&prop_sphere;prop.body.spheres.count=1;
    for(i=0;i<3;i++)prop.matrix[i*3+i]=1;context.props=props;context.prop_count=1;
    CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
    {rf_clutter_base_owner authored_prop=prop,*authored_props[1]={&authored_prop};float authored_position[3];
     memcpy(authored_position,p.position,12);context.allow_no_contact=context.authored_static_unchanged=1;
     context.authored_position=authored_position;context.authored_basis=p.basis;context.authored_props=authored_props;
     CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
     p.position[0]=.01f;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);p.position[0]=0;
     prop.state.position[0]=.01f;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);prop.state.position[0]=0;
     prop.uid++;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);prop.uid--;
     context.allow_no_contact=context.authored_static_unchanged=0;context.authored_position=context.authored_basis=NULL;context.authored_props=NULL;}
    {rf_clutter_base_owner authored_prop=prop,*authored_props[1]={&authored_prop};
     float start[3]={0,-7,0};
     prop.uid=10469;authored_prop.uid=10469;prop_class.name="BustedEscapePod";
     context.authored_static_unchanged=1;context.authored_props=authored_props;
     context.spawn_shell_uid=10469;context.spawn_shell_position=start;
     CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
     prop.uid=13627;authored_prop.uid=13627;prop_class.name="Escape Pod";
     context.spawn_shell_uid=13627;
     CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
     prop.uid=13628;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);prop.uid=13627;
     prop_class.name="BustedEscapePod";prop.uid=10469;authored_prop.uid=10469;context.spawn_shell_uid=10469;
     start[0]=.126f;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);start[0]=0;
     start[1]=-6.49f;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);start[1]=-7;
     prop_class.name="Other";CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
     prop_class.name="BustedEscapePod";prop.state.position[0]=.01f;
     CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);prop.state.position[0]=0;
     context.spawn_shell_uid=0;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
     context.authored_static_unchanged=0;context.authored_props=NULL;context.spawn_shell_position=NULL;}
    prop.state.flags=2;CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));context.prop_count=0;
    cube(obstacle_vertices,obstacle_faces,zero,1,0);mesh=(rf_geomod_mesh_view){obstacle_vertices,obstacle_faces,24,6,0};
    CHECK(!rf_geomod_collision_faces(&mesh,filters,obstacle_positions,24,obstacle,6));
    moving.faces=obstacle;moving.count=6;movers.owned=&moving;movers.views=&moving_view;movers.poses=&pose;movers.count=1;
    context.scratch=&scratch;context.scratch_count=1;memcpy(pose.position,p.position,12);
    for(i=0;i<3;i++){pose.input_matrix[i*3+i]=1;pose.minimum[i]=-1;pose.maximum[i]=1;}
    CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
    pose.position[0]=5;CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
    CHECK(support.material==7&&room.room==1);
    saved_room=room;saved_support=support;context.metadata=NULL;
    CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_RANGE);
    CHECK(!memcmp(&room,&saved_room,sizeof(room))&&!memcmp(&support,&saved_support,sizeof(support)));
    context.metadata=material;p.spheres=NULL;p.count=0;p.position[1]=0;
    CHECK(!scene_checkpoint_world_place(&context,&p,&room,&support));
    CHECK(room.room==1&&!memcmp(room.query_position,p.position,12)&&support.handle==0&&support.material==-1);
    saved_room=room;saved_support=support;p.position[0]=100;
    CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_NOT_FOUND);
    CHECK(!memcmp(&room,&saved_room,sizeof(room))&&!memcmp(&support,&saved_support,sizeof(support)));
    p.position[0]=NAN;CHECK(scene_checkpoint_world_place(&context,&p,&room,&support)==RF_FORMAT);
    CHECK(!memcmp(&room,&saved_room,sizeof(room))&&!memcmp(&support,&saved_support,sizeof(support)));
    rf_geomod_terrain_close(&terrain);
    puts("PASS actual ordinary-world support/material/room and current mover/prop obstruction checks");return 0;
}
