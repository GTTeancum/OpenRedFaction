/* Reuse the independent source, collision and saved-history comparisons. */
#define main history_check_test_main
#include "geomod_history_check_tests.c"
#undef main
static void snapshot(rf_geomod_terrain *live,rf_geomod_terrain *control,rf_geomod_terrain_view *before,uint32_t *bytes)
{
 CHECK(!rf_geomod_terrain_history_size(live,bytes));CHECK(!rf_geomod_terrain_history_encode(live,kept,*bytes));
 CHECK(!rf_geomod_terrain_history_decode(control,kept,*bytes));CHECK(!rf_geomod_terrain_get(control,&unchanged_control));
 CHECK(!rf_geomod_terrain_get(live,before));saved_tree=*before->tree;before->tree=&saved_tree;
 memcpy(saved_vertices,before->mesh.vertices,before->mesh.vertex_count*sizeof(*saved_vertices));
 memcpy(saved_faces,before->mesh.faces,before->mesh.face_count*sizeof(*saved_faces));
}
int main(int argc,char **argv)
{
 rf_geomod_mesh_view source;rf_geomod_template shape;rf_geomod_terrain *live,*control,*expected;
 rf_geomod_terrain_view before,a,b;visitor_context c;uint32_t bytes,calls;int failures[3]={RF_RANGE,RF_IO,RF_FORMAT};uint32_t i;
 const float center[3]={9,0,0},left[3]={-9,0,0},extent[3]={2,2,2},basis[9]={1,0,0,0,1,0,0,0,1};
 CHECK(argc==2);CHECK(!rf_geomod_template_load(argv[1],&shape));make_source(&source);
 live=open_terrain(&source,1024*1024,800);control=open_terrain(&source,1024*1024,800);expected=open_terrain(&source,1024*1024,800);
 CHECK(!rf_geomod_terrain_cut_box(live,left,extent,3));CHECK(!rf_geomod_terrain_cut_box(control,left,extent,3));CHECK(!rf_geomod_terrain_cut_box(expected,left,extent,3));
 CHECK(!rf_geomod_terrain_cut_template_scale(expected,&shape,center,basis,.5f,3));CHECK(!rf_geomod_terrain_get(expected,&c.expected));c.calls=0;
 CHECK(!rf_geomod_terrain_get(live,&before));saved_tree=*before.tree;before.tree=&saved_tree;
 CHECK(!rf_geomod_terrain_get(control,&unchanged_control));
 memcpy(saved_vertices,before.mesh.vertices,before.mesh.vertex_count*sizeof(*saved_vertices));
 memcpy(saved_faces,before.mesh.faces,before.mesh.face_count*sizeof(*saved_faces));
 CHECK(!rf_geomod_terrain_history_size(live,&bytes));CHECK(!rf_geomod_terrain_history_encode(live,kept,bytes));
 for(i=0;i<3;i++) {
  c.status=failures[i];
  CHECK(rf_geomod_terrain_cut_template_checked(live,&shape,center,basis,.5f,3,NULL,0,visitor,&c)==failures[i]);
  CHECK(c.calls==i+1);preserved(live,&before,bytes);
 }
 calls=c.calls;
 CHECK(rf_geomod_terrain_cut_template_checked(live,&shape,center,basis,-1,3,NULL,0,visitor,&c)==RF_FORMAT);
 CHECK(c.calls==calls);preserved(live,&before,bytes);
 c.status=0;CHECK(!rf_geomod_terrain_cut_template_checked(live,&shape,center,basis,.5f,3,NULL,0,visitor,&c));
 CHECK(c.calls==calls+1);CHECK(!rf_geomod_terrain_get(live,&a));compare(&a,&c.expected);
 CHECK(a.mesh.generation==before.mesh.generation+1);
 CHECK(!rf_geomod_terrain_cut_box(live,(float[3]){0,9,0},extent,3));
 CHECK(!rf_geomod_terrain_cut_box(expected,(float[3]){0,9,0},extent,3));
 CHECK(!rf_geomod_terrain_get(live,&a));CHECK(!rf_geomod_terrain_get(expected,&b));compare(&a,&b);
 CHECK(!rf_geomod_terrain_history_size(live,&bytes));CHECK(!rf_geomod_terrain_history_encode(live,encoded,bytes));
 CHECK(!rf_geomod_terrain_history_encode(expected,again,bytes));CHECK(!memcmp(encoded,again,bytes));
 snapshot(live,control,&before,&bytes);
 CHECK(!rf_geomod_terrain_cut_box(expected,(float[3]){0,-9,0},extent,3));
 CHECK(!rf_geomod_terrain_get(expected,&c.expected));c.status=RF_RANGE;calls=c.calls;
 CHECK(rf_geomod_terrain_cut_box_checked(live,(float[3]){0,-9,0},extent,3,visitor,&c)==RF_RANGE);
 CHECK(c.calls==calls+1);preserved(live,&before,bytes);c.status=0;
 CHECK(!rf_geomod_terrain_cut_box_checked(live,(float[3]){0,-9,0},extent,3,visitor,&c));
 CHECK(!rf_geomod_terrain_get(live,&a));compare(&a,&c.expected);
 snapshot(live,control,&before,&bytes);
 CHECK(!rf_geomod_terrain_reset(expected));CHECK(!rf_geomod_terrain_get(expected,&c.expected));c.status=RF_IO;calls=c.calls;
 CHECK(rf_geomod_terrain_reset_checked(live,visitor,&c)==RF_IO);CHECK(c.calls==calls+1);preserved(live,&before,bytes);
 c.status=0;CHECK(!rf_geomod_terrain_reset_checked(live,visitor,&c));CHECK(!rf_geomod_terrain_get(live,&a));compare(&a,&c.expected);
 CHECK(a.cuts==0 && a.mesh.generation==before.mesh.generation+1);
 rf_geomod_terrain_close(&live);rf_geomod_terrain_close(&control);rf_geomod_terrain_close(&expected);
 puts("PASS checked template/box/reset rejection preserves mesh, collision, generation and history; retry matches control");return 0;
}
