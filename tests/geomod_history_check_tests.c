#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"history check line%d: %s\n",__LINE__,#x);exit(1);}} while(0)
static rf_geomod_vertex original_vertices[24],saved_vertices[4096];
static rf_geomod_face original_faces[6],saved_faces[800];
static rf_collision_face_filter filters[6],generated;
static unsigned char encoded[12380],kept[12380],again[12380],bad[12380];
static uint32_t query_stack[8192];
static rf_collision_tree saved_tree;
static rf_geomod_terrain_view unchanged_control;
static void make_source(rf_geomod_mesh_view *source)
{
 uint32_t axis,side,j;const int u[4]={-1,1,1,-1},v[4]={-1,-1,1,1};
 for(axis=0;axis<3;axis++)for(side=0;side<2;side++){
  uint32_t f=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;original_faces[f]=(rf_geomod_face){f*4,4,2,f};filters[f].face_flags=256;
  for(j=0;j<4;j++){uint32_t k=side?j:3-j;rf_geomod_vertex *p=original_vertices+f*4+j;
   p->position[axis]=side?10:-10;p->position[a]=(float)u[k]*10;p->position[b]=(float)v[k]*10;p->uv[0]=(float)u[k];p->uv[1]=(float)v[k];}
 }
 generated.face_flags=256;*source=(rf_geomod_mesh_view){original_vertices,original_faces,24,6,1};
}
static rf_geomod_terrain *open_terrain(const rf_geomod_mesh_view *source,uint32_t budget,uint32_t faces)
{rf_geomod_terrain *t=NULL;CHECK(!rf_geomod_terrain_open(source,filters,&generated,0,4096,faces,budget,&t));CHECK(!rf_geomod_terrain_set_mapping(t,64,64));return t;}
static void compare(const rf_geomod_terrain_view *a,const rf_geomod_terrain_view *b)
{
 uint32_t i;CHECK(a->cuts==b->cuts && a->mesh.vertex_count==b->mesh.vertex_count && a->mesh.face_count==b->mesh.face_count);
 CHECK(!memcmp(a->mesh.vertices,b->mesh.vertices,a->mesh.vertex_count*sizeof(*a->mesh.vertices)));
 CHECK(!memcmp(a->mesh.faces,b->mesh.faces,a->mesh.face_count*sizeof(*a->mesh.faces)));
 for(i=0;i<a->mesh.face_count;i++){CHECK(!memcmp(a->faces[i].plane,b->faces[i].plane,sizeof(a->faces[i].plane)));CHECK(!memcmp(&a->faces[i].filter,&b->faces[i].filter,sizeof(a->faces[i].filter)));}
 for(i=0;i<3;i++){
  float p[3]={0,0,0},d[3]={0,0,0};rf_collision_tree_hit ha,hb;uint32_t ma,mb;p[i]=-20;d[i]=40;
  CHECK(a->tree->node_capacity<=8192 && b->tree->node_capacity<=8192);
  CHECK(!rf_collision_thin_tree(a->tree->nodes,a->tree->node_count,a->tree->faces,a->tree->face_count,0,p,d,1,query_stack,8192,&ha,&ma));
  CHECK(!rf_collision_thin_tree(b->tree->nodes,b->tree->node_count,b->tree->faces,b->tree->face_count,0,p,d,1,query_stack,8192,&hb,&mb));
  CHECK(ma==mb);if(ma)CHECK(ha.hit.fraction==hb.hit.fraction && !memcmp(ha.hit.normal,hb.hit.normal,12));
 }
}
typedef struct visitor_context {rf_geomod_terrain_view expected;uint32_t calls;int status;} visitor_context;
static int visitor(const rf_geomod_terrain_view *candidate,void *context)
{visitor_context *c=context;c->calls++;compare(candidate,&c->expected);return c->status;}
static void preserved(rf_geomod_terrain *t,const rf_geomod_terrain_view *before,uint32_t bytes)
{
 rf_geomod_terrain_view now;uint32_t n;CHECK(!rf_geomod_terrain_get(t,&now));
 CHECK(now.mesh.vertices==before->mesh.vertices && now.mesh.faces==before->mesh.faces && now.mesh.generation==before->mesh.generation && now.tree->nodes==before->tree->nodes && now.cuts==before->cuts);
 CHECK(!memcmp(now.mesh.vertices,saved_vertices,now.mesh.vertex_count*sizeof(*saved_vertices)) && !memcmp(now.mesh.faces,saved_faces,now.mesh.face_count*sizeof(*saved_faces)));
 compare(&now,&unchanged_control);
 CHECK(!rf_geomod_terrain_history_size(t,&n) && n==bytes);CHECK(!rf_geomod_terrain_history_encode(t,again,n) && !memcmp(again,kept,n));
}
int main(void)
{
 rf_geomod_mesh_view source;rf_geomod_terrain *live,*control,*candidate,*tight;rf_geomod_terrain_view before,a,b;visitor_context c;
 uint32_t bytes,old_bytes,j,calls,min_budget;const float left[3]={-9,0,0},right[3]={9,0,0},top[3]={0,9,0},extent[3]={2,2,2};
 make_source(&source);live=open_terrain(&source,1024*1024,800);control=open_terrain(&source,1024*1024,800);candidate=open_terrain(&source,1024*1024,800);
 CHECK(!rf_geomod_terrain_cut_box(live,left,extent,3));CHECK(!rf_geomod_terrain_cut_box(control,left,extent,3));
 CHECK(!rf_geomod_terrain_cut_box(candidate,right,extent,3));CHECK(!rf_geomod_terrain_cut_crater(candidate,top,2,3));
 CHECK(!rf_geomod_terrain_get(candidate,&c.expected));c.calls=0;c.status=0;
 CHECK(!rf_geomod_terrain_history_size(candidate,&bytes));CHECK(!rf_geomod_terrain_history_encode(candidate,encoded,bytes));
 CHECK(!rf_geomod_terrain_get(live,&before));saved_tree=*before.tree;before.tree=&saved_tree;CHECK(!rf_geomod_terrain_get(control,&unchanged_control));memcpy(saved_vertices,before.mesh.vertices,before.mesh.vertex_count*sizeof(*saved_vertices));memcpy(saved_faces,before.mesh.faces,before.mesh.face_count*sizeof(*saved_faces));
 CHECK(!rf_geomod_terrain_history_size(live,&old_bytes));CHECK(!rf_geomod_terrain_history_encode(live,kept,old_bytes));
 CHECK(!rf_geomod_terrain_history_check(live,encoded,bytes,visitor,&c) && c.calls==1);preserved(live,&before,old_bytes);
 c.status=RF_NOT_FOUND;CHECK(rf_geomod_terrain_history_check(live,encoded,bytes,visitor,&c)==RF_NOT_FOUND && c.calls==2);preserved(live,&before,old_bytes);c.status=0;
 CHECK(rf_geomod_terrain_history_check(live,encoded,bytes,NULL,&c)==RF_RANGE);preserved(live,&before,old_bytes);
 calls=c.calls;
 for(j=0;j<bytes;j++){memcpy(bad,encoded,bytes);bad[8]=(unsigned char)j;bad[9]=(unsigned char)(j>>8);bad[10]=(unsigned char)(j>>16);bad[11]=(unsigned char)(j>>24);CHECK(rf_geomod_terrain_history_check(live,bad,j,visitor,&c)!=RF_OK);}
 CHECK(c.calls==calls);preserved(live,&before,old_bytes);
 /* Tight budget rejects rollback scratch before visitor or publication. */
 tight=open_terrain(&source,1024*1024,800);CHECK(!rf_geomod_terrain_get(tight,&a));min_budget=a.peak_bytes;rf_geomod_terrain_close(&tight);tight=open_terrain(&source,min_budget,800);
 CHECK(rf_geomod_terrain_history_check(tight,encoded,bytes,visitor,&c)==RF_RANGE && c.calls==calls);CHECK(!rf_geomod_terrain_get(tight,&a) && !a.cuts);rf_geomod_terrain_close(&tight);
 /* Uninterrupted control never imported: successful/rejected checks leave next cut identical. */
 CHECK(!rf_geomod_terrain_cut_box(live,right,extent,3));CHECK(!rf_geomod_terrain_cut_box(control,right,extent,3));
 CHECK(!rf_geomod_terrain_get(live,&a) && !rf_geomod_terrain_get(control,&b));compare(&a,&b);
 CHECK(!rf_geomod_terrain_history_encode(candidate,again,bytes) && !memcmp(encoded,again,bytes));
 CHECK(!rf_geomod_terrain_history_decode(live,encoded,bytes));CHECK(!rf_geomod_terrain_get(live,&a));compare(&a,&c.expected);
 rf_geomod_terrain_close(&candidate);rf_geomod_terrain_close(&control);rf_geomod_terrain_close(&live);
 puts("PASS history check visitor, success/failure rollback, mixed history, truncation, budget and uninterrupted next cut");return 0;
}
