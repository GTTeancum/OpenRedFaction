#include "rf/burn.h"
static uint32_t burn_refs[4],burn_trace[72][2],burn_count;
static void burn_record_call(uint32_t fn,uint32_t value)
{if(burn_count<72){burn_trace[burn_count][0]=fn;burn_trace[burn_count][1]=value;}++burn_count;}
static void burn_reset(void *ctx,uint32_t emitter){(void)ctx;burn_record_call(0x4973d0,emitter);}
static void burn_free(void *ctx,uint32_t emitter){(void)ctx;burn_record_call(0x497d80,emitter);}
static void burn_stop(void *ctx,uint32_t voice){(void)ctx;burn_record_call(0x505a40,voice);}
static void burn_clear(void *ctx,uint32_t token)
{uint32_t i;(void)ctx;for(i=0;i<4;++i)if(burn_refs[i]==token){burn_refs[i]=0;break;}}
static int burn_pool_probe(void)
{
    uint32_t wire[138];rf_burn_pool pool;int status;
    rf_burn_release_backend backend={burn_reset,burn_free,burn_stop,burn_clear,NULL};
    _Static_assert(sizeof(pool)==524,"Burn pool wire");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&pool,wire,524);memcpy(burn_refs,wire+131,16);burn_count=0;memset(burn_trace,0,sizeof(burn_trace));
        status=wire[137]?rf_burn_pool_initialize(&pool,&backend):rf_burn_release(&pool,wire[135],wire[136],&backend);
        if(burn_count>72)return 4;
        fwrite(&status,4,1,stdout);fwrite(&pool,524,1,stdout);fwrite(burn_refs,16,1,stdout);fwrite(&burn_count,4,1,stdout);fwrite(burn_trace,576,1,stdout);
    }
    return ferror(stdin)?2:0;
}

static uint32_t bc_gate,bc_mask,bc_calls[16],bc_count,bc_emitters,bc_template,bc_error;
static void bc_record(uint32_t address){if(bc_count<16)bc_calls[bc_count]=address;++bc_count;}
static void bc_prepare(void *ctx,int32_t index)
{(void)ctx;bc_record(index==-1?0x42f810:0x42f840);bc_template=(uint32_t)index;}
static uint32_t bc_predicate(void *ctx,uint32_t stage,uint32_t target)
{static const uint32_t address[4]={0x426fc0,0x40a1e0,0x42cca0,0x5001d0};(void)ctx;bc_record(address[stage]);if(target!=0x12340001)bc_error=1;return stage<2?bc_gate!=stage+1:bc_gate==stage+1;}
static uint32_t bc_attachments(void *ctx,uint32_t target,int32_t indices[4])
{uint32_t i;(void)ctx;bc_record(0x42eb20);if(target!=0x12340001)bc_error=1;for(i=0;i<4;++i)indices[i]=10+(int32_t)i;return bc_gate!=5;}
static uint32_t bc_emitter(void *ctx,uint32_t target)
{uint32_t index=bc_emitters++;(void)ctx;bc_record(0x497ca0);if(target!=0x12340001 || bc_template!=(index<3?0u:1u))bc_error=1;return bc_mask&(1u<<index)?0:0x45670000+index;}
static uint32_t bc_sample(void *ctx){(void)ctx;bc_record(0x434da0);return bc_mask&16?UINT32_MAX:28;}
static uint32_t bc_play(void *ctx,uint32_t target,uint32_t sample)
{(void)ctx;bc_record(0x5056a0);if(target!=0x12340001 || sample!=(bc_mask&16?UINT32_MAX:28))bc_error=1;return bc_mask&32?UINT32_MAX:0x56780001;}
static int burn_create_probe(void)
{
    uint32_t wire[133],token;rf_burn_pool pool;int status;
    rf_burn_create_backend backend={bc_prepare,bc_predicate,bc_attachments,bc_emitter,bc_sample,bc_play,NULL};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&pool,wire,524);bc_gate=wire[131];bc_mask=wire[132];bc_count=0;bc_emitters=0;bc_error=0;memset(bc_calls,0,64);token=0xa5a5a5a5;
        status=rf_burn_create(&pool,0x12340001,0x23450002,&backend,&token);
        if(bc_count>16 || bc_error)return 4;
        fwrite(&status,4,1,stdout);fwrite(&pool,524,1,stdout);fwrite(&token,4,1,stdout);fwrite(&bc_count,4,1,stdout);fwrite(bc_calls,64,1,stdout);
    }
    return ferror(stdin)?2:0;
}

