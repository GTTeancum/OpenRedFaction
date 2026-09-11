typedef struct dying_probe_context {uint32_t answers[14],count,trace[48],segment[10];} dying_probe_context;
static uint32_t dying_probe_call(void *context,uint32_t op,uint32_t a,uint32_t b)
{
    dying_probe_context *c=context;uint32_t n=c->count++;
    if(n<16){c->trace[3*n]=op;c->trace[3*n+1]=a;c->trace[3*n+2]=b;}
    return c->answers[op];
}
static uint32_t dying_probe_segment(void *context,const float a[3],const float b[3],const float p[3],float radius)
{
    dying_probe_context *c=context;
    dying_probe_call(context,13,0,0);memcpy(c->segment,a,12);memcpy(c->segment+3,b,12);
    memcpy(c->segment+6,p,12);memcpy(c->segment+9,&radius,4);return c->answers[13];
}
static int dying_probe_main(void)
{
    rf_entity_dying_state s;rf_entity_dying_player player;uint32_t present,status;dying_probe_context c;
    rf_entity_dying_backend b={dying_probe_call,dying_probe_segment,&c,NULL};
    while(fread(&s,sizeof(s),1,stdin)==1) {
        memset(&c,0,sizeof(c));
        if(fread(&player,sizeof(player),1,stdin)!=1 || fread(&present,4,1,stdin)!=1 || fread(c.answers,sizeof(c.answers),1,stdin)!=1)return 2;
        b.player=present?&player:NULL;status=(uint32_t)rf_entity_dying_update(&s,&b);
        fwrite(&status,4,1,stdout);fwrite(&s,sizeof(s),1,stdout);fwrite(&c.count,4,59,stdout);
    }
    return 0;
}
