#include "rf/physics.h"
typedef struct response_probe_context {uint32_t *wire,trace[3];} response_probe_context;
static const float *response_probe_extra(void *context,uint32_t handle)
{
 response_probe_context *c=context;uint32_t i;c->trace[++c->trace[0]]=handle;
 for(i=0;i<2;++i)if(c->wire[i*45+16]==handle)return c->wire[i*45+44]?(const float*)(c->wire+i*45+41):NULL;
 return NULL;
}
static int actor_response_probe(void)
{
 uint32_t wire[90],result,i,j;rf_collision_actor_response actors[2];rf_physics_sphere spheres[2][4];response_probe_context context;
 _Static_assert(sizeof(rf_collision_actor_contact)==68,"contact wire");
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  memset(&context,0,sizeof(context));context.wire=wire;
  for(i=0;i<2;++i) {
   memset(actors+i,0,sizeof(actors[i]));memcpy(actors+i,wire+i*45,80);actors[i].spheres=spheres[i];
   if(actors[i].sphere_count>4)return 1;
   memset(spheres[i],0,sizeof(spheres[i]));for(j=0;j<4;++j)memcpy(&spheres[i][j].radius,wire+i*45+20+j,4);
   memcpy(&actors[i].contact,wire+i*45+24,68);
  }
  result=rf_collision_actors_normal_response(actors,actors+1,response_probe_extra,&context);
  if(fwrite(&result,4,1,stdout)!=1 || fwrite(context.trace,12,1,stdout)!=1)return 1;
  for(i=0;i<2;++i)if(fwrite(actors+i,80,1,stdout)!=1 || fwrite(&actors[i].contact,68,1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
