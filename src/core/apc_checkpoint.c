#include "rf/apc_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFVC2 requires binary32 storage");
static uint32_t apc_word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void apc_put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t apc_hash(const unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<128;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static void apc_float(unsigned char *p,float value)
{uint32_t bits;memcpy(&bits,&value,4);apc_put(p,bits);}
static float apc_read_float(const unsigned char *p)
{uint32_t bits=apc_word(p);float value;memcpy(&value,&bits,4);return value;}

int rf_apc_checkpoint_validate(const rf_apc_checkpoint *value)
{
    const rf_vehicle_checkpoint *v;const float *m;uint32_t i,j,k;double determinant;
    if(!value)return RF_RANGE;
    v=&value->vehicle;
    if(v->alive>1 || v->player_occupied>1 || v->accepted_drill_cuts || v->drill_spin!=0)return RF_FORMAT;
    if(!isfinite(v->health) || v->health< -1000000 || v->health>5000 || !isfinite(v->armor) || v->armor!=0 ||
       v->alive!=(uint32_t)(v->health>0))return RF_FORMAT;
    if(value->primary_reserve>999 || value->secondary_reserve>15 || !isfinite(value->aim_pitch) ||
       value->aim_pitch< -20*.01745329238474369f || value->aim_pitch>50*.01745329238474369f || value->aim_yaw!=0)return RF_FORMAT;
    /* Same bounded pose/motion policy as vehicle_checkpoint.c, duplicated
     * deliberately until common profile validation is factored. No clamping
     * of APC health into the Driller range or temporary legacy wire format. */
    for(i=0;i<3;i++)if(!isfinite(v->position[i]) || fabsf(v->position[i])>1000000 ||
        !isfinite(v->velocity[i]) || fabsf(v->velocity[i])>1000 ||
        !isfinite(v->angular_velocity[i]) || fabsf(v->angular_velocity[i])>100)return RF_FORMAT;
    m=v->orientation;for(i=0;i<9;i++)if(!isfinite(m[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=i;j<3;j++){
        double dot=0;for(k=0;k<3;k++)dot+=(double)m[i*3+k]*m[j*3+k];
        if(fabs(dot-(i==j?1:0))>1e-3)return RF_FORMAT;
    }
    determinant=(double)m[0]*((double)m[4]*m[8]-(double)m[5]*m[7])-
        (double)m[1]*((double)m[3]*m[8]-(double)m[5]*m[6])+
        (double)m[2]*((double)m[3]*m[7]-(double)m[4]*m[6]);
    return fabs(determinant-1)<=1e-3?RF_OK:RF_FORMAT;
}
int rf_apc_checkpoint_encode(const rf_apc_checkpoint *v,void *out,uint32_t bytes)
{
    unsigned char p[128]={0};uint32_t i;int status;
    if(!out||bytes!=RF_APC_CHECKPOINT_BYTES)return RF_RANGE;
    status=rf_apc_checkpoint_validate(v);if(status)return status;
    memcpy(p,"RFVC",4);apc_put(p+4,2);apc_put(p+8,128);apc_put(p+16,2);
    apc_put(p+20,v->vehicle.alive|(v->vehicle.player_occupied<<1));
    apc_float(p+24,v->vehicle.health);apc_float(p+28,v->vehicle.armor);
    for(i=0;i<3;i++){apc_float(p+32+i*4,v->vehicle.position[i]);apc_float(p+80+i*4,v->vehicle.velocity[i]);apc_float(p+92+i*4,v->vehicle.angular_velocity[i]);}
    for(i=0;i<9;i++)apc_float(p+44+i*4,v->vehicle.orientation[i]);
    apc_put(p+104,v->primary_reserve);apc_put(p+108,v->secondary_reserve);
    apc_float(p+112,v->aim_pitch);apc_float(p+116,v->aim_yaw);apc_put(p+120,v->primary_random);
    apc_put(p+12,apc_hash(p));memcpy(out,p,sizeof(p));return RF_OK;
}
int rf_apc_checkpoint_decode(const void *data,uint32_t bytes,rf_apc_checkpoint *out)
{
    const unsigned char *p=data;rf_apc_checkpoint value={0};uint32_t i,flags;int status;
    if(!data||!out)return RF_RANGE;
    if(bytes!=RF_APC_CHECKPOINT_BYTES||memcmp(p,"RFVC",4)||apc_word(p+4)!=2||apc_word(p+8)!=128||
       apc_word(p+16)!=2||apc_word(p+124)||apc_word(p+12)!=apc_hash(p))return RF_FORMAT;
    flags=apc_word(p+20);if(flags&~3u)return RF_FORMAT;
    value.vehicle.alive=flags&1;value.vehicle.player_occupied=(flags>>1)&1;
    value.vehicle.health=apc_read_float(p+24);value.vehicle.armor=apc_read_float(p+28);
    for(i=0;i<3;i++){value.vehicle.position[i]=apc_read_float(p+32+i*4);value.vehicle.velocity[i]=apc_read_float(p+80+i*4);value.vehicle.angular_velocity[i]=apc_read_float(p+92+i*4);}
    for(i=0;i<9;i++)value.vehicle.orientation[i]=apc_read_float(p+44+i*4);
    value.primary_reserve=apc_word(p+104);value.secondary_reserve=apc_word(p+108);
    value.aim_pitch=apc_read_float(p+112);value.aim_yaw=apc_read_float(p+116);value.primary_random=apc_word(p+120);
    status=rf_apc_checkpoint_validate(&value);if(status)return status;
    *out=value;return RF_OK;
}
