#include "rf/geomod_authored_post.h"
#include <stdio.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"drill half line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_geomod_template shape;
static rf_geomod_publication_work work;
static rf_geomod_vertex vertices[RF_GEOMOD_PUBLICATION_VERTICES];
static rf_geomod_face faces[RF_GEOMOD_PUBLICATION_FACES];
static rf_geomod_publication_origin origins[RF_GEOMOD_PUBLICATION_FACES];
static double intrusion_area(const rf_geomod_mesh_view *mesh,const rf_geomod_face *f)
{
 double p[2][128][3],area=0;uint32_t i,k,n=f->count,bank=0,plane;
 if(n>120)return -1;
 for(i=0;i<n;i++)for(k=0;k<3;k++)p[0][i][k]=mesh->vertices[f->first+i].position[k];
 for(plane=0;plane<2 && n;plane++){
  uint32_t out=0;
  for(i=0;i<n;i++){
   const double *a=p[bank][i],*b=p[bank][(i+1)%n];
   double da=plane?a[1]-4.0001:41.3124-a[0],db=plane?b[1]-4.0001:41.3124-b[0];
   if(da>=0){for(k=0;k<3;k++)p[bank^1][out][k]=a[k];out++;}
   if((da<0)!=(db<0)){double t=da/(da-db);for(k=0;k<3;k++)p[bank^1][out][k]=a[k]+t*(b[k]-a[k]);out++;}
  }
  n=out;bank^=1;
 }
 for(i=1;i+1<n;i++){
  double c[3];for(k=0;k<3;k++){uint32_t u=(k+1)%3,v=(k+2)%3;c[k]=(p[bank][i][u]-p[bank][0][u])*(p[bank][i+1][v]-p[bank][0][v])-(p[bank][i][v]-p[bank][0][v])*(p[bank][i+1][u]-p[bank][0][u]);}
  area+=.5*sqrt(c[0]*c[0]+c[1]*c[1]+c[2]*c[2]);
 }
 return area;
}
static void audit(const char *label,const rf_geomod_mesh_view *mesh,const rf_geomod_publication_origin *o)
{
 double inside_area=0;uint32_t i,j,n=0,bad=0,nfaces=0;float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
 for(i=0;i<mesh->face_count;i++)if(o?o[i].kind==RF_GEOMOD_PUBLICATION_CRATER:mesh->faces[i].material==999){
  const rf_geomod_face *f=mesh->faces+i;nfaces++;inside_area+=intrusion_area(mesh,f);
  for(j=0;j<f->count;j++){
   const float *p=mesh->vertices[f->first+j].position;uint32_t k;n++;
   for(k=0;k<3;k++){if(p[k]<lo[k])lo[k]=p[k];if(p[k]>hi[k])hi[k]=p[k];}
   if(p[0]<41.3125f-.0001f && p[1]>4.0001f){if(bad<8)printf("%s_INTRUSION face%u vertex%u %g %g %g\n",label,i,j,p[0],p[1],p[2]);bad++;}
  }
 }
 printf("%s interior_air_area=%.12g\n",label,inside_area);
 printf("%s generated_faces=%u vertices=%u inside_original_air=%u min=%g,%g,%g max=%g,%g,%g\n",label,nfaces,n,bad,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
}
int main(void)
{
 rf_vpp levels={0};rf_level level={0};rf_geometry geometry={0};rf_geomod_authored_post *owner=NULL;
 rf_geomod_authored_post_view a;rf_geomod_terrain *terrain=NULL;rf_geomod_terrain_view t;
 rf_geomod_publication_job job={0};rf_geomod_publication_cut cut={0};rf_geomod_mesh_view result;
 rf_collision_face_filter generated={0};float center[3]={41.3125f,6.73709393f,-167.045395f},basis[9]={0,0,-1,0,1,0,1,0,0};
 CHECK(!rf_vpp_open(&levels,"Installed_Game/levelsm.vpp"));CHECK(!rf_level_open(&level,&levels,"ctf06.rfl"));
 CHECK(!rf_geometry_open(&geometry,&level,8*1024*1024));
 CHECK(!rf_geomod_authored_cavity_open_source(&level,&geometry,148,2*1024*1024,&owner));CHECK(!rf_geomod_authored_post_get(owner,&a));
 CHECK(!rf_geomod_template_load("build/data/driller-double.bin",&shape));generated.face_flags=256;
 CHECK(!rf_geomod_terrain_open(&a.source,a.source_filters,&generated,1,4096,768,2*1024*1024,&terrain));
 CHECK(!rf_geomod_terrain_cut_template_scale(terrain,&shape,center,basis,1,999));CHECK(!rf_geomod_terrain_get(terrain,&t));
 audit("CORE",&t.mesh,NULL);
 CHECK(!rf_geomod_terrain_cutter_get(terrain,0,&cut.mesh,cut.kernel,&cut.star));
 job.terrain=t.mesh;job.windows=a.windows;job.neighbors=a.neighbors;job.window_origins=a.window_origins;
 job.neighbor_origins=a.neighbor_origins;job.source_planes=a.source_planes;job.source_plane_count=a.source.face_count;
 job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,148,UINT32_MAX,6405};job.cuts=&cut;job.cut_count=1;
 CHECK(!rf_geomod_publication_build_cavity(&job,&work,vertices,RF_GEOMOD_PUBLICATION_VERTICES,faces,RF_GEOMOD_PUBLICATION_FACES,origins,&result));
 audit("PUBLICATION",&result,origins);
 rf_geomod_terrain_close(&terrain);rf_geomod_authored_post_close(&owner);rf_geometry_close(&geometry);rf_vpp_close(&levels);return 0;
}
