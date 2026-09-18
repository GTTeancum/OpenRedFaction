#include "rf/composed_checkpoint.h"
#include "rf/authored_checkpoint_layout.h"
#include "rf/remote_checkpoint.h"
#include "rf/vehicle_checkpoint.h"
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
#include "rf/submarine_checkpoint.h"
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

typedef union composed_vehicle_candidate {
    rf_vehicle_checkpoint driller;rf_apc_checkpoint apc;rf_jeep_checkpoint jeep;rf_submarine_checkpoint submarine;
} composed_vehicle_candidate;
static int composed_vehicle_decode(const void *data,uint32_t bytes,composed_vehicle_candidate *candidate,
    uint32_t *profile,uint32_t *occupied,const void **record)
{
    const unsigned char *p=data;uint32_t kind;const void *value;int status;
    if(!p||!candidate||!profile||!occupied||!record)return RF_RANGE;
    if(bytes!=RF_VEHICLE_CHECKPOINT_BYTES&&bytes!=RF_JEEP_CHECKPOINT_BYTES)return RF_FORMAT;
    kind=read32(p+16);
    switch(kind){
    case 1:status=rf_vehicle_checkpoint_decode(data,bytes,&candidate->driller);value=&candidate->driller;break;
    case 2:status=rf_apc_checkpoint_decode(data,bytes,&candidate->apc);value=&candidate->apc;break;
    case 3:status=rf_jeep_checkpoint_decode(data,bytes,&candidate->jeep);value=&candidate->jeep;break;
    case 4:status=rf_submarine_checkpoint_decode(data,bytes,&candidate->submarine);value=&candidate->submarine;break;
    default:return RF_FORMAT;
    }
    if(status)return status;
    status=rf_player_checkpoint_vehicle_profile_validate(kind,value,occupied);if(status)return status;
    *record=value;*profile=kind;return RF_OK;
}
int rf_composed_checkpoint_encode_v3(uint32_t profile_id,const rf_player_checkpoint *player,
    const rf_player_checkpoint_catalog *catalog,const void *rfds,uint32_t rfds_bytes,
    const void *remote,uint32_t remote_bytes,uint32_t level_hash,uint32_t catalog_hash,
    const void *vehicle,uint32_t vehicle_bytes,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char packed[RF_PLAYER_CHECKPOINT_BYTES],*p=output;
    composed_vehicle_candidate checked;const void *record=NULL;uint32_t vehicle_profile=0,occupied=0,base,tail,bytes;int status;
    if(!p||!written||(remote_bytes&&!remote)||(vehicle_bytes&&!vehicle))return RF_RANGE;
    if(vehicle_bytes && vehicle_bytes!=RF_VEHICLE_CHECKPOINT_BYTES && vehicle_bytes!=RF_JEEP_CHECKPOINT_BYTES)return RF_FORMAT;
    status=rfds_header(rfds,rfds_bytes,profile_id);if(status)return status;
    base=RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES+rfds_bytes;
    if(remote_bytes>RF_REMOTE_CHECKPOINT_MAX||remote_bytes>RF_CHECKPOINT_FILE_MAX-base)return RF_RANGE;
    tail=base+remote_bytes;
    if(vehicle_bytes>RF_CHECKPOINT_FILE_MAX-tail)return RF_RANGE;
    bytes=tail+vehicle_bytes;if(capacity<bytes)return RF_RANGE;
    if(remote_bytes){status=rf_remote_checkpoint_preflight(remote,remote_bytes,level_hash,catalog_hash);if(status)return status;}
    if(vehicle_bytes){status=composed_vehicle_decode(vehicle,vehicle_bytes,&checked,&vehicle_profile,&occupied,&record);if(status)return status;}
    status=occupied?
        rf_player_checkpoint_seated_profile_encode(player,catalog,vehicle_profile,record,packed,sizeof(packed)):
        rf_player_checkpoint_encode(player,catalog,packed,sizeof(packed));if(status)return status;
    memset(p,0,RF_COMPOSED_CHECKPOINT_HEADER);memcpy(p,"RFCP",4);
    put32(p+4,3);put32(p+8,bytes);put32(p+12,vehicle_bytes);put32(p+16,profile_id);
    put32(p+20,RF_PLAYER_CHECKPOINT_BYTES);put32(p+24,rfds_bytes);put32(p+28,remote_bytes);
    memcpy(p+RF_COMPOSED_CHECKPOINT_HEADER,packed,sizeof(packed));
    if(rfds!=p+RF_COMPOSED_CHECKPOINT_HEADER+sizeof(packed))memcpy(p+RF_COMPOSED_CHECKPOINT_HEADER+sizeof(packed),rfds,rfds_bytes);
    if(remote_bytes&&remote!=p+base)memcpy(p+base,remote,remote_bytes);
    if(vehicle_bytes&&vehicle!=p+tail)memcpy(p+tail,vehicle,vehicle_bytes);
    *written=bytes;return RF_OK;
}
int rf_composed_checkpoint_preflight_v3(const void *data,uint32_t bytes,uint32_t profile_id,
    const rf_player_checkpoint_catalog *catalog,uint32_t level_hash,uint32_t catalog_hash,
    rf_composed_checkpoint_v3 *out)
{
    const unsigned char *p=data;rf_composed_checkpoint_v3 value={0};composed_vehicle_candidate checked;const void *record=NULL;
    uint32_t version,remaining,occupied=0;int status;
    if(!p||!out)return RF_RANGE;
    if(bytes<RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES+RF_COMPOSED_CHECKPOINT_RFDS_MIN||bytes>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    version=read32(p+4);
    if(version==1||version==2){
        status=rf_composed_checkpoint_preflight_v2(data,bytes,profile_id,catalog,level_hash,catalog_hash,&value.base);
        if(status)return status;
        *out=value;return RF_OK;
    }
    if(memcmp(p,"RFCP",4)||version!=3||read32(p+8)!=bytes||read32(p+16)!=profile_id||read32(p+20)!=RF_PLAYER_CHECKPOINT_BYTES)return RF_FORMAT;
    value.vehicle_bytes=read32(p+12);value.base.base.rfds_bytes=read32(p+24);value.base.remote_bytes=read32(p+28);
    if(value.vehicle_bytes&&value.vehicle_bytes!=RF_VEHICLE_CHECKPOINT_BYTES&&value.vehicle_bytes!=RF_JEEP_CHECKPOINT_BYTES)return RF_FORMAT;
    remaining=bytes-RF_COMPOSED_CHECKPOINT_HEADER-RF_PLAYER_CHECKPOINT_BYTES;
    if(value.base.base.rfds_bytes>remaining)return RF_FORMAT;
    remaining-=value.base.base.rfds_bytes;
    if(value.base.remote_bytes>remaining||value.base.remote_bytes>RF_REMOTE_CHECKPOINT_MAX)return RF_FORMAT;
    remaining-=value.base.remote_bytes;if(value.vehicle_bytes!=remaining)return RF_FORMAT;
    value.base.base.rfds=p+RF_COMPOSED_CHECKPOINT_HEADER+RF_PLAYER_CHECKPOINT_BYTES;
    status=rfds_header(value.base.base.rfds,value.base.base.rfds_bytes,profile_id);if(status)return status;
    if(value.base.remote_bytes){value.base.remote=value.base.base.rfds+value.base.base.rfds_bytes;
        status=rf_remote_checkpoint_preflight(value.base.remote,value.base.remote_bytes,level_hash,catalog_hash);if(status)return status;}
    if(value.vehicle_bytes){value.vehicle=value.base.base.rfds+value.base.base.rfds_bytes+value.base.remote_bytes;
        status=composed_vehicle_decode(value.vehicle,value.vehicle_bytes,&checked,&value.vehicle_profile,&occupied,&record);if(status)return status;}
    status=occupied?
        rf_player_checkpoint_seated_profile_decode(p+RF_COMPOSED_CHECKPOINT_HEADER,RF_PLAYER_CHECKPOINT_BYTES,catalog,value.vehicle_profile,record,&value.base.base.player):
        rf_player_checkpoint_decode(p+RF_COMPOSED_CHECKPOINT_HEADER,RF_PLAYER_CHECKPOINT_BYTES,catalog,&value.base.base.player);
    if(status)return status;
    *out=value;return RF_OK;
}
