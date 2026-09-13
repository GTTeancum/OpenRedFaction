typedef struct hand_probe_input {
 rf_weapon_hand_source source;rf_weapon_world_model models[64];int32_t hand;
 float basis[9],point[3],grip_point[3];int32_t next_weapon,grip,fail;
} hand_probe_input;
typedef struct hand_probe_context {hand_probe_input *in;uint32_t model,calls[3];} hand_probe_context;
static int hand_probe_tag(void *context,uint32_t model,const char *name,int32_t *tag)
{
 hand_probe_context *c=context;++c->calls[1];++c->calls[2];
 if(model!=rf_weapon_world_model_token(c->in->models,c->in->source.weapon) || strcmp(name,"grip_1"))return RF_FORMAT;
 *tag=c->in->grip;return c->in->fail==(int32_t)c->calls[2]?RF_IO:RF_OK;
}
static int hand_probe_transform(void *context,uint32_t model,int32_t tag,const float basis[9],const float pos[3],float out_basis[9],float point[3])
{
 hand_probe_context *c=context;hand_probe_input *in=c->in;uint32_t n=c->calls[0]++;++c->calls[2];
 if(n==0) {
  if(model!=in->source.actor_model || tag!=in->source.hands[in->hand] || memcmp(basis,in->source.basis,36) || memcmp(pos,in->source.position,12))return RF_FORMAT;
  memcpy(point,in->point,12);memcpy(out_basis,in->basis,36);in->source.weapon=in->next_weapon;
 } else {
  if(model!=c->model || tag!=in->models[in->source.weapon].grip || memcmp(basis,in->basis,36) || memcmp(pos,in->point,12))return RF_FORMAT;
  memcpy(point,in->grip_point,12);
 }
 return in->fail==(int32_t)c->calls[2]?RF_IO:RF_OK;
}
static int weapon_hand_probe(void)
{
 _Static_assert(sizeof(rf_weapon_hand_source)==68,"Two primary weapon hand tags");
 hand_probe_input in;rf_weapon_hand_ops ops={hand_probe_tag,hand_probe_transform};
 struct {int32_t status;rf_weapon_hand_placement placement;rf_weapon_world_model models[64];rf_weapon_hand_source source;uint32_t calls[3];} out;
 while(fread(&in,sizeof(in),1,stdin)==1) {
  hand_probe_context c={&in,rf_weapon_world_model_token(in.models,in.source.weapon),{0}};
  memset(&out.placement,0xa5,sizeof(out.placement));
  out.status=rf_weapon_place_in_hand(&in.source,in.hand,in.models,&ops,&c,&out.placement);
  memcpy(out.models,in.models,sizeof(out.models));out.source=in.source;memcpy(out.calls,c.calls,sizeof(out.calls));
  if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
 }
 return ferror(stdin)?1:0;
}
