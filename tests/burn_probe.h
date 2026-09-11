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
