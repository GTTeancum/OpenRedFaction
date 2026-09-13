typedef struct df_fixture {uint32_t in[8],hash,calls;int32_t count;rf_entity_contact_driller_state state;rf_entity_contact_player p[4];const rf_entity_contact_player *slots[4];rf_entity_contact_player_actor actor;} df_fixture;
static int df_trace(df_fixture *f,uint32_t event,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{uint32_t row[5]={event,a,b,c,d};++f->calls;for(uint32_t i=0;i<5;++i)f->hash=(f->hash^row[i])*16777619u;return f->in[7]==event?RF_IO:RF_OK;}
static int df_lookup(void *context,uint32_t handle,const rf_entity_contact_player_actor **actor)
{df_fixture *f=context;uint32_t i=handle-100;f->actor.linked_handle=(f->in[2]&(1u<<i))?f->state.handle:999;f->actor.camera=300+i;*actor=(f->in[1]&(1u<<i))?&f->actor:NULL;return df_trace(f,1,handle,0,0,0);}
static int df_shake(void *context,uint32_t camera,float strength,float duration)
{df_fixture *f=context;uint32_t a,b;memcpy(&a,&strength,4);memcpy(&b,&duration,4);int status=df_trace(f,2,camera,a,b,0);if(f->in[4])f->count=0;return status;}
static int df_direction(void *context,const rf_entity_contact_player *p,const float *normal,uint32_t *direction)
{df_fixture *f=context;uint32_t v[3];memcpy(v,normal,12);*direction=f->in[3];return df_trace(f,3,p->token,v[0],v[1],v[2]);}
static int df_mark(void *context,const rf_entity_contact_player *p,uint32_t direction)
{return df_trace(context,4,p->token,direction,0,0);}
static int driller_feedback_probe(void)
{df_fixture f;uint32_t out[4];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(f.in,32,1,stdin)==1){f.hash=2166136261u;f.calls=0;f.count=(int32_t)f.in[0];memset(&f.state,0,sizeof(f.state));f.state.handle=77;f.state.normal[0]=1;f.state.normal[1]=-2;f.state.normal[2]=3;
 for(uint32_t i=0;i<4;++i){f.p[i].token=200+i;f.p[i].entity_handle=100+i;f.slots[i]=f.p+i;}
 rf_entity_contact_feedback_backend b={&f,&f.count,4,f.slots,df_lookup,df_shake,df_direction,df_mark};out[0]=(uint32_t)rf_entity_contact_driller_feedback(&f.state,&b);out[1]=f.hash;out[2]=f.calls;out[3]=(uint32_t)f.count;if(fwrite(out,16,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
