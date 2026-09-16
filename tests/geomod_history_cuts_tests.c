/* Extended pending-history visitor lifetime/order and rollback. */
#define main legacy_history_check_tests
#include "geomod_history_check_tests.c"
#undef main
typedef struct cuts_context {
 rf_geomod_terrain *expected;rf_geomod_terrain_view expected_view;
 rf_geomod_vertex vertices[RF_GEOMOD_CUT_LIMIT][60];rf_geomod_face faces[RF_GEOMOD_CUT_LIMIT][20];
 float kernels[RF_GEOMOD_CUT_LIMIT][3];uint32_t vc[RF_GEOMOD_CUT_LIMIT],fc[RF_GEOMOD_CUT_LIMIT],mask,count,calls;int reject;
} cuts_context;
static cuts_context cuts;
static int cuts_visitor(const rf_geomod_terrain_view *view,const rf_geomod_history_view *history,void *context)
{
 cuts_context *c=context;uint32_t i,j;c->calls++;compare(view,&c->expected_view);
 CHECK(history&&history->count==view->cuts&&history->count<=RF_GEOMOD_CUT_LIMIT);
 CHECK(!(history->star_mask>>history->count));c->count=history->count;c->mask=history->star_mask;
 for(i=0;i<history->count;i++){
  rf_geomod_mesh_view expected;float kernel[3];uint32_t star;const rf_geomod_mesh_view *m=history->cutters+i;
  /* Distinct immutable control owner is permitted; never reenter checked terrain. */
  CHECK(!rf_geomod_terrain_cutter_get(c->expected,i,&expected,kernel,&star));
  CHECK(m->vertex_count==expected.vertex_count&&m->face_count==expected.face_count&&star==((history->star_mask>>i)&1));
  CHECK(!memcmp(m->vertices,expected.vertices,m->vertex_count*sizeof(*m->vertices)));
  CHECK(!memcmp(m->faces,expected.faces,m->face_count*sizeof(*m->faces))&&!memcmp(history->kernels[i],kernel,12));
  for(j=0;j<m->face_count;j++)CHECK(m->faces[j].source_face==UINT32_MAX);
  c->vc[i]=m->vertex_count;c->fc[i]=m->face_count;memcpy(c->vertices[i],m->vertices,m->vertex_count*sizeof(*m->vertices));
  memcpy(c->faces[i],m->faces,m->face_count*sizeof(*m->faces));memcpy(c->kernels[i],history->kernels[i],12);
 }
 return c->reject;
}
int main(void)
{
 rf_geomod_mesh_view source;rf_geomod_terrain *live,*control,*expected,*empty;rf_geomod_terrain_view before,a,b;
 const float left[3]={-9,0,0},right[3]={9,0,0},top[3]={0,9,0},bottom[3]={0,-9,0},extent[3]={2,2,2};uint32_t bytes,old_bytes,i,calls;
 make_source(&source);live=open_terrain(&source,1024*1024,800);control=open_terrain(&source,1024*1024,800);expected=open_terrain(&source,1024*1024,800);empty=open_terrain(&source,1024*1024,800);
 CHECK(!rf_geomod_terrain_cut_box(live,left,extent,3)&&!rf_geomod_terrain_cut_box(control,left,extent,3));
 CHECK(!rf_geomod_terrain_cut_box(expected,right,extent,5));
 {rf_geomod_mesh_view cutter;float kernel[3];uint32_t star;
  CHECK(!rf_geomod_terrain_cut_crater(empty,top,2,7));CHECK(!rf_geomod_terrain_cutter_get(empty,0,&cutter,kernel,&star));CHECK(!star);
  CHECK(!rf_geomod_terrain_cut_star(expected,&cutter,top));
  CHECK(!rf_geomod_terrain_cut_crater(empty,bottom,1.5f,9));CHECK(!rf_geomod_terrain_cutter_get(empty,1,&cutter,kernel,&star));CHECK(!star);
  CHECK(!rf_geomod_terrain_cut_star(expected,&cutter,bottom));CHECK(!rf_geomod_terrain_reset(empty));
 }
 cuts.expected=expected;CHECK(!rf_geomod_terrain_get(expected,&cuts.expected_view));CHECK(!rf_geomod_terrain_history_size(expected,&bytes));CHECK(!rf_geomod_terrain_history_encode(expected,encoded,bytes));
 CHECK(!rf_geomod_terrain_get(live,&before));saved_tree=*before.tree;before.tree=&saved_tree;CHECK(!rf_geomod_terrain_get(control,&unchanged_control));
 memcpy(saved_vertices,before.mesh.vertices,before.mesh.vertex_count*sizeof(*saved_vertices));memcpy(saved_faces,before.mesh.faces,before.mesh.face_count*sizeof(*saved_faces));
 CHECK(!rf_geomod_terrain_history_size(live,&old_bytes)&&!rf_geomod_terrain_history_encode(live,kept,old_bytes));
 CHECK(!rf_geomod_terrain_history_check_cuts(live,encoded,bytes,cuts_visitor,&cuts));CHECK(cuts.calls==1&&cuts.count==3&&cuts.mask==6);preserved(live,&before,old_bytes);
 /* Only explicit deep copies remain usable after callback; no borrowed pointer escapes. */
 for(i=0;i<cuts.count;i++){rf_geomod_mesh_view m;float kernel[3];uint32_t star;CHECK(!rf_geomod_terrain_cutter_get(expected,i,&m,kernel,&star));CHECK(!memcmp(cuts.vertices[i],m.vertices,cuts.vc[i]*sizeof(*m.vertices))&&!memcmp(cuts.faces[i],m.faces,cuts.fc[i]*sizeof(*m.faces))&&!memcmp(cuts.kernels[i],kernel,12));}
 cuts.reject=RF_IO;CHECK(rf_geomod_terrain_history_check_cuts(live,encoded,bytes,cuts_visitor,&cuts)==RF_IO);preserved(live,&before,old_bytes);
 calls=cuts.calls;CHECK(rf_geomod_terrain_history_check_cuts(live,encoded,bytes,NULL,&cuts)==RF_RANGE&&cuts.calls==calls);
 CHECK(rf_geomod_terrain_history_check_cuts(live,encoded,bytes-1,cuts_visitor,&cuts)==RF_FORMAT&&cuts.calls==calls);preserved(live,&before,old_bytes);
 cuts.reject=0;cuts.expected=empty;CHECK(!rf_geomod_terrain_get(empty,&cuts.expected_view));CHECK(!rf_geomod_terrain_history_encode(empty,bad,28));
 CHECK(!rf_geomod_terrain_history_check_cuts(live,bad,28,cuts_visitor,&cuts)&&!cuts.count&&!cuts.mask);preserved(live,&before,old_bytes);
 CHECK(!rf_geomod_terrain_cut_box(live,right,extent,3)&&!rf_geomod_terrain_cut_box(control,right,extent,3));CHECK(!rf_geomod_terrain_get(live,&a)&&!rf_geomod_terrain_get(control,&b));compare(&a,&b);
 rf_geomod_terrain_close(&empty);rf_geomod_terrain_close(&expected);rf_geomod_terrain_close(&control);rf_geomod_terrain_close(&live);
 puts("PASS pending cutter visitor ordered box/star kernels, callback copies, empty/malformed/rejected rollback and next edit");return 0;
}
