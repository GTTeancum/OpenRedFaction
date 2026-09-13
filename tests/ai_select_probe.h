/* Binary companion to the original408ac0 trace oracle. */
typedef struct ai_select_fixture {uint32_t input[35],trace[128],count;rf_entity_ai_actor actors[5];} ai_select_fixture;
static void ai_trace(ai_select_fixture *c,uint32_t op,uint32_t value)
{if(c->count<64){c->trace[c->count*2]=op;c->trace[c->count*2+1]=value;}++c->count;}
static int ai_lookup(void *context,uint32_t handle,rf_entity_ai_actor **out)
{
    ai_select_fixture *c=context;ai_trace(c,0x426fc0,handle);
    if(handle==55)*out=c->input[23]?c->actors:NULL;
    else if(handle==77)*out=c->input[24]?c->actors+4:NULL;
    else return RF_RANGE;return RF_OK;
}
static int ai_call(void *context,rf_entity_ai_actor *s,rf_entity_ai_actor *subject,uint32_t op,float point[3],uint32_t *value,double *scalar)
{
    static const uint32_t ops[]={0x427020,0x4174c0,0x4087a0,0x408dc0,0x408ef0,0x427fb0,0x42a020,0x40a110,0x40a210,0x408d90,0x406b70,0x40ac90,0x4062c0};
    ai_select_fixture *c=context;uint32_t i;(void)subject;
    *value=0;
    for(i=0;i<13;++i)if(op==ops[i]){*value=c->input[10+i];break;}
    ai_trace(c,op,*value);
    if(op==0x401060){point[0]=2;point[1]=3;point[2]=4;}
    if(op==0x4065d0)s->transition.state_2b4=(int32_t)c->input[25];
    if(op==0x401cc0){float f;memcpy(&f,c->input+26,4);*scalar=f;}
    return RF_OK;
}
static int ai_select_probe(void)
{
    ai_select_fixture c;uint32_t wire[35];
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        uint32_t out[144]={0},i;rf_entity_ai_actor *s,*peers[4];rf_random_state random;
        rf_entity_ai_select_frame frame={0};rf_entity_ai_select_backend backend={ai_lookup,ai_call,&c};
        memset(&c,0,sizeof(c));memcpy(c.input,wire,sizeof(wire));s=c.actors;
        s->timer_4d4=(int32_t)wire[0];s->transition.action_280=(int32_t)wire[1];s->transition.clock_288=(int32_t)wire[2];
        s->transition.argument_28c=wire[3];s->transition.argument_290=wire[4];s->transition.state_2b4=(int32_t)wire[5];s->transition.clock_2bc=(int32_t)wire[6];
        s->transition.flags_530=wire[7];s->flags_810=wire[8];s->word_834=wire[9];s->owner=s;s->handle=55;s->target_560=77;s->group=7;
        for(i=0;i<4;++i){peers[i]=c.actors+i;if(i){peers[i]->group=i<3?7:8;peers[i]->transition.action_280=i%2?2:4;}}
        memcpy(s->position,wire+29,12);memcpy(c.actors[4].position,wire+32,12);
        random.value=wire[28];frame.clock=12345.75f;frame.now_ms=12345;frame.network_a=wire[27]&1;frame.network_b=wire[27]>>1;
        frame.random=&random;frame.peers=peers;frame.peer_count=4;
        out[0]=(uint32_t)rf_entity_ai_select(s,&frame,&backend);
        out[1]=s->timer_4d4;out[2]=s->transition.action_280;out[3]=s->transition.clock_288;out[4]=s->transition.argument_28c;out[5]=s->transition.argument_290;
        out[6]=s->transition.state_2b4;out[7]=s->transition.clock_2bc;out[8]=s->transition.flags_530;out[9]=s->flags_810;out[10]=s->word_834;
        for(i=0;i<3;++i)out[11+i]=c.actors[i+1].transition.flags_530;
        out[14]=random.value;out[15]=c.count;memcpy(out+16,c.trace,sizeof(c.trace));
        if(c.count>64 || fwrite(out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}

