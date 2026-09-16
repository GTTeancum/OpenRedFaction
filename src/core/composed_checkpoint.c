#include "rf/composed_checkpoint.h"
#include <string.h>
static uint32_t read32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(unsigned char *p,uint32_t n)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static int rfds_header(const void *data,uint32_t bytes,uint32_t profile)
{
    const unsigned char *p=data;
    if(!p)return RF_RANGE;
    if(bytes<RF_COMPOSED_CHECKPOINT_RFDS_MIN||bytes>RF_COMPOSED_CHECKPOINT_RFDS_MAX)return RF_RANGE;
    if(profile!=RF_COMPOSED_PROFILE_CAVITY && profile!=RF_COMPOSED_PROFILE_AUTHORED)return RF_FORMAT;
    if(memcmp(p,"RFDS",4)||read32(p+4)!=profile||read32(p+8)!=bytes)return RF_FORMAT;
    if(profile==RF_COMPOSED_PROFILE_AUTHORED &&
       (bytes<416 || read32(p+276)!=416 || read32(p+280)!=128 || read32(p+284)!=2))return RF_FORMAT;
    return RF_OK;
}
int rf_composed_checkpoint_encode(uint32_t profile_id,const rf_player_checkpoint *player,
    const rf_player_checkpoint_catalog *catalog,const void *rfds,uint32_t rfds_bytes,
    void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char packed[RF_PLAYER_CHECKPOINT_BYTES],*p=output;uint32_t bytes;int status;
    if(!p||!written)return RF_RANGE;
    status=rfds_header(rfds,rfds_bytes,profile_id);if(status)return status;
    bytes=RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES+rfds_bytes;
    if(capacity<bytes)return RF_RANGE;
    status=rf_player_checkpoint_encode(player,catalog,packed,sizeof(packed));if(status)return status;
    memset(p,0,RF_COMPOSED_CHECKPOINT_HEADER);memcpy(p,"RFCP",4);
    put32(p+4,1);put32(p+8,bytes);put32(p+16,profile_id);put32(p+20,RF_PLAYER_CHECKPOINT_BYTES);put32(p+24,rfds_bytes);
    memcpy(p+RF_COMPOSED_CHECKPOINT_HEADER,packed,sizeof(packed));
    if(rfds!=p+RF_COMPOSED_CHECKPOINT_HEADER+sizeof(packed))
        memcpy(p+RF_COMPOSED_CHECKPOINT_HEADER+sizeof(packed),rfds,rfds_bytes);
    *written=bytes;return RF_OK;
}
int rf_composed_checkpoint_preflight(const void *data,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *catalog,rf_composed_checkpoint *out)
{
    const unsigned char *p=data;rf_composed_checkpoint value={0};uint32_t rfds_bytes;int status;
    if(!p||!out)return RF_RANGE;
    if(bytes<RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES+RF_COMPOSED_CHECKPOINT_RFDS_MIN||bytes>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    if(memcmp(p,"RFCP",4)||read32(p+4)!=1||read32(p+8)!=bytes||read32(p+12)||
       read32(p+16)!=profile_id||read32(p+20)!=RF_PLAYER_CHECKPOINT_BYTES||read32(p+28))return RF_FORMAT;
    rfds_bytes=read32(p+24);
    if(rfds_bytes!=bytes-RF_COMPOSED_CHECKPOINT_HEADER-RF_PLAYER_CHECKPOINT_BYTES)return RF_FORMAT;
    value.rfds=p+RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES;value.rfds_bytes=rfds_bytes;
    status=rfds_header(value.rfds,value.rfds_bytes,profile_id);if(status)return status;
    status=rf_player_checkpoint_decode(p+RF_COMPOSED_CHECKPOINT_HEADER,RF_PLAYER_CHECKPOINT_BYTES,catalog,&value.player);if(status)return status;
    *out=value;return RF_OK;
}
