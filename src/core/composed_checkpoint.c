#include "rf/composed_checkpoint.h"
#include "rf/authored_checkpoint_layout.h"
#include "rf/remote_checkpoint.h"
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
    if(profile!=RF_COMPOSED_PROFILE_CAVITY && profile!=RF_COMPOSED_PROFILE_AUTHORED && profile!=RF_COMPOSED_PROFILE_AUTHORED_COLLECTION)return RF_FORMAT;
    if(memcmp(p,"RFDS",4)||read32(p+4)!=profile||read32(p+8)!=bytes)return RF_FORMAT;
    if(profile==RF_COMPOSED_PROFILE_AUTHORED_COLLECTION) {
        rf_authored_checkpoint_layout layout;
        return rf_authored_collection_layout_read(data,bytes,&layout);
    }
    if(profile==RF_COMPOSED_PROFILE_AUTHORED) {
        rf_authored_checkpoint_layout layout;
        return rf_authored_checkpoint_layout_read(data,bytes,&layout);
    }
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

int rf_composed_checkpoint_encode_v2(uint32_t profile_id,const rf_player_checkpoint *player,
    const rf_player_checkpoint_catalog *catalog,const void *rfds,uint32_t rfds_bytes,
    const void *remote,uint32_t remote_bytes,uint32_t level_hash,uint32_t catalog_hash,
    void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char packed[RF_PLAYER_CHECKPOINT_BYTES],*p=output;uint32_t base,bytes;int status;
    if(!p || !written || (remote_bytes && !remote))return RF_RANGE;
    status=rfds_header(rfds,rfds_bytes,profile_id);if(status)return status;
    base=RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES+rfds_bytes;
    if(remote_bytes>RF_REMOTE_CHECKPOINT_MAX || remote_bytes>RF_CHECKPOINT_FILE_MAX-base)return RF_RANGE;
    bytes=base+remote_bytes;if(capacity<bytes)return RF_RANGE;
    if(remote_bytes){status=rf_remote_checkpoint_preflight(remote,remote_bytes,level_hash,catalog_hash);if(status)return status;}
    status=rf_player_checkpoint_encode(player,catalog,packed,sizeof(packed));if(status)return status;
    memset(p,0,RF_COMPOSED_CHECKPOINT_HEADER);memcpy(p,"RFCP",4);
    put32(p+4,2);put32(p+8,bytes);put32(p+16,profile_id);put32(p+20,RF_PLAYER_CHECKPOINT_BYTES);
    put32(p+24,rfds_bytes);put32(p+28,remote_bytes);memcpy(p+RF_COMPOSED_CHECKPOINT_HEADER,packed,sizeof(packed));
    if(rfds!=p+RF_COMPOSED_CHECKPOINT_HEADER+sizeof(packed))memcpy(p+RF_COMPOSED_CHECKPOINT_HEADER+sizeof(packed),rfds,rfds_bytes);
    if(remote_bytes && remote!=p+base)memcpy(p+base,remote,remote_bytes);
    *written=bytes;return RF_OK;
}
int rf_composed_checkpoint_preflight_v2(const void *data,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *catalog,uint32_t level_hash,uint32_t catalog_hash,
    rf_composed_checkpoint_v2 *out)
{
    const unsigned char *p=data;rf_composed_checkpoint_v2 value={0};uint32_t version,remaining;int status;
    if(!p || !out)return RF_RANGE;
    if(bytes<RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES+RF_COMPOSED_CHECKPOINT_RFDS_MIN || bytes>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    version=read32(p+4);
    if(version==1){status=rf_composed_checkpoint_preflight(data,bytes,profile_id,catalog,&value.base);
        if(status)return status;*out=value;return RF_OK;}
    if(memcmp(p,"RFCP",4) || version!=2 || read32(p+8)!=bytes || read32(p+12) || read32(p+16)!=profile_id || read32(p+20)!=RF_PLAYER_CHECKPOINT_BYTES)return RF_FORMAT;
    value.base.rfds_bytes=read32(p+24);value.remote_bytes=read32(p+28);
    remaining=bytes-RF_COMPOSED_CHECKPOINT_HEADER-RF_PLAYER_CHECKPOINT_BYTES;
    if(value.base.rfds_bytes>remaining || value.remote_bytes!=remaining-value.base.rfds_bytes || value.remote_bytes>RF_REMOTE_CHECKPOINT_MAX)return RF_FORMAT;
    value.base.rfds=p+RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES;
    status=rfds_header(value.base.rfds,value.base.rfds_bytes,profile_id);if(status)return status;
    status=rf_player_checkpoint_decode(p+RF_COMPOSED_CHECKPOINT_HEADER,RF_PLAYER_CHECKPOINT_BYTES,catalog,&value.base.player);if(status)return status;
    if(value.remote_bytes){value.remote=value.base.rfds+value.base.rfds_bytes;
        status=rf_remote_checkpoint_preflight(value.remote,value.remote_bytes,level_hash,catalog_hash);if(status)return status;}
    *out=value;return RF_OK;
}
