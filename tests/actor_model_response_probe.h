typedef struct model_response_probe_context {uint32_t *wire,trace[121],query;rf_collision_model_target target;} model_response_probe_context;
static const rf_collision_model_target *model_response_target(void *context,uint32_t handle)
{
 model_response_probe_context *c=context;uint32_t *r=c->trace+1+c->trace[0]++*24;r[0]=0;r[1]=handle;return c->wire[156]?&c->target:NULL;
}
static uint32_t model_response_query(void *context,uint32_t model,const rf_collision_solid_response_query *query,rf_collision_model_response_hit *hit)
{
 model_response_probe_context *c=context;uint32_t *r=c->trace+1+c->trace[0]++*24,*result=c->wire+160+c->query++*9;
 r[0]=1;r[1]=model;memcpy(r+2,query,80);memcpy(r+22,&hit->time,4);memcpy(hit,result+1,32);return result[0];
}
static int actor_model_response_probe(void)
{
 uint32_t wire[196],result,i,j;rf_collision_actor_general_response actors[2];rf_physics_sphere spheres[2][4];model_response_probe_context context;
 rf_collision_model_response_backend backend={model_response_target,model_response_query,&context};
 _Static_assert(sizeof(rf_collision_solid_response_query)==80,"solid query wire");_Static_assert(sizeof(rf_collision_model_response_hit)==32,"solid hit wire");
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  memset(&context,0,sizeof(context));context.wire=wire;memcpy(&context.target,wire+157,12);
  for(i=0;i<2;++i) {
   memset(actors+i,0,sizeof(actors[i]));memcpy(actors+i,wire+i*77,80);actors[i].actor.spheres=spheres[i];
   if(actors[i].actor.sphere_count>4)return 1;
   memset(spheres[i],0,sizeof(spheres[i]));for(j=0;j<4;++j){memcpy(&spheres[i][j].radius,wire+i*77+20+j,4);memcpy(spheres[i][j].center,wire+i*77+65+j*3,12);}
   memcpy(&actors[i].actor.contact,wire+i*77+24,68);memcpy(actors[i].orientation,wire+i*77+45,80);
  }
  result=rf_collision_actor_model_response(actors,actors+1,wire[154],wire[155],&backend);
  if(fwrite(&result,4,1,stdout)!=1 || fwrite(context.trace,sizeof(context.trace),1,stdout)!=1)return 1;
  for(i=0;i<2;++i)if(fwrite(actors+i,80,1,stdout)!=1 || fwrite(&actors[i].actor.contact,68,1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
