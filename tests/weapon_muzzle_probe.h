typedef struct muzzle_probe_input {
 rf_weapon_muzzle_source source;rf_weapon_world_model models[64];
 float hand_basis[9],hand_point[3],muzzle_point[3],aim_basis[9];int32_t tag;
} muzzle_probe_input;
typedef struct muzzle_probe_context {muzzle_probe_input *in;uint32_t calls[3];} muzzle_probe_context;
static int muzzle_tag(void *v,uint32_t model,const char *name,int32_t *tag)
{
 muzzle_probe_context *c=v;++c->calls[0];
 if(model!=rf_weapon_world_model_token(c->in->models,c->in->source.weapon) || strcmp(name,"muzzle_1"))return RF_FORMAT;
 *tag=c->in->tag;return RF_OK;
}
static int muzzle_transform(void *v,uint32_t model,int32_t tag,const float basis[9],const float pos[3],float out[9],float point[3])
{
 muzzle_probe_context *c=v;muzzle_probe_input *in=c->in;uint32_t n=c->calls[1]++;
 if(!n) {
  int32_t hand=in->source.weapon<in->source.primary_limit?in->source.primary_tags[in->source.primary_index]:in->source.secondary_tags[in->source.secondary_index];
  if(model!=in->source.actor_model || tag!=hand || memcmp(basis,in->source.basis,36) || memcmp(pos,in->source.position,12))return RF_FORMAT;
  memcpy(out,in->hand_basis,36);memcpy(point,in->hand_point,12);
 } else {
  if(n!=1 || model!=rf_weapon_world_model_token(in->models,in->source.weapon) || tag!=in->models[in->source.weapon].muzzle || memcmp(basis,in->hand_basis,36) || memcmp(pos,in->hand_point,12))return RF_FORMAT;
  memset(out,0x7e,36);memcpy(point,in->muzzle_point,12);
 }
 return RF_OK;
}
static int muzzle_aim(void *v,const float pos[3],float basis[9])
{
 muzzle_probe_context *c=v;++c->calls[2];
 if(memcmp(pos,c->in->muzzle_point,12) || memcmp(basis,c->in->hand_basis,36))return RF_FORMAT;
 memcpy(basis,c->in->aim_basis,36);return RF_OK;
}
static int weapon_muzzle_probe(void)
{
 muzzle_probe_input in;rf_weapon_hand_ops ops={muzzle_tag,muzzle_transform};
 struct {int32_t status;float position[3],basis[9];rf_weapon_world_model models[64];uint32_t calls[3];} out;
 while(fread(&in,sizeof(in),1,stdin)==1) {
  muzzle_probe_context c={&in,{0}};memset(&out,0xa5,sizeof(out));
  out.status=rf_weapon_muzzle_pose(&in.source,in.models,&ops,muzzle_aim,&c,out.position,out.basis);
  memcpy(out.models,in.models,sizeof(out.models));memcpy(out.calls,c.calls,sizeof(out.calls));
  if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
