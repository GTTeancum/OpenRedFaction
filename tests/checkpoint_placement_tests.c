/* Production placement API boundaries and physical detail eligibility. */
#define main checkpoint_synthetic_history_main
#include "scene_checkpoint_placement_probe.c"
#undef main
#include "rf/checkpoint_placement.h"
typedef struct extra_support {uint32_t stable,tie,calls;float up;int error;} extra_support;
static int support(void *opaque,const rf_physics_ground_probe *probe,float limit,rf_checkpoint_support_hit *out,uint32_t *matched)
{
 extra_support *c=opaque;(void)probe;c->calls++;if(c->error)return c->error;
 out->fraction=c->tie?limit:limit*.5f;out->normal[0]=out->normal[2]=0;out->normal[1]=c->up;
 out->stable=c->stable;*matched=1;return RF_OK;
}
int main(void)
{
 const float zero[3]={0,0,0},box[3]={-4,0,0};rf_geomod_mesh_view source,base;
 rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view view;
 rf_collision_tree trees[2]={{0}};rf_collision_room_view rooms[2]={{0}};uint32_t roots[1]={0},children[1]={1};
 rf_geometry_collision_world world={0};rf_checkpoint_placement p={0};rf_checkpoint_placement_result out,kept;
 rf_physics_sphere spheres[2]={{{0,0,0},.5f,0,0},{{0,2,0},.5f,0,0}};uint32_t i;
 cube(vertices,faces,zero,10,1);source=(rf_geomod_mesh_view){vertices,faces,24,6,0};generated.face_flags=256;
 cube(obstacle_vertices,obstacle_faces,box,1,0);base=(rf_geomod_mesh_view){obstacle_vertices,obstacle_faces,24,6,0};
 CHECK(!rf_geomod_collision_faces(&base,filters,obstacle_positions,24,obstacle,6));
 CHECK(!rf_geomod_terrain_open(&source,filters,&generated,1,4096,800,1024*1024,&terrain));CHECK(!rf_geomod_terrain_get(terrain,&view));
 trees[0]=*view.tree;trees[1].faces=obstacle;trees[1].face_count=6;
 rooms[0].tree=trees;rooms[0].child_count=1;rooms[1].tree=trees+1;
 world.views=rooms;world.primary=roots;world.primary_count=1;world.children=children;world.child_count=1;world.room_count=2;
 p.world=&world;p.spheres=spheres;p.count=2;p.query_flags=4;p.basis[0]=p.basis[4]=p.basis[8]=1;
 CHECK(!rf_checkpoint_placement_check(&view,&p,&out)&&out.sphere==UINT32_MAX&&out.reason==RF_CHECKPOINT_PLACEMENT_FITS);
 /* An initial ray through an open ceiling is inconclusive; other directions
  * can prove interior space. Exterior centers and real solid hits still fail. */
 {
  rf_collision_face open_faces[6];rf_geomod_terrain_view opened=view;
  memcpy(open_faces,view.faces,sizeof(open_faces));opened.faces=open_faces;
  for(i=0;i<6;i++)if(open_faces[i].plane[1]<-.99f)open_faces[i].filter.face_flags|=4;
  CHECK(!rf_checkpoint_placement_check(&opened,&p,&out)&&out.retries>0);
  p.position[1]=20;CHECK(rf_checkpoint_placement_check(&opened,&p,&out)==RF_NOT_FOUND);
  p.position[1]=0;p.position[0]=-4;
  CHECK(rf_checkpoint_placement_check(&opened,&p,&out)==RF_NOT_FOUND&&out.reason==RF_CHECKPOINT_PLACEMENT_SOLID);
  p.position[0]=0;
 }
 p.position[1]=7.5f;CHECK(!rf_checkpoint_placement_check(&view,&p,&out));p.position[1]=7.51f;
 CHECK(rf_checkpoint_placement_check(&view,&p,&out)==RF_NOT_FOUND&&out.sphere==1&&out.reason==RF_CHECKPOINT_PLACEMENT_SURFACE);
 p.position[1]=0;p.position[0]=-4;CHECK(rf_checkpoint_placement_check(&view,&p,&out)==RF_NOT_FOUND&&out.sphere==0&&out.reason==RF_CHECKPOINT_PLACEMENT_SOLID);
 /* Child skip byte is ignored by original body traversal. */
 rooms[1].skip=1;CHECK(rf_checkpoint_placement_check(&view,&p,&out)==RF_NOT_FOUND);rooms[1].skip=0;
 /* Primary children are not recursively traversed, and unlinked geometry is absent. */
 rooms[0].child_count=0;CHECK(!rf_checkpoint_placement_check(&view,&p,&out));rooms[0].child_count=1;
 for(i=0;i<6;i++)obstacle[i].filter.face_flags|=4;
 CHECK(!rf_checkpoint_placement_check(&view,&p,&out));for(i=0;i<6;i++)obstacle[i].filter.face_flags&=~4u;
 p.position[0]=0;memset(&out,0xa5,sizeof(out));kept=out;
#define BAD(expected) do{CHECK(rf_checkpoint_placement_check(&view,&p,&out)==(expected));CHECK(!memcmp(&out,&kept,sizeof(out)));}while(0)
 p.count=0;BAD(RF_RANGE);p.count=9;BAD(RF_RANGE);p.count=2;
 spheres[0].radius=-1;BAD(RF_FORMAT);spheres[0].radius=.5f;spheres[0].center[2]=NAN;BAD(RF_FORMAT);spheres[0].center[2]=0;
 p.position[0]=INFINITY;BAD(RF_FORMAT);p.position[0]=0;p.basis[4]=2;BAD(RF_FORMAT);p.basis[4]=1;p.basis[0]=-1;BAD(RF_FORMAT);p.basis[0]=1;
 p.replaced_room=2;BAD(RF_RANGE);p.replaced_room=0;p.query_flags=0x1004;BAD(RF_RANGE);p.query_flags=4;
 children[0]=2;BAD(RF_RANGE);children[0]=1;rooms[0].first_child=2;BAD(RF_RANGE);rooms[0].first_child=0;
 roots[0]=2;BAD(RF_RANGE);roots[0]=0;rooms[0].skip=1;BAD(RF_RANGE);p.query_flags=12;CHECK(!rf_checkpoint_placement_check(&view,&p,NULL));p.query_flags=4;rooms[0].skip=0;
 p.position[1]=-9.45f;CHECK(!rf_checkpoint_standing_check(&view,&p,1.0f/60,5,&out));
 p.position[1]=-9.5f;CHECK(!rf_checkpoint_standing_check(&view,&p,1.0f/60,5,&out));
 p.position[1]=0;CHECK(rf_checkpoint_standing_check(&view,&p,1.0f/60,5,&out)==RF_NOT_FOUND&&out.reason==RF_CHECKPOINT_PLACEMENT_UNSUPPORTED);
 {
  extra_support extra={1,0,0,1,0};
  CHECK(!rf_checkpoint_standing_check_with_support(&view,&p,1.0f/60,5,support,&extra,&out));
  extra.stable=0;CHECK(rf_checkpoint_standing_check_with_support(&view,&p,1.0f/60,5,support,&extra,&out)==RF_NOT_FOUND&&out.reason==RF_CHECKPOINT_PLACEMENT_UNSUPPORTED);
  extra.stable=1;extra.up=.4f;CHECK(rf_checkpoint_standing_check_with_support(&view,&p,1.0f/60,5,support,&extra,&out)==RF_NOT_FOUND);
  memset(&out,0xa5,sizeof(out));kept=out;extra.up=NAN;
  CHECK(rf_checkpoint_standing_check_with_support(&view,&p,1.0f/60,5,support,&extra,&out)==RF_FORMAT&&!memcmp(&out,&kept,sizeof(out)));
  extra.error=RF_IO;CHECK(rf_checkpoint_standing_check_with_support(&view,&p,1.0f/60,5,support,&extra,&out)==RF_IO&&!memcmp(&out,&kept,sizeof(out)));
  extra.error=0;extra.up=1;extra.stable=0;extra.tie=1;p.position[1]=-9.45f;
  CHECK(!rf_checkpoint_standing_check_with_support(&view,&p,1.0f/60,5,support,&extra,&out)); /* Static tie wins. */
  p.position[1]=0;
 }
 /* Authored detail top supplies support even though the replaced outer floor is far below. */
 p.position[0]=-4;p.position[1]=1.55f;CHECK(!rf_checkpoint_standing_check(&view,&p,1.0f/60,5,&out));
 for(i=0;i<6;i++)obstacle[i].filter.face_flags|=4;
 CHECK(rf_checkpoint_standing_check(&view,&p,1.0f/60,5,&out)==RF_NOT_FOUND&&out.reason==RF_CHECKPOINT_PLACEMENT_UNSUPPORTED);
 for(i=0;i<6;i++)obstacle[i].filter.face_flags&=~4u;
 memset(&out,0xa5,sizeof(out));kept=out;CHECK(rf_checkpoint_standing_check(&view,&p,NAN,5,&out)==RF_RANGE&&!memcmp(&out,&kept,sizeof(out)));
 CHECK(rf_checkpoint_standing_check(&view,&p,1.0f/60,-1,&out)==RF_RANGE&&!memcmp(&out,&kept,sizeof(out)));p.position[0]=p.position[1]=0;
 obstacle[0].plane[0]=NAN;BAD(RF_FORMAT);
 rf_geomod_terrain_close(&terrain);puts("PASS production placement sphere/basis/filter/hierarchy/rejection boundaries");return 0;
}
