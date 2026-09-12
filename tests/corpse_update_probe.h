typedef struct corpse_probe_context {
    rf_corpse_update_state *state;rf_corpse_item_view sound;
    uint32_t found,mutate,count,trace[84];double duration;
} corpse_probe_context;
static void cp_trace(corpse_probe_context *c,uint32_t op,uint32_t a,uint32_t b,uint32_t d,uint32_t e,uint32_t f,uint32_t g)
{if(c->count<12){uint32_t row[7]={op,a,b,d,e,f,g};memcpy(c->trace+c->count*7,row,sizeof(row));}++c->count;}
static void cp_reset(void *ctx,uint32_t model)
{corpse_probe_context *c=ctx;cp_trace(c,0x5033f0,model,0,0,0,0,0);if(c->mutate){c->state->model=model+100;c->state->motion_2b8=7;}}
static void cp_play(void *ctx,uint32_t model,int32_t motion)
{cp_trace(ctx,0x5033b0,model,(uint32_t)motion,0x3f800000,1,0,0);}
static double cp_duration(void *ctx,uint32_t model,int32_t motion)
{corpse_probe_context *c=ctx;cp_trace(c,0x5033e0,model,(uint32_t)motion,0,0,0,0);return c->duration;}
static void cp_advance(void *ctx,uint32_t model,float dt,const float *position,const float *basis)
{uint32_t bits;memcpy(&bits,&dt,4);cp_trace(ctx,0x503360,model,bits,0,position?0x3000003c:0,basis?0x30000048:0,1);}
static void cp_pose(void *ctx)
{corpse_probe_context *c=ctx;cp_trace(c,0x4164c0,0x30000000,c->state->fade.flags_29c&8,0,0,0,0);}
static rf_corpse_item_view *cp_sound(void *ctx,int32_t id)
{corpse_probe_context *c=ctx;cp_trace(c,0x459a20,(uint32_t)id,0,0,0,0,0);return c->found?&c->sound:NULL;}
static int cp_point(void *ctx,float point[3])
{cp_trace(ctx,0x48ac70,0x30000000,0,0,0,0,0);point[0]=1.25f;point[1]=-2;point[2]=3;return RF_OK;}
static void cp_move(void *ctx,rf_corpse_item_view *sound,const float point[3])
{uint32_t bits[3];memcpy(bits,sound->position,12);cp_trace(ctx,0x48a230,0x30003000,bits[0],bits[1],bits[2],0,0);(void)point;}
static int corpse_update_probe(void)
{
    rf_corpse_update_state state;uint32_t meta[9],i;float dt;int status;
    rf_corpse_emitter_link links[4];corpse_probe_context c;
    rf_corpse_update_backend backend={cp_reset,cp_play,cp_duration,cp_advance,cp_pose,cp_sound,cp_point,cp_move,&c};
    while(fread(&state,sizeof(state),1,stdin)==1) {
        memset(&c,0,sizeof(c));c.state=&state;
        if(fread(meta,sizeof(meta),1,stdin)!=1 || fread(&c.duration,8,1,stdin)!=1 || meta[2]>4)return 2;
        memcpy(&dt,meta,4);c.found=meta[7];c.mutate=meta[8];
        for(i=0;i<meta[2];i++){links[i].next=i+1<meta[2]?links+i+1:NULL;links[i].enabled=meta+3+i;}
        status=rf_corpse_update(&state,dt,(int32_t)meta[1],meta[2]?links:NULL,meta[2],&backend);
        fwrite(&status,4,1,stdout);fwrite(&state,sizeof(state),1,stdout);fwrite(meta+3,4,4,stdout);
        fwrite(c.sound.position,4,3,stdout);fwrite(&c.count,4,1,stdout);fwrite(c.trace,4,84,stdout);
    }
    return ferror(stdin)?1:0;
}