static uint32_t bf_flags,bf_entity,bf_trace[8][3],bf_count;
static void bf_record(uint32_t fn,uint32_t a,uint32_t b){if(bf_count<8){bf_trace[bf_count][0]=fn;bf_trace[bf_count][1]=a;bf_trace[bf_count][2]=b;}++bf_count;}
static void bf_stop(void *ctx,uint32_t emitter){(void)ctx;bf_record(0x4973d0,emitter,0);}
static uint32_t *bf_owner(void *ctx,uint32_t target){(void)ctx;bf_record(0x4174c0,target,0);return &bf_flags;}
static int bf_present(void *ctx,uint32_t target){(void)ctx;bf_record(0x426fc0,target,0);return bf_entity!=0;}
static void bf_reaction(void *ctx,uint32_t target){(void)ctx;(void)target;bf_record(0x407ee0,0x300042a0,0);}
static void bf_release(void *ctx,uint32_t token){(void)ctx;(void)token;bf_record(0x42ed20,0x30000000,0);}
static int burn_fade_probe(void)
{
    uint32_t wire[48];rf_burn_record record;rf_burn_emitter_view values[4],*views[4];uint32_t i;int status;
    rf_burn_fade_backend backend={bf_stop,bf_owner,bf_present,bf_reaction,bf_release,NULL};
    _Static_assert(sizeof(values)==112,"Fade emitter wire");
    for(i=0;i<4;++i)views[i]=&values[i];
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&record,wire,64);memcpy(values,wire+16,112);bf_flags=wire[44];bf_entity=wire[47];bf_count=0;memset(bf_trace,0,96);
        status=rf_burn_fade(&record,views,1,(int32_t)wire[45],(int32_t)wire[46],&backend);
        if(bf_count>8)return 4;
        fwrite(&status,4,1,stdout);fwrite(&record,64,1,stdout);fwrite(values,112,1,stdout);fwrite(&bf_flags,4,1,stdout);fwrite(&bf_count,4,1,stdout);fwrite(bf_trace,96,1,stdout);
    }
    return ferror(stdin)?2:0;
}

static uint32_t bu_missing;
static int bu_present(void *ctx,uint32_t target){(void)ctx;burn_record_call(0x40a0e0,target);return target!=bu_missing;}
static int bu_body(void *ctx,uint32_t token,rf_burn_record *record){(void)ctx;(void)record;burn_record_call(0x42ef3e,token);return RF_OK;}
static void bu_clear(void *ctx,uint32_t token){(void)ctx;burn_record_call(0x42ee13,token);}
static int burn_update_probe(void)
{
    uint32_t wire[132];rf_burn_pool pool;int status;
    rf_burn_release_backend release={burn_reset,burn_free,burn_stop,bu_clear,NULL};
    rf_burn_update_backend backend={bu_present,bu_body,&release,NULL};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&pool,wire,524);bu_missing=wire[131];burn_count=0;memset(burn_trace,0,sizeof(burn_trace));
        status=rf_burn_pool_update(&pool,1000,&backend);
        if(burn_count>64)return 4;
        fwrite(&status,4,1,stdout);fwrite(&pool,524,1,stdout);fwrite(&burn_count,4,1,stdout);fwrite(burn_trace,512,1,stdout);
    }
    return ferror(stdin)?2:0;
}

static uint32_t bo_trace[4][9],bo_count,bo_random;
static float bo_divisor;
static void bo_record(const uint32_t words[9]){if(bo_count<4)memcpy(bo_trace[bo_count],words,36);++bo_count;}
static void bo_audio(void *ctx,uint32_t voice,const float position[3],const float velocity[3],float volume)
{uint32_t row[9]={0x5058c0,voice};(void)ctx;memcpy(row+2,position,12);memcpy(row+5,velocity,12);memcpy(row+8,&volume,4);bo_record(row);}
static float bo_divide(void *ctx,float minimum,float maximum)
{uint32_t row[9]={0x504e40};(void)ctx;memcpy(row+1,&minimum,4);memcpy(row+2,&maximum,4);bo_record(row);return bo_divisor;}
static void bo_damage(void *ctx,uint32_t target,float amount)
{uint32_t row[9]={0x4892c0,target,0,UINT32_MAX,UINT32_MAX,4,0,UINT32_MAX,0};(void)ctx;memcpy(row+2,&amount,4);bo_record(row);}
static uint32_t bo_rand(void *ctx){uint32_t row[9]={0x57312d};(void)ctx;bo_record(row);return bo_random;}
static void bo_fade(void *ctx,uint32_t token){uint32_t row[9]={0x42f2f0,0x30000000};(void)ctx;(void)token;bo_record(row);}
static int burn_owner_probe(void)
{
    uint32_t wire[29];rf_burn_record record;rf_burn_owner_view owner;float delta;int status;
    rf_burn_owner_backend backend={bo_audio,bo_divide,bo_damage,bo_rand,bo_fade,NULL};
    _Static_assert(sizeof(owner)==40,"Burn owner wire");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&record,wire,64);memcpy(&owner,wire+16,40);memcpy(&delta,wire+26,4);memcpy(&bo_divisor,wire+27,4);bo_random=wire[28];bo_count=0;memset(bo_trace,0,144);
        status=rf_burn_owner_tick(&record,&owner,1,delta,&backend);
        if(bo_count>4)return 4;
        fwrite(&status,4,1,stdout);fwrite(&record,64,1,stdout);fwrite(&owner,40,1,stdout);fwrite(&bo_count,4,1,stdout);fwrite(bo_trace,144,1,stdout);
    }
    return ferror(stdin)?2:0;
}

