typedef struct de_fixture {uint32_t wire[10],facts[2];rf_entity_death_early_state state;rf_entity_death_player_view players[2];uint32_t count,trace[6];} de_fixture;
static uint32_t de_call(void *context,uint32_t op,uint32_t token)
{
 de_fixture *f=context;f->trace[2*f->count]=op;f->trace[2*f->count+1]=token;++f->count;
 if(op==0 && f->facts[1]==1)f->state.local_actor=f->state.actor;
 if(op==1 && f->facts[1]==2)f->state.player=&f->players[1];
 if(op==1 && f->facts[1]==3)f->state.local_actor=99;
 if(op==1 && f->facts[1]==4)f->state.player=NULL;
 if(op==2 && f->facts[1]==5)f->state.player=&f->players[1];
 return op==1?f->facts[0]:0;
}
static int death_early_probe(void)
{
 de_fixture f;int status;rf_entity_death_early_backend b={de_call,&f};
 while(fread(f.wire,48,1,stdin)==1){
  f.state.actor=f.wire[0];f.state.local_actor=f.wire[1];f.state.player=f.wire[2]?&f.players[f.wire[2]-1]:NULL;
  memcpy(&f.state.now_ms,f.wire+3,12);memcpy(f.players,f.wire+6,16);f.count=0;memset(f.trace,0,24);
  status=rf_entity_death_early_sp(&f.state,&b);
  f.wire[0]=f.state.actor;f.wire[1]=f.state.local_actor;f.wire[2]=f.state.player?f.state.player->token:0;
  memcpy(f.wire+3,&f.state.now_ms,12);memcpy(f.wire+6,f.players,16);
  if(fwrite(&status,4,1,stdout)!=1 || fwrite(f.wire,40,1,stdout)!=1 || fwrite(&f.count,28,1,stdout)!=1)return 3;
 }
 return 0;
}
