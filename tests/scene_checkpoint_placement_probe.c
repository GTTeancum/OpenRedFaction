/* Readiness experiment only: closed DEV room plus closed unchanged obstacle.
 * Does NOT claim general open/portal campaign room classification. */
#include "rf/geomod.h"
#include "rf/physics.h"
#include "rf/player_checkpoint.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do{if(!(v)){fprintf(stderr,"placement line%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static rf_geomod_vertex vertices[24],obstacle_vertices[24];
static rf_geomod_face faces[6],obstacle_faces[6];
static rf_collision_face_filter filters[6],generated;
static rf_collision_face obstacle[6];static float obstacle_positions[24][3];
static unsigned char history[12380],before[12380],after[12380];
static double dot(const double a[3],const double b[3]){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void cube(rf_geomod_vertex *v,rf_geomod_face *f,const float origin[3],float size,int inward)
{
 uint32_t axis,side,j;const int u[4]={-1,1,1,-1},w[4]={-1,-1,1,1};
 for(axis=0;axis<3;axis++)for(side=0;side<2;side++){
  uint32_t n=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;f[n]=(rf_geomod_face){n*4,4,0,n};filters[n].face_flags=256;
  for(j=0;j<4;j++){uint32_t k=(side!=(uint32_t)!!inward)?j:3-j;rf_geomod_vertex *p=v+n*4+j;
   p->position[axis]=origin[axis]+(side?size:-size);p->position[a]=origin[a]+u[k]*size;p->position[b]=origin[b]+w[k]*size;}
 }
}
/* Exact double segment distance includes edge and vertex regions. */
static double edge_distance(const double p[3],const float a[3],const float b[3])
{
 double d[3],q[3],r[3],t,n;unsigned k;
 for(k=0;k<3;k++){d[k]=(double)b[k]-a[k];q[k]=p[k]-a[k];}
 n=dot(d,d);t=n?dot(q,d)/n:0;if(t<0)t=0;if(t>1)t=1;
 for(k=0;k<3;k++)r[k]=q[k]-t*d[k];return dot(r,r);
}
static int face_measure(const rf_collision_face *f,const double p[3],double *distance,double *angle)
{
 double d=f->plane[3],best=HUGE_VAL,norm=0;float projected[3];uint32_t inside,i,k;int status;
 for(k=0;k<3;k++){d+=p[k]*f->plane[k];norm+=(double)f->plane[k]*f->plane[k];}
 if(norm<.999 || norm>1.001 || f->count<3)return RF_FORMAT;
 for(k=0;k<3;k++)projected[k]=(float)(p[k]-d*f->plane[k]/norm);
 status=rf_collision_polygon_contains(f->plane,projected,f->vertices,f->count,&inside);if(status)return status;
 if(inside)best=d*d/norm;
 for(i=0;i<f->count;i++){double q=edge_distance(p,f->vertices[i],f->vertices[(i+1)%f->count]);if(q<best)best=q;}
 if(best<*distance)*distance=best;
 for(i=1;i+1<f->count;i++){
  double a[3],b[3],c[3],cross[3],la,lb,lc,numerator,denominator;
  for(k=0;k<3;k++){a[k]=f->vertices[0][k]-p[k];b[k]=f->vertices[i][k]-p[k];c[k]=f->vertices[i+1][k]-p[k];}
  cross[0]=b[1]*c[2]-b[2]*c[1];cross[1]=b[2]*c[0]-b[0]*c[2];cross[2]=b[0]*c[1]-b[1]*c[0];
  la=sqrt(dot(a,a));lb=sqrt(dot(b,b));lc=sqrt(dot(c,c));numerator=dot(a,cross);
  denominator=la*lb*lc+dot(a,b)*lc+dot(b,c)*la+dot(c,a)*lb;
  *angle+=2*atan2(numerator,denominator);
 }
 return RF_OK;
}
typedef struct placement {
 rf_physics_sphere spheres[8];uint32_t count,base_count,calls;
 const rf_collision_face *base;float position[3],basis[9];
} placement;
/* Sphere transforms match scene body ownership. .002 is the existing DEV reset
 * tolerance; it is NOT inferred from original save code. */
static int fit(const rf_geomod_terrain_view *view,void *context)
{
 placement *c=context;uint32_t i,j,k;int status;c->calls++;
 if(!c->count || c->count>8)return RF_RANGE;
 for(i=0;i<c->count;i++){
  double center[3],distance=HUGE_VAL,angle=0,r=c->spheres[i].radius-.002;
  if(!isfinite(r)||r<=0)return RF_FORMAT;
  for(k=0;k<3;k++){
   center[k]=(double)c->position[k]+(double)c->spheres[i].center[0]*c->basis[k]+(double)c->spheres[i].center[1]*c->basis[3+k]+(double)c->spheres[i].center[2]*c->basis[6+k];
   if(!isfinite(center[k]))return RF_FORMAT;
  }
  for(j=0;j<view->mesh.face_count;j++){status=face_measure(view->faces+j,center,&distance,&angle);if(status)return status;}
  for(j=0;j<c->base_count;j++){status=face_measure(c->base+j,center,&distance,&angle);if(status)return status;}
  /* Inward closed air shell winds -4pi. Outward closed solid obstacles cancel
   * it for a center inside solid. Closest surface alone cannot classify rock. */
  if(distance<r*r || angle>-6.283185307179586)return RF_NOT_FOUND;
 }
 return RF_OK;
}
static void preserved(rf_geomod_terrain *live,const rf_geomod_terrain_view *old,uint32_t bytes)
{
 rf_geomod_terrain_view now;uint32_t n;CHECK(!rf_geomod_terrain_get(live,&now));
 CHECK(now.mesh.vertices==old->mesh.vertices&&now.mesh.faces==old->mesh.faces&&now.mesh.generation==old->mesh.generation&&!now.cuts);
 CHECK(!rf_geomod_terrain_history_size(live,&n)&&n==bytes);CHECK(!rf_geomod_terrain_history_encode(live,after,n)&&!memcmp(before,after,n));
}
int main(void)
{
 rf_geomod_mesh_view source,obstacle_mesh;rf_geomod_terrain *live=NULL,*saved=NULL;rf_geomod_terrain_view old;placement p={0};
 const float zero[3]={0,0,0},base_center[3]={-4,0,0},cut[3]={9,0,0},extent[3]={2,2,2};uint32_t bytes,old_bytes,cases=0;
 cube(vertices,faces,zero,10,1);source=(rf_geomod_mesh_view){vertices,faces,24,6,0};generated.face_flags=256;
 cube(obstacle_vertices,obstacle_faces,base_center,1,0);obstacle_mesh=(rf_geomod_mesh_view){obstacle_vertices,obstacle_faces,24,6,0};
 CHECK(!rf_geomod_collision_faces(&obstacle_mesh,filters,obstacle_positions,24,obstacle,6));
 CHECK(!rf_geomod_terrain_open(&source,filters,&generated,1,4096,800,1024*1024,&live));CHECK(!rf_geomod_terrain_open(&source,filters,&generated,1,4096,800,1024*1024,&saved));
 CHECK(!rf_geomod_terrain_cut_box(saved,cut,extent,0));CHECK(!rf_geomod_terrain_history_size(saved,&bytes));CHECK(!rf_geomod_terrain_history_encode(saved,history,bytes));
 CHECK(!rf_geomod_terrain_get(live,&old));CHECK(!rf_geomod_terrain_history_size(live,&old_bytes));CHECK(!rf_geomod_terrain_history_encode(live,before,old_bytes));
 p.count=1;p.spheres[0].radius=.5f;p.basis[0]=p.basis[4]=p.basis[8]=1;p.base=obstacle;p.base_count=6;
#define TRY(expected) do{CHECK(rf_geomod_terrain_history_check(live,history,bytes,fit,&p)==(expected));preserved(live,&old,old_bytes);cases++;}while(0)
 TRY(RF_OK); /* ordinary air */
 p.position[0]=9.5f;p.position[1]=6;TRY(RF_OK); /* exact boundary touch */
 p.position[0]=9.501f;TRY(RF_OK); /* within existing reset tolerance */
 p.position[0]=9.51f;TRY(RF_NOT_FOUND); /* meaningful penetration */
 p.position[0]=12;p.position[1]=0;TRY(RF_NOT_FOUND); /* rock, no nearby surface */
 p.position[0]=10.4f;TRY(RF_OK); /* saved cut admits body which live terrain rejects */
 CHECK(fit(&old,&p)==RF_NOT_FOUND);preserved(live,&old,old_bytes);
 p.count=2;p.spheres[1].radius=.5f;p.spheres[1].center[1]=2;TRY(RF_NOT_FOUND); /* head clips pocket edge */
 p.spheres[1].center[1]=1;TRY(RF_OK);p.count=1;
 p.position[0]=-4;TRY(RF_NOT_FOUND); /* wholly inside unchanged solid */
 p.position[0]=-2.5f;TRY(RF_OK);p.position[0]=-2.51f;TRY(RF_NOT_FOUND); /* base-world touch vs intrusion */
 p.position[0]=0;p.spheres[0].center[0]=10.4f;TRY(RF_OK);
 p.basis[0]=0;p.basis[2]=1;p.basis[6]=-1;p.basis[8]=0;TRY(RF_NOT_FOUND); /* same local sphere rotated into uncut z wall */
 p.position[0]=NAN;TRY(RF_FORMAT);
 CHECK(!rf_geomod_terrain_history_encode(saved,after,bytes)&&!memcmp(history,after,bytes));
 rf_geomod_terrain_close(&saved);rf_geomod_terrain_close(&live);
 printf("PASS %u candidate sphere placement cases; published history unchanged\n",cases);return 0;
}
