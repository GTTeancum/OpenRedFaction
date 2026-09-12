typedef struct dd_input {
 rf_entity_death_drop_source source;rf_entity_death_drop_hit hit;
 uint32_t item[8];float size;uint32_t present;char name[32];
} dd_input;
typedef struct dd_fixture {dd_input input;rf_entity_death_drop_item item;float query[6],point[3];uint32_t calls[3];} dd_fixture;
static int dd_query(void *context,const float start[3],const float delta[3],rf_entity_death_drop_hit *hit)
{dd_fixture *f=context;memcpy(f->query,start,12);memcpy(f->query+3,delta,12);++f->calls[0];*hit=f->input.hit;return f->input.present==2?RF_IO:RF_OK;}
static rf_entity_death_drop_item *dd_create(void *context,int32_t index,uint32_t owner,const float point[3])
{dd_fixture *f=context;(void)index;(void)owner;++f->calls[1];memcpy(f->point,point,12);return f->input.present?&f->item:NULL;}
static int dd_bounds(void *context,uint32_t model,float *size)
{dd_fixture *f=context;(void)model;++f->calls[2];*size=f->input.size;return f->input.present==3?RF_IO:RF_OK;}
static int death_drop_probe(void)
{
 dd_fixture f;rf_entity_death_drop_item *item;rf_entity_death_drop_backend b={dd_query,dd_create,dd_bounds,&f};int status;uint32_t created;
 _Static_assert(sizeof(dd_input)==188,"death drop fixture ABI");
 while(fread(&f.input,sizeof(f.input),1,stdin)==1) {
  memcpy(&f.item,f.input.item,32);f.item.name=f.input.name;memset(f.query,0,36);memset(f.calls,0,12);
  status=rf_entity_death_drop(&f.input.source,&b,&item);created=item!=NULL;
  if(fwrite(&status,4,1,stdout)!=1 || fwrite(&created,4,1,stdout)!=1 || fwrite(&f.item,32,1,stdout)!=1 ||
     fwrite(f.query,36,1,stdout)!=1 || fwrite(f.calls,12,1,stdout)!=1)return 3;
 }
 return 0;
}
