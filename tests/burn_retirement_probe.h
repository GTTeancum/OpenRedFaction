/* Traversal -> owner tail -> fade -> release composition. */
static rf_burn_pool br_pool;static uint32_t br_rows[128][2],br_count;static int br_error;
static rf_burn_emitter_view br_emitters[4];static rf_burn_emitter_view *br_views[4]={br_emitters,br_emitters+1,br_emitters+2,br_emitters+3};
static void br_log(uint32_t address,uint32_t arg){if(br_count<128){br_rows[br_count][0]=address;br_rows[br_count][1]=arg;}++br_count;}
static void br_reset(void *ctx,uint32_t e){(void)ctx;br_log(0x4973d0,e);}
static void br_free(void *ctx,uint32_t e){(void)ctx;br_log(0x497d80,e);}
static void br_stop(void *ctx,uint32_t e){(void)ctx;br_log(0x505a40,e);}
static void br_clear(void *ctx,uint32_t token){(void)ctx;br_log(0x42ee13,token);}
static rf_burn_release_backend br_release_be={br_reset,br_free,br_stop,br_clear,NULL};
static int br_present(void *ctx,uint32_t target){(void)ctx;br_log(0x40a0e0,target);return 1;}
static int br_entity(void *ctx,uint32_t target){(void)ctx;br_log(0x426fc0,target);return 0;}
static uint32_t *br_flags(void *ctx,uint32_t target){(void)ctx;(void)target;br_error=RF_FORMAT;return NULL;}
static void br_reaction(void *ctx,uint32_t target){(void)ctx;(void)target;br_error=RF_FORMAT;}
static void br_release(void *ctx,uint32_t token){(void)ctx;int status=rf_burn_release(&br_pool,token,0,&br_release_be);if(status)br_error=status;}
static rf_burn_fade_backend br_fade_be={br_reset,br_flags,br_entity,br_reaction,br_release,NULL};
static void br_fade(void *ctx,uint32_t token){(void)ctx;br_log(0x42f2f0,token);int status=rf_burn_fade(&br_pool.records[token-1],br_views,token,br_pool.spread_deadline,1000,&br_fade_be);if(status)br_error=status;}
static void br_audio(void *ctx,uint32_t voice,const float p[3],const float v[3],float volume){(void)ctx;(void)p;(void)v;(void)volume;br_log(0x5058c0,voice);}
static float br_random(void *ctx,float a,float b){(void)ctx;(void)a;(void)b;br_error=RF_FORMAT;return 6;}
static void br_damage(void *ctx,uint32_t t,float a){(void)ctx;(void)t;(void)a;br_error=RF_FORMAT;}
static uint32_t br_rand(void *ctx){(void)ctx;br_error=RF_FORMAT;return 0;}
static int br_body(void *ctx,uint32_t token,rf_burn_record *record)
{rf_burn_owner_view owner={0};rf_burn_owner_backend be={br_audio,br_random,br_damage,br_rand,br_fade,NULL};(void)ctx;owner.flags_810=1;br_log(0x42ef3e,token);int status=rf_burn_owner_tick(record,&owner,token,.125f,&be);return status?status:br_error;}
static int burn_retirement_probe(void)
{
    rf_burn_update_backend be={br_present,br_body,&br_release_be,NULL};int status;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&br_pool,524,1,stdin)==1){br_count=0;br_error=0;memset(br_rows,0,sizeof(br_rows));memset(br_emitters,0,sizeof(br_emitters));status=rf_burn_pool_update(&br_pool,1000,&be);if(br_count>128)return 4;fwrite(&status,4,1,stdout);fwrite(&br_pool,524,1,stdout);fwrite(&br_count,4,1,stdout);fwrite(br_rows,1024,1,stdout);}
    return ferror(stdin)?2:0;
}
