typedef struct world_probe_context {int32_t value,status;uint32_t calls,model,kind;} world_probe_context;
static int world_probe_lookup(void *context,uint32_t model,const char *name,int32_t *tag)
{
 world_probe_context *c=context;++c->calls;
 if(model!=c->model || strcmp(name,c->kind?"muzzle_1":"grip_1"))return RF_FORMAT;
 *tag=c->value;return c->status;
}
static int weapon_world_probe(void)
{
 struct {rf_weapon_world_model models[64];int32_t weapon;uint32_t kind;int32_t value,status;} in;
 struct {int32_t status,tag;uint32_t calls;rf_weapon_world_model models[64];} out;
 while(fread(&in,sizeof(in),1,stdin)==1) {
  world_probe_context c={in.value,in.status,0,rf_weapon_world_model_token(in.models,in.weapon),in.kind};
  out.tag=-99;out.status=rf_weapon_world_tag(in.models,in.weapon,in.kind,world_probe_lookup,&c,&out.tag);
  out.calls=c.calls;memcpy(out.models,in.models,sizeof(in.models));
  if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
