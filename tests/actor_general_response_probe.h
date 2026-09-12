#include "rf/physics.h"
typedef struct general_response_probe_context {uint32_t *wire,trace[33];} general_response_probe_context;
static const float *general_response_probe_extra(void *context,uint32_t handle)
{
 general_response_probe_context *c=context;uint32_t i;c->trace[++c->trace[0]]=handle;
 for(i=0;i<2;++i)if(c->wire[i*77+16]==handle)return c->wire[i*77+44]?(const float*)(c->wire+i*77+41):NULL;
 return NULL;
}
static int actor_general_response_probe(void)
{
 uint32_t wire[154],result,i,j;rf_collision_actor_general_response actors[2];rf_physics_sphere spheres[2][4];general_response_probe_context context;
 _Static_assert(sizeof(rf_collision_actor_contact)==68,"contact wire");
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  memset(&context,0,sizeof(context));context.wire=wire;
  for(i=0;i<2;++i) {
   memset(actors+i,0,sizeof(actors[i]));memcpy(actors+i,wire+i*77,80);actors[i].actor.spheres=spheres[i];
   if(actors[i].actor.sphere_count>4)return 1;
   memset(spheres[i],0,sizeof(spheres[i]));for(j=0;j<4;++j)memcpy(&spheres[i][j].radius,wire+i*77+20+j,4);
   memcpy(&actors[i].actor.contact,wire+i*77+24,68);
   memcpy(actors[i].orientation,wire+i*77+45,80);
   for(j=0;j<4;++j)memcpy(spheres[i][j].center,wire+i*77+65+j*3,12);
  }
  result=rf_collision_actors_general_response(actors,actors+1,general_response_probe_extra,&context);
  if(fwrite(&result,4,1,stdout)!=1 || fwrite(context.trace,132,1,stdout)!=1)return 1;
  for(i=0;i<2;++i)if(fwrite(actors+i,80,1,stdout)!=1 || fwrite(&actors[i].actor.contact,68,1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
