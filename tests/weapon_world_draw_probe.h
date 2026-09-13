typedef struct world_draw_input {rf_weapon_world_draw draw;rf_weapon_world_model models[64];rf_weapon_hand_placement poses[2];uint32_t missing,scratch[20];} world_draw_input;
typedef struct world_draw_output {int32_t status;rf_weapon_world_draw draw;uint32_t scratch[20],placed,submitted;struct {uint32_t model;rf_weapon_hand_placement pose;uint32_t state[20];} records[2];} world_draw_output;
typedef struct world_draw_context {world_draw_input *in;world_draw_output *out;} world_draw_context;
static int world_draw_place(void *user,int32_t hand,rf_weapon_hand_placement *pose)
{world_draw_context *c=user;++c->out->placed;if(c->out->placed==1 && (c->in->missing&256))c->in->draw.hand_count=0;if(c->in->missing&(hand?16384u:4096u))return RF_IO;if(c->in->missing&(1u<<hand))return RF_NOT_FOUND;*pose=c->in->poses[hand];return RF_OK;}
static int world_draw_submit(void *user,uint32_t model,const rf_weapon_hand_placement *pose,const uint32_t state[20])
{world_draw_context *c=user;uint32_t n=c->out->submitted++;if(n==0 && (c->in->missing&512)){c->in->draw.hand_count=2;c->in->draw.view.flags_810^=256;}if(n>=2)return RF_RANGE;c->out->records[n].model=model;c->out->records[n].pose=*pose;memcpy(c->out->records[n].state,state,80);return (c->in->missing&(n?32768u:8192u))?RF_IO:RF_OK;}
static int world_draw_probe(void)
{
 world_draw_input in;world_draw_output out;rf_weapon_world_draw_ops ops={world_draw_place,world_draw_submit};world_draw_context c={&in,&out};
 _Static_assert(sizeof(in)==1288,"Draw input layout");_Static_assert(sizeof(out)==440,"Draw output layout");
 while(fread(&in,sizeof(in),1,stdin)==1){memset(&out,0,sizeof(out));ops.place=(in.missing&65536)?NULL:world_draw_place;ops.submit=(in.missing&131072)?NULL:world_draw_submit;out.status=rf_weapon_world_draw_run(&in.draw,in.models,in.scratch,&ops,&c);out.draw=in.draw;memcpy(out.scratch,in.scratch,80);if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;}return ferror(stdin)?1:0;
}
