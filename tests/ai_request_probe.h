typedef struct request_fixture {
 uint32_t in[6],calls,lists[3];rf_entity_navigation_candidate goal,first;
 rf_entity_navigation_retained_route route;
} request_fixture;
static int request_step(request_fixture *c,uint32_t step){c->calls|=1u<<step;return c->in[5]==step?RF_FORMAT:RF_OK;}
static int request_clear(void *v){return request_step(v,1);}
static int request_prepare(void *v){request_fixture *c=v;int s=request_step(c,2);if(s)return s;c->goal.query_point[0]=10;c->goal.query_point[1]=20;c->goal.query_point[2]=30;return RF_OK;}
static int request_start(void *v,uint32_t *out){request_fixture *c=v;int s=request_step(c,3);if(!s)*out=c->in[1];return s;}
static int request_goal(void *v,uint32_t *out){request_fixture *c=v;int s=request_step(c,4);uint32_t i;if(s)return s;*out=c->in[2];if((*out&255u)==1)for(i=0;i<3;++i)++c->lists[i];return RF_OK;}
static int request_search(void *v,uint32_t *out){request_fixture *c=v;int s=request_step(c,5);if(s)return s;c->route.count=3;*out=c->in[3];return RF_OK;}
static int request_cleanup(void *v){request_fixture *c=v;uint32_t i;int s=request_step(c,6);if(s)return s;for(i=0;i<3;++i)--c->lists[i];c->goal.query_point[0]=40;c->goal.query_point[1]=50;c->goal.query_point[2]=60;return RF_OK;}
static int ai_request_probe(void){
 uint32_t in[6];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(in,sizeof(in),1,stdin)==1){
  request_fixture c={0};rf_entity_navigation_request q;rf_entity_navigation_request_backend b={request_clear,request_prepare,request_start,request_goal,request_search,request_cleanup,&c};uint32_t out[15]={0},i;
  memcpy(c.in,in,sizeof(in));c.route.count=9;for(i=0;i<3;++i){c.goal.position[i]=(float)(i+1);c.goal.query_point[i]=(float)(i+4);c.lists[i]=3;}c.goal.rejected_035=7;c.first.rejected_035=8;
  q.goal=&c.goal;q.first_start=in[4]?&c.first:NULL;q.route=&c.route;q.search_mode=in[0];out[1]=99;
  out[0]=rf_entity_navigation_request_run(&q,&b,out+1);out[2]=c.route.count;memcpy(out+3,c.goal.position,12);memcpy(out+6,c.goal.query_point,12);out[9]=c.goal.rejected_035;out[10]=c.first.rejected_035;memcpy(out+11,c.lists,12);out[14]=c.calls;
  if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }return ferror(stdin)?3:0;
}
