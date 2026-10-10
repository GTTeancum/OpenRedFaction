#include "rf/remote_checkpoint.h"
#include <math.h>
#include <string.h>
static uint32_t remote_read32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void remote_write32(unsigned char *p,uint32_t n)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
/* One field list defines both directions without relying on struct layout. */
static void remote_record(unsigned char *bytes,rf_remote_charge *c,uint32_t *slot,uint32_t *tag,uint32_t *owner_key,uint32_t *host_key,uint32_t read)
{
    uint32_t at=0,i,bits;
#define WORD(x) do{if(read)(x)=remote_read32(bytes+at);else remote_write32(bytes+at,(x));at+=4;}while(0)
#define FLOAT(x) do{if(read){bits=remote_read32(bytes+at);memcpy(&(x),&bits,4);}else{memcpy(&bits,&(x),4);remote_write32(bytes+at,bits);}at+=4;}while(0)
#define VECTOR(x,n) do{for(i=0;i<(n);i++)FLOAT((x)[i]);}while(0)
    WORD(*slot);WORD(*tag);WORD(*owner_key);WORD(*host_key);
    VECTOR(c->flight.position,3);VECTOR(c->flight.velocity,3);FLOAT(c->flight.radius);WORD(c->flight.resting);
    FLOAT(c->flight.lifecycle.fuse);FLOAT(c->flight.lifecycle.life);WORD(c->flight.lifecycle.active);
    WORD(c->flight.lifecycle.class_flags);WORD(c->flight.lifecycle.instance_flags);
    WORD(c->owner);WORD(c->type);WORD(c->object_flags);WORD(c->attached);
    FLOAT(c->contact.hit.fraction);VECTOR(c->contact.hit.point,3);VECTOR(c->contact.hit.normal,3);
    WORD(c->contact.object);WORD(c->contact.room);WORD(c->contact.face);
    FLOAT(c->authored_fuse);VECTOR(c->orientation,9);VECTOR(c->local_offset,3);VECTOR(c->local_orientation,9);
    WORD(c->host);WORD(c->host_bound);WORD(c->attachment_initialized);WORD(c->physics_flags);
#undef VECTOR
#undef FLOAT
#undef WORD
}
static int remote_charge_valid(const rf_remote_charge *c,uint32_t owner_key,uint32_t host_key)
{
    uint32_t i;
    if(c->flight.lifecycle.active!=1 || !(c->flight.lifecycle.class_flags&0x40) || c->flight.resting>1 ||
       c->attached>1 || c->host_bound>1 || c->attachment_initialized>1 || owner_key==UINT32_MAX ||
       !isfinite(c->flight.radius) || c->flight.radius<0 || !isfinite(c->flight.lifecycle.fuse) ||
       !isfinite(c->flight.lifecycle.life) || !isfinite(c->authored_fuse) || c->authored_fuse<=0 ||
       !isfinite(c->contact.hit.fraction) || c->contact.hit.fraction<0 || c->contact.hit.fraction>1)return 0;
    if(c->host_bound && (!c->attachment_initialized || !c->attached || c->host==UINT32_MAX || host_key==UINT32_MAX))return 0;
    if(c->attachment_initialized && !(c->flight.lifecycle.instance_flags&0x40))return 0;
    for(i=0;i<3;i++)if(!isfinite(c->flight.position[i]) || !isfinite(c->flight.velocity[i]) ||
        !isfinite(c->contact.hit.point[i]) || !isfinite(c->contact.hit.normal[i]) || !isfinite(c->local_offset[i]))return 0;
    for(i=0;i<9;i++)if(!isfinite(c->orientation[i]) || !isfinite(c->local_orientation[i]))return 0;
    return 1;
}
static int remote_scheduler_valid(uint32_t held,uint32_t pending,uint32_t delay,uint32_t cooldown)
{return held<=3 && pending<=2 && delay<=21 && cooldown<=45 && (pending || !delay) && (pending!=2 || delay<=15);}
static int remote_followup_valid(uint32_t selected,uint32_t pending,uint32_t delay,uint32_t target,uint32_t ticks)
{
    if(!target)return !ticks;
    if(target>2 || !ticks || ticks>RF_REMOTE_FOLLOWUP_MAX_TICKS)return 0;
    if(target==1)return selected==2 && !pending;
    return selected==1 && (!pending || ticks>delay);
}
int rf_remote_checkpoint_encode(const rf_remote_checkpoint *state,uint32_t level,uint32_t catalog,
    void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,count=0,at,header,bytes;
    if(!state || !p || !written)return RF_RANGE;
    if(state->selected_mode>2 || !remote_scheduler_valid(state->held,state->pending,state->delay,state->cooldown) ||
       !remote_followup_valid(state->selected_mode,state->pending,state->delay,state->followup_target,state->followup_ticks))return RF_FORMAT;
    for(i=0;i<32;i++){
        if(state->charges[i].flight.lifecycle.active>1)return RF_FORMAT;
        if(state->charges[i].flight.lifecycle.active){if(!remote_charge_valid(state->charges+i,state->owner_keys[i],state->host_keys[i]))return RF_FORMAT;++count;}}
    header=state->followup_target?RF_REMOTE_CHECKPOINT_HEADER:RF_REMOTE_CHECKPOINT_HEADER_V1;at=header;
    bytes=header+count*RF_REMOTE_CHECKPOINT_RECORD;if(capacity<bytes)return RF_RANGE;
    memset(p,0,header);memcpy(p,"RFRM",4);remote_write32(p+4,state->followup_target?2:1);remote_write32(p+8,bytes);
    remote_write32(p+12,count);remote_write32(p+16,level);remote_write32(p+20,catalog);
    remote_write32(p+24,state->held);remote_write32(p+28,state->pending);remote_write32(p+32,state->delay);remote_write32(p+36,state->cooldown);remote_write32(p+40,state->selected_mode);
    if(state->followup_target){remote_write32(p+48,state->followup_target);remote_write32(p+52,state->followup_ticks);}
    for(i=0;i<32;i++)if(state->charges[i].flight.lifecycle.active){
        rf_remote_charge c=state->charges[i];uint32_t slot=i,tag=state->tags[i],owner_key=state->owner_keys[i],host_key=state->host_keys[i];
        remote_record(p+at,&c,&slot,&tag,&owner_key,&host_key,0);at+=RF_REMOTE_CHECKPOINT_RECORD;}
    *written=bytes;return RF_OK;
}
int rf_remote_checkpoint_preflight(const void *input,uint32_t bytes,uint32_t level,uint32_t catalog)
{
    const unsigned char *p=input;uint32_t count,i,mask=0,slot,tag,owner_key,host_key,version,header;
    rf_remote_charge c;
    if(!p)return RF_RANGE;
    if(bytes<RF_REMOTE_CHECKPOINT_HEADER_V1 || bytes>RF_REMOTE_CHECKPOINT_MAX || memcmp(p,"RFRM",4) ||
       remote_read32(p+8)!=bytes || remote_read32(p+16)!=level || remote_read32(p+20)!=catalog ||
       remote_read32(p+40)>2 || remote_read32(p+44))return RF_FORMAT;
    version=remote_read32(p+4);if(version!=1 && version!=2)return RF_FORMAT;
    header=version==2?RF_REMOTE_CHECKPOINT_HEADER:RF_REMOTE_CHECKPOINT_HEADER_V1;
    count=remote_read32(p+12);if(count>32 || bytes!=header+count*RF_REMOTE_CHECKPOINT_RECORD ||
       !remote_scheduler_valid(remote_read32(p+24),remote_read32(p+28),remote_read32(p+32),remote_read32(p+36)))return RF_FORMAT;
    if(version==2 && !remote_followup_valid(remote_read32(p+40),remote_read32(p+28),remote_read32(p+32),
        remote_read32(p+48),remote_read32(p+52)))return RF_FORMAT;
    /* First pass validates all records before touching the candidate. */
    for(i=0;i<count;i++){
        memset(&c,0,sizeof(c));remote_record((unsigned char*)p+header+i*RF_REMOTE_CHECKPOINT_RECORD,&c,&slot,&tag,&owner_key,&host_key,1);
        if(slot>=32 || (mask&(1u<<slot)) || !remote_charge_valid(&c,owner_key,host_key))return RF_FORMAT;mask|=1u<<slot;}
    return RF_OK;
}
int rf_remote_checkpoint_decode(const void *input,uint32_t bytes,uint32_t level,uint32_t catalog,rf_remote_checkpoint *out)
{
    const unsigned char *p=input;uint32_t count,i,slot,tag,owner_key,host_key,version,header;rf_remote_charge c={0};int status;
    if(!out)return RF_RANGE;
    status=rf_remote_checkpoint_preflight(input,bytes,level,catalog);if(status)return status;
    count=remote_read32(p+12);version=remote_read32(p+4);header=version==2?RF_REMOTE_CHECKPOINT_HEADER:RF_REMOTE_CHECKPOINT_HEADER_V1;
    memset(out,0,sizeof(*out));out->held=remote_read32(p+24);out->pending=remote_read32(p+28);out->delay=remote_read32(p+32);out->cooldown=remote_read32(p+36);out->selected_mode=remote_read32(p+40);
    if(version==2){out->followup_target=remote_read32(p+48);out->followup_ticks=remote_read32(p+52);}
    for(i=0;i<count;i++){
        remote_record((unsigned char*)p+header+i*RF_REMOTE_CHECKPOINT_RECORD,&c,&slot,&tag,&owner_key,&host_key,1);
        out->charges[slot]=c;out->tags[slot]=tag;out->owner_keys[slot]=owner_key;out->host_keys[slot]=host_key;}
    return RF_OK;
}