static float ba_points[4][3];static uint32_t ba_trace[8][5],ba_count;
static int ba_attachment(void *ctx,int32_t index,float point[3])
{(void)ctx;if(index<0 || index>3 || ba_count>=8)return RF_RANGE;ba_trace[ba_count][0]=0;ba_trace[ba_count++][1]=(uint32_t)index;memcpy(point,ba_points[index],12);return RF_OK;}
static int ba_move(void *ctx,uint32_t emitter,const float point[3])
{(void)ctx;if(ba_count>=8)return RF_RANGE;ba_trace[ba_count][0]=1;ba_trace[ba_count][1]=emitter;memcpy(ba_trace[ba_count++]+2,point,12);return RF_OK;}
static int burn_attachment_probe(void)
{
    uint32_t wire[28];rf_burn_record record;rf_burn_attachment_result result;int status;
    rf_burn_attachment_backend be={ba_attachment,ba_move,NULL};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&record,wire,64);memcpy(ba_points,wire+16,48);memset(&result,0,sizeof(result));memset(ba_trace,0,sizeof(ba_trace));ba_count=0;
        status=rf_burn_attachments(&record,&be,&result);
        fwrite(&status,4,1,stdout);fwrite(&result,16,1,stdout);fwrite(&ba_count,4,1,stdout);fwrite(ba_trace,160,1,stdout);
    }
    return ferror(stdin)?2:0;
}

static uint32_t bs_gates[4],bs_trace[6][9],bs_count;static float bs_divisor;
static void bs_row(const uint32_t row[9]){if(bs_count<6)memcpy(bs_trace[bs_count],row,36);++bs_count;}
static uint32_t bs_predicate(void *ctx,uint32_t stage,rf_burn_spread_target *target)
{static const uint32_t ids[4]={0x429990,0x427020,0x40a110,0x4290d0};uint32_t row[9]={0};(void)ctx;(void)target;row[0]=ids[stage];row[1]=0x30003000;bs_row(row);return bs_gates[stage];}
static float bs_random(void *ctx,float minimum,float maximum)
{uint32_t row[9]={0x504e40};rf_burn_spread_target *target=ctx;if(!(target->flags_814&0x2000))return 0;memcpy(row+1,&minimum,4);memcpy(row+2,&maximum,4);bs_row(row);return bs_divisor;}
static void bs_damage(void *ctx,rf_burn_spread_target *target,float amount,uint32_t source,uint32_t global_value,uint32_t uid)
{uint32_t row[9]={0x4892c0,target->handle,0,source,global_value,4,0,uid,0};(void)ctx;memcpy(row+2,&amount,4);bs_row(row);}
static int burn_spread_probe(void)
{
    uint32_t wire[20];rf_burn_record record={0};rf_burn_spread_target owner={0},target={0};float spine[3];int status;
    rf_burn_spread_backend be={bs_predicate,bs_random,bs_damage,&target};
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(bs_gates,wire,16);memcpy(target.position,wire+4,12);memcpy(&target.class_health,wire+7,4);target.flags_814=wire[8];target.handle=wire[9];
        memcpy(spine,wire+10,12);record.target=wire[13];memcpy(&bs_divisor,wire+16,4);owner.next=&target;target.next=NULL;
        bs_count=0;memset(bs_trace,0,sizeof(bs_trace));
        status=rf_burn_spread(&owner,&owner,spine,&record,wire[14],wire[15],wire[17],&be);
        if(bs_count>6)return 4;
        fwrite(&status,4,1,stdout);fwrite(&target.flags_814,4,1,stdout);fwrite(&bs_count,4,1,stdout);fwrite(bs_trace,216,1,stdout);
    }
    return ferror(stdin)?2:0;
}
