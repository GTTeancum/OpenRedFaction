typedef struct wd_input {
 rf_weapon_drop_source source;rf_weapon_inventory inventory;rf_weapon_drop_definition definitions[64];
 int32_t excluded;uint32_t parameter;rf_random_state random;rf_weapon_drop_pose pose;
 uint32_t handled;int32_t mapped;uint32_t remote;int32_t replacement;
 rf_entity_death_drop_hit hit;uint32_t allocation,flags;float bound;
} wd_input;
typedef struct wd_fixture {wd_input input;float query[6];int32_t query_current;rf_weapon_drop_request request;
 rf_entity_death_drop_item item;uint32_t count,events[16],notification[5];} wd_fixture;
static void wd_event(wd_fixture *f,uint32_t e){f->events[f->count++]=e;}
static uint32_t wd_pose(void *context,rf_weapon_drop_pose *pose){wd_fixture *f=context;wd_event(f,0);*pose=f->input.pose;return f->input.handled;}
static int32_t wd_map(void *context,int32_t weapon){wd_fixture *f=context;(void)weapon;wd_event(f,1);return f->input.mapped;}
static uint32_t wd_remote(void *context,int32_t item){wd_fixture *f=context;(void)item;return f->input.remote;}
static int32_t wd_resolve(void *context){wd_fixture *f=context;wd_event(f,2);return f->input.replacement;}
static void wd_remove(void *context,int32_t weapon){wd_fixture *f=context;wd_event(f,3);if(weapon>=0 && weapon<64)f->input.inventory.owned[weapon]=0;}
static int wd_query(void *context,const float start[3],const float delta[3],rf_entity_death_drop_hit *hit)
{wd_fixture *f=context;wd_event(f,4);memcpy(f->query,start,12);memcpy(f->query+3,delta,12);f->query_current=f->input.source.current;*hit=f->input.hit;return f->input.allocation==2?RF_IO:RF_OK;}
static rf_entity_death_drop_item *wd_create(void *context,const rf_weapon_drop_request *request)
{wd_fixture *f=context;wd_event(f,5);f->request=*request;f->item.flags_2bc=f->input.flags;f->item.model=0x12345678;
 memcpy(f->item.position,request->pose.position,12);memcpy(f->item.base_position,request->pose.position,12);return f->input.allocation?&f->item:NULL;}
static void wd_notify(void *context,uint32_t owner,int32_t item,const float point[3])
{wd_fixture *f=context;wd_event(f,6);f->notification[0]=owner;f->notification[1]=(uint32_t)item;memcpy(f->notification+2,point,12);}
static int wd_bounds(void *context,uint32_t model,float *bound){wd_fixture *f=context;(void)model;wd_event(f,7);*bound=f->input.bound;return f->input.allocation==3?RF_IO:RF_OK;}
static int weapon_drop_probe(void)
{
 wd_fixture f;rf_entity_death_drop_item *result;uint32_t header[4],zero[8]={0};int status;
 rf_weapon_drop_backend backend={wd_pose,wd_map,wd_remote,wd_resolve,wd_remove,wd_query,wd_create,wd_notify,wd_bounds,&f};
 _Static_assert(sizeof(wd_input)==1108,"weapon drop fixture ABI");
 while(fread(&f.input,sizeof(f.input),1,stdin)==1) {
  memset((char*)&f+sizeof(f.input),0,sizeof(f)-sizeof(f.input));
  status=rf_weapon_drop_sp(&f.input.source,&f.input.inventory,f.input.definitions,f.input.excluded,f.input.parameter,&f.input.random,&backend,&result);
  header[0]=(uint32_t)status;header[1]=(uint32_t)f.input.source.current;header[2]=f.input.random.value;header[3]=result!=NULL;
  if(fwrite(header,16,1,stdout)!=1 || fwrite(f.input.inventory.owned,64,1,stdout)!=1 || fwrite(f.query,28,1,stdout)!=1 ||
     fwrite(&f.request,60,1,stdout)!=1 || fwrite(result?(void*)result:(void*)zero,32,1,stdout)!=1 ||
     fwrite(&f.count,68,1,stdout)!=1 || fwrite(f.notification,20,1,stdout)!=1)return 3;
 }
 return 0;
}
