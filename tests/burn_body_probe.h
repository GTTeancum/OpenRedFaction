/* Complete burn body probe with persistent mutation-visible state. */
static uint32_t bb_rows[32][9],bb_count,bb_mode,bb_dead;
static rf_burn_record bb_record;static rf_burn_spread_target bb_targets[2];static float bb_points[4][3];
static void bb_row(uint32_t row[9]){if(bb_count<32)memcpy(bb_rows[bb_count],row,36);++bb_count;}
static int bb_attach(void *ctx,int32_t tag,float point[3])
{uint32_t row[9]={0x503230,(uint32_t)tag};(void)ctx;if(tag<0 || tag>3)return RF_RANGE;bb_row(row);memcpy(point,bb_points[tag],12);return RF_OK;}
static int bb_move(void *ctx,uint32_t emitter,const float point[3])
{uint32_t row[9]={0x4972a0,emitter};(void)ctx;memcpy(row+2,point,12);bb_row(row);memset(row,0,36);row[0]=0x4972f0;row[1]=emitter;bb_row(row);return RF_OK;}
static uint32_t bb_pred(void *ctx,uint32_t stage,rf_burn_spread_target *target)
{static const uint32_t ids[]={0x429990,0x427020,0x40a110,0x4290d0};uint32_t row[9]={0};(void)ctx;row[0]=ids[stage];row[1]=target==bb_targets?1:2;bb_row(row);return stage==1 && target==bb_targets+1 ? bb_dead:0;}
static float bb_random(void *ctx,float minimum,float maximum)
{uint32_t row[9]={0x504e40};(void)ctx;memcpy(row+1,&minimum,4);memcpy(row+2,&maximum,4);bb_row(row);return 6.5f;}
static void bb_spread_damage(void *ctx,rf_burn_spread_target *target,float amount,uint32_t source,uint32_t global,uint32_t uid)
{uint32_t row[9]={0x4892c0,target->handle,0,source,global,4,0,uid,0};(void)ctx;memcpy(row+2,&amount,4);bb_row(row);if(target==bb_targets && bb_mode==1)bb_dead=1;if(target==bb_targets && bb_mode==2)bb_record.fading=1;}
static void bb_audio(void *ctx,uint32_t voice,const float position[3],const float velocity[3],float volume)
{uint32_t row[9]={0x5058c0,voice};(void)ctx;memcpy(row+2,position,12);memcpy(row+5,velocity,12);memcpy(row+8,&volume,4);bb_row(row);}
static void bb_owner_damage(void *ctx,uint32_t target,float amount)
{uint32_t row[9]={0x4892c0,target,0,UINT32_MAX,UINT32_MAX,4,0,UINT32_MAX,0};(void)ctx;memcpy(row+2,&amount,4);bb_row(row);}
static uint32_t bb_rand(void *ctx){uint32_t row[9]={0x57312d};(void)ctx;bb_row(row);return 19;}
static void bb_fade(void *ctx,uint32_t token){uint32_t row[9]={0x42f2f0,token};(void)ctx;bb_row(row);}
static int burn_body_probe(void)
{
    uint32_t wire[55],flags[2];rf_burn_owner_view owner;float basis[9];int32_t deadline;rf_burn_spread_target spread_owner={0},*head=&spread_owner;int status;unsigned j;
    rf_burn_body_context context={&owner,basis,&spread_owner,&head,&deadline,1000,.125f,0x4321,0xabcdef01,3};
    rf_burn_body_backend be={{bb_attach,bb_move,NULL},{bb_pred,bb_random,bb_spread_damage,NULL},{bb_audio,bb_random,bb_owner_damage,bb_rand,bb_fade,NULL}};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&bb_record,wire,64);memcpy(&owner,wire+16,40);memcpy(basis,wire+26,36);memcpy(&deadline,wire+35,4);bb_mode=wire[36];memcpy(bb_points,wire+43,48);
        memset(bb_targets,0,sizeof(bb_targets));spread_owner.next=bb_targets;bb_targets[0].next=bb_targets+1;
        for(j=0;j<2;++j){memcpy(bb_targets[j].position,wire+37+j*3,12);bb_targets[j].class_health=100;bb_targets[j].handle=(j+1)*0x1111;}
        bb_count=bb_dead=0;memset(bb_rows,0,sizeof(bb_rows));status=rf_burn_body(&bb_record,1,&context,&be);if(bb_count>32)return 4;
        flags[0]=bb_targets[0].flags_814;flags[1]=bb_targets[1].flags_814;
        fwrite(&status,4,1,stdout);fwrite(&bb_record,64,1,stdout);fwrite(&owner,40,1,stdout);fwrite(flags,8,1,stdout);fwrite(&deadline,4,1,stdout);fwrite(&bb_count,4,1,stdout);fwrite(bb_rows,1152,1,stdout);
    }
    return ferror(stdin)?2:0;
}
