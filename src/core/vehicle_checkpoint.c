#include "rf/vehicle_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFVC requires binary32 storage");
static uint32_t read32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static float read_float(const unsigned char *p)
{uint32_t u=read32(p);float f;memcpy(&f,&u,4);return f;}
static void put_float(unsigned char *p,float f)
{uint32_t u;memcpy(&u,&f,4);put32(p,u);}
static uint32_t checksum(const unsigned char *p)
{uint32_t h=2166136261u,i;for(i=0;i<RF_VEHICLE_CHECKPOINT_BYTES;i++){h^=(i>=12 && i<16)?0u:p[i];h*=16777619u;}return h;}
int rf_vehicle_checkpoint_validate(const rf_vehicle_checkpoint *v)
{
    uint32_t i,j,k,spin_bits=0x412fede0u;float spin_limit;double determinant;const float *m;
    if(!v)return RF_RANGE;
    if(v->alive>1 || v->player_occupied>1 || v->accepted_drill_cuts>25)return RF_FORMAT;
    if(!isfinite(v->health) || v->health< -1000000 || v->health>900 || !isfinite(v->armor) || v->armor!=0 ||
       v->alive!=(uint32_t)(v->health>0))return RF_FORMAT;
    memcpy(&spin_limit,&spin_bits,4);
    if(!isfinite(v->drill_spin) || v->drill_spin<0 || v->drill_spin>spin_limit)return RF_FORMAT;
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
int rf_vehicle_checkpoint_encode(const rf_vehicle_checkpoint *v,void *data,uint32_t bytes)
{
    unsigned char p[RF_VEHICLE_CHECKPOINT_BYTES]={0};uint32_t i;int status;
    if(!data || bytes!=RF_VEHICLE_CHECKPOINT_BYTES)return RF_RANGE;
    status=rf_vehicle_checkpoint_validate(v);if(status)return status;
    memcpy(p,"RFVC",4);put32(p+4,1);put32(p+8,RF_VEHICLE_CHECKPOINT_BYTES);put32(p+16,1);
    put32(p+20,v->alive|(v->player_occupied<<1));put_float(p+24,v->health);put_float(p+28,v->armor);
    for(i=0;i<3;i++){put_float(p+32+i*4,v->position[i]);put_float(p+80+i*4,v->velocity[i]);put_float(p+92+i*4,v->angular_velocity[i]);}
    for(i=0;i<9;i++)put_float(p+44+i*4,v->orientation[i]);
    put32(p+104,v->accepted_drill_cuts);put_float(p+108,v->drill_spin);put32(p+12,checksum(p));
    memcpy(data,p,sizeof(p));return RF_OK;
}
int rf_vehicle_checkpoint_decode(const void *data,uint32_t bytes,rf_vehicle_checkpoint *out)
{
    const unsigned char *p=data;rf_vehicle_checkpoint v={0};uint32_t i,flags;int status;
    if(!data || !out)return RF_RANGE;
    if(bytes!=RF_VEHICLE_CHECKPOINT_BYTES)return RF_FORMAT;
    if(memcmp(p,"RFVC",4) || read32(p+4)!=1 || read32(p+8)!=RF_VEHICLE_CHECKPOINT_BYTES ||
       read32(p+16)!=1 || read32(p+12)!=checksum(p))return RF_FORMAT;
    flags=read32(p+20);if(flags&~3u)return RF_FORMAT;
    for(i=112;i<RF_VEHICLE_CHECKPOINT_BYTES;i++)if(p[i])return RF_FORMAT;
    v.alive=flags&1;v.player_occupied=(flags>>1)&1;v.health=read_float(p+24);v.armor=read_float(p+28);
    for(i=0;i<3;i++){v.position[i]=read_float(p+32+i*4);v.velocity[i]=read_float(p+80+i*4);v.angular_velocity[i]=read_float(p+92+i*4);}
    for(i=0;i<9;i++)v.orientation[i]=read_float(p+44+i*4);
    v.accepted_drill_cuts=read32(p+104);v.drill_spin=read_float(p+108);
    status=rf_vehicle_checkpoint_validate(&v);if(status)return status;
    *out=v;return RF_OK;
}
