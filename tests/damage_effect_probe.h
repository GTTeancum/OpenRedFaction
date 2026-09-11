/* Binary oracle bridge for the shared effect orchestrator. */
static rf_damage_effect_state *de_state;
static uint32_t de_mutation[6];
static uint32_t de_facts[11],de_trace[16][6],de_count;
static void de_record(uint32_t fn,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e)
{uint32_t row[6]={fn,a,b,c,d,e};if(de_count<16)memcpy(de_trace[de_count],row,24);++de_count;
 if(fn==de_mutation[0]){memcpy(&de_state->health,de_mutation+1,4);de_state->flags_810^=de_mutation[2];de_state->flags_814^=de_mutation[3];de_state->burn^=de_mutation[4];de_state->voice^=de_mutation[5];}}
static uint32_t de_bits(float x){uint32_t v;memcpy(&v,&x,4);return v;}
static uint32_t de_predicate(void *ctx,uint32_t id,uint32_t handle)
{
    static const uint32_t fn[5]={0x429990,0x4290d0,0x42a8e0,0x429a80,0x4895d0};(void)ctx;
    de_record(fn[id],handle==0x12340001?0x30000000:0x30006000,0,0,0,0);
    return id==2?de_facts[handle==0x12340001?4:5]:de_facts[id<2?7+id:id==3?9:10];
}
static uint32_t de_uid(void *ctx,int32_t uid)
{(void)ctx;de_record(0x425210,(uint32_t)uid,0,0,0,0);return de_facts[2]?0x34560003:UINT32_MAX;}
static int de_source(void *ctx,uint32_t handle,uint32_t *affiliation)
{(void)ctx;de_record(0x426fc0,handle,0,0,0,0);*affiliation=de_facts[1];return de_facts[0]!=0;}
static uint32_t de_create(void *ctx,uint32_t target,uint32_t source)
{(void)ctx;de_record(0x42e910,target,source,0,0,0);return de_facts[3]?0x30007000:0;}
static float de_random(void *ctx,float minimum,float maximum)
{(void)ctx;de_record(0x504e40,de_bits(minimum),de_bits(maximum),0,0,0);return 4;}
static void de_notify(void *ctx,uint32_t id,uint32_t target,float value,uint32_t source)
{
    (void)ctx;(void)target;
    switch(id) {
    case RF_DAMAGE_PAIN_ANIMATION:de_record(0x428740,0x30000000,0,0,0,0);break;
    case RF_DAMAGE_PAIN_SOUND:de_record(0x4196f0,0x30000000,de_bits(value),0,0,0);break;
    case RF_DAMAGE_BURN_REACTION:de_record(0x4089f0,0x300002a0,de_bits(value),0,0,0);break;
    case RF_DAMAGE_ARMOR_REACTION:de_record(0x4085f0,0x300002a0,source,de_bits(value),0,0);break;
    case RF_DAMAGE_PLAYER_FEEDBACK:de_record(0x4a7520,0,0,0,0,0);break;
    case RF_DAMAGE_AI_REACTION:de_record(0x407fb0,0x300002a0,source,de_bits(value),0,0);break;
    }
}
static uint32_t de_playing(void *ctx,uint32_t voice)
{(void)ctx;de_record(0x505c00,voice,0,0,0,0);return de_facts[6];}
static uint32_t de_play(void *ctx,uint32_t target)
{(void)ctx;(void)target;de_record(0x5056a0,0x23,0x3000003c,0x3f800000,0x173c378,0);return 0x76540001;}
static int damage_effect_probe(void)
{
    uint32_t wire[34];rf_damage_effect_state state;rf_damage_effect_input input;int status;
    rf_damage_effect_backend backend={de_predicate,de_uid,de_source,de_create,de_random,de_notify,de_playing,de_play,NULL};
    _Static_assert(sizeof(state)==44 && sizeof(input)==24,"Damage effect wire");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        memcpy(&state,wire,44);memcpy(&input,wire+11,24);memcpy(de_facts,wire+17,44);
        memcpy(de_mutation,wire+28,24);de_state=&state;de_count=0;memset(de_trace,0,sizeof(de_trace));status=rf_entity_damage_effects(&state,&input,&backend);
        if(de_count>16)return 4;
        fwrite(&status,4,1,stdout);fwrite(&state,44,1,stdout);fwrite(&de_count,4,1,stdout);fwrite(de_trace,384,1,stdout);
    }
    return ferror(stdin)?2:0;
}
