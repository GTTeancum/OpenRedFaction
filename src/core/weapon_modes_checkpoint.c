#include "rf/weapon_modes_checkpoint.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t value)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(value>>(i*8));}
static uint32_t hash(const unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<32;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
int rf_weapon_modes_checkpoint_encode(uint32_t catalog_hash,
    const rf_weapon_modes_checkpoint *state,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;
    if(!state||!output||!written||capacity<RF_WEAPON_MODES_CHECKPOINT_BYTES)return RF_RANGE;
    if(state->flags&~3u)return RF_FORMAT;
    memset(p,0,RF_WEAPON_MODES_CHECKPOINT_BYTES);memcpy(p,"RFWM",4);
    put(p+4,1);put(p+8,RF_WEAPON_MODES_CHECKPOINT_BYTES);put(p+16,catalog_hash);
    put(p+20,state->flags);put(p+24,state->conventional_rng);put(p+12,hash(p));
    *written=RF_WEAPON_MODES_CHECKPOINT_BYTES;return RF_OK;
}
int rf_weapon_modes_checkpoint_decode(const void *data,uint32_t bytes,
    uint32_t catalog_hash,rf_weapon_modes_checkpoint *output)
{
    const unsigned char *p=data;rf_weapon_modes_checkpoint state;
    if(!data||!output)return RF_RANGE;
    if(bytes!=RF_WEAPON_MODES_CHECKPOINT_BYTES||memcmp(p,"RFWM",4)||word(p+4)!=1||word(p+8)!=bytes||
       word(p+16)!=catalog_hash||word(p+28)||word(p+12)!=hash(p))return RF_FORMAT;
    state.flags=word(p+20);state.conventional_rng=word(p+24);
    if(state.flags&~3u)return RF_FORMAT;
    *output=state;return RF_OK;
}
