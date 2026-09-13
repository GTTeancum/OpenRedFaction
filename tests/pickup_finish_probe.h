typedef struct pf_fixture {rf_entity_pickup_finish_state state;uint32_t handler,player;int32_t now;uint32_t result;int32_t after_grant,after_hide;uint32_t fail,trace;} pf_fixture;
static int pf_grant(void *ctx,uint32_t handler,uint32_t actor,uint32_t item,uint32_t quantity,uint32_t *result)
{pf_fixture *f=ctx;if(handler!=f->handler || actor!=f->state.actor_handle || item!=f->state.item_handle || quantity!=f->state.quantity)return RF_FORMAT;f->trace=1;f->state.respawn_delay=f->after_grant;*result=f->result;return f->fail==1?RF_IO:RF_OK;}
static int pf_step(void *ctx,rf_entity_pickup_finish_state *state,uint32_t stage)
{pf_fixture *f=ctx;if(state!=&f->state)return RF_FORMAT;f->trace=f->trace*10+stage;if(stage==5){state->respawn_delay=f->after_hide;f->now=17;}return f->fail==stage?RF_IO:RF_OK;}
static int pf_npc(void *c,rf_entity_pickup_finish_state *s){return pf_step(c,s,2);}
static int pf_mark(void *c,rf_entity_pickup_finish_state *s,uint32_t player){pf_fixture *f=c;if(player!=f->player)return RF_FORMAT;return pf_step(c,s,3);}
static int pf_retire(void *c,rf_entity_pickup_finish_state *s){return pf_step(c,s,4);}
static int pf_hide(void *c,rf_entity_pickup_finish_state *s){return pf_step(c,s,5);}
static int pf_sound(void *c,rf_entity_pickup_finish_state *s){return pf_step(c,s,6);}
static int pickup_finish_probe(void)
{pf_fixture f;uint32_t out[6];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&f,48,1,stdin)==1){rf_entity_pickup_finish_backend b={&f,&f.now,pf_grant,pf_npc,pf_mark,pf_retire,pf_hide,pf_sound};f.trace=0;out[1]=0xa5a5a5a5;out[0]=(uint32_t)rf_entity_pickup_finish_sp(&f.state,f.handler,f.player,&b,out+1);out[2]=(uint32_t)f.state.respawn_delay;out[3]=(uint32_t)f.state.deadline;out[4]=(uint32_t)f.now;out[5]=f.trace;if(fwrite(out,24,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
