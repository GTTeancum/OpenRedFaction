#include "rf/player_checkpoint.h"
#include "rf/eye.h"
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFPL requires binary32 storage");
static uint32_t read32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(unsigned char *p,uint32_t v)
{unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static float read_float(const unsigned char *p)
{uint32_t v=read32(p);float f;memcpy(&f,&v,4);return f;}
static void put_float(unsigned char *p,float f)
{uint32_t v;memcpy(&v,&f,4);put32(p,v);}
static int basis_valid(const float m[9])
{
 unsigned i,j,k;double determinant;
 for(i=0;i<9;i++)if(!isfinite(m[i]))return 0;
 for(i=0;i<3;i++)for(j=i;j<3;j++){double dot=0;for(k=0;k<3;k++)dot+=(double)m[i*3+k]*m[j*3+k];if(fabs(dot-(i==j?1:0))>1e-4)return 0;}
 determinant=(double)m[0]*(m[4]*m[8]-m[5]*m[7])-(double)m[1]*(m[3]*m[8]-m[5]*m[6])+(double)m[2]*(m[3]*m[7]-m[4]*m[6]);
 return fabs(determinant-1)<1e-4;
}
int rf_player_checkpoint_validate(const rf_player_checkpoint *v,const rf_player_checkpoint_catalog *c,float body[9],float eye[9])
{
 uint32_t i;uint8_t used[32]={0};float b[9],e[9],angles[3];int status;
 if(!v || !c || !c->count || c->count>64 || !isfinite(c->health_capacity) || c->health_capacity<=0 ||
    !isfinite(c->armor_capacity) || c->armor_capacity<0)return RF_RANGE;
 for(i=0;i<32;i++)if(c->reserve_capacity[i]<0)return RF_RANGE;
 for(i=0;i<64;i++) {
  const rf_weapon_acquire_definition *d=c->weapons+i;
  if(c->supported[i]>1 || (i>=c->count && c->supported[i]))return RF_RANGE;
  if(!c->supported[i])continue;
  if(d->ammo_type < -1 || d->ammo_type>=32 || d->magazine<0 || d->capacity<0 || (d->ammo_type<0 && d->magazine))return RF_RANGE;
  if(d->ammo_type>=0)used[d->ammo_type]=1;
 }
 if(v->player.catalog_hash!=c->hash || !isfinite(v->player.health) || v->player.health<=0 || v->player.health>c->health_capacity ||
    !isfinite(v->player.armor) || v->player.armor<0 || v->player.armor>c->armor_capacity)return RF_FORMAT;
 for(i=0;i<64;i++) {
  uint32_t owned=v->player.inventory.owned[i];int32_t loaded=v->player.inventory.loaded[i];
  if(owned>1 || loaded<0 || (!c->supported[i] && (owned || loaded)) || (loaded && !owned) ||
     (c->supported[i] && loaded>c->weapons[i].magazine))return RF_FORMAT;
 }
 for(i=0;i<32;i++)if(v->player.inventory.reserve[i]<0 || v->player.inventory.reserve[i]>c->reserve_capacity[i] || (!used[i] && v->player.inventory.reserve[i]))return RF_FORMAT;
 if(v->player.weapon!=UINT32_MAX && (v->player.weapon>=c->count || !c->supported[v->player.weapon] || !v->player.inventory.owned[v->player.weapon]))return RF_FORMAT;
 for(i=0;i<3;i++)if(!isfinite(v->position[i]) || !isfinite(v->body_angles[i]) || !isfinite(v->eye_angles[i]))return RF_FORMAT;
 for(i=0;i<3;i++)if(!isfinite(v->velocity[i]) || fabsf(v->velocity[i])>.001f)return RF_FORMAT;
 if(v->body_angles[0]!=0 || v->body_angles[2]!=0 || v->eye_angles[1]!=0 || v->eye_angles[2]!=0)return RF_FORMAT;
 if(v->body_angles[1]<-6.2831854820251465f || v->body_angles[1]>6.2831854820251465f ||
    v->eye_angles[0]<-1.5707963705062866f || v->eye_angles[0]>1.5707963705062866f)return RF_FORMAT;
 status=rf_look_orientation(v->body_angles,b);if(status)return status;
 for(i=0;i<3;i++)angles[i]=v->body_angles[i]+v->eye_angles[i];
 status=rf_look_orientation(angles,e);if(status)return status;
 if(!basis_valid(b) || !basis_valid(e))return RF_FORMAT;
 if(body)memcpy(body,b,sizeof(b));if(eye)memcpy(eye,e,sizeof(e));return RF_OK;
}
int rf_player_checkpoint_encode(const rf_player_checkpoint *v,const rf_player_checkpoint_catalog *c,void *data,uint32_t bytes)
{
 unsigned char *p=data;uint32_t i;int status;if(!data || bytes!=RF_PLAYER_CHECKPOINT_BYTES)return RF_RANGE;
 status=rf_player_checkpoint_validate(v,c,NULL,NULL);if(status)return status;
 {static const float zero[3]={0};
 memset(p,0,bytes);memcpy(p,"RFPL",4);put32(p+4,memcmp(v->velocity,zero,12)?2:1);put32(p+8,bytes);}
 put32(p+16,v->player.catalog_hash);put32(p+20,v->player.weapon);put_float(p+24,v->player.health);put_float(p+28,v->player.armor);
 for(i=0;i<3;i++){put_float(p+32+i*4,v->position[i]);put_float(p+44+i*4,v->body_angles[i]);put_float(p+56+i*4,v->eye_angles[i]);}
 for(i=0;i<3;i++)put_float(p+68+i*4,v->velocity[i]);
 memcpy(p+80,v->player.inventory.owned,64);
 for(i=0;i<32;i++)put32(p+144+i*4,(uint32_t)v->player.inventory.reserve[i]);
 for(i=0;i<64;i++)put32(p+272+i*4,(uint32_t)v->player.inventory.loaded[i]);return RF_OK;
}
int rf_player_checkpoint_decode(const void *data,uint32_t bytes,const rf_player_checkpoint_catalog *c,rf_player_checkpoint *out)
{
 const unsigned char *p=data;rf_player_checkpoint v={0};uint32_t i,n;int status;
 if(!data || !out)return RF_RANGE;
 if(bytes!=RF_PLAYER_CHECKPOINT_BYTES || memcmp(p,"RFPL",4) || (read32(p+4)!=1 && read32(p+4)!=2) || read32(p+8)!=bytes || read32(p+12))return RF_FORMAT;
 if(read32(p+4)==1)for(i=68;i<80;i++)if(p[i])return RF_FORMAT;for(i=528;i<544;i++)if(p[i])return RF_FORMAT;
 v.player.catalog_hash=read32(p+16);v.player.weapon=read32(p+20);v.player.health=read_float(p+24);v.player.armor=read_float(p+28);
 for(i=0;i<3;i++){v.position[i]=read_float(p+32+i*4);v.body_angles[i]=read_float(p+44+i*4);v.eye_angles[i]=read_float(p+56+i*4);}
 if(read32(p+4)==2)for(i=0;i<3;i++)v.velocity[i]=read_float(p+68+i*4);
 memcpy(v.player.inventory.owned,p+80,64);
 for(i=0;i<32;i++){n=read32(p+144+i*4);if(n>INT32_MAX)return RF_FORMAT;v.player.inventory.reserve[i]=(int32_t)n;}
 for(i=0;i<64;i++){n=read32(p+272+i*4);if(n>INT32_MAX)return RF_FORMAT;v.player.inventory.loaded[i]=(int32_t)n;}
 status=rf_player_checkpoint_validate(&v,c,NULL,NULL);if(status)return status;*out=v;return RF_OK;
}
int rf_player_checkpoint_vehicle_profile_validate(uint32_t profile,const void *record,uint32_t *occupied)
{
 int status;uint32_t value;
 if(!record||!occupied)return RF_RANGE;
 switch(profile){
 case 1:status=rf_vehicle_checkpoint_validate(record);value=((const rf_vehicle_checkpoint*)record)->player_occupied;break;
 case 2:status=rf_apc_checkpoint_validate(record);value=((const rf_apc_checkpoint*)record)->vehicle.player_occupied;break;
 case 3:status=rf_jeep_checkpoint_validate(record);value=((const rf_jeep_checkpoint*)record)->vehicle.player_occupied;break;
 default:return RF_FORMAT;
 }
 if(status)return status;
 *occupied=value;return RF_OK;
}
int rf_player_checkpoint_seated_profile_validate(const rf_player_checkpoint *v,const rf_player_checkpoint_catalog *c,
 uint32_t profile,const void *vehicle)
{
 uint32_t i,occupied;int status;
 if(!v||!vehicle)return RF_RANGE;
 status=rf_player_checkpoint_vehicle_profile_validate(profile,vehicle,&occupied);if(status)return status;
 if(!occupied)return RF_FORMAT;
 status=rf_player_checkpoint_validate(v,c,NULL,NULL);if(status)return status;
 for(i=0;i<3;i++)if(v->velocity[i]!=0)return RF_FORMAT;
 return RF_OK;
}
int rf_player_checkpoint_seated_profile_encode(const rf_player_checkpoint *v,const rf_player_checkpoint_catalog *c,
 uint32_t profile,const void *vehicle,void *data,uint32_t bytes)
{
 unsigned char packed[RF_PLAYER_CHECKPOINT_BYTES];int status;
 if(!data||bytes!=RF_PLAYER_CHECKPOINT_BYTES)return RF_RANGE;
 status=rf_player_checkpoint_seated_profile_validate(v,c,profile,vehicle);if(status)return status;
 status=rf_player_checkpoint_encode(v,c,packed,sizeof(packed));if(status)return status;
 put32(packed+4,3);put32(packed+12,1);memset(packed+68,0,12);
 memcpy(data,packed,sizeof(packed));return RF_OK;
}
int rf_player_checkpoint_seated_profile_decode(const void *data,uint32_t bytes,const rf_player_checkpoint_catalog *c,
 uint32_t profile,const void *vehicle,rf_player_checkpoint *out)
{
 const unsigned char *p=data;unsigned char packed[RF_PLAYER_CHECKPOINT_BYTES];rf_player_checkpoint v;int status;
 if(!data||!out||!vehicle)return RF_RANGE;
 if(bytes!=RF_PLAYER_CHECKPOINT_BYTES||memcmp(p,"RFPL",4)||read32(p+4)!=3||read32(p+8)!=bytes||read32(p+12)!=1)return RF_FORMAT;
 /* Reuse all established scalar/inventory/reserved-byte validation without
  * allowing legacy callers to consume a driver record as a standing player. */
 memcpy(packed,p,sizeof(packed));put32(packed+4,1);put32(packed+12,0);
 status=rf_player_checkpoint_decode(packed,sizeof(packed),c,&v);if(status)return status;
 status=rf_player_checkpoint_seated_profile_validate(&v,c,profile,vehicle);if(status)return status;
 *out=v;return RF_OK;
}
int rf_player_checkpoint_seated_validate(const rf_player_checkpoint *v,const rf_player_checkpoint_catalog *c,const rf_vehicle_checkpoint *vehicle)
{return rf_player_checkpoint_seated_profile_validate(v,c,1,vehicle);}
int rf_player_checkpoint_seated_encode(const rf_player_checkpoint *v,const rf_player_checkpoint_catalog *c,const rf_vehicle_checkpoint *vehicle,void *data,uint32_t bytes)
{return rf_player_checkpoint_seated_profile_encode(v,c,1,vehicle,data,bytes);}
int rf_player_checkpoint_seated_decode(const void *data,uint32_t bytes,const rf_player_checkpoint_catalog *c,const rf_vehicle_checkpoint *vehicle,rf_player_checkpoint *out)
{return rf_player_checkpoint_seated_profile_decode(data,bytes,c,1,vehicle,out);}
