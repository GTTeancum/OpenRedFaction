typedef struct pg_fixture {rf_entity_pickup_gate_state state;uint32_t kind,player,blocked,fail,second_kind,trace,lookups;} pg_fixture;
static int pg_linked(void *context,uint32_t handle,int32_t *kind){pg_fixture *f=context;if(handle!=f->state.linked_handle)return RF_FORMAT;f->trace=f->trace*10+1;*kind=(int32_t)(f->lookups++?f->second_kind:f->kind);return f->fail==1?RF_IO:RF_OK;}
static int pg_player(void *context,uint32_t handle,uint32_t *player){pg_fixture *f=context;if(handle!=f->state.actor_handle)return RF_FORMAT;f->trace=f->trace*10+2;*player=f->player;return f->fail==2?RF_IO:RF_OK;}
static int pg_occluded(void *context,const float *position,const float *eye,uint32_t *blocked){pg_fixture *f=context;if(memcmp(position,f->state.item_position,12) || memcmp(eye,f->state.actor_eye,12))return RF_FORMAT;f->trace=f->trace*10+3;*blocked=f->blocked;return f->fail==3?RF_IO:RF_OK;}
static int pickup_gate_probe(void)
{pg_fixture f;uint32_t out[4];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&f,64,1,stdin)==1){rf_entity_pickup_gate_backend b={&f,pg_linked,pg_player,pg_occluded};f.trace=f.lookups=0;out[1]=out[2]=0xa5a5a5a5u;out[0]=(uint32_t)rf_entity_pickup_prepare_sp(&f.state,&b,out+1,out+2);out[3]=f.trace;if(fwrite(out,16,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
