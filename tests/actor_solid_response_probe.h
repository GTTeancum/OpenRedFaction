typedef struct solid_response_probe_context {uint32_t *wire,trace[145],query;} solid_response_probe_context;
static void solid_response_prepare(void *context,uint32_t solid,const float *minimum,const float *maximum)
{
 solid_response_probe_context *c=context;uint32_t *r=c->trace+1+c->trace[0]++*24;r[0]=0;r[1]=solid;memcpy(r+2,minimum,12);memcpy(r+5,maximum,12);
}
static void solid_response_query(void *context,uint32_t solid,const rf_collision_solid_response_query *query,rf_collision_solid_response_hit *hit)
{
 solid_response_probe_context *c=context;uint32_t *r=c->trace+1+c->trace[0]++*24;r[0]=1;r[1]=solid;memcpy(r+2,query,80);memcpy(r+22,&hit->time,4);
 memcpy(hit,c->wire+155+c->query++*10,40);
}
static void solid_response_finish(void *context)
{
 solid_response_probe_context *c=context;uint32_t *r=c->trace+1+c->trace[0]++*24;r[0]=2;
}
static int actor_solid_response_probe(void)
{
 uint32_t wire[195],result,i,j;rf_collision_actor_general_response actors[2];rf_physics_sphere spheres[2][4];solid_response_probe_context context;
 rf_collision_solid_response_backend backend={solid_response_prepare,solid_response_query,solid_response_finish,&context};
 _Static_assert(sizeof(rf_collision_solid_response_query)==80,"solid query wire");_Static_assert(sizeof(rf_collision_solid_response_hit)==40,"solid hit wire");
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  memset(&context,0,sizeof(context));context.wire=wire;
  for(i=0;i<2;++i) {
   memset(actors+i,0,sizeof(actors[i]));memcpy(actors+i,wire+i*77,80);actors[i].actor.spheres=spheres[i];
   if(actors[i].actor.sphere_count>4)return 1;
   memset(spheres[i],0,sizeof(spheres[i]));for(j=0;j<4;++j){memcpy(&spheres[i][j].radius,wire+i*77+20+j,4);memcpy(spheres[i][j].center,wire+i*77+65+j*3,12);}
   memcpy(&actors[i].actor.contact,wire+i*77+24,68);memcpy(actors[i].orientation,wire+i*77+45,80);
  }
  result=rf_collision_actor_solid_response(actors,actors+1,wire[154],&backend);
  if(fwrite(&result,4,1,stdout)!=1 || fwrite(context.trace,sizeof(context.trace),1,stdout)!=1)return 1;
  for(i=0;i<2;++i)if(fwrite(actors+i,80,1,stdout)!=1 || fwrite(&actors[i].actor.contact,68,1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
