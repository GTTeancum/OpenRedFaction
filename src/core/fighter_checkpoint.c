#include "rf/fighter_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFVC5 requires binary32 storage");
static uint32_t fighter_word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void fighter_put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static uint32_t fighter_hash(const unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<160;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static void fighter_float(unsigned char *p,float value)
{uint32_t bits;memcpy(&bits,&value,4);fighter_put(p,bits);}
static float fighter_read_float(const unsigned char *p)
{uint32_t bits=fighter_word(p);float value;memcpy(&value,&bits,4);return value;}

int rf_fighter_checkpoint_validate(const rf_fighter_checkpoint *value)
{
    const rf_vehicle_checkpoint *v;const float *m;uint32_t i,j,k;double determinant;
    if(!value)return RF_RANGE;
    v=&value->vehicle;
    if(v->alive>1 || v->player_occupied>1 || v->accepted_drill_cuts || v->drill_spin!=0)return RF_FORMAT;
    if(!isfinite(v->health) || v->health< -1000000 || v->health>900 || !isfinite(v->armor) || v->armor!=0 ||
       v->alive!=(uint32_t)(v->health>0))return RF_FORMAT;
    if(value->primary_reserve>900 || value->rocket_reserve>20 ||
       !isfinite(value->primary_cooldown) || value->primary_cooldown<-.05f || value->primary_cooldown>.05f ||
       !isfinite(value->rocket_cooldown) || value->rocket_cooldown<-.05f || value->rocket_cooldown>3)return RF_FORMAT;
    /* Same bounded pose/motion policy as vehicle_checkpoint.c, duplicated
     * deliberately until common profile validation is factored. No clamping
     * of FIGHTER health into the Driller range or temporary legacy wire format. */
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
int rf_fighter_checkpoint_encode(const rf_fighter_checkpoint *v,void *out,uint32_t bytes)
{
    unsigned char p[160]={0};uint32_t i;int status;
    if(!out||bytes!=RF_FIGHTER_CHECKPOINT_BYTES)return RF_RANGE;
    status=rf_fighter_checkpoint_validate(v);if(status)return status;
    memcpy(p,"RFVC",4);fighter_put(p+4,5);fighter_put(p+8,160);fighter_put(p+16,5);
    fighter_put(p+20,v->vehicle.alive|(v->vehicle.player_occupied<<1));
    fighter_float(p+24,v->vehicle.health);fighter_float(p+28,v->vehicle.armor);
    for(i=0;i<3;i++){fighter_float(p+32+i*4,v->vehicle.position[i]);fighter_float(p+80+i*4,v->vehicle.velocity[i]);fighter_float(p+92+i*4,v->vehicle.angular_velocity[i]);}
    for(i=0;i<9;i++)fighter_float(p+44+i*4,v->vehicle.orientation[i]);
    fighter_put(p+104,v->primary_reserve);fighter_put(p+108,v->rocket_reserve);
    fighter_float(p+112,v->primary_cooldown);fighter_float(p+116,v->rocket_cooldown);
    fighter_put(p+120,v->primary_shots);fighter_put(p+124,v->rocket_shots);
    fighter_put(p+12,fighter_hash(p));memcpy(out,p,sizeof(p));return RF_OK;
}
int rf_fighter_checkpoint_decode(const void *data,uint32_t bytes,rf_fighter_checkpoint *out)
{
    const unsigned char *p=data;rf_fighter_checkpoint value={0};uint32_t i,flags;int status;
    if(!data||!out)return RF_RANGE;
    if(bytes!=RF_FIGHTER_CHECKPOINT_BYTES||memcmp(p,"RFVC",4)||fighter_word(p+4)!=5||fighter_word(p+8)!=160||
       fighter_word(p+16)!=5||fighter_word(p+12)!=fighter_hash(p))return RF_FORMAT;
    for(i=128;i<160;i++)if(p[i])return RF_FORMAT;
    flags=fighter_word(p+20);if(flags&~3u)return RF_FORMAT;
    value.vehicle.alive=flags&1;value.vehicle.player_occupied=(flags>>1)&1;
    value.vehicle.health=fighter_read_float(p+24);value.vehicle.armor=fighter_read_float(p+28);
    for(i=0;i<3;i++){value.vehicle.position[i]=fighter_read_float(p+32+i*4);value.vehicle.velocity[i]=fighter_read_float(p+80+i*4);value.vehicle.angular_velocity[i]=fighter_read_float(p+92+i*4);}
    for(i=0;i<9;i++)value.vehicle.orientation[i]=fighter_read_float(p+44+i*4);
    value.primary_reserve=fighter_word(p+104);value.rocket_reserve=fighter_word(p+108);
    value.primary_cooldown=fighter_read_float(p+112);value.rocket_cooldown=fighter_read_float(p+116);
    value.primary_shots=fighter_word(p+120);value.rocket_shots=fighter_word(p+124);
    status=rf_fighter_checkpoint_validate(&value);if(status)return status;
    *out=value;return RF_OK;
}
