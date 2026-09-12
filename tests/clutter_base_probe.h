typedef struct clutter_base_probe_input {
 uint32_t kind,loaded,count,allocation_flags,flags,room,parent_byte,parent_group,generation,free_count,budget,fail;
 float position[3],matrix[9],radius,center[3],model_radius,material[3];
} clutter_base_probe_input;
typedef struct clutter_base_probe_context {
 clutter_base_probe_input in;rf_model_collision_sphere rows[8];rf_object_registry *registry;rf_object_list *objects;uint32_t calls,releases;
} clutter_base_probe_context;
static int cbp_event(clutter_base_probe_context *c,uint32_t stage)
{rf_clutter_state *s=rf_object_registry_lookup(c->registry,(c->in.generation<<16)|7);
 c->calls|=1u<<stage;if(!s || c->objects->count!=2 || s->model!=(stage==1?0:c->in.loaded))return RF_FORMAT;
 return c->in.fail==stage?RF_IO:RF_OK;}
static int cbp_load(void *v,uint32_t k,const char *n,uint32_t a,uint32_t b,uint32_t *m)
{clutter_base_probe_context *c=v;int s=cbp_event(c,1);(void)n;(void)a;(void)b;if(k!=c->in.kind)return RF_FORMAT;if(!s)*m=c->in.loaded;return s;}
static int cbp_bounds(void *v,uint32_t m,float p[3],float *r)
{clutter_base_probe_context *c=v;int s=cbp_event(c,2);(void)m;if(!s){memcpy(p,c->in.center,12);*r=c->in.model_radius;}return s;}
static int cbp_animate(void *v,uint32_t m,int32_t motion,float speed)
{(void)m;if(motion || speed!=1)return RF_FORMAT;return cbp_event(v,3);}
static int cbp_property(void *v,uint32_t m,int32_t *p)
{int s=cbp_event(v,4);(void)m;if(!s)*p=17;return s;}
static int cbp_spheres(void *v,uint32_t m,rf_clutter_model_view *p)
{clutter_base_probe_context *c=v;int s=cbp_event(c,5);(void)m;if(!s){p->spheres=c->rows;p->count=c->in.count;p->wrapper_kind=1;p->matrices=NULL;p->bones=0;}return s;}
static void cbp_release(void *v,uint32_t m)
{clutter_base_probe_context *c=v;if(m==c->in.loaded)++c->releases;}
static int clutter_base_probe(void)
{
 clutter_base_probe_context c;rf_object_registry registry;rf_object_list list;rf_object_link prior;
 rf_clutter_base_backend b={{cbp_load,cbp_bounds,cbp_animate,cbp_property,&c},cbp_spheres,cbp_release};
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&c.in,sizeof(c.in),1,stdin)==1) {
  rf_clutter_create_descriptor d={0};rf_clutter_base_owner *owner=NULL;uint32_t uid=UINT32_MAX,header[9],tail[9];int status,closed;
  if(c.in.count>8 || c.in.free_count>2 || fread(c.rows,sizeof(c.rows[0]),c.in.count,stdin)!=c.in.count)return 2;
  c.registry=&registry;c.objects=&list;c.calls=c.releases=0;
  rf_object_registry_init(&registry);registry.count=c.in.free_count;registry.free_slots[0]=7;registry.free_slots[1]=9;registry.generation=c.in.generation;
  rf_object_list_init(&list);memset(&prior,0,sizeof(prior));rf_object_list_append(&list,&prior);
  d.model=c.in.kind?"clutter.v3m":NULL;d.kind=c.in.kind;d.material=2;d.flags=c.in.flags;d.allocation_flags=c.in.allocation_flags;d.identifier=123;
  memcpy(d.position,c.in.position,12);memcpy(d.matrix,c.in.matrix,36);d.radius=c.in.radius;
  status=rf_clutter_base_open(&d,&registry,&list,&uid,c.in.room,c.in.parent_byte,c.in.parent_group,c.in.material,&b,c.in.budget,&owner);
  header[0]=status;header[1]=owner!=NULL;header[2]=uid;header[3]=list.count;header[4]=list.peak;header[5]=registry.generation;header[6]=registry.count;
  header[7]=registry.free_slots[registry.head];header[8]=registry.count?registry.free_slots[(registry.head+registry.count-1)%1024]:UINT32_MAX;
  fwrite(header,4,9,stdout);
  if(owner) {
   rf_clutter_base_owner copy=*owner;copy.state.token=1;copy.object_link.next=copy.object_link.previous=(rf_object_link *)(uintptr_t)1;
   copy.body.spheres.items=(rf_physics_sphere *)(uintptr_t)(copy.body.spheres.items!=NULL);
   fwrite(&copy,sizeof(copy),1,stdout);fwrite(owner->body.spheres.items,24,owner->body.spheres.count,stdout);
  }
  closed=rf_clutter_base_close(&owner,&registry,&list,&b);tail[0]=closed;tail[1]=list.count;tail[2]=list.peak;tail[3]=registry.generation;tail[4]=registry.count;
  tail[5]=registry.free_slots[registry.head];tail[6]=registry.count?registry.free_slots[(registry.head+registry.count-1)%1024]:UINT32_MAX;tail[7]=c.releases;tail[8]=c.calls;
  fwrite(tail,4,9,stdout);if(owner || rf_clutter_base_close(&owner,&registry,&list,&b))return 3;
 }
 return 0;
}
