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
